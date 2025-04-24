// Declaration of all the libraries
#include <SPI.h>
#include <SD.h>
#include "RTClib.h"

// RTC declaration. Here, we are using PCF8523 RTC
RTC_PCF8523 rtc;
bool headerWritten = false;  // Flag to track whether the header has been written

// Declaration of all the constants
const byte chipSelect = 4;
const byte PPGVolt = A0;
const byte PPGSensor = A1;
const byte Xout = A4;
const byte Yout = A3;
const byte Zout = A2;

File myFile;
bool USB_CONNECTED = false;

bool checkUSBAttached() {
  if (REG_USB_DEVICE_FNUM != 0) {
    USB_CONNECTED = true;
  } else {
    USB_CONNECTED = false;
  }
  return USB_CONNECTED;
}

void debugPrint(String data) {
  if (!USB_CONNECTED) { return; }
  Serial.println(data);
}
void setup() {
  delay(1000);
  if (checkUSBAttached()) {
    Serial.begin(115200);
    rtc.begin();
    debugPrint("USB detected");
    return;
  }
  PM->CPUSEL.bit.CPUDIV = 0x3;  // divide by 8  
  // Reduce clock speed, for details go to engineering notebook
  // PM->CPUSEL.bit.CPUDIV = 0x4; // divide by 16

  // Setting up pins for the device
  pinMode(PPGSensor, INPUT);
  pinMode(Xout, INPUT);
  pinMode(Yout, INPUT);
  pinMode(Zout, INPUT);

  delay(125);

  // Initialize SD card and RTC
  if (!SD.begin(chipSelect) || !rtc.begin()) {
    // The PPG sensor LED will rapidly blink if the SD card is not inserted
    while (true) {
      analogWrite(PPGVolt, 900);  // Set LED to 2.5V (PWM duty cycle 100%)
      delay(4);                   // Wait for 30 milliseconds
      analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
      delay(4);                   // Wait for 30 milliseconds
    }
  } else {
    // PPG Sensor LED blinking 4 times with 100ms delay
    analogWrite(PPGVolt, 800);  // Set LED to 2.5V (PWM duty cycle 100%)
    delay(12);                  // Wait for 100 milliseconds
    analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
    delay(12);                  // Wait for 100 milliseconds
    analogWrite(PPGVolt, 800);  // Set LED to 2.5V (PWM duty cycle 100%)
    delay(12);                  // Wait for 100 milliseconds
    analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
    delay(12);                  // Wait for 100 milliseconds
    analogWrite(PPGVolt, 800);  // Set LED to 2.5V (PWM duty cycle 100%)
    delay(12);                  // Wait for 100 milliseconds
    analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
    delay(12);                  // Wait for 100 milliseconds
    analogWrite(PPGVolt, 800);  // Set LED to 2.5V (PWM duty cycle 100%)
    delay(12);                  // Wait for 100 milliseconds
    analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
    delay(12);                  // Wait for 100 milliseconds
  }

  analogWrite(PPGVolt, 839);  // Set LED to 2.7V to measure 'PPG' or Heart rate

  // Setting the header of the column
  myFile = SD.open("test.csv", FILE_WRITE);
  myFile.println("Date & Time,PPGVal,Xval,Yval,Zval");
  myFile.close();
}

void orgLoop() {
  // PPG and accelerometer temp values
  int PPGVal, Xval, Yval, Zval;
  int delayTime = 2;   // Delay time between samples (2 ms -> 500 Hz sampling rate)
  int loopNums = 350;  // Number of samples to collect before closing/writing file to SD card

  String data = "";

  myFile = SD.open("test.csv", FILE_WRITE);  // Try opening file on the SD card

  if (myFile) {  // if file can be opened, continue as normal
    // Reading data and storing the values to temp values
    for (int i = 1; i <= loopNums; i++) {
      DateTime now = rtc.now();

      // Read data from PPG and accelerometer
      PPGVal = analogRead(PPGSensor);
      Xval = analogRead(Xout);
      Yval = analogRead(Yout);
      Zval = analogRead(Zout);

      // Writing time to the SD card
      myFile.print(now.year(), DEC);
      myFile.print('/');
      myFile.print(now.month(), DEC);
      myFile.print('/');
      myFile.print(now.day(), DEC);
      myFile.print(" ");
      myFile.print(now.hour(), DEC);
      myFile.print(':');
      myFile.print(now.minute(), DEC);
      myFile.print(':');
      myFile.print(now.second(), DEC);

      // Writing PPG and ACC data to the SD card
      myFile.print(',');
      myFile.print(PPGVal);
      myFile.print(',');
      myFile.print(Xval);
      myFile.print(',');
      myFile.print(Yval);
      myFile.print(',');
      myFile.print(Zval);
      myFile.println();

      delay(delayTime);
    }

    myFile.close();

  } else {                      // if the file cannot be opened (e.g. SD card is disconnected)
    analogWrite(PPGVolt, 900);  // Set LED to 2.5V (PWM duty cycle 100%)
    delay(4);                   // Wait for 30 milliseconds
    analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
    delay(4);                   // Wait for 30 milliseconds
  }
}

void readSerialCommands() {
  String commands = "";
  commands = Serial.readString();
  debugPrint(commands);
  if (commands.length() == 0) { return; }
  switch(commands[0]) {
    case 's': {
      if (commands.length() < 22) { 
        debugPrint("Date and time format too short (mm-dd-yyyy-hh-mm-ss");
        return; 
      }
      
        uint16_t year = commands.substring(8,12).toInt();
        uint8_t month = commands.substring(2,4).toInt();
        uint8_t day = commands.substring(5,7).toInt();
        uint8_t hour = commands.substring(13,15).toInt();
        uint8_t minute = commands.substring(16,18).toInt();
        uint8_t second = commands.substring(19,21).toInt();

      DateTime dt(year, month, day, hour, minute, second);
      
      Serial.println("Setting data to Month: " + String(month) + " Day: " + String(day) + " Year " + String(year) + " Hour " + String(hour) + " Minute " + String(minute) + " Second " + String(second));
      rtc.adjust(dt);
      break;
    }
    case 'q': {
      DateTime dt = rtc.now();
      Serial.println("Retrieved data - Month: " + String(dt.month()) + " Day: " + String(dt.day()) + " Year " + String(dt.year()) + " Hour " + String(dt.hour()) + " Minute " + String(dt.minute()) + " Second " + String(dt.second()));
    }
  }
}

void loop() {
  if (!USB_CONNECTED) {
    orgLoop();
  } else {
    if (Serial.available()) {
      readSerialCommands();
    }
  }
}
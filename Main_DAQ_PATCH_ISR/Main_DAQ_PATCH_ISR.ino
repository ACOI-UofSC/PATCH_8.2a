#define USING_TIMER_TC3 true
#include <SAMDTimerInterrupt.h>
SAMDTimer ITimer0(TIMER_TC3);

// Declaration of all the libraries
#include <SPI.h>
#include <SdFat.h>
SdFat SD;

#include "RTClib.h"

// RTC declaration. Here, we are using PCF8523 RTC
RTC_PCF8523 rtc;

// Declaration of all the constants
const byte chipSelect = 4;
const byte PPGVolt = A0;
const byte PPGSensor = A1;
const byte Xout = A4;
const byte Yout = A3;
const byte Zout = A2;

// Define for the CPUDIV used (3 = div 8, 4 = div 16, 5 = div 32)
uint8_t CPUDIVP = 0x4;
// Flag for whether USB is connected or not, only updated once at startup
bool USB_CONNECTED = false;

/*
  The interrupt is called every 1S / updateRate
  
  The software will wait for the second to change before starting sampling in order to star on an even 0.0 seconds
  
  Data for each second is stored without timing information but is evenly spaced according to {updateRate}, each whole second time & date data is stored
  
  In order to avoid collissions in between the interrupt reading data and the SD card saving it to disc,
  only HALF of the data is saved at any time, namely whichever half the sensor is not currently populating
*/

// Sets updates per second
#define updateRate 60

// Structure for sub-second readings
typedef struct {
  uint16_t PPGVal, Xval, Yval, Zval;
} singleReading;

// Structure for each second containing date and time
typedef struct {
  DateTime dt;
  singleReading values[updateRate];
} singleSecond;

// total number of seconds to sample
#define secondsToSample 30
// main data 
singleSecond data[secondsToSample];
// current second reading into {data}
uint8_t currentSecond = 0;
// current sub-second reading
uint8_t currentReading = 0;

// set to true when data is to be saved to SD-card
bool saveFlag = false;
// true = save the low range of data (0 - secondsToSample / 2)
// false = save the high range of data ((secondsToSample / 2) - secondsToSample)
bool saveLowRange = true;

// File handler
File myFile;

// If debugp is defined data will be looged to the serial port
void debugPrint(String data) {
  if (!USB_CONNECTED) { return; }
  Serial.println(data);
}

// Test for disabling the USB port and conserve data
void disableUSB() {
  REG_USB_CTRLA &= (!USB_CTRLA_ENABLE);
  REG_USB_DEVICE_CTRLB |= USB_DEVICE_CTRLB_DETACH;
  REG_USB_DEVICE_INTFLAG |= USB_DEVICE_INTFLAG_SUSPEND;
}

// Write a string, mainly for tests and on-time writes
void writeToFile(String data) {
  myFile = SD.open("test3.csv", O_WRITE | O_CREAT | O_APPEND);
  myFile.println(data);
  myFile.close();
}

// Tests for various USB registers
void debugPrintUSBStatus() {
  writeToFile("STATUS " + String(REG_USB_DEVICE_STATUS));
  debugPrint(String(REG_USB_DEVICE_STATUS));

  writeToFile("CTRLB " + String(REG_USB_DEVICE_CTRLB));
  debugPrint(String(REG_USB_DEVICE_CTRLB));

  writeToFile("DADD " + String(REG_USB_DEVICE_DADD));
  debugPrint(String(REG_USB_DEVICE_CTRLB));

  writeToFile("FNUM " + String(REG_USB_DEVICE_FNUM));
  debugPrint(String(REG_USB_DEVICE_CTRLB));

  writeToFile("INTENCLR " + String(REG_USB_DEVICE_INTENCLR));
  debugPrint(String(REG_USB_DEVICE_CTRLB));

  writeToFile("INTENSET " + String(REG_USB_DEVICE_INTENSET));
  debugPrint(String(REG_USB_DEVICE_CTRLB));

  writeToFile("INTFLAG " + String(REG_USB_DEVICE_INTFLAG));
  debugPrint(String(REG_USB_DEVICE_INTFLAG));
  
  writeToFile("EPINTSMRY " + String(REG_USB_DEVICE_EPINTSMRY));
  debugPrint(String(REG_USB_DEVICE_CTRLB));
}

// Returns true if a SD card is detected
bool startSDCard() {
  return SD.begin(chipSelect, 12000000);
}

// Returns true if a USB device is attached, should be updated in the future as FNUM is probably not reliable
bool checkUSBAttached() {
  if (REG_USB_DEVICE_FNUM != 0) {
    USB_CONNECTED = true;
  } else {
    USB_CONNECTED = false;
  }
  return USB_CONNECTED;
}

void setup() {
  // Setting up pins for the device
  pinMode(PPGSensor, INPUT);
  pinMode(Xout, INPUT);
  pinMode(Yout, INPUT);
  pinMode(Zout, INPUT);

  delay(125);

  // Blink if RTC is busted
  if (!rtc.begin()) {
    while(true) {
      analogWrite(PPGVolt, 800);  // Set LED to 2.5V (PWM duty cycle 100%)
      delay(300);                  // Wait for 100 milliseconds
      analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
      delay(100);                  // Wait for 100 milliseconds
    }
  }

  // Detect SD Card and start if found, just blink if not
  if (!startSDCard()) {
    debugPrint("No SD-Card found");
    while (true) {
      analogWrite(PPGVolt, 900);  // Set LED to 2.5V (PWM duty cycle 100%)
      delay(250);                   // Wait for 30 milliseconds
      analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
      delay(250);                   // Wait for 30 milliseconds
    }
  } else {
    // PPG Sensor LED blinking 4 times with 100ms delay
    for (int i=0; i<3; i++) {
      analogWrite(PPGVolt, 800);  // Set LED to 2.5V (PWM duty cycle 100%)
      delay(100);                  // Wait for 100 milliseconds
      analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
      delay(100);                  // Wait for 100 milliseconds
    }
  }
  analogWrite(PPGVolt, 839);  // Set LED to 2.7V to measure 'PPG' or Heart rate

  // Check if USB is connected, if so go full CPU speed
  // could also check REG_USB_DEVICE_STATUS which is 128 when connected, but not avaliable at startup
  Serial.begin(115200);
  if (checkUSBAttached()) {
    writeToFile(",,,,,USB detected");
    Serial.println("USB detected");
    CPUDIVP = 1;
    USB_CONNECTED = true;
  } else {
    writeToFile(",,,,,No USB detected");
  }
  // Reduce clock speed, for details go to engineering notebook 
  PM->CPUSEL.bit.CPUDIV = CPUDIVP;
  // Don't write data if USB is connected 
  if (!USB_CONNECTED) { writeToFile("Date & Time,PPGVal,Xval,Yval,Zval"); }

  // Wait for the RTC to transition to a new second
  waitForZeroSecond();
  // Set the date & time information for the first data entry
  updateDate();
  // Start the interrupt that samples the data
  ITimer0.attachInterrupt(60, timerHandler);

  // Disable unused peripherals if we're in battery mode
  if (!USB_CONNECTED) { powerDisable(); }
}

// Disables various peripherals
// TODO: Figure out which SERCOM modules could be taken out
void powerDisable() {
  // Disable RTC, PAC0 & WDT
  REG_PM_APBAMASK &= ~PM_APBAMASK_RTC & ~PM_APBAMASK_PAC0 & ~PM_APBAMASK_WDT;
  // Disable DMAC, PAC1, USB & DSU
  REG_PM_APBBMASK &= ~PM_APBBMASK_DMAC & ~PM_APBBMASK_PAC1 & ~PM_APBBMASK_USB & ~PM_APBBMASK_DSU;
  // Disable DAC, TC0-7, TCC0-2, SERCOM5, EVSYS & PAC2, = I2S, AC, PTC, 
  REG_PM_APBCMASK &= ~PM_APBCMASK_DAC & ~PM_APBCMASK_TC7 & ~PM_APBCMASK_TC6 & ~PM_APBCMASK_TC5 & ~PM_APBCMASK_TC4 & ~PM_APBCMASK_TCC0 & ~PM_APBCMASK_TCC1 & ~PM_APBCMASK_TCC2 &
    ~PM_APBCMASK_SERCOM5 & ~PM_APBCMASK_EVSYS & ~PM_APBCMASK_PAC2 & ~PM_APBCMASK_I2S & ~PM_APBCMASK_AC & ~PM_APBCMASK_PTC;
}

// Retrieve date/time from the RTC and put it in the current position in the data array
void updateDate() {
  data[currentSecond].dt = rtc.now();
}

// Interrupt handler for sampling sensor data
// Throws a flag if we need to save to disk
void timerHandler() {
  data[currentSecond].values[currentReading].PPGVal = analogRead(PPGSensor);
  data[currentSecond].values[currentReading].Xval = analogRead(Xout);
  data[currentSecond].values[currentReading].Yval = analogRead(Yout);
  data[currentSecond].values[currentReading].Zval = analogRead(Zout);

  currentReading++;
  // If we have sampled all data for a single second, go to the next second.
  // Do a check if we're at {secondsToSample}, set {saveFlag} for the upper range and reset the index to the first (0) second in the data
  // If the current data read is at HALF of {seccondsToSample}, set {saveFlag} for the lower range
  if (currentReading == updateRate) {
    currentReading = 0;
    currentSecond++;
    if (currentSecond == (secondsToSample / 2)) {
      saveFlag = true;
      saveLowRange = true;
    } else
    if (currentSecond == secondsToSample) {
      currentSecond = 0;
      saveFlag = true;
      saveLowRange = false;
    }
    // Update time & date for the new second
    updateDate();
  }

}

// Save data to disk
void saveData() {
  // Don't write data if USB is connected
  if (USB_CONNECTED) { return; }
  // If we got here without a {saveFlag}, return
  if (!saveFlag) { return; }
  debugPrint("Saving.. ");
  //debugPrintUSBStatus();
  uint8_t low, high;
  // Check which range to save
  if (saveLowRange) {
    low = 0;
    high = (secondsToSample / 2); // -1
  } else {
    low = (secondsToSample / 2);  // -1
    high = secondsToSample;
  }

  // Speed up the CPU during save
  PM->CPUSEL.bit.CPUDIV = 0x0;
  myFile = SD.open("test3.csv", O_WRITE | O_CREAT | O_APPEND);
  
  int length = 0;
  String saveData;
  // Loop through each second of data in the range
  for (int i=low; i<high; i++) {
    // Each second will have the same timing information so we create one string that we can reuse for all data entries
    saveData = String(data[i].dt.year()) + "/" + String(data[i].dt.month()) + "/" + String(data[i].dt.day()) + " " + String(data[i].dt.hour()) + ":" + String(data[i].dt.minute()) + ":" + String(data[i].dt.second()) + ",";
    for (int j=0; j<updateRate; j++) {
      String saveData2 = saveData + String(data[i].values[j].PPGVal) + "," + String(data[i].values[j].Xval) + "," + String(data[i].values[j].Yval) + "," + String(data[i].values[j].Zval);
      myFile.println(saveData2);
    }
  }
  myFile.close();
  // Slow down the CPU again
  PM->CPUSEL.bit.CPUDIV = CPUDIVP;
  saveFlag = false;
}

// This function waits for the RTC to transition to a new second
void waitForZeroSecond() {
  uint8_t last = rtc.now().second();
  do {
  } while(rtc.now().second() != last);
}

// Read serial commands sent to the Patch
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

// Main loop
void loop() {
  saveData();
  if (Serial.available()) {
    readSerialCommands();
  }
  delay(10);
}

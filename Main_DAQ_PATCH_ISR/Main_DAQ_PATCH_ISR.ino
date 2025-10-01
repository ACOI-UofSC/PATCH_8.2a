#define USING_TIMER_TC3 true
#include <SAMDTimerInterrupt.h>
SAMDTimer ITimer0(TIMER_TC3);

#define ENABLE_DEDICATED_SPI 1
#define USE_SPI_ARRAY_TRANSFER 3
#define MAINTAIN_FREE_CLUSTER_COUNT 1

// USE 512 byte write buffers if possible for optimal performance!

// Declaration of all the libraries
#include <SPI.h>
#include <SdFat.h>
SdFat SD;

// Sets updates per second
#define updateRate 45

// Enables USB debug messages and writing
// #define DEBUG

// comment out to generate CSV instead of BIN
#define BINWRITE

#define BINFILEPREFIX "test"
String BINFILE = "test.bin";
//#define BINFILE "test.bin"

#include "RTClib.h"
//#include "customdatetime.hpp"
// RTC declaration. Here, we are using PCF8523 RTC
RTC_PCF8523 rtc;
//CustomDateTime *cdt;

// Declaration of all the constants
const byte chipSelect = 4;
const byte PPGVolt = A0;
const byte PPGSensor = A1;
const byte Xout = A4;
const byte Yout = A3;
const byte Zout = A2;

const uint16_t PPGVOut = 839;  // Supposed to be 839 = Set LED to 2.7V to measure 'PPG' or Heart rate

// Define for the CPUDIV used (3 = div 8, 4 = div 16, 5 = div 32)
uint8_t CPUDIVP = 0x6;
// Flag for whether USB is connected or not, only updated once at startup
bool USB_CONNECTED = false;
// Placeholder for the latest REG_USB_DEVICE_FNUM, should always be '0' if USB is never connected
int usb_fnum = 0;

/*
  The interrupt is called every 1S / updateRate
  
  The software will wait for the second to change before starting sampling in order to star on an even 0.0 seconds
  
  Data for each second is stored without timing information but is evenly spaced according to {updateRate}, each whole second time & date data is stored
  
  In order to avoid collissions in between the interrupt reading data and the SD card saving it to disc,
  only HALF of the data is saved at any time, namely whichever half the sensor is not currently populating
*/

// Structure for sub-second readings - 8 bytes size
typedef struct {
  uint16_t PPGVal, Xval, Yval, Zval;
} singleReading;

typedef struct {
  DateTime dt;  // 6 bytes
  singleReading values[updateRate];
} singleSecond;

// total number of seconds to sample - WILL NOT WORK IF UNEVEN NUMBER
// Max is whatever fits into the existing memory (~2.7K bytes)
// So max = 2700 / samples per second but even
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

void setBinFile() {
  String binFileName;
  int it = 0;
  char itS[3];
  do {
    snprintf(itS, 3, "%02d", it);
    binFileName = String(BINFILEPREFIX) + "_" + String(itS) + ".bin";
    debugPrint("See if exists " + binFileName);
    it++;
  } while (SD.exists(binFileName));
  debugPrint("No file with name found, continuing");
  BINFILE = binFileName;
}

// Returns true if a USB device is attached, should be updated in the future as FNUM is probably not reliable
bool checkUSBAttached() {
  int temp = REG_USB_DEVICE_FNUM;
  if (temp == usb_fnum) {
    USB_CONNECTED = false;
  } else {
    USB_CONNECTED = true;
  }
  usb_fnum = temp;
  // This function will not work repeatedly unless this is here
  delay(1);
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
      analogWrite(PPGVolt, PPGVOut);  // Turn on LED
      delay(300);                  // Wait for 100 milliseconds
      analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
      delay(100);                  // Wait for 100 milliseconds
    }
  }

  // Check if USB is connected, if so go full CPU speed
  // could also check REG_USB_DEVICE_STATUS which is 128 when connected, but not avaliable at startup
  
  if (checkUSBAttached()) {
    Serial.begin(115200);
    delay(250);
    Serial.println("USB detected");
  }

  // Detect SD Card and start if found, just blink if not
  if (!startSDCard()) {
    debugPrint("No SD-Card found");
    do {
      analogWrite(PPGVolt, PPGVOut);  // Set LED to 2.5V (PWM duty cycle 100%)
      delay(250);                   // Wait for 30 milliseconds
      analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
      delay(250);                   // Wait for 30 milliseconds
    } while ((!USB_CONNECTED) && (!startSDCard()));
  } else {
    // PPG Sensor LED blinking 4 times with 100ms delay
    for (int i=0; i<3; i++) {
      analogWrite(PPGVolt, PPGVOut);  // Set LED to 2.5V (PWM duty cycle 100%)
      delay(100);                  // Wait for 100 milliseconds
      analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
      delay(100);                  // Wait for 100 milliseconds
    }
  }
  analogWrite(PPGVolt, PPGVOut);  

#ifdef DEBUG
  CPUDIVP = 0;
#endif
  // Reduce clock speed, for details go to engineering notebook 
  if (!USB_CONNECTED) { PM->CPUSEL.bit.CPUDIV = CPUDIVP; }
  // Don't write data if USB is connected 
#ifndef BINWRITE
#ifndef DEBUG
  if (!USB_CONNECTED) { writeToFile("Date & Time,PPGVal,Xval,Yval,Zval"); }
#else
  writeToFile("Date & Time,PPGVal,Xval,Yval,Zval");
#endif
#else
  setBinFile();
#endif

  // Wait for the RTC to transition to a new second
  waitForZeroSecond();
//  // Stop the RTC comms
//  Wire.end();
  // Set the date & time information for the first data entry
  updateDate();

  // Start the interrupt that samples the data and
  // disable unused peripherals if we're in battery mode
#ifndef DEBUG
  if (!USB_CONNECTED) { 
    ITimer0.attachInterrupt(updateRate, timerHandler);
    powerDisable(); 
  }
#else
    ITimer0.attachInterrupt(updateRate, timerHandler);
#endif
/*
#ifdef BINWRITE
  if (USB_CONNECTED) { convertBinFile(); }
#endif
*/
}

void convertBinFile() {
  if (SD.exists(BINFILE)) {
    Serial.println("Found binary file, converting...");
    analogWrite(PPGVolt, PPGVOut);

    File binFile = SD.open(BINFILE);
    myFile = SD.open("testconverted.csv", O_WRITE | O_CREAT);
    myFile.println("Date & Time,PPGVal,Xval,Yval,Zval");
    int length = 0;
    String saveData;
    uint16_t dataLength = sizeof(data[0]) * (secondsToSample / 2);
    while (binFile.available()) {
      binFile.read(&data, dataLength);
      // Loop through each second of data in the range
      debugPrint("Writing block of " + String(dataLength) + " bytes. ");
      for (int i=0; i<(secondsToSample / 2); i++) {
        // Each second will have the same timing information so we create one string that we can reuse for all data entries
    //    saveData = String(data[i].dt.year()) + "/" + String(data[i].dt.month()) + "/" + String(data[i].dt.day()) + " " + String(data[i].dt.hour()) + ":" + String(data[i].dt.minute()) + ":" + String(data[i].dt.second()) + ",";
        saveData = String(data[i].dt.year()) + "/" + String(data[i].dt.month()) + "/" + String(data[i].dt.day()) + " " + String(data[i].dt.hour()) + ":" + String(data[i].dt.minute()) + ":" + String(data[i].dt.second()) + ",";
        for (int j=0; j<updateRate; j++) {
          String saveData2 = saveData + String(data[i].values[j].PPGVal) + "," + String(data[i].values[j].Xval) + "," + String(data[i].values[j].Yval) + "," + String(data[i].values[j].Zval);
          myFile.println(saveData2);
        }
      }
    }
    myFile.close();
    binFile.close();    

    Serial.println("Conversion done");
    for (int i=0; i<10; i++) {
        analogWrite(PPGVolt, PPGVOut);  // Set LED to 2.5V (PWM duty cycle 100%)
        delay(100);                  // Wait for 100 milliseconds
        analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
        delay(100);                  // Wait for 100 milliseconds
    }
  } else {
    Serial.println("No binary file to convert");
  }
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
//  data[currentSecond].dt = cdt->now();
}

// Interrupt handler for sampling sensor data
// Throws a flag if we need to save to disk
void timerHandler() {
  data[currentSecond].values[currentReading].PPGVal = analogRead(PPGSensor);
  data[currentSecond].values[currentReading].Xval = analogRead(Xout);
  data[currentSecond].values[currentReading].Yval = analogRead(Yout);
  data[currentSecond].values[currentReading].Zval = analogRead(Zout);

  //debugPrint("Reading data for second " + String(currentSecond) + " interval " + String(currentReading));
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
  #ifndef DEBUG
  if (USB_CONNECTED) { return; }
  #endif
  // If we got here without a {saveFlag}, return
  if (!saveFlag) { return; }
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
  // Tests show that a CPUDIV of 0 draws the least power
  PM->CPUSEL.bit.CPUDIV = 0x0;

  debugPrint("Saving range " + String(low) + " to " + String(high));
#ifndef BINWRITE  
  myFile = SD.open("test3.csv", O_WRITE | O_CREAT | O_APPEND);
  int length = 0;
  String saveData;
  // Loop through each second of data in the range
  for (int i=low; i<high; i++) {
    // Each second will have the same timing information so we create one string that we can reuse for all data entries
//    saveData = String(data[i].dt.year()) + "/" + String(data[i].dt.month()) + "/" + String(data[i].dt.day()) + " " + String(data[i].dt.hour()) + ":" + String(data[i].dt.minute()) + ":" + String(data[i].dt.second()) + ",";
    saveData = String(data[i].dt.year()) + "/" + String(data[i].dt.month()) + "/" + String(data[i].dt.day()) + " " + String(data[i].dt.hour()) + ":" + String(data[i].dt.minute()) + ":" + String(data[i].dt.second()) + ",";
    for (int j=0; j<updateRate; j++) {
      String saveData2 = saveData + String(data[i].values[j].PPGVal) + "," + String(data[i].values[j].Xval) + "," + String(data[i].values[j].Yval) + "," + String(data[i].values[j].Zval);
      myFile.println(saveData2);
    }
  }
#else
  myFile = SD.open(BINFILE, O_WRITE | O_CREAT | O_APPEND);
  int size = sizeof(data[0]) * (secondsToSample / 2);
  debugPrint("Chunk size " + String(size) + " bytes");
  myFile.write(&data[low], size);
#endif
  myFile.close();
  // Slow down the CPU again
  PM->CPUSEL.bit.CPUDIV = CPUDIVP;
  saveFlag = false;
}

// This function waits for the RTC to transition to a new second
void waitForZeroSecond() {
  return;
  uint8_t last = rtc.now().second();
  do {
  } while(rtc.now().second() != last);
//  DateTime dt = rtc.now();
//  cdt = new CustomDateTime(dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
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
    case 'd': {
      DateTime dt = rtc.now();
      Serial.println("DateTime: " + String(sizeof(dt)) + " bytes");
    }
  }
}

bool previousUSB_CONNECTED = false;
bool usbStateChanged = false;

// Main loop
void loop() {
#ifndef DEBUG
  if (!USB_CONNECTED) {
    while (true) {
      saveData();
    }
  }
#else
  saveData();
#endif

  if (Serial.available()) {
    readSerialCommands();
  }

/*
  // Testing switching from USBMODE to NORMAL on the fly
  // Doesn't work because CPUDIV needs to be set to a speed were USB doesn't function anymore hence frames will not be detected i.e. no USB detection

  checkUSBAttached();
  if (previousUSB_CONNECTED != USB_CONNECTED) {
    usbStateChanged = true;
    previousUSB_CONNECTED = USB_CONNECTED;
    debugPrint("USB State changed");
  }

  if (!USB_CONNECTED) {
    PM->CPUSEL.bit.CPUDIV = CPUDIVP;
    delay(990);
    if (usbStateChanged) {
      ITimer0.attachInterrupt(updateRate, timerHandler);    
      waitForZeroSecond();
      updateDate();
    }
//    while (!USB_CONNECTED) { 
    saveData();
    PM->CPUSEL.bit.CPUDIV = 0;
    delay(10);
//    }
  } else {
    if (usbStateChanged) { 
      PM->CPUSEL.bit.CPUDIV = 0;
      ITimer0.stopTimer(); 
    }
    if (Serial.available()) {
      readSerialCommands();
    }
    
    analogWrite(PPGVolt, 800);  // Set LED to 2.5V (PWM duty cycle 100%)
    delay(100);                  // Wait for 100 milliseconds
    analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
    delay(100);                  // Wait for 100 milliseconds
    checkUSBAttached();
  }
*/

}

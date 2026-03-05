/*
 * This file is part of The Patch firmware
 *
 * The Patch firmware is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The Patch firmware is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with The Ekdahl FAR firmware. If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2026 
 */

/** @file Main_DAQ_PATCH_ISR
  * @mainpage Patch main file
  * @section Introduction
  * This code samples the PPG using interrupts and saves the data in binary format. The code is designed to run the SAMD21 at 2.5V without an additional clock battery. Hardware USB detection is performed by sampling the VUSB line \n
  * and a LOW will reduce the processors speed below that which is required for normal USB operations in order to save power. Hardware SD-Card detection is performed by checking the physical card switch. \n
  * A ring buffer is used for sampling and the previously sampled half of the buffer is saved to disk whenever the read counter passes either the half-read mark or the upper bounds. \n
  * @section Notes
  * - In order to allow for low voltage operation a modified version of the Adafruit Feather M0 bootloader is used where NVM states are set to 3 (longer delay in between FLASH read/write due to the lower power) and the Brown-out \n
  * detection is set to 1.7V (Automatic restart at too low power detection).
  * - This code demands that the SD-Card is formatted ExFat as it (seemingly) is the best option for smooth read/write SD-Card operations and uses a modified version of the SDFAT library version SDFAT ADAFRUIT FORK
  *   The library needs to have the added line '#define SDFAT_FILE_TYPE 3' in SdFatConfig.h around line ~50
  * - The code uses a modified version of the RTC library in order to disable the automatic switchover from VDD to battery power, this is necessary due to the fact that a fresh clock battery will output a higher voltage than \n
  *   the internal 2.5V regulators.
  * @section issues Known issues
  * - There is still an issue with a 2 second gap in data reads happening every once in a while, the source is still unknown.
  * - Pre-allocation will not work twice even on different files unless device is restarted in between. Assuming something is up with the SD-Card or SPI still being open but simply resetting the CPU for now to overcome it.
  * @section Settings
  *   The code uses a number of #defines to modify compile-time settings;
  *   - PPGDEBUGOUT - Enables serial output of the PPG data in order to do live monitoring
  *   - BINWRITE - If defined, saves the data in binary format (More or less required at this stage)
  *   - PREALLOCATE - If defined, pre-allocates a file size in order to minimize write delays (More or less required at this stage)
  *   - PREALLOCSIZE - Defines the pre allocated file size
  *   - DEBUG - Enables SD-Card data writes even when USB is connected
  *   - CPUDIVNOUSB - Sets the clock divider used when running in battery mode (No USB detected). Default is 6 (0.75MHz)
  *   - CPUDIVHIGH - Sets the clock divider used in normal USB mode. Default is 0 (48Mhz)
  *   - updateRate - Sets the sample rate, given in samples per second
  *   - secondsToSamples - Sets the size of the ring buffer, the maximum size is dependent on [/ref updateRate]. The theoretical maximum memory usage is 22K, the compiler DOES NOT RELIABLY TELL YOU if you go over this limit.
  *     Reference values are a buffer of 60 seconds for a sample rate of 45Hz, 120 seconds for 20Hz and 50 seconds for 60Hz
  *   - readBits - Sets the bit depth to sample at. The default is 22
  *   - divider - If readBits is > 16, this sets the divider used to divide down the incoming data to 16 bits. Use divider = 2^(readBits - 16), for 22 bits divider = 64
  * @section workflow Detailed workflow
  * Setup:
  *   -# Set High CPU speed (Safeguard against dirty reboot at low CPU speed) [\ref setCPUSpeed]
  *   -# Set the brown-out low voltage detection to 1.7V (Needed to run the device at 1.8V) [\ref setBODlow]
  *   -# Set port input/output modes, note the use of \ref pinPeripheral as the SAMD21 has a multitude of functions per pin. \n
  *      pinPPGVolt is purposefully left out in order to leave it in PWM mode.
  *   -# Set the ADC resolution to 22-Bits [\ref analogReadResolution] (\ref readBits)
  *   -# Check if USB is connected, if it is enable the USB hardware and the USB Serial device, otherwise do not. \n
  *      Leaving the USB turned off during battery operation is cruical as the device is slowed down to a speed incompatible with USB communcations, any attempt to use the USB port here can hang the processor.
  *   -# Initialize the RTC module over I2C and wait until a proper response is received. A hardware error with the RTC will make the device stop here.
  *   -# Disable the RTC battery switchover [\ref disableRTCSwitchover] and verify it [\ref readRTCSwitchover]
  *   -# Enable PPG LED and wait in order to signal restart
  *   -# Attempt initiation of SD-Card, a failure here will still allow execution to continue in order to receive USB commands to set the clock etc  and does initiate the SPI hardware (?)
  *   -# Modify CPU speed in accordance to whether USB is plugged in or not [\ref setCPUSpeedAccordingToUSB]
  *   -# Run another standard SD-Card check, this is necessary as it will set various flags etc. [\ref waitForCard]
  *   -# Start the data read interrupt
  * .
  * Loop:
  *   -# Modify the CPU speed according to the state of the USB cable [\ref setCPUSpeedAccordingToUSB]
  *   -# Check if we still have a SD-Card, if we do, continue to save data [\ref saveData]. If we do not, and we do not have USB plugged in (Patch running on low non-USB compatible speed), make sure to turn all USB communications off. \n
  *        Run the standard SD-Card check [\ref waitForCard]
  *   -# Check if USB is connected or if the [\ref allowLoop] flag is set. If neither of these are true, jump back to the beginning of the function. \n
  *      While in battery mode we deliberately prevent the execution of the main Arduino service loop as it performs USB functions that do nothing but draw extra power while USB is not functional.
  *   -# If USB is indeed connected, check if there are any USB commands that need to be taken care of before executing the main Arudino service loop [\ref readSerialCommands]
*/

// Has to be compiled with Feather M0 bootloader!!!
#define USING_TIMER_TC3 true              ///< SAMD21 Required
#include <SAMDTimerInterrupt.h>
SAMDTimer ITimer0(TIMER_TC3);             ///< Interrupt timer
/*
#define ENABLE_DEDICATED_SPI 1
#define USE_SPI_ARRAY_TRANSFER 3
#define MAINTAIN_FREE_CLUSTER_COUNT 1
*/
#include <SPI.h>
#include <SdFat_Adafruit_Fork.h>          // If the compiler complains about SDFAT, chances are you are using the wrong bootloader / board selection
#include <ExFatLib/ExFatLib.h>
SdFat SD;                                 ///< SDFat main entry point

#define SDCLOCK 12000000                  ///< SD-Card SPI Speed
#define I2CC 100000                       ///< I2C clock speed, default is 100kHz

#include <wiring_private.h>
#include "RTClibModded/src/RTClib.h"
#include "RTClibModded/src/RTClib.cpp"
#include "RTClibModded/src/RTC_PCF8523.cpp"
RTC_PCF8523 rtc;                          ///< RTC main class definition

// ** IMPORTANT SETTINGS *************************************************************************
//#define PPGDEBUGOUT                          ///< Enables serial output of the PPG data in order to do live monitoring
#define BINWRITE                            ///< If defined, saves the data in binary format (More or less required at this stage)
#define PREALLOCATE                         ///< If defined, pre-allocates a file size in order to minimize write delays (More or less required at this stage)
#define PREALLOCSIZE 1024ULL * 1024 * 512   ///< Defines the pre allocated file size
#define DEBUG                               ///< Enables SD-Card data writes even when USB is connected
uint8_t CPUDIVNOUSB = 0x6;                  ///< Sets the clock divider used when running in battery mode (No USB detected). Default is 6 (0.75MHz)
uint8_t CPUDIVHIGH = 0;                     ///< Sets the clock divider used in normal USB mode. Default is 0 (48Mhz)
#define updateRate 20                       ///< Sets the sample rate, given in samples per second
#define secondsToSample 20                  ///< Sets the size of the ring buffer, the maximum size is dependent on [\ref updateRate]. The theoretical maximum memory usage is 22K, the compiler DOES NOT RELIABLY TELL YOU if you go over this limit. Reference values are a buffer of 60 seconds for a sample rate of 45Hz, 120 seconds for 20Hz and 50 seconds for 60Hz

// **********************************************************************************************
#define readBits 22                         ///< Sets the bit depth to sample at. The default is 22
#define divider 64                          ///< If readBits is > 16, this sets the divider used to divide down the incoming data to 16 bits. Use divider = 2^(readBits - 16), for 22 bits divider = 64

#define BINFILEPREFIX "test"                ///< Prefix for data save file
String binFileName = "test.bin";            ///< Placeholder for the currently used filename                
bool saveError = false;                     ///< Set to true if an SD-Card error has occured

/* Declaration of hardware constants */
const byte pinTestOutput = (25UL);          ///< Pin for testing outputs
const byte pinChipSelect = 4;               ///< CS Select for SD-Card
const byte pinPPGVolt = A0;                 ///< Output pin for PPG LED
const byte pinPPGSensor = A1;               ///< Input pin from PPG sensor
const byte pinXout = A4;                    ///< Input pin from accelerometer X
const byte pinYout = A3;                    ///< Input pin from accelerometer y
const byte pinZout = A2;                    ///< Input pin from accelerometer z
const byte pinCardDetect = 1;               ///< Input pin from the SD-Card switch
const byte pinUSBDetect = (0UL);            ///< Input pin from VUSB

#define CARD_DETECTED digitalRead(pinCardDetect)///< Use this macro to detect whether an SD-Card is inserted in case of future changes

const uint16_t PPGVOut = 1023;                    ///< PWM to use for the given voltage. Default for 2.8V is 1023
uint8_t CPUDIVP = CPUDIVNOUSB;                    ///< Contains the current CPU clock divider
#define USB_CONNECTED digitalRead(pinUSBDetect)   ///< Macro for detecting VUSB
//bool RTCActive = false;

/// Data structure for sub-second readings - 8 bytes size
typedef struct {
  uint16_t PPGVal, Xval, Yval, Zval;
} singleReading;

/// Data structure for each second
typedef struct {
  DateTime dt;  // 6 bytes
  singleReading values[updateRate];
} singleSecond;

singleSecond data[secondsToSample];               ///< Main ring buffer
uint8_t currentSecond = 0;                        ///< Current second position in ring buffer
uint8_t currentReading = 0;                       ///< Current sub-second position in ring buffer
bool saveFlag = false;                            ///< set to true when data is to be saved to SD-card
bool saveLowRange = true;                         ///< true = save the low range of data (0 - secondsToSample / 2), false = save the high range of data ((secondsToSample / 2) - secondsToSample)
ExFatFile permanentBinFile;                       ///< Class used for ExFat SD-Card file save
bool newFile = true;                              ///< Set to true when a new file is created
uint32_t testRamp = 0;                            ///< Variable used for ramping tests

bool updateFlag = false;                          ///< Set to true when a new reading has been logged
bool allowLoop = false;                           ///< Set to true if wanting to allow the firmware to process the normal Arduino service loop (once!) even if USB is disconnected

/// Outputs debug data if USB is connected, should be used instead of direct printing
void debugPrint(String data) {
  if (!USB_CONNECTED) { return; }
  Serial.println(data);
}

int oldSpeed = 999;                               ///< Placeholder for keeping track of the previous CPU clock divisor so we don't change the CPU speed unnecessarily

/// Sets the CPU clock divisor
void setCPUSpeed(int speed) {
//  return;
  if (oldSpeed = speed) { return; }
  oldSpeed = speed;
  PM->CPUSEL.bit.CPUDIV = speed;
}

/// Returns a ADC reading and uses \rel divider to divide the data down if oversampling
int readADC(byte port) {
  int a = analogRead(port);
  a = (a / divider);
  if (a < 0) { a= 0; }
  if (a > 65535) { a = 65535; }
  return a;
}

/// Looks for the next unused filename with \ref BINFILEPREFIX _xx.bin and puts the result in \ref binFileName
void setBinFile() {
  String newBinFileName;
  int it = 0;
  char itS[3];
  FsFile exists;
  do {
    snprintf(itS, 3, "%02d", it);
    newBinFileName = "/" + String(BINFILEPREFIX) + "_" + String(itS) + ".bin";
    debugPrint("See if exists " + newBinFileName);
    it++;
  } while(exists.open(newBinFileName.c_str())); // We would use the 'exists()'-function here normally, but it doesn't work with ExFat it seems
  debugPrint("No file with name '" + newBinFileName + "' found, creating...");
  if (newBinFileName != binFileName) {
    newFile = true;
    debugPrint("New filename!");
  }
  binFileName = newBinFileName;
}

/// Sets the brown-out (automatic restart at too low input voltage) at 1.5V
void setBODlow() {
  SYSCTRL->BOD33.reg = (
      SYSCTRL_BOD33_LEVEL(0) |  // Voltage threshold is about 1.5V + LEVEL * 34mV
      SYSCTRL_BOD33_ACTION_NONE |
      SYSCTRL_BOD33_HYST);

  // Enable the brown-out detector and then wait for the voltage level to settle.
  SYSCTRL->BOD33.bit.ENABLE = 1;
  while (!SYSCTRL->PCLKSR.bit.BOD33RDY) {}
}

/// Disables the RTCs automatic switchover from VDD to battery, needed when using a lower VDD
void disableRTCSwitchover() {
  debugPrint("Disabling RTC battery switchover");
  rtc.write_register(2, 0xE0);  // Disable battery switchover
}

/// Read / Verify RTC settings
void readRTCSwitchover() {
  uint8_t c1 = rtc.read_register(2);
  Serial.print("RTC switchover mode: ");
  Serial.println(c1, HEX);
}

/// Reset function for the SAMD21
void reset() {
  analogWrite(pinPPGVolt, 0);
  __disable_irq();
  NVIC_SystemReset();
  while (true);
}

/// Changes the CPU speed according to whether USB is connected or not. When USB is connected, CPU clock divisor is @ref CPUDIVHIGH, if not it is @ref CPUDIVNOUSB
void setCPUSpeedAccordingToUSB(bool init = false) {
  if (USB_CONNECTED) {
    CPUDIVP = CPUDIVHIGH;
  } else {
    CPUDIVP = CPUDIVNOUSB;
  }

  setCPUSpeed(CPUDIVP);
}

/** \brief Waits for a SD-Card to be inserted for [\ref timeout] milliseconds before automatically returning.
  * the PPG LED is turned off during the wait in order to visually indicate that an SD-Card isn't present, when a card is detected the LED is set to \ref PPGVOut again.
  * It then uses \ref waitForZeroSecond to wait for an exact second transition before starting to log readings, and resets \ref currentSecond and \ref currentReading.
  * The date/time is being updated and the SD-Card is tested to make sure it works.
  * Lastly \ref setBinFile is used to make a new file for data logging.
*/
bool waitForCard(int timeout = 1000) {
  int start = millis();
  analogWrite(pinPPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)  
  while((!CARD_DETECTED) && (millis() < (start+timeout))) {
    Serial.println("Waiting for card");
    delay(1000 >> CPUDIVP);
  }
  if (!CARD_DETECTED) { return false; }
  setCPUSpeedAccordingToUSB();

  analogWrite(pinPPGVolt, PPGVOut);
  debugPrint("Waiting for zero second");
  waitForZeroSecond();
  currentSecond = 0;
  currentReading = 0;

  debugPrint("Updating date");
  updateDate();

  if (SD.card()->errorCode()) {
    debugPrint("SD card error");
  } else
  if (SD.vol()->fatType() == 0) {
    debugPrint("No valid file system found");
  } else {
    debugPrint("FS: " + String(SD.vol()->fatType()));
  }

#ifndef BINWRITE
#ifndef DEBUG
    if (!USB_CONNECTED) { writeToFile("Date & Time,PPGVal,Xval,Yval,Zval"); }
#else
    writeToFile("Date & Time,PPGVal,Xval,Yval,Zval");
#endif
#else
    setBinFile();
#endif

  return true;
}

///. Enables the USB hardware, not entirely sure whether this is strictly necessary since we do not explicitly disable the USB at any time
void enableUSB() {
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_USB;
  GCLK->CLKCTRL.bit.CLKEN = 1;
  while (GCLK->STATUS.bit.SYNCBUSY);

  USB->DEVICE.CTRLA.bit.ENABLE = 1;
  while (USB->DEVICE.SYNCBUSY.bit.ENABLE);  

  startSerial();  
}

/// Starts the serial USB device
void startSerial() {
  delay(125);
  Serial.begin(115200);
  while(!Serial) {};
  delay(250);
}

/// The main setup, please see \ref workflow for details.
void setup() {
  setCPUSpeed(CPUDIVHIGH);
  setBODlow();

  pinMode(pinPPGSensor, INPUT);
  pinMode(pinXout, INPUT);
  pinMode(pinYout, INPUT);
  pinMode(pinZout, INPUT);
  pinPeripheral(pinUSBDetect, PIO_INPUT);
  pinMode(pinUSBDetect, INPUT_PULLDOWN);
  pinMode(pinCardDetect, INPUT_PULLUP);
  pinPeripheral(pinCardDetect, PIO_INPUT_PULLUP);

  for (int i=0; i<secondsToSample; i++) {
    for (int j=0; j<updateRate; j++) {
      data[i].values[j].PPGVal = 0;
      data[i].values[j].Xval = 0;
      data[i].values[j].Yval = 0;
      data[i].values[j].Zval = 0;
    }
  }

  analogReadResolution(readBits);

  if (USB_CONNECTED) {
    enableUSB();
    Serial.println("USB detected");
  } else {
    Serial.end();
  }

  Wire.setClock(I2CC);

  while (!rtc.begin(&Wire)) {
    debugPrint("No connection to RTC! Retrying..");
    delay(500);
  };

//  RTCActive = true;
  debugPrint("RTC found.");
  Wire.setClock(I2CC);
  rtc.start();
  Wire.setClock(I2CC);
  
  disableRTCSwitchover();
  readRTCSwitchover();

  analogWrite(pinPPGVolt, PPGVOut);

  delay(1000);

  if (!SD.begin(pinChipSelect, SDCLOCK)) { debugPrint("Error starting SD card"); }

  setCPUSpeedAccordingToUSB(true);
/*
  debugPrint("Has dedicated SPI: " + String(SD.hasDedicatedSpi()));
  debugPrint("Is dedicated SPI: " + String(SD.isDedicatedSpi()));
*/
  waitForCard();

  debugPrint("Starting interrupt");
  ITimer0.attachInterrupt(updateRate, timerHandler);

  debugPrint("Initialization done");
}

/// Retrieve date/time from the RTC and put it in the current position in the data array
void updateDate() {
//  if (!RTCActive) { return; }
  data[currentSecond].dt = rtc.now();
}

//singleReading currentData;

/** \brief Interrupt handler for sampling sensor data, logs it into the ringbuffer and updates \ref currentReading += 1. If \ref currentReading is at its bound it is set to zero and \ref currentSecond += 1.
  * If \ref currentSecond == (\ref secondsToSample / 2) (i.e. at half the ringbuffer) or \ref currentSecond == \ref secondsToSample (i.e. at the upper bound of the buffer), set \ref saveFlag to true and indicate
  * which half of the buffer to save through setting \ref saveLowRange accordingly. Resync the date/time with \ref updateDate and set \ref updateFlag to true
*/ 
void timerHandler() {
/*
  data[currentSecond].values[currentReading].PPGVal = readADC(pinPPGSensor);
  data[currentSecond].values[currentReading].Xval = readADC(pinXout);
  data[currentSecond].values[currentReading].Yval = readADC(pinYout);
  data[currentSecond].values[currentReading].Zval = readADC(pinZout);
*/
  data[currentSecond].values[currentReading].PPGVal = testRamp;
  data[currentSecond].values[currentReading].Xval = testRamp;
  data[currentSecond].values[currentReading].Yval = testRamp;
  data[currentSecond].values[currentReading].Zval = testRamp;
  testRamp++;
  //currentData = data[currentSecond].values[currentReading];

  currentReading++;
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
    updateDate();
  }

  updateFlag = true;
}

/** \brief Check if \ref saveFlag is set, if not return immediately. Check if the SD controller can be started, otherwise return and clear \ref saveFlag and set \ref saveError
  * Unless DEBUG is set, check if USB is connected and return if so. Set the range of data to set depending on \ref saveLowRange.
  * In order to maximize power efficiency, set the processor speed to max during save. Save as binary or .csv depending on compile-time flag.
  * If binary, see if the file is already open and if not open it and pre-allocate. Write and flush but do not close file. \n
  * \n
  * Not closing the file and using flush could be a bad move from a power efficiency perspective, but ExFat is very picky and doesn't seem to work right unless it is done exactly so. 
  * Pre-allocation on different files without resetting the processor does not work for unknown reasons either.
*/
void saveData() {
  if (!saveFlag) { return; }
  //allowLoop = true;
  if (!SD.begin(pinChipSelect, SDCLOCK)) {
    saveFlag = false;
    saveError = true;
    debugPrint("Error starting SD card");
    return;
  }

  #ifndef DEBUG
  if (USB_CONNECTED) { return; }
  #endif
  
  uint8_t low, high;
  if (saveLowRange) {
    low = 0;
    high = (secondsToSample / 2); // -1
  } else {
    low = (secondsToSample / 2);  // -1
    high = secondsToSample;
  }

  setCPUSpeed(CPUDIVHIGH);  // Speed up the CPU during save, Tests show that a CPUDIV of 0 draws the least power

  debugPrint("Saving range " + String(low) + " to " + String(high));
#ifndef BINWRITE  
  myFile = SD.open("test3.csv", O_WRITE | O_CREAT | O_APPEND);
  int length = 0;
  String saveData;
  for (int i=low; i<high; i++) {
    saveData = String(data[i].dt.year()) + "/" + String(data[i].dt.month()) + "/" + String(data[i].dt.day()) + " " + String(data[i].dt.hour()) + ":" + String(data[i].dt.minute()) + ":" + String(data[i].dt.second()) + ",";
    for (int j=0; j<updateRate; j++) {
      String saveData2 = saveData + String(data[i].values[j].PPGVal) + "," + String(data[i].values[j].Xval) + "," + String(data[i].values[j].Yval) + "," + String(data[i].values[j].Zval);
      myFile.println(saveData2);
    }
  }
#else
  if (newFile or !permanentBinFile) {
    if (!permanentBinFile.open(binFileName.c_str(), O_RDWR | O_CREAT | O_EXCL)) {
      debugPrint("Couldn't open for pre-allocation!");
      saveError = true;
      return;
    } else {
      debugPrint("Opening file..");
    };
    if (newFile) {
      if (!permanentBinFile.preAllocate((uint64_t)PREALLOCSIZE)) {
        debugPrint("Couldn't pre-allocate!");      
        saveError = true;
        return;
      }
    }
  }

  newFile = false;
  int size = sizeof(data[0]) * (secondsToSample / 2);
  debugPrint("Chunk size " + String(size) + " bytes");
  permanentBinFile.write(&data[low], size);
  permanentBinFile.flush();
#endif
  saveError = false;
  setCPUSpeed(CPUDIVP);
  saveFlag = false;
}

/// This function waits for the RTC to transition to a new second, used to sync
void waitForZeroSecond() {
//  if (!RTCActive) { return; }
  uint8_t last = rtc.now().second();
  do {
  } while(rtc.now().second() != last);
}

/// Read serial commands sent to the Patch and takes the apropriate action
void readSerialCommands() {
  String commands = "";
  commands = Serial.readString();
  debugPrint(commands);
  if (commands.length() == 0) { return; }
/*  if ((!RTCActive)) {
    debugPrint("No connection to RTC, all dates and times will be wrong.");
  }
*/
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
      
      debugPrint("Setting data to Month: " + String(month) + " Day: " + String(day) + " Year " + String(year) + " Hour " + String(hour) + " Minute " + String(minute) + " Second " + String(second));
      rtc.adjust(dt);
      break;
    }
    case 'q': {
      DateTime dt = rtc.now();
      debugPrint("Retrieved data - Month: " + String(dt.month()) + " Day: " + String(dt.day()) + " Year " + String(dt.year()) + " Hour " + String(dt.hour()) + " Minute " + String(dt.minute()) + " Second " + String(dt.second()));
      break;
    }
    case 'd': {
      DateTime dt = rtc.now();
      debugPrint("DateTime: " + String(sizeof(dt)) + " bytes");
      break;
    }
    case 'i': {
      debugPrint("RTC running status: " + String(rtc.isrunning()));
      break;
    }
    case 'r': {
      reset();
      break;
    }
  }
}

/// Defined behaviors when the saveError flag is set
void errorBehavior() {
  debugPrint("Error behavior");
  for (uint8_t i=0; i<3; i++) {
    analogWrite(pinPPGVolt, 0);
    delay(100);
    analogWrite(pinPPGVolt, 1023);
    delay(100);
  }
  waitForCard(1000 >> CPUDIVP);
}

/// Main loop, see \ref workflow for details
void loop() {
  do {
    setCPUSpeedAccordingToUSB();

#ifdef PPGDEBUGOUT
    if (updateFlag) {
      updateFlag = false;
      Serial.println(readADC(PPGSensor));
    }
#endif

    if (!CARD_DETECTED) {
      if (!USB_CONNECTED) { Serial.end(); }
      analogWrite(pinPPGVolt, 0);
      waitForCard(1000);
      if (CARD_DETECTED) { reset(); }
    } else {
      saveData();
      if (saveError) { 
        allowLoop = true;
        errorBehavior(); 
      }
    }
  } while(!USB_CONNECTED && !allowLoop);
  allowLoop = false;
  if (Serial.available()) {
    readSerialCommands();
  }
}

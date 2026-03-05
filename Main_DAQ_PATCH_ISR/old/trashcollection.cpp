/*
// Test for disabling the USB port and conserve data
void disableUSB() {
  REG_USB_CTRLA &= (!USB_CTRLA_ENABLE);
  REG_USB_DEVICE_CTRLB |= USB_DEVICE_CTRLB_DETACH;
  REG_USB_DEVICE_INTFLAG |= USB_DEVICE_INTFLAG_SUSPEND;
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

  //
      THIS CODE WORKS WITHOUT AN EXTERNAL PIN CONNECTED TO VUSB, CHECKS IF PROCESSED PACKAGES > 0
  
  int temp = REG_USB_DEVICE_FNUM;
  if (temp == usb_fnum) {
    USB_CONNECTED = false;
  } else {
    USB_CONNECTED = true;
  }
  usb_fnum = temp;
  // This function will not work repeatedly unless this is here
  delay(1);
  //

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

void setVDDHigh() {
  pinMode(coreVLOW, OUTPUT);
  digitalWrite(coreVLOW, LOW);
  pulsSwitcherEN();
  NVIC_SystemReset();
}

void setVDDLow() {
  pinMode(coreVLOW, OUTPUT);
  digitalWrite(coreVLOW, HIGH);
  pulsSwitcherEN();
}

void testVoltageSwitch() {
    if (updateFlag) {
      updateFlag = false;
      testSecond++;
      if (testSecond >= (updateRate * 5)) {
        if (digitalRead(coreVLOW) == LOW) {
          setVDDLow();
          CPUDIVP = CPUDIVNOUSB;
        } else {
          setVDDHigh();
        }
        testSecond = 0; 
        setCPUSpeed(CPUDIVP);
      }
    }
}

void testOutputOsc() {
    if ((millis() - (eventTime  / (1+CPUDIVP))) > lastEvent) {
      lastEvent = millis();
      if (lastSignal == LOW) {
        lastSignal = HIGH;
      } else {
        lastSignal = LOW;
      }
      digitalWrite(testOutput, lastSignal);
      //debugPrint("Change!");
    }
}

-- main trash --
    if (checkUSBAttachedChanged()) {
      if (USB_CONNECTED) {
        debugPrint("USB Connected");
        #ifdef CORE18
        setVDDHigh();
        #endif
        CPUDIVP = CPUDIVHIGH;
      } else {
        debugPrint("USB Disconnected?");
        #ifdef CORE18
        setVDDLow();
        #endif
        CPUDIVP = CPUDIVNOUSB;
      }
      setCPUSpeed(CPUDIVP);
//    testVoltageSwitch();
#ifdef CORE18
    testOutputOsc();
#endif

void pulsSwitcherEN() {
  pinMode(switcherEN, OUTPUT);
  digitalWrite(switcherEN, HIGH);
  delayMicroseconds(30);
  digitalWrite(switcherEN, LOW);
}

-- setup trash


#ifdef CORE18
  digitalWrite(switcherEN, LOW);
  pinMode(switcherEN, OUTPUT);
  digitalWrite(switcherEN, LOW);

  digitalWrite(coreVLOW, 0);
  pinMode(coreVLOW, OUTPUT);
  digitalWrite(coreVLOW, 0);

  pinMode(testOutput, OUTPUT);
#endif


// Returns true if a SD card is detected
bool startSDCard(bool boot) {
  // Detect SD Card and start if found, just blink if not
  if ((!SD.begin(chipSelect, 12000000)) || (!CARD_DETECTED)) {
    if (!boot) { 
      reset(); 
    }
    SDCardEjected = true;
    debugPrint("No SD-Card found");

    analogWrite(PPGVolt, PPGVOut);  // Set LED to 2.5V (PWM duty cycle 100%)
    delay(250);                   // Wait for 30 milliseconds
    analogWrite(PPGVolt, 0);    // Turn off LED (PWM duty cycle 0%)
    delay(250);                   // Wait for 30 milliseconds
    return false;
  } else {
    SDCardEjected = false;
    debugPrint("Opened SD-Card");
    analogWrite(PPGVolt, PPGVOut);
  }

  // If an ejection has been detected then we have to do this again
  if (SDCardEjected) {
    if (!boot) { reset(); }
  // Initiate file write conditionally depending on whether binary or text is defined
#ifndef BINWRITE
#ifndef DEBUG
    if (!USB_CONNECTED) { writeToFile("Date & Time,PPGVal,Xval,Yval,Zval"); }
#else
    writeToFile("Date & Time,PPGVal,Xval,Yval,Zval");
#endif
#else
    setBinFile();
#ifdef PREALLOCATE
    File binFile = SD.open(BINFILE, O_WRITE | O_CREAT | O_APPEND);
    binFile.preAllocate(100000000);
    binFile.flush();
    binFile.close();
#endif
#endif
  }
  SDCardEjected = false;
  return true;
}

*/

/*
// Returns true if a USB device is attached, should be updated in the future as FNUM is probably not reliable
bool checkUSBAttached() {
  USB_CONNECTED = digitalRead(usbDetectPin);
  return USB_CONNECTED;
}

bool checkUSBAttachedChanged() {
  bool now = digitalRead(usbDetectPin);
  if (now != USB_CONNECTED) {
    USB_CONNECTED = now;
    return true;
  }
  return false;
}

bool SDCardEjected = true;
//#include "customdatetime.hpp"
//CustomDateTime *cdt;
//const uint16_t PPGVOut = 839;  // Supposed to be 839 = Set LED to 2.7V to measure 'PPG' or Heart rate - FOR 3.3V!!!

*/
#include <SPI.h>
#include <SD.h>
#include "RTClib.h"

RTC_PCF8523 rtc;
//File myFile;

void setup() {
  Serial.begin(9600);

  // Wait for the serial port to connect. Needed for native USB only
  while (!Serial) {
    ; // Wait for serial port to connect.
  }

  // Initialize the RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  Serial.println("RTC is NOT initialized, or has lost power. Setting the time to the compile time:");

  // Print the compile time
  Serial.print("Compile Date & Time: ");
  Serial.print(__DATE__);
  Serial.print(" ");
  Serial.println(__TIME__);

  // Set the RTC to the compile time
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Retrieve the current time from the RTC
  DateTime now = rtc.now();

  // Create a DateTime object that represents 13 seconds later
  DateTime delayTime = now + TimeSpan(0, 0, 0, 12); // Jamal Correction for Time, you can change inside the box (0,0,0,25) Do not change anywhere
// Meaning of the box: (Day, Hour, Minute, Second)
  // Set the RTC to this new time
  rtc.adjust(delayTime);

}

void loop() {
  DateTime now = rtc.now();

  //Print the current date and time
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  //delay(1000);
}
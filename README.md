This is the repository for the Patch 8.2a and it's predecessing developments including newer test codes etc.

------------------------------------

Main_DAQ_PATCH_V2 - The original code as has been used for the Patch 8.2a

Time_Setting_PATCH_V1 - The original time-setting code as has been used for the Patch 8.2a

bootloader_programmer - Code for uploading the bootloader on a fresh Patch 8.2a

Main_DAQ_PATCH_ISR - New test-code using interrupts for even sampling time and time-setting over USB

Main_DAQ_PATCH_USBTIME - The original Main_DAQ_PATCH_V2 code but with USB time-setting

------------------------------------

-- Using the original code --

A factory-fresh Patch will need to have the bootloader programmed before you can upload the actual code into the Patch using the bootloader_programmer.

To reset the RTC the Time_Setting_PATCH_V1 code needs to be uploaded, this adds the current time at compile time and sets the RTC the first time it is run.

After the time has been set the sampling code Main_DAQ_PATCH_V2 needs to be uploaded, this is the code that actually performs the measurements.

-- Using the Main_DAQ_PATCH_ISR or the Main_DAQ_PATCH_USBTIME code --

These codes exhibit two distinct behaviors depending on whether the Patch is connected via USB or not. 

In USB-mode the Patch does not sample any data but is set into a serial communications mode which can be accessed by connecting to the Patch via the 
Arduino IDE and enabling the serial monitor.

As of this writing there are only two recognized commands, these commands are executed by simply sending text to the Patch via the serial monitor.

Sending a single 'q' performs a 'query' and will return the current time and date as set in the RTC, this is useful for debugging purposes and to monitor
any time lag in the RTC.

Sending a 's:MM-DD-YYYY-HH-MM-SS' where MM is month, DD is day, YYYY is year, HH is hour (24 hour clock), MM is minute and SS is second will set the RTCs
time and date. Please note that this format needs to be RELIGIOUSLY followed, writing '9' instead of '09' will make the code complain back at you.

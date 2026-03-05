#define SerialUSB Serial
// Disable the BOD33 fuse in the SAMD21's user row word 0
void setup() {
  
  delay(1000);
  SerialUSB.begin(115200);                                    // Start serial communication on the native USB port
  while(!SerialUSB);                                          // Wait for the console to open
  SerialUSB.println(F("Fuse settings before:"));
  SerialUSB.println((*(uint32_t*)NVMCTRL_USER), HEX);         // Display the current user word 0 fuse settings
  SerialUSB.println((*(uint32_t*)(NVMCTRL_USER + 4)), HEX);   // Display the current user word 1 fuse settings
/*  uint32_t userWord0 = *((uint32_t*)NVMCTRL_USER);            // Read fuses for user word 0
  uint32_t userWord1 = *((uint32_t*)(NVMCTRL_USER + 4));      // Read fuses for user word 1
  NVMCTRL->CTRLB.bit.CACHEDIS = 1;                            // Disable the cache
  NVMCTRL->ADDR.reg = NVMCTRL_AUX0_ADDRESS / 2;               // Set the address
  NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMD_EAR |                // Erase the auxiliary user page row
                       NVMCTRL_CTRLA_CMDEX_KEY;
  while(!NVMCTRL->INTFLAG.bit.READY)                          // Wait for the NVM command to complete
  NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK;                 // Clear the error flags
  NVMCTRL->ADDR.reg = NVMCTRL_AUX0_ADDRESS / 2;               // Set the address
  NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMD_PBC |                // Clear the page buffer
                       NVMCTRL_CTRLA_CMDEX_KEY;
  while(!NVMCTRL->INTFLAG.bit.READY)                          // Wait for the NVM command to complete
  NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK;                 // Clear the error flags
  *((uint32_t*)NVMCTRL_USER) = userWord0 & ~FUSES_BOD33_EN_Msk;  // Disable the BOD33 enable fuse in user word 0
  *((uint32_t*)(NVMCTRL_USER + 4)) = userWord1;               // Copy back user word 1 unchanged
  NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMD_WAP  |               // Write to the user page
                       NVMCTRL_CTRLA_CMDEX_KEY;
  while(!NVMCTRL->INTFLAG.bit.READY)                          // Wait for the NVM command to complete
  NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK;                 // Clear the error flags
  NVMCTRL->CTRLB.bit.CACHEDIS = 0;                            // Enable the cache
  SerialUSB.println(F("Fuse settings after:"));
  SerialUSB.println((*(uint32_t*)NVMCTRL_USER), HEX);         // Display the current user word 0 fuse settings
  SerialUSB.println((*(uint32_t*)(NVMCTRL_USER + 4)), HEX);   // Display the current user word 1 fuse settings
  */
}

void loop() {}
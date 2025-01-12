/***************************************************************************************
   SET FAN CONTROL
   Gets fan information and then adjusts the fan duty cycle based on internal temp
   - No inputs
   - No outputs
****************************************************************************************/
void setFanControl() {

  uint8_t dutyCycle = 30;

  tempC = fan.getInternalTemperature();
  dutyCycle = fan.getDutyCycle();
  rpm = fan.getFanRPM();

  if (tempC > 50) {
    dutyCycle = 100;
  } else if (tempC > 40) {
    dutyCycle = 70;
  } else if (tempC > 30) {
    dutyCycle = 50;
  } else if (tempC > 25) {
    dutyCycle = 40;
  } else {
    dutyCycle = 30;
  }

  fan.setDutyCycle(dutyCycle);
}


/***************************************************************************************
   BUTTON PRESSED CHECK
   Function to check whether a button (already debounced) has transitioned from off
   (returned 0) to on (returned 1) indicating user has pushed it down
   - INPUTS:
    - {uint16_t} prevButton = 16 bit register of previous button conditions
    - {uint16_t} newButton = 16 bit register of updated button conditions
    - {uint16_t} mask = mask to use for the compare; ensure coorect function
      definition is being operated on
   - Returns bool
****************************************************************************************/
bool buttonPressed(uint16_t prevButton, uint16_t newButton, uint16_t mask) {
  //logic check that the toggle has been activated but that the previous state was 0
  if (newButton & mask && ~prevButton & mask) {
    return true;
  } else {
    return false;
  }
}


/***************************************************************************************
   BUTTON RELEASED CHECK
   Function to check whether a button (already debounced) has transitioned from on
   (returned 1) to off (returned 0) indicating user has let it go
   - INPUTS:
    - {uint16_t} prevButton = 16 bit register of previous button conditions
    - {uint16_t} newButton = 16 bit register of updated button conditions
    - {uint16_t} mask = mask to use for the compare; ensure coorect function
      definition is being operated on
   - Returns bool
****************************************************************************************/
bool buttonReleased(uint16_t prevButton, uint16_t newButton, uint16_t mask) {
  //logic check that the toggle has been activated but that the previous state was 1
  if (~newButton & mask && prevButton & mask) {
    return true;
  } else {
    return false;
  }
}


/***************************************************************************************
   TOGGLE REGISTER BIT
   Function to change bit state in declared register. Uses logical XOR operator
   - INPUTS:
    - {uint16_t} registerInput = value of register that needs to be operated on
    - {uint16_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint16_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint16_t toggleBit(uint16_t registerInput, uint16_t mask) {
  uint16_t registerOutput;
  registerOutput = registerInput ^ mask;

  return registerOutput;
}


/***************************************************************************************
   TOGGLE REGISTER BIT
   Function to change bit state in declared register. Uses logical XOR operator
   - INPUTS:
    - {uint8_t} registerInput = value of register that needs to be operated on
    - {uint8_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint8_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint8_t toggleBit(uint8_t registerInput, uint8_t mask) {
  uint8_t registerOutput;
  registerOutput = registerInput ^ mask;

  return registerOutput;
}


/***************************************************************************************
   SET REGISTER BIT
   Function to change bit state to 1 in declared register. Uses logical OR operator
   - INPUTS:
    - {uint16_t} registerInput = value of register that needs to be operated on
    - {uint16_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint16_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint16_t setBit(uint16_t registerInput, uint16_t mask) {
  uint16_t registerOutput;
  registerOutput = registerInput | mask;

  return registerOutput;
}


/***************************************************************************************
   SET REGISTER BIT
   Function to change bit state to 1 in declared register. Uses logical OR operator
   - INPUTS:
    - {uint8_t} registerInput = value of register that needs to be operated on
    - {uint8_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint8_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint8_t setBit(uint8_t registerInput, uint8_t mask) {
  uint8_t registerOutput;
  registerOutput = registerInput | mask;

  return registerOutput;
}


/***************************************************************************************
   CLEAR REGISTER BIT
   Function to change bit state to 0 in declared register. Uses logical AND operator
   - INPUTS:
    - {uint16_t} registerInput = value of register that needs to be operated on
    - {uint16_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint16_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint16_t clearBit(uint16_t registerInput, uint16_t mask) {
  uint16_t registerOutput;
  registerOutput = registerInput & ~mask;

  return registerOutput;
}


/***************************************************************************************
   CLEAR REGISTER BIT
   Function to change bit state to 0 in declared register. Uses logical AND operator
   - INPUTS:
    - {uint8_t} registerInput = value of register that needs to be operated on
    - {uint8_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint8_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint8_t clearBit(uint8_t registerInput, uint8_t mask) {
  uint8_t registerOutput;
  registerOutput = registerInput & ~mask;

  return registerOutput;
}


/***************************************************************************************
   IS A BIT ENABLED CHECK
   Function to check whether a bit in the referenced register is set
   - INPUTS:
    - {uint16_t} registerInput = value of register that needs to be operated on
    - {uint16_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - Returns bool
****************************************************************************************/
bool isBitEnabled(uint16_t registerInput, uint16_t mask) {
  if (registerInput & mask) {
    return true;
  } else {
    return false;
  }
}


/***************************************************************************************
   IS A BIT ENABLED CHECK
   Function to check whether a bit in the referenced register is set
   - INPUTS:
    - {uint8_t} registerInput = value of register that needs to be operated on
    - {uint8_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - Returns bool
****************************************************************************************/
bool isBitEnabled(uint8_t registerInput, uint8_t mask) {
  if (registerInput & mask) {
    return true;
  } else {
    return false;
  }
}


/***************************************************************************************
   SET ACTION GROUPS
   Sets action group inputs based on current Control Group
   - No inputs
   - No outputs
****************************************************************************************/
void setActionGroups() {
  uint8_t grpMod = ctrlGrp * ctrlGrpAdd;

  ag1 = grpMod + ag1_base;
  ag2 = grpMod + ag2_base;
  ag3 = grpMod + ag3_base;
  ag4 = grpMod + ag4_base;
  ag5 = grpMod + ag5_base;
  ag6 = grpMod + ag6_base;
  ag7 = grpMod + ag7_base;
  ag8 = grpMod + ag8_base;
  ag9 = grpMod + ag9_base;
  ag10 = grpMod + ag10_base;
  solar_array = grpMod + solar_array_base;
  antenna = grpMod + antenna_base;
  cargo_door = grpMod + cargo_door_base;
  radiator = grpMod + radiator_base;
  drogue = grpMod + drogue_base;
  parachute = grpMod + parachute_base;
  ladder = grpMod + ladder_base;
  drogue_cut = grpMod + drogue_cut_base;
  les = grpMod + les_base;
  science1 = grpMod + science1_base;
  science2 = grpMod + science2_base;
  collectSci = grpMod + collectSci_base;
  engine1 = grpMod + engine1_base;
  engine2 = grpMod + engine2_base;
  engineMode = grpMod + engineMode_base;
  intake = grpMod + intake_base;
  ctrlPt1 = grpMod + ctrlPt1_base;
  ctrlPt2 = grpMod + ctrlPt2_base;
  airbrake = grpMod + airbrake_base;
}


/***************************************************************************************
   SET SAS LOGIC
   Sets the SAS logic definitions based on SAS variable received from Simpit
   - INPUTS:
    - {byte} SAS_input = value of SAS definition from Simpit
   - No outputs
****************************************************************************************/
void setSASLogic(byte SAS_input) {
  switch (SAS_input) {
    case 255:  //SMOFF
      stabAssist_on = false;
      maneuver_on = false;
      prograde_on = false;
      retrograde_on = false;
      normal_on = false;
      antiNormal_on = false;
      radialIn_on = false;
      radialOut_on = false;
      target_on = false;
      antiTarget_on = false;
      ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
      break;
    case AP_STABILITYASSIST:  //SAS Stability Assit
      stabAssist_on = true;
      maneuver_on = false;
      prograde_on = false;
      retrograde_on = false;
      normal_on = false;
      antiNormal_on = false;
      radialIn_on = false;
      radialOut_on = false;
      target_on = false;
      antiTarget_on = false;
      ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
      ledStabCtrl = setBit(ledStabCtrl, pSASEnable);
      ledStabCtrl = setBit(ledStabCtrl, pStabAssist);
      break;
    case AP_MANEUVER:  //SAS Maneuver
      stabAssist_on = false;
      maneuver_on = true;
      prograde_on = false;
      retrograde_on = false;
      normal_on = false;
      antiNormal_on = false;
      radialIn_on = false;
      radialOut_on = false;
      target_on = false;
      antiTarget_on = false;
      ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
      ledStabCtrl = setBit(ledStabCtrl, pSASEnable);
      ledStabCtrl = setBit(ledStabCtrl, pManeuver);
      break;
    case AP_PROGRADE:  //SAS Prograde
      stabAssist_on = false;
      maneuver_on = false;
      prograde_on = true;
      retrograde_on = false;
      normal_on = false;
      antiNormal_on = false;
      radialIn_on = false;
      radialOut_on = false;
      target_on = false;
      antiTarget_on = false;
      ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
      ledStabCtrl = setBit(ledStabCtrl, pSASEnable);
      ledStabCtrl = setBit(ledStabCtrl, pPrograde);
      break;
    case AP_RETROGRADE:  //SAS Retrograde
      stabAssist_on = false;
      maneuver_on = false;
      prograde_on = false;
      retrograde_on = true;
      normal_on = false;
      antiNormal_on = false;
      radialIn_on = false;
      radialOut_on = false;
      target_on = false;
      antiTarget_on = false;
      ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
      ledStabCtrl = setBit(ledStabCtrl, pSASEnable);
      ledStabCtrl = setBit(ledStabCtrl, pRetrograde);
      break;
    case AP_NORMAL:  //SAS Normal
      stabAssist_on = false;
      maneuver_on = false;
      prograde_on = false;
      retrograde_on = false;
      normal_on = true;
      antiNormal_on = false;
      radialIn_on = false;
      radialOut_on = false;
      target_on = false;
      antiTarget_on = false;
      ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
      ledStabCtrl = setBit(ledStabCtrl, pSASEnable);
      ledStabCtrl = setBit(ledStabCtrl, pNormal);
      break;
    case AP_ANTINORMAL:  //SAS Anti-Normal
      stabAssist_on = false;
      maneuver_on = false;
      prograde_on = false;
      retrograde_on = false;
      normal_on = false;
      antiNormal_on = true;
      radialIn_on = false;
      radialOut_on = false;
      target_on = false;
      antiTarget_on = false;
      ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
      ledStabCtrl = setBit(ledStabCtrl, pSASEnable);
      ledStabCtrl = setBit(ledStabCtrl, pAntiNormal);
      break;
    case AP_RADIALIN:  //SAS Radial In
      stabAssist_on = false;
      maneuver_on = false;
      prograde_on = false;
      retrograde_on = false;
      normal_on = false;
      antiNormal_on = false;
      radialIn_on = true;
      radialOut_on = false;
      target_on = false;
      antiTarget_on = false;
      ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
      ledStabCtrl = setBit(ledStabCtrl, pSASEnable);
      ledStabCtrl = setBit(ledStabCtrl, pRadialIn);
      break;
    case AP_RADIALOUT:  //SAS Radial Out
      stabAssist_on = false;
      maneuver_on = false;
      prograde_on = false;
      retrograde_on = false;
      normal_on = false;
      antiNormal_on = false;
      radialIn_on = false;
      radialOut_on = true;
      target_on = false;
      antiTarget_on = false;
      ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
      ledStabCtrl = setBit(ledStabCtrl, pSASEnable);
      ledStabCtrl = setBit(ledStabCtrl, pRadialOut);
      break;
    case AP_TARGET:  //SAS Target
      stabAssist_on = false;
      maneuver_on = false;
      prograde_on = false;
      retrograde_on = false;
      normal_on = false;
      antiNormal_on = false;
      radialIn_on = false;
      radialOut_on = false;
      target_on = true;
      antiTarget_on = false;
      ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
      ledStabCtrl = setBit(ledStabCtrl, pSASEnable);
      ledStabCtrl = setBit(ledStabCtrl, pTarget);
      break;
    case AP_ANTITARGET:  //SAS Anti-Target
      stabAssist_on = false;
      maneuver_on = false;
      prograde_on = false;
      retrograde_on = false;
      normal_on = false;
      antiNormal_on = false;
      radialIn_on = false;
      radialOut_on = false;
      target_on = false;
      antiTarget_on = true;
      ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
      ledStabCtrl = setBit(ledStabCtrl, pSASEnable);
      ledStabCtrl = setBit(ledStabCtrl, pAntiTarget);
      break;
  }
}


/***************************************************************************************
   PROCESS PANEL CONTROL INPUTS/OUTPUT
   Read incoming button/switch presses from the Panel Control module and update the
   LED states to match
   - INPUTS:
    - {uint8_t} i2c_addr = I2C address of target module
   - No outputs
****************************************************************************************/
void handlePanelCtrl(uint8_t i2c_addr) {  //Determin panel control mode via switch selected position

  /***************************************************************
    Receive button/switch inputs
  ****************************************************************/
  int16_t interrupt_val = digitalRead(PanelCtrl_INT);
  if (interrupt_val == LOW) {
    if (debug) { Serial.println("Panel Control Interrupt Detected!"); }
    Wire.requestFrom(i2c_addr, 3);      // request 3 bytes from target i2c device
    while (Wire.available()) {          // slave may send less than requested
      newButtonPanel = Wire.read();     // receive first byte
      uint8_t Enc1_high = Wire.read();  // receive second byte
      uint8_t Enc1_low = Wire.read();   // receive third byte
      uint8_t Enc2_high = Wire.read();  // receive fourth byte
      uint8_t Enc2_low = Wire.read();   // receive five byte
    }

    enc1_pos = (Enc1_high << 8) + Enc1_low;
    enc2_pos = (Enc2_high << 8) + Enc2_low;


    /***************************************************************
    Handle button input information
  ****************************************************************/
  }
}


/***************************************************************************************
   PROCESS STABILITY ASSIST PANEL INPUTS/OUTPUT
   Read incoming button/switch presses from the Stability Assist panel and update the
   LED states to match
   Stability Assist panel contains Stability Assist Group
   - INPUTS:
    - {uint8_t} mux = Controls the mux board; 0 = Main Board, 1 = Display Board
   - No outputs
****************************************************************************************/
void handleStabAssistPanel(uint8_t i2c_addr) {

  /***************************************************************
    Receive button/switch inputs
  ****************************************************************/
  int16_t interrupt_val = digitalRead(StabCtrl_INT);
  if (interrupt_val == LOW) {
    if (debug) { Serial.println("Stability Control Module Interrupt Detected!"); }
    Wire.requestFrom(i2c_addr, 2);          // request 3 bytes from target i2c device
    while (Wire.available()) {              // slave may send less than requested
      uint8_t StabCtrl_high = Wire.read();  // receive first byte
      uint8_t StabCtrl_low = Wire.read();   // receive Second byte
    }
    newButtonStabCtrl = (StabCtrl_high << 8) + StabCtrl_low;
    if (debug) {
      Serial.print("newButtonStabCtrl = ");
      Serial.println(newButtonStabCtrl, BIN);
    }

    /***************************************************************
    Process button/switch inputs
  ****************************************************************/
    //Check if SAS Enabled Switch is in position
    if (buttonPressed(prevButtonStabCtrl, newButtonStabCtrl, pSASEnable)) {
      mySimpit.activateAction(SAS_ACTION);
      mySimpit.setSASMode(AP_STABILITYASSIST);
      if (demo) {
        ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
        ledStabCtrl = setBit(ledStabCtrl, pSASEnable);
        ledStabCtrl = setBit(ledStabCtrl, pStabAssist);
      }

      if (debug) { lastAction = "SAS Enable ACTIVATED"; }
    } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pSASEnable)) {
      mySimpit.deactivateAction(SAS_ACTION);
      if (demo) {
        ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
        ledStabCtrl = clearBit(ledStabCtrl, pSASEnable);
      }
      if (debug) { lastAction = "SAS Enable DEACTIVATED"; }
    }

    //If SAS is on, the process whichever SAS mode is pressed. Only 1 can be activated at a time
    if (SAS_on) {
      if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pStabAssist) && !isBitEnabled(ledStabCtrl, pStabAssist)) {
        mySimpit.setSASMode(AP_STABILITYASSIST);
        if (demo) {
          ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
          ledStabCtrl = setBit(ledStabCtrl, pStabAssist);
        }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pManeuver) && !isBitEnabled(ledStabCtrl, pManeuver)) {
        mySimpit.setSASMode(AP_MANEUVER);
        if (demo) {
          ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
          ledStabCtrl = setBit(ledStabCtrl, pManeuver);
        }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pPrograde) && !isBitEnabled(ledStabCtrl, pPrograde)) {
        mySimpit.setSASMode(AP_PROGRADE);
        if (demo) {
          ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
          ledStabCtrl = setBit(ledStabCtrl, pPrograde);
        }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pRetrograde) && !isBitEnabled(ledStabCtrl, pRetrograde)) {
        mySimpit.setSASMode(AP_RETROGRADE);
        if (demo) {
          ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
          ledStabCtrl = setBit(ledStabCtrl, pRetrograde);
        }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pNormal) && !isBitEnabled(ledStabCtrl, pNormal)) {
        mySimpit.setSASMode(AP_NORMAL);
        if (demo) {
          ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
          ledStabCtrl = setBit(ledStabCtrl, pNormal);
        }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pAntiNormal) && !isBitEnabled(ledStabCtrl, pAntiNormal)) {
        mySimpit.setSASMode(AP_ANTINORMAL);
        if (demo) {
          ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
          ledStabCtrl = setBit(ledStabCtrl, pAntiNormal);
        }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pRadialIn) && !isBitEnabled(ledStabCtrl, pRadialIn)) {
        mySimpit.setSASMode(AP_RADIALIN);
        if (demo) {
          ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
          ledStabCtrl = setBit(ledStabCtrl, pRadialIn);
        }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pRadialOut) && !isBitEnabled(ledStabCtrl, pRadialOut)) {
        mySimpit.setSASMode(AP_RADIALOUT);
        if (demo) {
          ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
          ledStabCtrl = setBit(ledStabCtrl, pRadialOut);
        }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pTarget) && !isBitEnabled(ledStabCtrl, pTarget)) {
        mySimpit.setSASMode(AP_TARGET);
        if (demo) {
          ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
          ledStabCtrl = setBit(ledStabCtrl, pTarget);
        }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pAntiTarget) && !isBitEnabled(ledStabCtrl, pAntiTarget)) {
        mySimpit.setSASMode(AP_ANTITARGET);
        if (demo) {
          ledStabCtrl = clearBit(ledStabCtrl, pStabAsstGrp);
          ledStabCtrl = setBit(ledStabCtrl, pAntiTarget);
        }
      }
      if (debug) { lastAction = "SAS Mode Set"; }
    }

    //Check if RCS Enabled Switch is in position
    if (buttonPressed(prevButtonStabCtrl, newButtonStabCtrl, pRCSEnable)) {
      mySimpit.activateAction(RCS_ACTION);
      if (demo) { ledStabCtrl = setBit(ledStabCtrl, pRCSEnable); }
      if (debug) { lastAction = "RCS Enable ACTIVATED"; }
    } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pRCSEnable)) {
      mySimpit.deactivateAction(RCS_ACTION);
      ledStabCtrl = clearBit(ledStabCtrl, pRCSEnable);
      if (demo) { ledStabCtrl = clearBit(ledStabCtrl, pRCSEnable); }
      if (debug) { lastAction = "RCS Enable DEACTIVATED"; }
    }

    //Operate the Precision Control Selector
    if (buttonPressed(prevButtonStabCtrl, newButtonStabCtrl, pPrecisionEn)) {
      precisionEn = true;
      if (demo) { ledStabCtrl = setBit(ledStabCtrl, pPrecision); }
      if (debug) { lastAction = "Precision Connector ACTIVATED"; }
    } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pPrecisionEn)) {
      precisionEn = false;
      if (demo) { ledStabCtrl = clearBit(ledStabCtrl, pPrecision); }
      if (debug) { lastAction = "Precision Connector DEACTIVATED"; }
    }

    //Operate the Invert SAS button
    if (buttonPressed(prevButtonStabCtrl, newButtonStabCtrl, pInvertSAS)) {
      keyboardEmulatorMessage msg(F_KEY, KEY_DOWN_MOD);
      mySimpit.send(KEYBOARD_EMULATOR, msg);
      if (debug) { lastAction = "Invert SAS button PRESSED"; }
    } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pInvertSAS)) {
      keyboardEmulatorMessage msg(F_KEY, KEY_UP_MOD);
      mySimpit.send(KEYBOARD_EMULATOR, msg);
      if (debug) { lastAction = "Invert SAS button RELEASED"; }
    }
  }

  /***************************************************************
     Write the LED states based on the function definition
  ****************************************************************/
  Stg_StabCtrl_LED.write16(ledStabCtrl);
  Serial.print("ledStabCtrl = ");
  Serial.println(ledStabCtrl, BIN);
  if (debug) { lastAction = "LEDS written"; }

  prevButtonStabCtrl = newButtonStabCtrl;
}


/***************************************************************************************
   STAGING
   Function performs a soft reboot on the teensy
   - No inputs
   - No outputs
****************************************************************************************/
//Check if Staging Enabled Switch is in position and if so set the staging button light on
if (buttonPressed(prevButtonStabCtrl, newButtonStabCtrl, pStageEnable)) {
  ledStabCtrl = setBit(ledStabCtrl, pStageEnable);
} else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pStageEnable)) {
  ledStabCtrl = clearBit(ledStabCtrl, pStageEnable);
  ledStabCtrl = clearBit(ledStabCtrl, pStage);
}
if (debug) { lastAction = "Staging Enabled Check COMPLETE"; }
//If Staging Enbabled, then fire off stage action
if (buttonPressed(prevButtonStabCtrl, newButtonStabCtrl, pStage) && isBitEnabled(ledStabCtrl, pStageEnable)) {
  ledStabCtrl = clearBit(ledStabCtrl, pStage);
} else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pStage) && isBitEnabled(ledStabCtrl, pStageEnable)) {
  mySimpit.activateAction(STAGE_ACTION);
  ledStabCtrl = setBit(ledStabCtrl, pStage);
}
if (debug) { lastAction = "Staging Check COMPLETE"; }


/***************************************************************************************
   EXECUTE REBOOT
   Function performs a soft reboot on the teensy
   - No inputs
   - No outputs
****************************************************************************************/
void executeReboot() {
  // send reboot command -----
  SCB_AIRCR = 0x05FA0004;
}


/***************************************************************************************
   DISCONNECT USB
   Function disconnects the USB connection
   - No inputs
   - No outputs
****************************************************************************************/
void disconnectUSB() {
  // turnoff USB controller and reset it.
  USB1_USBCMD = 0;  // turn off USB controller
  USB1_USBCMD = 2;  // begin USB controller reset
  delay(20);
}


/***************************************************************************************
   CONNECT USB
   Function reruns the USB Connection
   - No inputs
   - No outputs
****************************************************************************************/
void connectUSB() {
  // turnoff USB controller and reset it.
  usb_init();
  delay(2000);
}


/***************************************************************************************
   WATCHDOG TIMER CALL BACK
   Callback function called by the watchdog timer function
   - No inputs
   - No outputs
****************************************************************************************/
void myCallback() {
  Serial.println("**********MAIN MICROCONTROLLER FROZEN...RESET IMMINENT**********");
  Serial.print("Last Action = ");
  Serial.println(lastAction);
  Serial.print("Total Run Time = ");
  Serial.print((millis() - runtime_start) / 1000);
  Serial.println(" s");
  Serial.println("****************************************************************");
  mstrMCActive = false;
}
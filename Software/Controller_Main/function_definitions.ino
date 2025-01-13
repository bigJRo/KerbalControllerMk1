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
   SET SAS LED State
   Sets the SAS leds based on the SAS mode definition
   - INPUTS:
    - {byte} SAS_input = value of SAS definition from Simpit
   - No outputs
****************************************************************************************/
uint16_t setSASLEDState(uint8_t SAS_input) {
  uint16_t ledOutput = 0;
  switch (SAS_input) {
    case 255:  //SMOFF
      break;
    case AP_STABILITYASSIST:  //SAS Stability Assit
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pStabAssist);
      break;
    case AP_MANEUVER:  //SAS Maneuver
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pManeuver);
      break;
    case AP_PROGRADE:  //SAS Prograde
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pPrograde);
      break;
    case AP_RETROGRADE:  //SAS Retrograde
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pRetrograde);
      break;
    case AP_NORMAL:  //SAS Normal
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pNormal);
      break;
    case AP_ANTINORMAL:  //SAS Anti-Normal
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pAntiNormal);
      break;
    case AP_RADIALIN:  //SAS Radial In
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pRadialIn);
      break;
    case AP_RADIALOUT:  //SAS Radial Out
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pRadialOut);
      break;
    case AP_TARGET:  //SAS Target
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pTarget);
      break;
    case AP_ANTITARGET:  //SAS Anti-Target
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pAntiTarget);
      break;
  }

  return ledOutput;
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
  uint8_t Enc1_high, Enc1_low, Enc2_high, Enc2_low;

  int16_t interrupt_val = digitalRead(PanelCtrl_INT);
  if (interrupt_val == LOW) {
    if (debug) { Serial.println("Panel Control Interrupt Detected!"); }
    Wire.requestFrom(i2c_addr, 3);   // request 3 bytes from target i2c device
    while (Wire.available()) {       // slave may send less than requested
      newButtonPanel = Wire.read();  // receive first byte
      Enc1_high = Wire.read();       // receive second byte
      Enc1_low = Wire.read();        // receive third byte
      Enc2_high = Wire.read();       // receive fourth byte
      Enc2_low = Wire.read();        // receive five byte
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
    - {uint8_t} i2c_addr = I2C address of target module
   - No outputs
****************************************************************************************/
void handleStabAssistPanel(uint8_t i2c_addr) {

  /***************************************************************
    Receive button/switch inputs
  ****************************************************************/
  uint8_t StabCtrl_high, StabCtrl_low;
  uint16_t ledStabCtrl = 0;

  int16_t interrupt_val = digitalRead(StabCtrl_INT);
  if (interrupt_val == LOW) {
    if (debug) { Serial.println("Stability Control Module Interrupt Detected!"); }
    Wire.requestFrom(i2c_addr, 2);  // request 3 bytes from target i2c device
    while (Wire.available()) {      // slave may send less than requested
      StabCtrl_high = Wire.read();  // receive first byte
      StabCtrl_low = Wire.read();   // receive Second byte
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
      if (demo) { SAS_mode = AP_STABILITYASSIST; }
      if (debug) {
        lastAction = "SAS Enable ACTIVATED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pSASEnable)) {
      mySimpit.deactivateAction(SAS_ACTION);
      if (demo) { SAS_mode = 255; }
      if (debug) {
        lastAction = "SAS Enable DEACTIVATED";
        Serial.println(lastAction);
      }
    }

    //If SAS is on, the process whichever SAS mode is pressed. Only 1 can be activated at a time
    if (SAS_on) {
      if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pStabAssist) && !isBitEnabled(ledStabCtrl, pStabAssist)) {
        mySimpit.setSASMode(AP_STABILITYASSIST);
        if (demo) { SAS_mode = AP_STABILITYASSIST; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pManeuver) && !isBitEnabled(ledStabCtrl, pManeuver)) {
        mySimpit.setSASMode(AP_MANEUVER);
        if (demo) { SAS_mode = AP_MANEUVER; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pPrograde) && !isBitEnabled(ledStabCtrl, pPrograde)) {
        mySimpit.setSASMode(AP_PROGRADE);
        if (demo) { SAS_mode = AP_PROGRADE; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pRetrograde) && !isBitEnabled(ledStabCtrl, pRetrograde)) {
        mySimpit.setSASMode(AP_RETROGRADE);
        if (demo) { SAS_mode = AP_RETROGRADE; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pNormal) && !isBitEnabled(ledStabCtrl, pNormal)) {
        mySimpit.setSASMode(AP_NORMAL);
        if (demo) { SAS_mode = AP_NORMAL; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pAntiNormal) && !isBitEnabled(ledStabCtrl, pAntiNormal)) {
        mySimpit.setSASMode(AP_ANTINORMAL);
        if (demo) { SAS_mode = AP_ANTINORMAL; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pRadialIn) && !isBitEnabled(ledStabCtrl, pRadialIn)) {
        mySimpit.setSASMode(AP_RADIALIN);
        if (demo) { SAS_mode = AP_RADIALIN; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pRadialOut) && !isBitEnabled(ledStabCtrl, pRadialOut)) {
        mySimpit.setSASMode(AP_RADIALOUT);
        if (demo) { SAS_mode = AP_RADIALOUT; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pTarget) && !isBitEnabled(ledStabCtrl, pTarget)) {
        mySimpit.setSASMode(AP_TARGET);
        if (demo) { SAS_mode = AP_TARGET; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pAntiTarget) && !isBitEnabled(ledStabCtrl, pAntiTarget)) {
        mySimpit.setSASMode(AP_ANTITARGET);
        if (demo) { SAS_mode = AP_ANTITARGET; }
      }
      if (debug) {
        lastAction = "SAS Mode Set";
        Serial.println(lastAction);
        Serial.print("ledStabCtrl = ");
        Serial.println(ledStabCtrl, BIN);
      }
    }

    //Check if RCS Enabled Switch is in position
    if (buttonPressed(prevButtonStabCtrl, newButtonStabCtrl, pRCSEnable)) {
      mySimpit.activateAction(RCS_ACTION);
      if (demo) { RCS_on = true; }
      if (debug) {
        lastAction = "RCS Enable ACTIVATED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pRCSEnable)) {
      mySimpit.deactivateAction(RCS_ACTION);
      ledStabCtrl = clearBit(ledStabCtrl, pRCSEnable);
      if (demo) { RCS_on = false; }
      if (debug) {
        lastAction = "RCS Enable DEACTIVATED";
        Serial.println(lastAction);
      }
    }

    //Operate the Precision Control Selector
    if (buttonPressed(prevButtonStabCtrl, newButtonStabCtrl, pPrecision)) {
      precisionEn = true;  // Apply the precision factor to the translation and rotation inputs
      if (debug) {
        lastAction = "Precision Mode ACTIVATED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pPrecision)) {
      precisionEn = false;
      if (debug) {
        lastAction = "Precision Mode DEACTIVATED";
        Serial.println(lastAction);
      }
    }

    //Operate the Invert SAS button
    if (buttonPressed(prevButtonStabCtrl, newButtonStabCtrl, pInvertSAS)) {
      keyboardEmulatorMessage msg(F_KEY, KEY_DOWN_MOD);
      mySimpit.send(KEYBOARD_EMULATOR, msg);
      if (debug) {
        lastAction = "Invert SAS button PRESSED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pInvertSAS)) {
      keyboardEmulatorMessage msg(F_KEY, KEY_UP_MOD);
      mySimpit.send(KEYBOARD_EMULATOR, msg);
      if (debug) {
        lastAction = "Invert SAS button RELEASED";
        Serial.println(lastAction);
      }
    }
  }

  /***************************************************************
     Write the LED states based on the function definition
  ****************************************************************/
  ledStabCtrl = setSASLEDState(SAS_mode);
  if (RCS_on) { ledStabCtrl = setBit(ledStabCtrl, pRCSEnable); }
  if (precisionEn) { ledStabCtrl = setBit(ledStabCtrl, pPrecision); }

  Wire.beginTransmission(i2c_addr);   // send data to the i2c target
  Wire.write(highByte(ledStabCtrl));  // send LED state high byte
  Wire.write(lowByte(ledStabCtrl));   // send LED state low byte
  Wire.endTransmission();             // COmplete transmission
  if (debug) {
    lastAction = "LEDS written";
    Serial.println(lastAction);
  }

  prevButtonStabCtrl = newButtonStabCtrl;
}


/***************************************************************************************
   PROCESS VEHCILE CONTROL PANEL INPUTS/OUTPUT
   Read incoming button/switch presses from the Vehicle Control panel and update the
   LED states to match
   Stability Assist panel contains Stability Assist Group
   - INPUTS:
    - {uint8_t} i2c_addr = I2C address of target module
   - No outputs
****************************************************************************************/
void handleVehCtrlPanel(uint8_t i2c_addr) {

  /***************************************************************
    Receive button/switch inputs
  ****************************************************************/
  uint8_t vehCtrl_high, vehCtrl_low;
  uint16_t ledVehCtrl = 0;

  int16_t interrupt_val = digitalRead(VehCtrl_INT);
  if (interrupt_val == LOW) {
    if (debug) { Serial.println("Vehicle Control Module Interrupt Detected!"); }
    Wire.requestFrom(i2c_addr, 2);  // request 3 bytes from target i2c device
    while (Wire.available()) {      // slave may send less than requested
      vehCtrl_high = Wire.read();   // receive first byte
      vehCtrl_low = Wire.read();    // receive Second byte
    }
    newButtonVehCtrl = (vehCtrl_high << 8) + vehCtrl_low;
    if (debug) {
      Serial.print("newButtonVehCtrl = ");
      Serial.println(newButtonVehCtrl, BIN);
    }

    /***************************************************************
    Process button/switch inputs
  ****************************************************************/
    // Toggle Light Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pLights) && !lights_lock) {
      mySimpit.toggleAction(LIGHT_ACTION);
      if (demo) { lights_on = !lights_on; }
      if (debug) {
        lastAction = "Lights button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Gear Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pGear) && !gear_lock) {
      mySimpit.toggleAction(GEAR_ACTION);
      if (demo) { gear_on = !gear_on; }
      if (debug) {
        lastAction = "Gear button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Activate or Deactivate Brakes Group if push button is pressed & released
    if (buttonPressed(prevButtonVehCtrl, newButtonVehCtrl, pBrakes) && !brakes_lock) {
      mySimpit.activateAction(BRAKES_ACTION);
      if (demo) { brakes_on = true; }
      if (debug) {
        lastAction = "Brakes button PRESSED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pBrakes) && !brakes_lock) {
      mySimpit.deactivateAction(BRAKES_ACTION);
      if (demo) { brakes_on = false; }
      if (debug) {
        lastAction = "Brakes button RELEASED";
        Serial.println(lastAction);
      }
    }

    // Toggle Ladder Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pLadder)) {
      mySimpit.toggleCAG(ladder);
      if (demo) { ladder_on = !ladder_on; }
      if (debug) {
        lastAction = "Ladder button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Solar Array Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pSolarArray)) {
      mySimpit.toggleCAG(solar_array);
      if (demo) { solarArray_on = !solarArray_on; }
      if (debug) {
        lastAction = "Solar Array button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Antenna Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pAntenna)) {
      mySimpit.toggleCAG(antenna);
      if (demo) { antenna_on = !antenna_on; }
      if (debug) {
        lastAction = "Antenna button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Radiator Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pRadiator)) {
      mySimpit.toggleCAG(radiator);
      if (demo) { radiator_on = !radiator_on; }
      if (debug) {
        lastAction = "Radiator button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Cargo Door Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pCargoDoor)) {
      mySimpit.toggleCAG(cargo_door);
      if (demo) { cargoDoor_on = !cargoDoor_on; }
      if (debug) {
        lastAction = "Cargo Door button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Parachute Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pParachute) && !parachute_auto) {
      mySimpit.toggleCAG(parachute);
      if (demo) { parachute_on = !parachute_on; }
      if (debug) {
        lastAction = "Parachute button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Drogue Parachute Group if push button is pressed (and drogue not deployed)
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pDrogue) && !parachute_auto) {
      mySimpit.toggleCAG(drogue);
      if (demo) { drogue_on = !drogue_on; }
      if (debug) {
        lastAction = "Drogue button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Drogue Cut Parachute Group if push button is pressed (and drogue is deployed)
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pDrogue) && !parachute_auto && drogue_on) {
      mySimpit.toggleCAG(drogue_cut);
      if (demo) { drogue_on = !drogue_on; }
      if (debug) {
        lastAction = "Drogue button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Activate or Deactivate Brake & Brake Lock Group if switch is pressed & released
    if (buttonPressed(prevButtonVehCtrl, newButtonVehCtrl, pBrakeLock)) {
      mySimpit.activateAction(BRAKES_ACTION);
      brakes_lock = true;
      if (demo) { brakes_on = true; }
      if (debug) {
        lastAction = "Brakes Lock ACTIVATED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pBrakeLock)) {
      mySimpit.deactivateAction(BRAKES_ACTION);
      brakes_lock = false;
      if (demo) { brakes_on = false; }
      if (debug) {
        lastAction = "Brakes Lock DEACTIVATED";
        Serial.println(lastAction);
      }
    }

    // Activate or Deactivate Gear & Gear Lock Group if switch is pressed & released
    if (buttonPressed(prevButtonVehCtrl, newButtonVehCtrl, pGearLock)) {
      mySimpit.activateAction(GEAR_ACTION);
      gear_lock = true;
      if (demo) { gear_on = true; }
      if (debug) {
        lastAction = "Gear Lock ACTIVATED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pGearLock)) {
      mySimpit.deactivateAction(GEAR_ACTION);
      gear_lock = false;
      if (demo) { gear_on = false; }
      if (debug) {
        lastAction = "Gear Lock DEACTIVATED";
        Serial.println(lastAction);
      }
    }

    // Activate or Deactivate Lights & Lights Lock Group if switch is pressed & released
    if (buttonPressed(prevButtonVehCtrl, newButtonVehCtrl, pLightsLock)) {
      mySimpit.activateAction(LIGHT_ACTION);
      lights_lock = true;
      if (demo) { lights_on = true; }
      if (debug) {
        lastAction = "Lights Lock ACTIVATED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pLightsLock)) {
      mySimpit.deactivateAction(LIGHT_ACTION);
      lights_lock = false;
      if (demo) { lights_on = false; }
      if (debug) {
        lastAction = "Lights Lock DEACTIVATED";
        Serial.println(lastAction);
      }
    }

    // Activate or Deactivate Parachute Auto mode if switch is pressed & released
    if (buttonPressed(prevButtonVehCtrl, newButtonVehCtrl, pChutesAuto)) {
      parachute_auto = true;
      if (debug) {
        lastAction = "Parachute Auto Mode ACTIVATED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pChutesAuto)) {
      parachute_auto = false;
      if (debug) {
        lastAction = "Parachute Auto Mode DEACTIVATED";
        Serial.println(lastAction);
      }
    }
  }

  //Process Parachute Auto
  if (parachute_auto) {
    if (alt_surf < 1500.) {  //Deploy Parachutes if altitude is below 1500m above terrain
      mySimpit.activateCAG(parachute);
      if (demo) { parachute_on = true; }
      if (debug) {
        lastAction = "Parachute ACTIVATED (Parachute Auto)";
        Serial.println(lastAction);
      }
      mySimpit.activateCAG(drogue_cut);
      if (demo) { drogue_on = !drogue_on; }
      if (debug) {
        lastAction = "Drogue CUT (Parachute Auto)";
        Serial.println(lastAction);
      }
    } else if (alt_surf < 3000.) {  //Deploy Drogues chutes if altitude is velow 3000m above terrain
      mySimpit.activateCAG(drogue);
      if (demo) { drogue_on = true; }
      if (debug) {
        lastAction = "Drogue ACTIVATED (Parachute Auto)";
        Serial.println(lastAction);
      }
    }
  }

  /***************************************************************
     Write the LED states based on the function definition
  ****************************************************************/
  if (lights_on) { ledVehCtrl = setBit(ledVehCtrl, pLights); }
  if (gear_on) { ledVehCtrl = setBit(ledVehCtrl, pGear); }
  if (brakes_on) { ledVehCtrl = setBit(ledVehCtrl, pBrakes); }
  if (ladder_on) { ledVehCtrl = setBit(ledVehCtrl, pLadder); }
  if (solarArray_on) { ledVehCtrl = setBit(ledVehCtrl, pSolarArray); }
  if (antenna_on) { ledVehCtrl = setBit(ledVehCtrl, pAntenna); }
  if (radiator_on) { ledVehCtrl = setBit(ledVehCtrl, pRadiator); }
  if (cargoDoor_on) { ledVehCtrl = setBit(ledVehCtrl, pCargoDoor); }
  if (parachute_on) { ledVehCtrl = setBit(ledVehCtrl, pParachute); }
  if (drogue_on) { ledVehCtrl = setBit(ledVehCtrl, pDrogue); }
  if (lights_lock) { ledVehCtrl = setBit(ledVehCtrl, pLightsLock); }
  if (gear_lock) { ledVehCtrl = setBit(ledVehCtrl, pGearLock); }
  if (brakes_lock) { ledVehCtrl = setBit(ledVehCtrl, pBrakeLock); }
  if (parachute_auto) { ledVehCtrl = setBit(ledVehCtrl, pChutesAuto); }


  Wire.beginTransmission(i2c_addr);  // send data to the i2c target
  Wire.write(highByte(ledVehCtrl));  // send LED state high byte
  Wire.write(lowByte(ledVehCtrl));   // send LED state low byte
  Wire.endTransmission();            // COmplete transmission
  if (debug) {
    lastAction = "LEDS written";
    Serial.println(lastAction);
  }

  prevButtonVehCtrl = newButtonVehCtrl;
}


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


/************************************
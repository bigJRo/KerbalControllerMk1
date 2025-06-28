/********************************************************************************************************************************
  Dual Rotary Module for Kerbal Controller

  Handles input from two rotary encoders and I2C communication with the host.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>               // I2C communication
#include "RotaryEncoderCore.h"  // Rotary encoder core library

/***************************************************************************************
  Pin Definitions (ATtiny816 w/ megaTinyCore Pin Mapping)
****************************************************************************************/
#define SDA PIN_PB1  // I2C data pin
#define SCL PIN_PB0  // I2C clock pin

#define INT_OUT PIN_PB2  // Interrupt output to notify host of state change

// Encoder 1
constexpr uint8_t enc1_A = PIN_PA1;    // Encoder 1 A output
constexpr uint8_t enc1_B = PIN_PB3;    // Encoder 1 B ouptut
constexpr uint8_t enc1_BTN = PIN_PA5;  // Encoder 1 button

// Encoder 2
constexpr uint8_t enc2_A = PIN_PC3;    // Encoder 2 A output
constexpr uint8_t enc2_B = PIN_PC2;    // Encoder 2 B output
constexpr uint8_t enc2_BTN = PIN_PC1;  // Encoder 2 button

/***************************************************************************************
  Constants and Globals
****************************************************************************************/
constexpr uint8_t panel_addr = 0x22;  // I2C slave address

/***************************************************************************************
  Helper functions for interrupt processing
****************************************************************************************/
void clearInterrupt() {
  digitalWrite(INT_OUT, HIGH);
}

void setInterrupt() {
  digitalWrite(INT_OUT, LOW);
}

/***************************************************************************************
  Encoder Objects
****************************************************************************************/
RotaryEncoder encoder1;
RotaryEncoder encoder2;

volatile uint8_t encoder1_event = 0;
volatile uint8_t encoder2_event = 0;

/***************************************************************************************
  I2C Event Handlers

  ATtiny816 acts as an I2C SLAVE device at address 0x232.
  The protocol between master and slave uses 4 bytes:

  - Master reads 8 bytes:
    [0-1] encoder1 position (int16_t)
    [2]   encoder1 button state (0x01 short, 0x02 long, 0x00 none)
    [3-4] encoder2 position (int16_t)
    [5]   encoder2 button state
    [6]   reserved
    [7]   reserved

****************************************************************************************/
void handleRequestEvent() {
  // Respond to master read request with 8-byte status report
  uint8_t data[8] = { 0 };
  data[0] = encoder1.position >> 8;
  data[1] = encoder1.position & 0xFF;
  data[2] = encoder1_event;
  data[3] = encoder2.position >> 8;
  data[4] = encoder2.position & 0xFF;
  data[5] = encoder2_event;
  data[6] = 0;
  data[7] = 0;

  Wire.write(data, 8);

  // Clear button events after sending
  encoder1_event = 0;
  encoder2_event = 0;
  
  clearInterrupt();  // Clear interrupt since the master responded
}


/***************************************************************************************
  Setup
****************************************************************************************/
void setup() {
  pinMode(INT_OUT, OUTPUT);
  clearInterrupt();

  noInterrupts();

  attachEncoder(encoder1, enc1_A, enc1_B, enc1_BTN);
  attachEncoder(encoder2, enc2_A, enc2_B, enc2_BTN);

  encoder1.onChange = [] (int32_t) { setInterrupt(); };
  encoder2.onChange = [] (int32_t) { setInterrupt(); };

  encoder1.onShortPress = [] () { encoder1_event = 0x01; setInterrupt(); };
  encoder1.onLongPress  = [] () { encoder1_event = 0x02; setInterrupt(); };
  encoder2.onShortPress = [] () { encoder2_event = 0x01; setInterrupt(); };
  encoder2.onLongPress  = [] () { encoder2_event = 0x02; setInterrupt(); };

  // Start I2C and register callbacks
  Wire.begin(SDA, SCL, panel_addr);
  Wire.onRequest(handleRequestEvent);
}

/***************************************************************************************
  Main Loop
****************************************************************************************/
void loop() {
  // Update encoders and detect changes
  bool changed = false;
  changed |= updateEncoder(encoder1);
  changed |= updateEncoder(encoder2);

  if (changed) {
    encoderChanged = true;
    setInterrupt();
  }
}

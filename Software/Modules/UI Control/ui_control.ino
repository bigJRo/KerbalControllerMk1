/********************************************************************************************************************************
  UI Control Module for Kerbal Controller Mk1

  References UntitledSpaceCraft module code written by CodapopKSP (https://github.com/CodapopKSP/UntitledSpaceCraft)

  Licensed under the GNU General Public License v3.0 (GPL-3.0).

  Final code was written by Jason Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>     // i2c libary compatible with ATtiny816
#include <ShiftIn.h>  // Input Shift Register Library
#include <Bounce2.h>  // Button Debounce Library

/***************************************************************************************
   Pin Definitions
****************************************************************************************/
#define SDA PIN_PB1  // SDA
#define SCL PIN_PB0  // SCL

// Final Board Pin Configuration
#define INT_OUT PIN_PB3      // Output interrupt
#define load PIN_PA4         // 74HC165 Input Load Pin
#define clockEnable PIN_PB7  // 74HC165 Input Clock Enable Pin
#define clockIn PIN_PA5      // 74HC165 Input Clock Input
#define dataInPin PIN_PB6    // 74HC165 Input Data Input Pin

/*
// Test Board Pin Configuration
#define INT_OUT PIN_PB2       // Output interrupt
#define load PIN_PA6          // 74HC165 Input Load Pin
#define clockEnable PIN_PC1   // 74HC165 Input Clock Enable Pin
#define clockIn PIN_PC2       // 74HC165 Input Clock Input
#define dataInPin PIN_PC0     // 74HC165 Input Data Input Pin
*/

/***************************************************************************************
   I2C Address Definitions
****************************************************************************************/
#define Panel_MOD 0x21

/***************************************************************************************
   Global definitions for input (control)shift registers (74HC165)
****************************************************************************************/
int16_t x[16] = {};
uint8_t button_bytes[2] = {};
ShiftIn<2> shift;  // Object for control shift registers


/***************************************************************************************
  Arduino Setup Function
****************************************************************************************/
void setup() {
  /********************************************************
    Set Pin Modes for necessary inputs/outputs
  *********************************************************/
  pinMode(INT_OUT, OUTPUT);

  /********************************************************
    Set Output Interrupt Configuration
  *********************************************************/
  digitalWrite(INT_OUT, HIGH);  //Set INT_OUT to default state

  /********************************************************
    Start object for 74HC165 Input Shift register
  *********************************************************/
  shift.begin(load, clockEnable, dataInPin, clockIn);

  encButton.setPressedState(LOW);  // INDICATE THAT THE LOW STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON

  /********************************************************
    Begin I2C comm and register receive and request events
  *********************************************************/
  Wire.begin(SDA, SCL, Panel_MOD);  // new syntax: join i2c bus (address required for slave)
  Wire.onReceive(receiveEvent);     // register event
  Wire.onRequest(requestEvent);     // register event
}


/***************************************************************************************
  Arduino Loop Function
****************************************************************************************/
void loop() {
  /********************************************************
    Update the Button configurations
  *********************************************************/
  if (shift.update()) {  // updates button input and returns value if pressed
    for (uint8_t i = 0; i < 16; i++) {
      x[i] = shift.state(i);  // get state of button i
    }

    // set the inputs bytes that can be used to LED display and transmit
    for (int i = 0; i < 8; i++) {
      bitWrite(button_bytes[0], i, x[i]);
    }
    for (int i = 0; i < 8; i++) {
      bitWrite(button_bytes[1], i, x[i + 8]);
    }

    digitalWrite(INT_OUT, LOW);  //Set INT_OUT to indicate to MC there is a change
  }
}


/***************************************************************************************
   I2C Request Event
   function that executes whenever data is requested by master
    this function is registered as an event, see setup()
   - No inputs
   - No outputs
****************************************************************************************/
void requestEvent() {
  Wire.write(button_bytes[0]);  // respond with button message high byte
  Wire.write(button_bytes[1]);  // respond with button message low byte
}

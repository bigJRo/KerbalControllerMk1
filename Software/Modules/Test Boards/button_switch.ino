/********************************************************************************************************************************
  Button Swith Tester for Kerbal Controller Mk1

  References UntitledSpaceCraft module code written by CodapopKSP (https://github.com/CodapopKSP/UntitledSpaceCraft)

  Licensed under the GNU General Public License v3.0 (GPL-3.0).

  Final code written by Jason Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>                  // i2c libary compatible with ATtiny816
#include <ShiftRegister74HC595.h>  // Output Shift Register Library
#include <ShiftIn.h>               // Input Shift Register Library
#include <Bounce2.h>               // Button Debounce Library

/***************************************************************************************
   Pin Definitions
****************************************************************************************/
#define clockPin PIN_PA4      // 74HC595 Outputs LED Serial Clock
#define latchPin PIN_PA5      // 74HC595 Outputs LED Latch
#define dataOutPin PIN_PA3    // 74HC595 Outputs LED Data Line
#define load PIN_PA6          // 74HC165 Input Load Pin
#define clockEnable PIN_PB5   // 74HC165 Input Clock Enable Pin
#define clockIn PIN_PA7       // 74HC165 Input Clock Input
#define dataInPin PIN_PB4     // 74HC165 Input Data Input Pin
#define rotarySwitch PIN_PA2  // Rotary Encoder Switch Input
#define rotaryClk PIN_PA1     // Rotary Encoder CLK
#define rotaryDT PIN_PC3      // Rotary Encoder DT
#define axis1Pin PIN_PC2      // Potentiometer axis1 (Note: nonfunctional due to non-analog pin)
#define axis2Pin PIN_PC1      // Potentiometer axis2 (Note: nonfunctional due to non-analog pin)
#define axis3Pin PIN_PC0      // Potentiometer axis3 (Note: nonfunctional due to non-analog pin)
#define INT_OUT PIN_PB2       // Output interrupt
#define SDA PIN_PB1           // SDA
#define SCL PIN_PB0           // SCL

/***************************************************************************************
   I2C Address Definitions
****************************************************************************************/
#define Panel_MOD 0x20

/***************************************************************************************
   Global definitions for ouput (display) shift registers (74HC595)
****************************************************************************************/
uint8_t led_bytes[] = { 0, 0 };
ShiftRegister74HC595<2> lights(dataOutPin, clockPin, latchPin);  // Object for display output shift registers

/***************************************************************************************
   Global definitions for input (control)shift registers (74HC165)
****************************************************************************************/
int16_t x[16] = {};
uint8_t button_bytes[2] = {};
ShiftIn<2> shift;  // Object for control shift registers

/***************************************************************************************
   Global Rotary Encoder Variables
****************************************************************************************/
int16_t counter = 0;
int16_t aState;
int16_t aLastState;

/***************************************************************************************
  Encoder Button setup
****************************************************************************************/
Bounce2::Button encButton = Bounce2::Button();
bool encPressed = false;

/***************************************************************************************
   Global Joystick Variable
****************************************************************************************/
int16_t axis1 = 0;
int16_t axis2 = 0;
int16_t axis3 = 0;




/***************************************************************************************
  Arduino Setup Function
****************************************************************************************/
void setup() {
  /********************************************************
    Set Pin Modes for necessary inputs/outputs
  *********************************************************/
  //pinMode(rotarySwitch, INPUT);  // Setup rotary button as an input
  pinMode(rotaryClk, INPUT);  // Setup rotary clock as an input
  pinMode(rotaryDT, INPUT);   // Setup rotary DT as an input
  pinMode(axis1Pin, INPUT);   // Setup potentiometer axis 1 as an input
  pinMode(axis2Pin, INPUT);   // Setup potentiometer axis 2 as an input
  pinMode(axis3Pin, INPUT);   // Setup potentiometer axis 3 as an input
  pinMode(INT_OUT, OUTPUT);

  /********************************************************
    Set Output Interrupt Configuration
  *********************************************************/
  digitalWrite(INT_OUT, HIGH);  //Set INT_OUT to default state

  /********************************************************
    LED Setup
  *********************************************************/
  lights.setAllLow();  // Ensure all LEDs are off at startup

  /********************************************************
    Start object for 74HC165 Input Shift register
  *********************************************************/
  shift.begin(load, clockEnable, dataInPin, clockIn);

  /********************************************************
    Rotary Encoder Initial Configuration
  *********************************************************/
  aLastState = digitalRead(rotaryClk);

  encButton.attach(rotarySwitch, INPUT);  // // BUTTON SETUP W/ EXTERNAL PULL-UP
  encButton.interval(10);                 // DEBOUNCE INTERVAL IN MILLISECONDS
  encButton.setPressedState(LOW);         // INDICATE THAT THE LOW STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON

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

  /********************************************************
    Check Encoder and update counter & check button
  *********************************************************/
  aState = digitalRead(rotaryClk);
  if (aState != aLastState) {
    if (digitalRead(rotaryDT) != aState) {
      counter++;
    } else {
      counter--;
    }
    digitalWrite(INT_OUT, LOW);  //Set INT_OUT to indicate to MC there is a change
  }
  aLastState = aState;

  encButton.update();
  if (encButton.released()) {
    encPressed = !encPressed;
    digitalWrite(INT_OUT, LOW);  //Set INT_OUT to indicate to MC there is a change
  }

  /********************************************************
    Update LEDs
  *********************************************************/
  led_bytes[0] = button_bytes[1];
  led_bytes[1] = button_bytes[0];
  lights.setAll(led_bytes);
}


/***************************************************************************************
   I2C Receive Event
   Function that executes whenever data is received from master
    this function is registered as an event, see setup()
   - INPUTS:
    - {size_t} howMany = how many bits recieved
   - No outputs
****************************************************************************************/
void receiveEvent(size_t howMany) {
  (void)howMany;
  while (1 < Wire.available()) {  // loop through all but the last
    led_bytes[1] = Wire.read();   // receive led instructions
    led_bytes[0] = Wire.read();   // receive led instructions
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
  Wire.write(button_bytes[0]);    // respond with button message high byte
  Wire.write(button_bytes[1]);    // respond with button message low byte
  Wire.write(highByte(counter));  // respond with encoder counter low byte
  Wire.write(lowByte(counter));   // respond with encoder counter high byte
}

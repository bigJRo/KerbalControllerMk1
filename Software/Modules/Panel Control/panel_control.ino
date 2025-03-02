/********************************************************************************************************************************
  Panel Control Module for Kerbal Controller Mk1

  References UntitledSpaceCraft module code written by CodapopKSP (https://github.com/CodapopKSP/UntitledSpaceCraft)

  Licensed under the GNU General Public License v3.0 (GPL-3.0).

  Final code written by Jason Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>     // i2c libary compatible with ATtiny816
#include <Bounce2.h>  // Button Debounce Library

/***************************************************************************************
   Pin Definitions
****************************************************************************************/
#define SDA PIN_PB1  // SDA
#define SCL PIN_PB0  // SCL

// Final Board Pin Configuration
#define INT_OUT PIN_PC3         // Output interrupt
#define rotarySwitch_A PIN_PA3  // Rotary Encoder Switch Input
#define rotaryClk_A PIN_PA2     // Rotary Encoder CLK
#define rotaryDT_A PIN_PA1      // Rotary Encoder DT
#define rotarySwitch_B PIN_PA4  // Rotary Encoder Switch Input
#define rotaryClk_B PIN_PA5     // Rotary Encoder CLK
#define rotaryDT_B PIN_PA6      // Rotary Encoder DT
#define button01 PIN_PB4        // Button 1
#define button02 PIN_PB5        // Button 2
#define button03 PIN_PA7        // Button 3
#define button04 PIN_PB2        // Button 4
#define button05 PIN_PB3        // Button 5
#define led01 PIN_PC0           // LED 1
#define led02 PIN_PC1           // LED 2
#define led03 PIN_PC2           // LED 3

/*
// Test Board Pin Configuration
#define INT_OUT PIN_PB2       // Output interrupt
#define rotarySwitch_A PIN_PC0  // Rotary Encoder Switch Input
#define rotaryClk_A PIN_PC2     // Rotary Encoder CLK
#define rotaryDT_A PIN_PC1      // Rotary Encoder DT
#define rotarySwitch_B PIN_PA2  // Rotary Encoder Switch Input
#define rotaryClk_B PIN_PA1     // Rotary Encoder CLK
#define rotaryDT_B PIN_PC3      // Rotary Encoder DT
#define button01 PIN_PB3      // Button 1
#define button02 PIN_PB4      // Button 2
#define button03 PIN_PB5      // Button 3
#define button04 PIN_PA7      // Button 4
#define button05 PIN_PA6      // Button 5
#define led01 PIN_PA5      // LED 1
#define led02 PIN_PA4      // LED 2
#define led03 PIN_PA3      // LED 3
*/



/***************************************************************************************
   I2C Address Definitions
****************************************************************************************/
#define Panel_MOD 0x20

/***************************************************************************************
   Global Rotary Encoder Variables
****************************************************************************************/
int16_t counter_A = 0;
int16_t state_A;
int16_t lastState_A;

int16_t counter_B = 0;
int16_t state_B;
int16_t lastState_B;

/***************************************************************************************
  Encoder Button setup
****************************************************************************************/
Bounce2::Button but_01 = Bounce2::Button();
Bounce2::Button but_02 = Bounce2::Button();
Bounce2::Button but_03 = Bounce2::Button();
Bounce2::Button but_04 = Bounce2::Button();
Bounce2::Button but_05 = Bounce2::Button();
Bounce2::Button encA_button = Bounce2::Button();
Bounce2::Button encB_button = Bounce2::Button();

/***************************************************************************************
  Global variable setup
****************************************************************************************/
uint8_t buttons = 0;
uint8_t led_State = 0;





/***************************************************************************************
  Arduino Setup Function
****************************************************************************************/
void setup() {
  /********************************************************
    Set Pin Modes for necessary inputs/outputs
  *********************************************************/
  pinMode(INT_OUT, OUTPUT);
  pinMode(rotaryClk_A, INPUT);  // Setup encoder clock as input
  pinMode(rotaryDT_A, INPUT);   // Setup encoder clock as input
  pinMode(rotaryClk_B, INPUT);  // Setup encoder clock as input
  pinMode(rotaryDT_B, INPUT);   // Setup encoder clock as input
  pinMode(led01, OUTPUT);       // Setup leds as outputs
  pinMode(led02, OUTPUT);       // Setup leds as outputs
  pinMode(led03, OUTPUT);       // Setup leds as outputs

  /********************************************************
    Set Output Interrupt Configuration
  *********************************************************/
  digitalWrite(INT_OUT, HIGH);  //Set INT_OUT to default state

  /********************************************************
    Rotary Encoder Initial Configuration
  *********************************************************/
  lastState_A = digitalRead(rotaryClk_A);
  lastState_B = digitalRead(rotaryClk_B);

  encA_button.attach(rotarySwitch_A, INPUT);  // // BUTTON SETUP W/ EXTERNAL PULL-UP
  encA_button.interval(10);                   // DEBOUNCE INTERVAL IN MILLISECONDS
  encA_button.setPressedState(LOW);           // INDICATE THAT THE LOW STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON

  encB_button.attach(rotarySwitch_B, INPUT);  // // BUTTON SETUP W/ EXTERNAL PULL-UP
  encB_button.interval(10);                   // DEBOUNCE INTERVAL IN MILLISECONDS
  encB_button.setPressedState(LOW);           // INDICATE THAT THE LOW STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON

  but_01.attach(button01, INPUT);  // // BUTTON SETUP W/ EXTERNAL PULL-UP
  but_01.interval(10);            // DEBOUNCE INTERVAL IN MILLISECONDS
  but_01.setPressedState(HIGH);   // INDICATE THAT THE HIGH STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON

  but_02.attach(button02, INPUT);  // // BUTTON SETUP W/ EXTERNAL PULL-UP
  but_02.interval(10);            // DEBOUNCE INTERVAL IN MILLISECONDS
  but_02.setPressedState(HIGH);   // INDICATE THAT THE HIGH STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON

  but_03.attach(button03, INPUT);  // // BUTTON SETUP W/ EXTERNAL PULL-UP
  but_03.interval(10);            // DEBOUNCE INTERVAL IN MILLISECONDS
  but_03.setPressedState(HIGH);   // INDICATE THAT THE HIGH STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON

  but_04.attach(button04, INPUT);  // // BUTTON SETUP W/ EXTERNAL PULL-UP
  but_04.interval(10);            // DEBOUNCE INTERVAL IN MILLISECONDS
  but_04.setPressedState(HIGH);   // INDICATE THAT THE HIGH STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON

  but_05.attach(button05, INPUT);  // // BUTTON SETUP W/ EXTERNAL PULL-UP
  but_05.interval(10);            // DEBOUNCE INTERVAL IN MILLISECONDS
  but_05.setPressedState(HIGH);   // INDICATE THAT THE HIGH STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON


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
    Check Encoder A and update counter & check button
  *********************************************************/
  state_A = digitalRead(rotaryClk_A);
  if (state_A != lastState_A) {
    if (digitalRead(rotaryDT_A) != state_A) {
      counter_A++;
    } else {
      counter_A--;
    }
    digitalWrite(INT_OUT, LOW);  //Set INT_OUT to indicate to MC there is a change
  }
  lastState_A = state_A;

  encA_button.update();
  if (encA_button.released()) {
    buttons |= (1 << 5);
    digitalWrite(INT_OUT, LOW);  //Set INT_OUT to indicate to MC there is a change
  }

  /********************************************************
    Check Encoder B and update counter & check button
  *********************************************************/
  state_B = digitalRead(rotaryClk_B);
  if (state_B != lastState_B) {
    if (digitalRead(rotaryDT_B) != state_B) {
      counter_B++;
    } else {
      counter_B--;
    }
    digitalWrite(INT_OUT, LOW);  //Set INT_OUT to indicate to MC there is a change
  }
  lastState_B = state_B;

  encB_button.update();
  if (encB_button.released()) {
    buttons |= (1 << 6);
    digitalWrite(INT_OUT, LOW);  //Set INT_OUT to indicate to MC there is a change
  }

  /********************************************************
    Check remaining buttons
  *********************************************************/
  but_01.update();
  if (but_01.released()) {
    buttons |= (1 << 0);
    digitalWrite(INT_OUT, LOW);  //Set INT_OUT to indicate to MC there is a change
  }
  but_02.update();
  if (but_02.released()) {
    buttons |= (1 << 1);
    digitalWrite(INT_OUT, LOW);  //Set INT_OUT to indicate to MC there is a change
  }
  but_03.update();
  if (but_03.released()) {
    buttons |= (1 << 2);
    digitalWrite(INT_OUT, LOW);  //Set INT_OUT to indicate to MC there is a change
  }
  but_04.update();
  if (but_04.released()) {
    buttons |= (1 << 3);
    digitalWrite(INT_OUT, LOW);  //Set INT_OUT to indicate to MC there is a change
  }
  but_05.update();
  if (but_05.released()) {
    buttons |= (1 << 4);
    digitalWrite(INT_OUT, LOW);  //Set INT_OUT to indicate to MC there is a change
  }

  /********************************************************
    Set LEDs
  *********************************************************/
  if (led_State & 0x01) {
    digitalWrite(led01, HIGH);
  } else {
    digitalWrite(led01, LOW);
  }

  if (led_State & 0x02) {
    digitalWrite(led02, HIGH);
  } else {
    digitalWrite(led02, LOW);
  }

  if (led_State & 0x03) {
    digitalWrite(led03, HIGH);
  } else {
    digitalWrite(led03, LOW);
  }
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
    led_State = Wire.read();      // receive led instructions
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
  Wire.write(buttons);              // respond with button message state
  Wire.write(highByte(counter_A));  // respond with encoder counter low byte
  Wire.write(lowByte(counter_A));   // respond with encoder counter high byte
  Wire.write(highByte(counter_B));  // respond with encoder counter low byte
  Wire.write(lowByte(counter_B));   // respond with encoder counter high byte
  buttons = 0;                      //Reset button state once it is read
  digitalWrite(INT_OUT, HIGH);      //Reset INT_OUT since change was registered
}

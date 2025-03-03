/********************************************************************************************************************************
  Throttle Module for Kerbal Controller Mk1

  References UntitledSpaceCraft module code written by CodapopKSP (https://github.com/CodapopKSP/UntitledSpaceCraft)

  Licensed under the GNU General Public License v3.0 (GPL-3.0).

  Final code written by Jason Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>                           // i2c libary compatible with ATtiny816
#include <Bounce2.h>                        // Button Debounce Library
#include <KerbalControllerMk1_Functions.h>  // Custom functions library

/***************************************************************************************
   Pin Definitions
****************************************************************************************/
#define SDA PIN_PB1  // SDA
#define SCL PIN_PB0  // SCL

// Final Board Pin Configuration
#define INT_OUT PIN_PB2      // Output interrupt
#define THRTL_ENA PIN_PC1    // Throttle Enable input
#define ENA_IND PIN_PC0      // Throttle Enable LED indicator output
#define THRTL_100 PIN_PC2    // Throttle to full input
#define THRTL_00 PIN_PA2     // Throttle to off input
#define THRTL_UP PIN_PC3     // Throttle step up input
#define THRTL_DOWN PIN_PA1   // Throttle step up input
#define SPEED PIN_PA4        // Motor speed control output
#define MOTOR_FWD PIN_PA5    // Motor forward command output
#define MOTOR_REV PIN_PA6    // Motor reverse command output
#define TOUCH PIN_PA5        // Throttle touch input
#define THRTL_INPUT PIN_PA5  // Throttle potentiometer wiper input

/***************************************************************************************
   I2C Address Definitions
****************************************************************************************/
#define Panel_MOD 0x26

/***************************************************************************************
  Encoder Button setup
****************************************************************************************/
Bounce2::Button thrtlEna = Bounce2::Button();
Bounce2::Button thrtlOff = Bounce2::Button();
Bounce2::Button thrtlDown = Bounce2::Button();
Bounce2::Button thrtlUp = Bounce2::Button();
Bounce2::Button thrtlFull = Bounce2::Button();


/***************************************************************************************
   Global Variable Definitions
****************************************************************************************/
int16_t prev_throttle = 0;
bool hostCommand = false;
int16_t hostThrtlCmd = -1000;
int16_t buttonThrtlCmd = -1000;
bool touch = false;
bool actionEnabled = false;
int16_t prevPosition = -1000;




/***************************************************************************************
  Arduino Setup Function
****************************************************************************************/
void setup() {
  /********************************************************
    Set Pin Modes for necessary inputs/outputs
  *********************************************************/
  pinMode(TOUCH, INPUT);        // Setup touch sensor high as an input
  pinMode(THRTL_INPUT, INPUT);  // Setup potentiometer wiper as an input
  pinMode(ENA_IND, OUTPUT);     // Set Throttle Enable indicator LED as output
  pinMode(MOTOR_FWD, OUTPUT);   // Motorized fader output FWD direction
  pinMode(MOTOR_REV, OUTPUT);   // Motorized fader out REV direction
  pinMode(SPEED, OUTPUT);       // Motorized fader output for speed control
  pinMode(INT_OUT, OUTPUT);

  /********************************************************
    Set Output Interrupt Configuration
  *********************************************************/
  clearInterrupt();

  /********************************************************
    Button Initial Configuration
  *********************************************************/
  thrtlEna.attach(THRTL_ENA, INPUT);  // BUTTON SETUP W/ EXTERNAL PULL-DOWN
  thrtlEna.interval(10);              // DEBOUNCE INTERVAL IN MILLISECONDS
  thrtlEna.setPressedState(HIGH);     // INDICATE THAT THE HIGH STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON

  thrtlFull.attach(THRTL_100, INPUT);  // BUTTON SETUP W/ EXTERNAL PULL-DOWN
  thrtlFull.interval(10);              // DEBOUNCE INTERVAL IN MILLISECONDS
  thrtlFull.setPressedState(HIGH);     // INDICATE THAT THE HIGH STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON

  thrtlUp.attach(THRTL_UP, INPUT);  // BUTTON SETUP W/ EXTERNAL PULL-DOWN
  thrtlUp.interval(10);             // DEBOUNCE INTERVAL IN MILLISECONDS
  thrtlUp.setPressedState(HIGH);    // INDICATE THAT THE HIGH STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON

  thrtlDown.attach(THRTL_DOWN, INPUT);  // BUTTON SETUP W/ EXTERNAL PULL-DOWN
  thrtlDown.interval(10);               // DEBOUNCE INTERVAL IN MILLISECONDS
  thrtlDown.setPressedState(HIGH);      // INDICATE THAT THE HIGH STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON

  thrtlOff.attach(THRTL_00, INPUT);  // BUTTON SETUP W/ EXTERNAL PULL-DOWN
  thrtlOff.interval(10);             // DEBOUNCE INTERVAL IN MILLISECONDS
  thrtlOff.setPressedState(HIGH);    // INDICATE THAT THE HIGH STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON

  /********************************************************
    Send throttle to 0 position
  *********************************************************/
  goToPosition(0);

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
  int16_t commandedPosition;  //commanded position is value between 0 and 1023 that corresponds to potentiometer wiper position

  /********************************************************
    Chect Trottle Enables and Set Parameter if HIGH
  *********************************************************/
  thrtlEna.update();
  if (thrtlEna.isPressed()) {
    actionEnabled = true;
    digitalWrite(ENA_IND, HIGH);  // set the throttle enable LED on
  } else {
    actionEnabled = false;
    digitalWrite(ENA_IND, LOW);  // set the throttle enable LED off
  }

  /********************************************************
    Set the Commanded Position for the potentiometer
  *********************************************************/
  if (hostCommand) {
    commandedPosition = hostThrtlCmd;  // if host command position received over I2C, set as commanded position
    hostCommand = false;
  } else if (setButtonThrtl() && actionEnabled) {  // allow button command position to be set, if throttle enabled
    commandedPosition = buttonThrtlCmd;
  } else {  // otherwise set to the current position
    commandedPosition = analogRead(THRTL_INPUT);
  }

  /********************************************************
    Set the Commanded Position for the potentiometer
  *********************************************************/
  if (digitalRead(TOUCH) == LOW) {  // Only operate if operator does not have their handle on the control
    goToPosition(commandedPosition);
  }

  /********************************************************
    Return updated position of throttle
  *********************************************************/
  if (actionEnabled && (commandedPosition != prevPosition)) {  // Only operate if operator does not have their handle on the control
    setInterrupt();
  }

  /********************************************************
    Save commanded throttle potentiometer position
  *********************************************************/
  prevPosition = commandedPosition;
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
    hostThrtlCmd = receiveInt16();
    hostCommand = true;
  }
}


/***************************************************************************************
   I2C Request Event
   Function that executes whenever data is requested by master
    this function is registered as an event, see setup()
   - No inputs
   - No outputs
****************************************************************************************/
void requestEvent() {
  Wire.write(highByte(prevPosition));  // respond with encoder counter low byte
  Wire.write(lowByte(prevPosition));   // respond with encoder counter high byte
  clearInterrupt();
}


/***************************************************************************************
   Set Interrupt
   Function that sets the interrupt when called, LOW indicates interrupt fired
   - No inputs
   - No outputs
****************************************************************************************/
void setInterrupt() {
  digitalWrite(INT_OUT, LOW);
}

/***************************************************************************************
   Set Interrupt
   Function that clears the interrupt when called, LOW indicates interrupt fired
   - No inputs
   - No outputs
****************************************************************************************/
void clearInterrupt() {
  digitalWrite(INT_OUT, HIGH);
}


/***************************************************************************************
   Go To Position
   Function to move the motorized fader into the commanded position
   - INPUTS:
    - {int16_t} cmd_position = Requested motor fader position; value between 0 and 1023
   - No outputs
****************************************************************************************/
void goToPosition(int16_t cmd_position) {
  //Local cotnrol parameter definitions
  uint8_t pwmSpeed = 230;
  int16_t motor_deadzone = 6;


  // Command the motor to match the throttle position to what is being requested
  while (abs(analogRead(THRTL_INPUT) - cmd_position) > motor_deadzone) {
    if (analogRead(THRTL_INPUT) > cmd_position) {  // Throttle is higher than what is requested
      analogWrite(SPEED, pwmSpeed);
      digitalWrite(MOTOR_FWD, LOW);
      digitalWrite(MOTOR_REV, HIGH);
    }
    if (analogRead(THRTL_INPUT) < cmd_position) {  // Throttle is lower than what is requested
      analogWrite(SPEED, pwmSpeed);
      digitalWrite(MOTOR_FWD, HIGH);
      digitalWrite(MOTOR_REV, LOW);
    }
  }
  analogWrite(SPEED, 0);         //Disable the motor
  digitalWrite(MOTOR_REV, LOW);  //Disable the motor
  digitalWrite(MOTOR_FWD, LOW);  //Disable the motor
}


/***************************************************************************************
   Set Button Throttle 
   Function sets the throttle value from button pressed
   - INPUTS:
    - {int16_t} buttonThrtl = a commanded position based on the button pressed
   - No outputs
****************************************************************************************/
bool setButtonThrtl() {
  bool buttonCommand = false;
  int16_t throttleInc = 1023 / 20;
  int16_t smallThrottleInc = throttleInc / 5;
  int16_t throttleThreshold = 1023 / 10;
  int16_t curThrtl = analogRead(THRTL_INPUT);

  thrtlFull.update();  // Check the full throttle button
  if (thrtlFull.released()) {
    buttonThrtlCmd = 1023;
    buttonCommand = true;
  }
  thrtlOff.update();  // Check the throttle off button
  if (thrtlOff.released()) {
    buttonThrtlCmd = 0;
    buttonCommand = true;
  }
  thrtlUp.update();  // Check the throttle up button
  if (thrtlUp.released()) {
    if (curThrtl < throttleThreshold) {
      buttonThrtlCmd = curThrtl + smallThrottleInc;
    } else {
      buttonThrtlCmd = curThrtl + throttleInc;
    }
    buttonThrtlCmd = constrain(buttonThrtlCmd, 0, 1023);
    buttonCommand = true;
  }
  thrtlDown.update();  // Check the throttle down button
  if (thrtlDown.released()) {
    if (curThrtl < throttleThreshold) {
      buttonThrtlCmd = curThrtl + smallThrottleInc;
    } else {
      buttonThrtlCmd = curThrtl + throttleInc;
    }
    buttonThrtlCmd = constrain(buttonThrtlCmd, 0, 1023);
    buttonCommand = true;
  }

  return buttonCommand;
}

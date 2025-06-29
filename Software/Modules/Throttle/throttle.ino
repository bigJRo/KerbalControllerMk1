/********************************************************************************************************************************
  Throttle Module for Kerbal Controller Mk1

  Handles throttle implentation for use in Kerbal Controller Mk1
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>  // I2C communication

/***************************************************************************************
   Pin Definitions
****************************************************************************************/
#define SDA PIN_PB1  // SDA
#define SCL PIN_PB0  // SCL

#define INT_OUT PIN_PA3  // Output interrupt

#define WIPER PIN_PA1      // Throttle potentiometer wiper input
#define TOUCH PIN_PA2      // Throttle touch input
#define FINE_MODE PIN_PA4  // Engage fine mode

#define MOTOR_REV PIN_PA5  // Motor reverse command output
#define MOTOR_FWD PIN_PA6  // Motor forward command output
#define SPEED PIN_PA7      // Motor speed control output

#define THRTL_100 PIN_PB4   // Throttle to full input
#define LED_100 PIN_PB5     // Throttle to full LED
#define THRTL_UP PIN_PB3    // Throttle step up input
#define LED_UP PIN_PB2      // Throttle step up LED
#define THRTL_DOWN PIN_PC1  // Throttle step down input
#define LED_DOWN PIN_PC0    // Throttle step down LED
#define THRTL_00 PIN_PC2    // Throttle to off input
#define LED_00 PIN_PC3      // Throttle to off input

#define CMD_ENABLE_THROTTLE 0
#define MAX_THROTTLE 1023
#define MIN_THROTTLE 0
#define THROTTLE_STEP 5  // Percentage of throttle for each step taken by Throttle Up/Down

/***************************************************************************************
   I2C Address Definitions
****************************************************************************************/
constexpr uint8_t panel_addr = 0x29;  // I2C address for this module

/***************************************************************************************
   Global Variable Definitions
****************************************************************************************/
int16_t host_throttle_position = 0;
uint8_t hostCommand = 0;
bool throttleEnabled = false;
const int16_t throttleIncrement = (MAX_THROTTLE / 100) * THROTTLE_STEP;


bool touch = false;
int16_t prevPosition = -1000;

const uint8_t buttonPins[4] = { THRTL_100, THRTL_UP, THRTL_DOWN, THRTL_00 };  // Active HIGH buttons
const unsigned long debounceDelay = 50;
bool buttonStates[4] = { false };            // Stable states
bool lastReadings[4] = { false };            // Last raw readings
unsigned long lastDebounceTimes[4] = { 0 };  // Timestamps per button

/***************************************************************************************
   Interrupt Helpers
****************************************************************************************/
void setInterrupt() {
  digitalWrite(INT_OUT, LOW);
}

void clearInterrupt() {
  digitalWrite(INT_OUT, HIGH);
}

/***************************************************************************************
  Motor Control
****************************************************************************************/
void goToPosition(int16_t cmd_position) {
  cmd_position = constrain(cmd_position, MIN_THROTTLE, MAX_THROTTLE);
  unsigned long startTime = millis();
  const uint8_t pwmSpeed = 230;
  const int16_t motor_deadzone = 6;

  int16_t current_position = analogRead(WIPER);
  while (abs(current_position - cmd_position) > motor_deadzone) {
    if (millis() - startTime > 2000) break;
    current_position = analogRead(WIPER);

    if (current_position > cmd_position) {
      analogWrite(SPEED, pwmSpeed);
      digitalWrite(MOTOR_FWD, LOW);
      digitalWrite(MOTOR_REV, HIGH);
    } else if (current_position < cmd_position) {
      analogWrite(SPEED, pwmSpeed);
      digitalWrite(MOTOR_FWD, HIGH);
      digitalWrite(MOTOR_REV, LOW);
    }
  }

  analogWrite(SPEED, 0);
  digitalWrite(MOTOR_REV, LOW);
  digitalWrite(MOTOR_FWD, LOW);
}

/***************************************************************************************
  Button Handler
****************************************************************************************/
uint8_t readButtonInputs() {
  uint8_t result = 0;
  for (uint8_t i = 0; i < 4; i++) {
    bool reading = digitalRead(buttonPins[i]);
    if (reading != lastReadings[i]) {
      lastDebounceTimes[i] = millis();
      lastReadings[i] = reading;
    }

    if ((millis() - lastDebounceTimes[i]) > debounceDelay) {
      if (reading != buttonStates[i]) {
        buttonStates[i] = reading;
        if (buttonStates[i]) {
          result |= (1 << i);
        }
      }
    }
  }
  return result;
}

/***************************************************************************************
  Bulb Test Routine
  - Cycles all throttle LEDs ON for 500ms
  - Sweeps motor from MIN to MAX and back
****************************************************************************************/
void bulbTest() {
  setAllLEDs(HIGH);
  delay(500);
  setAllLEDs(LOW);
  delay(250);

  goToPosition(MAX_THROTTLE);
  delay(250);
  goToPosition(MIN_THROTTLE);
  delay(250);
}

/***************************************************************************************
  LED support function
****************************************************************************************/
void setAllLEDs(bool state) {
  digitalWrite(LED_100, state);
  digitalWrite(LED_UP, state);
  digitalWrite(LED_DOWN, state);
  digitalWrite(LED_00, state);
}

/***************************************************************************************
  I2C Handlers
****************************************************************************************/
void handleRequestEvent() {
  uint8_t response[2] = {
    (uint8_t)(host_throttle_position & 0xFF),
    (uint8_t)(host_throttle_position >> 8)
  };
  Wire.write(response, sizeof(response));
  clearInterrupt();
}

void handleReceiveEvent(int howMany) {
  if (howMany < 1) return;
  hostCommand = Wire.read();
  throttleEnabled = bitRead(hostCommand, CMD_ENABLE_THROTTLE);
}

/***************************************************************************************
  Arduino Setup
****************************************************************************************/
void setup() {
  pinMode(INT_OUT, OUTPUT);
  clearInterrupt();

  pinMode(WIPER, INPUT);
  pinMode(TOUCH, INPUT);
  pinMode(FINE_MODE, INPUT);

  pinMode(MOTOR_FWD, OUTPUT);
  pinMode(MOTOR_REV, OUTPUT);
  pinMode(SPEED, OUTPUT);

  pinMode(THRTL_100, INPUT);
  pinMode(LED_100, OUTPUT);
  pinMode(THRTL_UP, INPUT);
  pinMode(LED_UP, OUTPUT);
  pinMode(THRTL_DOWN, INPUT);
  pinMode(LED_DOWN, OUTPUT);
  pinMode(THRTL_00, INPUT);
  pinMode(LED_00, OUTPUT);

  digitalWrite(LED_100, LOW);
  digitalWrite(LED_UP, LOW);
  digitalWrite(LED_DOWN, LOW);
  digitalWrite(LED_00, LOW);

  bulbTest();
  goToPosition(MIN_THROTTLE);

  Wire.begin(SDA, SCL, panel_addr);
  Wire.onReceive(handleReceiveEvent);
  Wire.onRequest(handleRequestEvent);
}

/***************************************************************************************
  Arduino Loop
****************************************************************************************/
void loop() {
  int16_t commandedPosition;

  bool fineModeEnabled = digitalRead(FINE_MODE);
  
  uint8_t pressed = readButtonInputs();

  if (throttleEnabled) {
    setAllLEDs(HIGH);

    if (pressed & 0b0001) {
      commandedPosition = MAX_THROTTLE;
    } else if (pressed & 0b0010) {
      commandedPosition = prevPosition + throttleIncrement;
    } else if (pressed & 0b0100) {
      commandedPosition = prevPosition - throttleIncrement;
    } else if (pressed & 0b1000) {
      commandedPosition = MIN_THROTTLE;
    } else {
      commandedPosition = prevPosition;
    }

    if (digitalRead(TOUCH) == LOW) {
      goToPosition(commandedPosition);
    }

    int16_t throttle_position = analogRead(WIPER);
    if (throttleEnabled && (throttle_position != prevPosition)) {
      host_throttle_position = map(throttle_position, MIN_THROTTLE, MAX_THROTTLE, 0, INT16_MAX);
      setInterrupt();
    }

    prevPosition = throttle_position;
  } else {
    setAllLEDs(LOW);
  }
}

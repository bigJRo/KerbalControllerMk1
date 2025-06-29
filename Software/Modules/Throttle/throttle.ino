/********************************************************************************************************************************
  Throttle Module for Kerbal Controller Mk1

  Handles throttle implentation for use in Kerbal Controller Mk1
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>  // I2C communication library

/***************************************************************************************
   Pin Definitions
****************************************************************************************/
#define SDA PIN_PB1  // I2C data
#define SCL PIN_PB0  // I2C clock

#define INT_OUT PIN_PA3  // Interrupt output to host (active LOW)

#define WIPER PIN_PA1      // Analog throttle potentiometer input
#define TOUCH PIN_PA2      // Capacitive touch sensor (active LOW)
#define FINE_MODE PIN_PA4  // Fine mode switch (active HIGH)

#define MOTOR_REV PIN_PA5  // Motor reverse direction
#define MOTOR_FWD PIN_PA6  // Motor forward direction
#define SPEED PIN_PA7      // PWM speed control for motor

#define THRTL_100 PIN_PB4   // Button: full throttle (active HIGH)
#define LED_100 PIN_PB5     // LED: full throttle indicator
#define THRTL_UP PIN_PB3    // Button: increase throttle (active HIGH)
#define LED_UP PIN_PB2      // LED: increase indicator
#define THRTL_DOWN PIN_PC1  // Button: decrease throttle (active HIGH)
#define LED_DOWN PIN_PC0    // LED: decrease indicator
#define THRTL_00 PIN_PC2    // Button: zero throttle (active HIGH)
#define LED_00 PIN_PC3      // LED: zero indicator

#define CMD_ENABLE_THROTTLE 0  // Host sets bit 0 to enable throttle processing
#define MAX_THROTTLE 1023      // Maximum analog input value
#define MIN_THROTTLE 0         // Minimum analog input value
#define THROTTLE_STEP 5        // Step increment in percent
#define FINE_MODE_FACTOR 10    // Fine mode reduces range to 10%

/***************************************************************************************
   I2C Address Definitions
****************************************************************************************/
constexpr uint8_t panel_addr = 0x29;  // I2C address for this module

/***************************************************************************************
   Global Variable Definitions
****************************************************************************************/
int16_t host_throttle_position = 0;  // Value sent to host (0 to INT16_MAX)
uint8_t hostCommand = 0;             // Host command byte
bool throttleEnabled = false;        // Whether throttle is enabled
constexpr int16_t throttleIncrement = (MAX_THROTTLE / 100) * THROTTLE_STEP;
bool lastFineMode = false;  // Track fine mode state transitions

int16_t prevPosition = -1000;  // Last raw throttle position

const uint8_t buttonPins[4] = { THRTL_100, THRTL_UP, THRTL_DOWN, THRTL_00 };  // Button pin array
const unsigned long debounceDelay = 50;                                       // Debounce delay in ms
bool buttonStates[4] = { false };                                             // Last stable state of each button
bool lastReadings[4] = { false };                                             // Last raw reading of each button
unsigned long lastDebounceTimes[4] = { 0 };                                   // Last debounce timestamp per button

/***************************************************************************************
   Interrupt Helpers
****************************************************************************************/
void setInterrupt() {
  digitalWrite(INT_OUT, LOW);  // Notify host: new data available
}

void clearInterrupt() {
  digitalWrite(INT_OUT, HIGH);  // Reset interrupt
}

/***************************************************************************************
  Motor Control: Moves motor to target position using PWM and direction pins
****************************************************************************************/
void goToPosition(int16_t cmd_position) {
  cmd_position = constrain(cmd_position, MIN_THROTTLE, MAX_THROTTLE);
  unsigned long startTime = millis();
  const uint8_t pwmSpeed = 230;      // Fixed motor speed
  const int16_t motor_deadzone = 6;  // Tolerance in analog units

  int16_t current_position = analogRead(WIPER);
  while (abs(current_position - cmd_position) > motor_deadzone) {
    if (millis() - startTime > 2000) break;  // Safety timeout
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

  // Stop motor
  analogWrite(SPEED, 0);
  digitalWrite(MOTOR_REV, LOW);
  digitalWrite(MOTOR_FWD, LOW);
}

/***************************************************************************************
  Button Handler: Reads all 4 buttons with debounce
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
          result |= (1 << i);  // Mark button i as newly pressed
        }
      }
    }
  }
  return result;
}

/***************************************************************************************
  Bulb Test Routine: Lights all LEDs, sweeps motor from MIN to MAX and back
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
  LED Control Helper
****************************************************************************************/
void setAllLEDs(bool state) {
  digitalWrite(LED_100, state);
  digitalWrite(LED_UP, state);
  digitalWrite(LED_DOWN, state);
  digitalWrite(LED_00, state);
}

/***************************************************************************************
  I2C Handlers

  I2C Protocol:
  - Master sends 1 byte (hostCommand) with bit 0 = throttle enable flag
  - Slave replies with 2 bytes (little endian) representing a 16-bit throttle value:
      host_throttle_position = map(raw, 0–1023) → 0–INT16_MAX or 0–(INT16_MAX/10)
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
  Arduino Setup: Initializes pins, performs bulb test, starts I2C
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

  setAllLEDs(LOW);
  bulbTest();
  goToPosition(MIN_THROTTLE);

  Wire.begin(SDA, SCL, panel_addr);
  Wire.onReceive(handleReceiveEvent);
  Wire.onRequest(handleRequestEvent);
}

/***************************************************************************************
  Arduino Loop: Handles button input, fine mode behavior, and motor control
****************************************************************************************/
void loop() {
  int16_t commandedPosition;
  bool fineModeEnabled = digitalRead(FINE_MODE);

  // Handle fine mode transitions
  if (fineModeEnabled && !lastFineMode) {
    goToPosition(MIN_THROTTLE);  // Entering fine mode: move motor to 0%
  } else if (!fineModeEnabled && lastFineMode) {
    // Exiting fine mode: remap fine-scale pos back to full scale
    prevPosition = constrain(prevPosition, MIN_THROTTLE, MAX_THROTTLE / FINE_MODE_FACTOR);
    goToPosition(map(prevPosition, MIN_THROTTLE, MAX_THROTTLE / FINE_MODE_FACTOR, MIN_THROTTLE, MAX_THROTTLE));
  }

  uint8_t pressed = readButtonInputs();

  if (throttleEnabled) {
    setAllLEDs(HIGH);

    // Handle buttons
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

    // Move to commanded position if user is touching the throttle
    if (digitalRead(TOUCH) == LOW) {
      goToPosition(commandedPosition);
    }

    // Read and report new throttle position to host
    int16_t throttle_position = analogRead(WIPER);
    if (throttle_position != prevPosition) {
      if (fineModeEnabled) {
        host_throttle_position = map(throttle_position, MIN_THROTTLE, MAX_THROTTLE,
                                     0, INT16_MAX / FINE_MODE_FACTOR);
      } else {
        host_throttle_position = map(throttle_position, MIN_THROTTLE, MAX_THROTTLE,
                                     0, INT16_MAX);
      }
      setInterrupt();  // Signal host of new value
    }

    prevPosition = throttle_position;
    lastFineMode = fineModeEnabled;
  } else {
    setAllLEDs(LOW);
  }
}

/********************************************************************************************************************************
  GPWS Display Module for Kerbal Controller

  Handles input from rotary encoders, 3 RGB LED buttons, outputs on 4-digit, 7-segment display,
   and I2C communication with the host.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>               // I2C communication
#include <tinyNeoPixel.h>       // NeoPixel support for ATtiny series
#include "RotaryEncoderCore.h"  // Rotary encoder core library
#include <LedControl.h>         // Library for MAX7219 control of 7-segment LED
#include <math.h>               // Arduino math library

/***************************************************************************************
  Pin Definitions (ATtiny816 w/ megaTinyCore Pin Mapping)
****************************************************************************************/
#define SDA PIN_PB1  // I2C data pin
#define SCL PIN_PB0  // I2C clock pin

#define INT_OUT PIN_PA4  // Interrupt output to notify host of state change

#define dataIn PIN_PA5    // MAX7219 Data In Pin
#define loadData PIN_PA6  // MAX7219 Load Data Pin
#define clockIn PIN_PB7   // MAX7219 Clock Pin

#define neopixCmd PIN_PC0  // NeoPixel data output pin
#define BTN1 PIN_PC1       // Button 1 input pin (active high)
#define BTN2 PIN_PC2       // Button 2 input pin (active high)
#define BTN3 PIN_PC3       // Button 3 input pin (active high)

constexpr uint8_t enc_A = PIN_PA3;
constexpr uint8_t enc_B = PIN_PA1;
constexpr uint8_t enc_BTN = PIN_PA2;

/***************************************************************************************
  Constants and Globals
****************************************************************************************/
constexpr uint8_t panel_addr = 0x21;
constexpr uint8_t NUM_LEDS = 3;
constexpr uint8_t NUM_BUTTONS = 3;
constexpr uint16_t default_value = 1000;
constexpr uint8_t SEGMENT_BRIGHTNESS = 8;

volatile bool updateLED;
uint8_t button_event_bits = 0;
uint8_t encoder_event_bits = 0;
uint8_t led_bits = 0;
uint16_t response_value = default_value;
uint16_t last_response_value = default_value;

uint8_t digitPosition = 0;
bool digitModeActive = false;
unsigned long lastDigitInteraction = 0;

bool displayEnabled = true;
bool displayPreviouslyEnabled = true;

const uint8_t buttonPins[NUM_BUTTONS] = { BTN1, BTN2, BTN3 };
const unsigned long debounceDelay = 20;
const unsigned long shortPressThreshold = 50;
const unsigned long longPressThreshold = 800;
const unsigned long digitModeTimeout = 2000;
const unsigned long digitModeManualExit = 1000;
const unsigned long digitModeReset = 3000;

unsigned long lastDebounceTime[NUM_BUTTONS] = { 0 };
bool buttonState[NUM_BUTTONS] = { false };
bool lastButtonReading[NUM_BUTTONS] = { false };
unsigned long pressStartTime[NUM_BUTTONS] = { 0 };
bool pressHandled[NUM_BUTTONS] = { true };

/***************************************************************************************
  Structs and Enums
****************************************************************************************/
struct RGB {
  uint8_t r, g, b;
};

enum LEDMode : uint8_t {
  LED_OFF = 0,
  LED_DIM = 1,
  LED_GREEN = 2,
  LED_AMBER = 3
};

/***************************************************************************************
  RGB Color Palette
****************************************************************************************/
const RGB ledColorTable[] = {
  { 0, 0, 0 },     // LED_OFF
  { 32, 32, 32 },  // LED_DIM
  { 0, 128, 0 },   // LED_GREEN
  { 128, 96, 0 }   // LED_AMBER
};
const RGB RED = { 128, 0, 0 };
const RGB BLUE = { 0, 0, 128 };
const RGB WHITE = { 128, 128, 128 };

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
RotaryEncoder encoder;

/***************************************************************************************
  7-Segment Display Objects
****************************************************************************************/
LedControl segDisplay = LedControl(dataIn, clockIn, loadData, 1);
uint16_t lastDisplayedValue = 0;
int8_t lastFlashPos = -1;

void displayValue(uint16_t value, int8_t flashPos = -1) {
  value = constrain(value, 0, 9999);  // Ensure safe range

  if (!displayEnabled) {
    for (int i = 0; i < 4; i++) segDisplay.setChar(0, i, ' ', false);
    return;
  }

  if (value == lastDisplayedValue && flashPos == lastFlashPos) return;
  lastDisplayedValue = value;
  lastFlashPos = flashPos;

  bool leadingZero = true;
  for (int i = 0; i < 4; i++) {
    uint8_t digit = (value / (int)pow(10, 3 - i)) % 10;
    if (digit != 0 || i == 3) leadingZero = false;

    if (flashPos == i) {
      segDisplay.setChar(0, i, '-', false);
    } else if (leadingZero) {
      segDisplay.setChar(0, i, ' ', false);
    } else {
      segDisplay.setDigit(0, i, digit, false);
    }
  }
}

void displayText(const char* text) {
  for (int i = 0; i < 4; i++) {
    segDisplay.setChar(0, i, text[i], false);
  }
}

/***************************************************************************************
  RGB LED Setup
****************************************************************************************/
RGB currentColors[NUM_LEDS] = {};
tinyNeoPixel leds = tinyNeoPixel(NUM_LEDS, neopixCmd, NEO_GRB);

void runBulbTest() {
  const RGB sequence[4][4] = {
    { RED, GREEN, BLUE, WHITE },
    { GREEN, BLUE, WHITE, RED },
    { BLUE, WHITE, RED, GREEN }
  };
  for (uint8_t step = 0; step < 4; step++) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      RGB color = sequence[i][step];
      leds.setPixelColor(i, color.r, color.g, color.b);
    }
    leds.show();
    delay(300);
  }

  displayText("8888");
  delay(500);
  displayText("DEAD");
  delay(500);
  displayText("BEEF");
  delay(500);
  displayText("0000");
  delay(500);
  displayValue(response_value);

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds.setPixelColor(i, 0, 0, 0);
  }
  leds.show();
}

/***************************************************************************************
  RGB LED Update Logic
****************************************************************************************/
void updateRGBLeds(uint8_t bits) {
  bool updated = false;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t mode = (bits >> (i * 2)) & 0x03;
    RGB newColor = ledColorTable[mode];

    if (newColor.r != currentColors[i].r || newColor.g != currentColors[i].g || newColor.b != currentColors[i].b) {
      leds.setPixelColor(i, newColor.r, newColor.g, newColor.b);
      currentColors[i] = newColor;
      updated = true;
    }
  }

  if (updated) {
    leds.show();
  }
}

/***************************************************************************************
  Button Input with Debounce, Short/Long Press Detection
****************************************************************************************/
uint8_t checkButtons() {
  uint8_t result = 0;

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    bool reading = digitalRead(buttonPins[i]);

    if (reading != lastButtonReading[i]) {
      lastDebounceTime[i] = millis();
      lastButtonReading[i] = reading;
    }

    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (reading && !buttonState[i]) {
        buttonState[i] = true;
        pressStartTime[i] = millis();
        pressHandled[i] = false;
      }

      if (!reading && buttonState[i]) {
        buttonState[i] = false;
        unsigned long pressDuration = millis() - pressStartTime[i];

        if (!pressHandled[i]) {
          uint8_t event = 0;
          if (pressDuration >= longPressThreshold) {
            event = 0b10;
          } else if (pressDuration >= shortPressThreshold) {
            event = 0b01;
          }

          result |= (event << (i * 2));
          pressHandled[i] = true;
        }
      }
    }
  }

  return result;
}

/***************************************************************************************
  Check Buttons with Debounce and Press Event Detection

  Returns a bitfield:
  - 2 bits per button:
    00 = no event
    01 = short press
    10 = long press
    11 = reserved
****************************************************************************************/
uint8_t checkButtons() {
  uint8_t result = 0;

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    bool reading = digitalRead(buttonPins[i]);

    // Debounce logic
    if (reading != lastButtonReading[i]) {
      lastDebounceTime[i] = millis();
      lastButtonReading[i] = reading;
    }

    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      // Button pressed
      if (reading && !buttonState[i]) {
        buttonState[i] = true;
        pressStartTime[i] = millis();
        pressHandled[i] = false;
      }

      // Button released
      if (!reading && buttonState[i]) {
        buttonState[i] = false;
        unsigned long pressDuration = millis() - pressStartTime[i];

        if (!pressHandled[i]) {
          uint8_t event = 0;
          if (pressDuration >= longPressThreshold) {
            event = 0b10;
          } else if (pressDuration >= shortPressThreshold) {
            event = 0b01;
          }

          // Pack 2 bits per button
          result |= (event << (i * 2));
          pressHandled[i] = true;
        }
      }
    }
  }

  return result;
}

/***************************************************************************************
  I2C Request Handler

  Master Read (4 bytes):
  [0-1] 16-bit response_value
  [2]   button_event_bits
  [3]   reserved (0)
****************************************************************************************/
void handleRequestEvent() {
  uint8_t data[4];
  data[0] = response_value >> 8;
  data[1] = response_value & 0xFF;
  data[2] = button_event_bits;
  data[3] = 0;
  Wire.write(data, 4);
  button_event_bits = 0;  // Clear after report
  clearInterrupt();
}

/***************************************************************************************
  I2C Receive Handler

  Master Write (1 byte):
  [7]   Display enable flag (1 = ON, 0 = OFF)
  [5:0] LED mode control bits (2 bits per LED for up to 3 RGB LEDs)
****************************************************************************************/
void handleReceiveEvent(int howMany) {
  if (Wire.available()) {
    led_bits = Wire.read();
    updateLED = true;
    bool newDisplayEnabled = (led_bits & 0x80) != 0;
    if (newDisplayEnabled && !displayPreviouslyEnabled) {
      response_value = default_value;
    }
    displayEnabled = newDisplayEnabled;
    displayPreviouslyEnabled = displayEnabled;
  }
}

/***************************************************************************************
  Setup
****************************************************************************************/
void setup() {

  pinMode(INT_OUT, OUTPUT);  // Configure interrupt pin
  clearInterrupt();          // Set interrupt to high for default state

  pinMode(BTN1, INPUT);  // Configure pins for button inputs
  pinMode(BTN2, INPUT);  // Configure pins for button inputs
  pinMode(BTN3, INPUT);  // Configure pins for button inputs

  leds.begin();  // Begin RGB LED objects
  leds.clear();  // Clear RGB LEDs
  leds.show();   // output value to RGB LED

  segDisplay.shutdown(0, false);                   // Configure 7-Segment Display with MAX7219
  segDisplay.setIntensity(0, SEGMENT_BRIGHTNESS);  // Configure 7-Segment Display brightness
  segDisplay.clearDisplay(0);                      // Clear 7-Segment Display

  attachEncoder(encoder, enc_A, enc_B, enc_BTN);  // configure encoder

  Wire.begin(SDA, SCL, panel_addr);    // Initiate i2c
  Wire.onRequest(handleRequestEvent);  // Registed Request event
  Wire.onReceive(handleReceiveEvent);  // Registed Reeceive event

  runBulbTest();  // Run LED bulb test
}

/***************************************************************************************
  Loop
****************************************************************************************/
void loop() {
  // Perform LED Update if new commands received from host
  if (updateLED) {
    updateRGBLeds(led_bits);
    updateLED = false;
  }

  // Check the RGB LED buttons
  uint8_t new_button_bits = checkButtons();
  if (new_button_bits != button_event_bits) {
    button_event_bits = new_button_bits;
    setInterrupt();
  }

  // Update based on the encoder rotation
  updateEncoder(encoder);
  // Update based on the encoder button press
  updateEncoderButton(encoder);
  if (encoder.position != 0) {
    handleEncoderRotation();
    encoder.position = 0;
    lastDigitInteraction = millis();
    setInterrupt();
  }

  // Check whether the display is in digit mode and ensure it times-out
  if (digitModeActive && (millis() - lastDigitInteraction > digitModeTimeout)) {
    digitModeActive = false;
    digitPosition = 0;
  }

  displayValue(response_value, digitModeActive ? digitPosition : -1);
}

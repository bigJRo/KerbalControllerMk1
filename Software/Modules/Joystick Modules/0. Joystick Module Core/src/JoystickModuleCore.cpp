/********************************************************************************************************************************
  Joystick Module Core for Kerbal Controller

  Handles common functions for all Joystick Modules for use in Kerbal Controller Mk1
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include "JoystickModuleCore.h"

/***************************************************************************************
  Lookup table of RGB color definitions for LED feedback
  Stored in PROGMEM to save precious SRAM on the ATtiny816
  Each entry is a custom-named color used to indicate status of individual functions
  Indexes are mapped via the ColorIndex enum for readability
****************************************************************************************/
const buttonPixel colorTable[] PROGMEM = {
  { 128, 0, 0 },      // RED: General warning or critical function
  { 128, 96, 0 },     // AMBER: Secondary status or transitional state
  { 128, 50, 0 },     // ORANGE: UI prompt or toggle
  { 128, 108, 0 },    // GOLDEN_YELLOW: Status indicator (e.g., deployed)
  { 64, 128, 160 },   // SKY_BLUE: Represents atmosphere or power
  { 0, 128, 0 },      // GREEN: Safe or confirmed
  { 64, 128, 96 },    // MINT: Passive/idle
  { 128, 0, 128 },    // MAGENTA: Communication
  { 0, 128, 128 },    // CYAN: Cooling or environmental
  { 64, 128, 0 },     // LIME: Active
  { 255, 255, 255 },  // WHITE: All-clear or generic
  { 0, 0, 0 },        // BLACK: Off or disabled
  { 32, 32, 32 },     // DIM_GRAY: Neutral, inactive but valid
  { 0, 0, 255 },      // BLUE: Default function
  { 255, 0, 0 },      // BRIGHT_RED: Emergency
  { 0, 255, 0 },      // BRIGHT_GREEN: Enabled
  { 0, 0, 255 },      // BRIGHT_BLUE: Attention
  { 255, 255, 0 },    // YELLOW: Alert
  { 0, 255, 255 },    // AQUA: Coolant/temperature
  { 255, 0, 255 }     // FUCHSIA: Comms or science
};

/***************************************************************************************
  Global variable setup
****************************************************************************************/
volatile bool updateLED = false;
uint8_t led_bits = 0;
uint8_t prev_led_bits = 0;
uint8_t buttonBits = 0;
int16_t axis1 = 0, axis2 = 0, axis3 = 0;

static unsigned long lastDebounceTime[NUM_BUTTONS] = { 0 };
static uint8_t buttonStates[NUM_BUTTONS] = { 0 };
static uint8_t lastButtonReadings[NUM_BUTTONS] = { 0 };

static const buttonPixel* colorTablePtr = nullptr;

/***************************************************************************************
  Setup for libary objects
****************************************************************************************/
tinyNeoPixel leds = tinyNeoPixel(NUM_LEDS, neopixCmd, NEO_GRB);  // Neopixel LED object

/***************************************************************************************
  Module configuration helper function
****************************************************************************************/
void beginModule(uint8_t address) {
  pinMode(INT_OUT, OUTPUT);
  clearInterrupt();

  // Button and analog inputs
  pinMode(BUTTON01, INPUT);
  pinMode(BUTTON02, INPUT);
  pinMode(BUTTON_JOY, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  
  // Initialize NeoPixels
  leds.begin();
  delay(10);
  
  bulbTest();
  
  // Set up I2C and define handlers
  Wire.begin(address);
  Wire.onReceive(handleReceiveEvent);
  Wire.onRequest(handleRequestEvent);
}

/***************************************************************************************
  Function to read button state using shift register if update is detected
****************************************************************************************/
void readJoystickInputs(uint8_t buttonPins[NUM_BUTTONS]) {
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT);
  }

  // Read and debounce all 3 button inputs
  uint8_t rawButtons[NUM_BUTTONS] = {
    digitalRead(BUTTON01),
    digitalRead(BUTTON02),
    digitalRead(BUTTON_JOY)
  };

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    if (rawButtons[i] != lastButtonReadings[i]) {
      lastDebounceTime[i] = millis();
      lastButtonReadings[i] = rawButtons[i];
    }
    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (rawButtons[i] != buttonStates[i]) {
        buttonStates[i] = rawButtons[i];
      }
    }
  }

  buttonBits = 0;
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    if (buttonStates[i]) buttonBits |= (1 << i);
  }

  // Read raw analog values from all 3 joystick axes
  int16_t raw1 = analogRead(A1);
  int16_t raw2 = analogRead(A2);
  int16_t raw3 = analogRead(A3);

  if (abs(raw1) > JOY_DEADZONE) {
    axis1 = map(raw1, 0, 1023, INT16_MIN, INT16_MAX);
    setInterrupt();  //  Trigger interrupt to inform master of change
  } else {
    axis1 = 0;
  }
  if (abs(raw2) > JOY_DEADZONE) {
    axis2 = map(raw2, 0, 1023, INT16_MIN, INT16_MAX);
    setInterrupt();  //  Trigger interrupt to inform master of change
  } else {
    axis2 = 0;
  }
  if (abs(raw3) > JOY_DEADZONE) {
    axis1 = map(raw3, 0, 1023, INT16_MIN, INT16_MAX);
    setInterrupt();  //  Trigger interrupt to inform master of change
  } else {
    axis3 = 0;
  }
}

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
  Helper to fetch RGB color struct from PROGMEM
****************************************************************************************/
buttonPixel getColorFromTable(ColorIndex index) {
  buttonPixel px;
  const uint8_t* ptr = (const uint8_t*)colorTable + index * sizeof(buttonPixel);
  px.r = pgm_read_byte(ptr);
  px.g = pgm_read_byte(ptr + 1);
  px.b = pgm_read_byte(ptr + 2);
  return px;
}

/***************************************************************************************
  Overlay Color Helper: handles logic for LEDs 8–11
****************************************************************************************/
buttonPixel overlayColor(bool overlayEnabled, bool modeActive, bool localActive, uint8_t colorIndex) {
  if (!overlayEnabled || !modeActive) return getColorFromTable(BLACK);
  if (localActive) return getColorFromTable(static_cast<ColorIndex>(colorIndex));
  return getColorFromTable(DIM_GRAY);
}

/***************************************************************************************
  Bulb Test Function
  - Cycles through RED, GREEN, BLUE, WHITE on all pixels
  - Then displays a specific pattern on pixel_Array
  - Sets odd discrete outputs HIGH and even LOW during the pattern
  - Holds the pattern for 2 seconds before resetting
****************************************************************************************/
void bulbTest() {
  const ColorIndex sequence[] = { RED, GREEN, BLUE, WHITE };

  // Cycle all NeoPixels through the base color sequence
  for (ColorIndex color : sequence) {
    buttonPixel px = getColorFromTable(color);
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      leds.setPixelColor(i, px.r, px.g, px.b);
    }

    leds.show();
    delay(150);

    // Clear everything
  for (uint8_t i = 0; i < NUM_LEDS; i++) leds.setPixelColor(i, 0, 0, 0);
  leds.show();
  }

/***************************************************************************************
  I2C Event Handlers

  ATtiny816 acts as an I2C SLAVE device at address 0x23.
  The protocol between master and slave uses 4 bytes:

  - Master reads 4 bytes:
    [0] = button state bits 0–7
    [1] = button state bits 8–15
    [2] = LED control bits LSB (LED0–7)
    [3] = LED control bits MSB (LED8–15)

  - Master writes 2 bytes:
    [0] = LED control bits LSB
    [1] = LED control bits MSB

  The LED bits control color changes or status indication depending on the bit state.

****************************************************************************************/
void handleRequestEvent() {
  // Respond to master read request with 7-byte status report
uint8_t response[7] = {
      buttonBits,
      (uint8_t)(axis1 >> 8), (uint8_t)(axis1 & 0xFF),
      (uint8_t)(axis2 >> 8), (uint8_t)(axis2 & 0xFF),
      (uint8_t)(axis3 >> 8), (uint8_t)(axis3 & 0xFF)
    };
    Wire.write(response, sizeof(response));
  
    clearInterrupt();                        // Clear interrupt since the master responded
}

void handleReceiveEvent(int16_t howMany) {
  // Master has sent LED state change request
  if (Wire.available() >= 1) {
      led_bits = Wire.read();
      updateLED = true;  // Set flag to process LED changes in main loop
  }
}

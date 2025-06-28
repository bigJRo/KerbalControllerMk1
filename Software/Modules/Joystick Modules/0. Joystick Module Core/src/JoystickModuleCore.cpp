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

/***************************************************************************************
  Setup for library objects
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

/****************************************************************************************
  Read debounced button states and scaled joystick analog input
****************************************************************************************/
void readJoystickInputs(uint8_t buttonPins[NUM_BUTTONS]) {

  constexpr int16_t JOY_CENTER = 512;
  
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
        setInterrupt();
      }
    }
  }

  buttonBits = 0;
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    if (buttonStates[i]) buttonBits |= (1 << i);
  }

  int16_t raw1 = analogRead(A1);
  int16_t raw2 = analogRead(A2);
  int16_t raw3 = analogRead(A3);

  axis1 = abs(raw1 - JOY_CENTER) > JOY_DEADZONE ? map(raw1, 0, 1023, INT16_MIN, INT16_MAX) : 0;  // Scale 10-bit analog input [-512, +511] to full 16-bit signed range
  axis2 = abs(raw2 - JOY_CENTER) > JOY_DEADZONE ? map(raw2, 0, 1023, INT16_MIN, INT16_MAX) : 0;  // Scale 10-bit analog input [-512, +511] to full 16-bit signed range
  axis3 = abs(raw3 - JOY_CENTER) > JOY_DEADZONE ? map(raw3, 0, 1023, INT16_MIN, INT16_MAX) : 0;  // Scale 10-bit analog input [-512, +511] to full 16-bit signed range

  if (axis1 != 0 || axis2 != 0 || axis3 != 0) {
    setInterrupt();
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
  delay(1000);
}

/***************************************************************************************
  I2C Event Handlers

  ATtiny816 acts as an I2C SLAVE device.
  The protocol between master and slave uses 7 bytes:

  - Master reads 7 bytes:
    [0] = button state bits 0–7
    [1–2] = axis1 (int16_t, big-endian)
    [3–4] = axis2 (int16_t, big-endian)
    [5–6] = axis3 (int16_t, big-endian)

  - Master writes 1 byte:
    [0] = LED control bits (bitmap for module-specific purposes)

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

  clearInterrupt();  // Clear interrupt since the master responded
}

void handleReceiveEvent(int howMany) {
  // Master has sent LED state change request
  if (Wire.available() >= 1) {
    led_bits = Wire.read();
    updateLED = true;  // Set flag to process LED changes in main loop
  }
}

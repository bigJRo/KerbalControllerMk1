/********************************************************************************************************************************
  Translation Control Module for Kerbal Controller Mk1

  Based on rotation.ino. Uses JoystickModuleCore library for core input, LED, and I2C functionality.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <JoystickModuleCore.h>  // Core library for joystick-based modules

/****************************************************************************************
  Configuration and Lookup Tables
****************************************************************************************/
constexpr uint8_t panel_addr = 0x27;  // I2C address for this module

// Map each pixel to a color index from the shared colorTable
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  GREEN,    // LED 0: Reset Camera
  MAGENTA   // LED 1: Camera Mode
};

// Human-readable labels for each input button (for documentation/debug)
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "Reset Camera",  // BUTTON01
  "Camera Mode",   // BUTTON02
  "Cycle Camera"   // Joystick button
};

/****************************************************************************************
  LED Update Handler
****************************************************************************************/
void handle_ledUpdate() {
  bool updated = false;
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    bool newState = bitRead(led_bits, i);
    bool oldState = bitRead(prev_led_bits, i);
    if (newState != oldState) {
      buttonPixel px = newState ? getColorFromTable(pixel_Array[i]) : getColorFromTable(DIM_GRAY);
      leds.setPixelColor(i, px.r, px.g, px.b);
      updated = true;
    }
  }
  if (updated) leds.show();
  prev_led_bits = led_bits;
}

/****************************************************************************************
  Arduino Setup and Loop
****************************************************************************************/
void setup() {
  beginModule(panel_addr);  // Initializes I2C, input pins, NeoPixels, interrupt
}

void loop() {
  if (updateLED) {
    handle_ledUpdate();
    updateLED = false;
  }

  const uint8_t buttonPins[NUM_BUTTONS] = { BUTTON01, BUTTON02, BUTTON_JOY };
  readJoystickInputs((uint8_t*)buttonPins);  // Core input + joystick logic
}

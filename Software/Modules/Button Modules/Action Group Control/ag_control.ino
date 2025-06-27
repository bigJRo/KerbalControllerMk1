/********************************************************************************************************************************
  Action Group Control Module for Kerbal Controller

  Adapted from the Vehicle Control Module by J. Rostoker for Jeb's Controller Works.
  Uses shared ButtonModuleCore for common logic and I2C handling.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <ButtonModuleCore.h>

/***************************************************************************************
  Module-Specific Constants
****************************************************************************************/
constexpr uint8_t panel_addr = 0x29;  // I2C address for this module

/***************************************************************************************
  LED Color Mapping (Index-based from ColorIndex enum)
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  GREEN,  //  0: AG1
  GREEN,  //  1: AG2
  GREEN,  //  2: AG3
  GREEN,  //  3: AG4
  GREEN,  //  4: AG5
  GREEN,  //  5: AG6
  GREEN,  //  6: AG7
  GREEN,  //  7: AG8
  GREEN,  //  8: AG9
  GREEN,  //  9: AG10
  GREEN,  // 10: AG11
  GREEN   // 11: AG12
};

/***************************************************************************************
  Button Command Names (for mapping and reference)
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "AG1",   //  0: Action Group 1
  "AG2",   //  1: Action Group 2
  "AG3",   //  2: Action Group 3
  "AG4",   //  3: Action Group 4
  "AG5",   //  4: Action Group 5
  "AG6",   //  5: Action Group 6
  "AG7",   //  6: Action Group 7
  "AG8",   //  7: Action Group 8
  "AG9",   //  8: Action Group 9
  "AG10",  //  9: Action Group 10
  "AG11",  // 10: Action Group 11
  "AG12",  // 11: Action Group 12
  "",      // 12 → unused
  "",      // 13 → unused
  "",      // 14 → unused
  ""       // 15 → unused
};

/***************************************************************************************
  Module LED Update Logic
****************************************************************************************/
void handle_ledUpdate() {
  uint16_t bits = led_bits;
  uint16_t prevBits = prev_led_bits;
  bool updated = false;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    bool newState = bitRead(bits, i);
    bool oldState = bitRead(prevBits, i);
    if (newState == oldState) continue;  // Skip unchanged states

    // Assign defined color if LED bit is set, or dim gray if not
    buttonPixel px = newState ? getColorFromTable(pixel_Array[i]) : getColorFromTable(DIM_GRAY);
    leds.setPixelColor(i, px.r, px.g, px.b);
    updated = true;
  }

  if (updated) leds.show();
  prev_led_bits = bits;
}

/***************************************************************************************
  Setup
****************************************************************************************/
beginModule(panel_addr);  // Calls bulbTest internally
}

/***************************************************************************************
  Main Loop
****************************************************************************************/
void loop() {
  if (updateLED) {
    handle_ledUpdate();
    updateLED = false;
  }

  readButtonStates();
}

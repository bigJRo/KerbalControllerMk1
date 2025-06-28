/********************************************************************************************************************************
  Time Control Module for Kerbal Controller

  Adapted from the Vehicle Control Module by J. Rostoker for Jeb's Controller Works.
  Handles time-related commands, LED feedback, and I2C communication with the host.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <ButtonModuleCore.h>

/***************************************************************************************
  Module-Specific Constants
****************************************************************************************/
constexpr uint8_t panel_addr = 0x24;  // I2C slave address for TimeControl module

/***************************************************************************************
  LED Color Mapping (Index-based from ColorIndex enum)
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  AMBER,          // 0: Pause
  GOLDEN_YELLOW,  // 1: Warp to Morn
  GOLDEN_YELLOW,  // 2: Warp to SOI
  GOLDEN_YELLOW,  // 3: Warp to MNVR
  GOLDEN_YELLOW,  // 4: Warp to PeA
  GOLDEN_YELLOW,  // 5: Warp to ApA
  SKY_BLUE,       // 6: Save
  SKY_BLUE,       // 7: Load
  GOLDEN_YELLOW,  // 8: Warp
  GOLDEN_YELLOW,  // 9: Warp +
  GOLDEN_YELLOW,  //10: Physics Warp
  RED             //11: Cancel Warp
};

/***************************************************************************************
  Button Command Names (for mapping and reference)
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "Pause",           // 0
  "Warp to Morn",    // 1
  "Warp to SOI",     // 2
  "Warp to MNVR",    // 3
  "Warp to PeA",     // 4
  "Warp to ApA",     // 5
  "Save",            // 6
  "Load",            // 7
  "Warp",            // 8
  "Warp +",          // 9
  "Physics Warp",    //10
  "Cancel Warp",     //11
  "",                //12 → unused/reserved
  "",                //13 → unused/reserved
  "",                //14 → unused/reserved
  ""                 //15 → unused/reserved
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
    if (newState == oldState) continue;

    buttonPixel px;
    if (newState) {
      px = getColorFromTable(pixel_Array[i]);
    } else {
      px = (i == 11) ? getColorFromTable(BLACK) : getColorFromTable(DIM_GRAY);
    }

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

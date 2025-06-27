/********************************************************************************************************************************
  Function Control Module for Kerbal Controller Mk1

  Adapted from the Vehicle Control Module by J. Rostoker for Jeb's Controller Works.
  Uses shared ButtonModuleCore for common logic and I2C handling.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <ButtonModuleCore.h>

/***************************************************************************************
  Module-Specific Constants
****************************************************************************************/
constexpr uint8_t panel_addr = 0x28;  // I2C slave address for Function Control module

/***************************************************************************************
  LED Color Mapping (Index-based from ColorIndex enum)
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  SKY_BLUE,     // 0: Air Intake
  RED,          // 1: Launch Escape
  BLUE,         // 2: Sci Collect
  AMBER,        // 3: Engine Alt
  CYAN,         // 4: Science Grp 1
  GREEN,        // 5: Engine Grp 1
  CYAN,         // 6: Science Grp 2
  GREEN,        // 7: Engine Grp 2
  LIME,         // 8: Ctrl Pt Alt
  LIME,         // 9: Ctrl Pt Pri
  AMBER,        //10: Lock Ctrl
  MINT          //11: Heat Shield
};

/***************************************************************************************
  Button Command Names (for mapping and reference)
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "Air Intake",     // 0
  "Launch Escape",  // 1
  "Sci Collect",    // 2
  "Engine Alt",     // 3
  "Science Grp 1",  // 4
  "Engine Grp 1",   // 5
  "Science Grp 2",  // 6
  "Engine Grp 2",   // 7
  "Ctrl Pt Alt",    // 8
  "Ctrl Pt Pri",    // 9
  "Lock Ctrl",      //10
  "Heat Shield",    //11
  "Throttle Lock",  //12 → LED13 (discrete output)
  "Precision",      //13 → LED14 (discrete output)
  "SCE",            //14 → LED15 (discrete output)
  "Audio"           //15 → LED16 (discrete output)
};

/***************************************************************************************
  Module LED Update Logic
****************************************************************************************/
void handle_ledUpdate() {
  uint16_t bits = led_bits;
  uint16_t prevBits = prev_led_bits;
  bool updated = false;

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    bool newState = bitRead(bits, i);
    bool oldState = bitRead(prevBits, i);
    if (newState == oldState) continue;  // No change

    if (i < NUM_LEDS) {
      // RGB LED update
      buttonPixel px = newState ? getColorFromTable(pixel_Array[i]) : getColorFromTable(DIM_GRAY);
      leds.setPixelColor(i, px.r, px.g, px.b);
      updated = true;

    } else {
      // Discrete LED output (buttons 12–15)
      digitalWrite(discreteLEDs[i - NUM_LEDS], newState ? HIGH : LOW);
    }
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

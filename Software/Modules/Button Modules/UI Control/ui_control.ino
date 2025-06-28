/********************************************************************************************************************************
  UI Control Module for Kerbal Controller

  Adapted from the Vehicle Control Module by J. Rostoker for Jeb's Controller Works.
  Handles UI-related commands, LED status feedback, and I2C communication with the host.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <ButtonModuleCore.h>

/***************************************************************************************
  Module-Specific Constants
****************************************************************************************/
constexpr uint8_t panel_addr = 0x20;  // I2C slave address for UIControl

/***************************************************************************************
  LED Color Mapping (Index-based from ColorIndex enum)
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  AMBER,     // 0: Map Enable
  GREEN,     // 1: Cycle Map -
  GREEN,     // 2: Cycle Map +
  BLUE,      // 3: Navball Mode
  AMBER,     // 4: IVA
  SKY_BLUE,  // 5: Cycle Cam
  GREEN,     // 6: Cycle Ship -
  GREEN,     // 7: Cycle Ship +
  GREEN,     // 8: Reset Focus
  MAGENTA,   // 9: Screen Shot
  AMBER,     //10: UI
  RED        //11: DEBUG
};

/***************************************************************************************
  Button Command Names (for mapping and reference)
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "Map Enable",      // 0
  "Cycle Map -",     // 1
  "Cycle Map +",     // 2
  "Navball Mode",    // 3
  "IVA",             // 4
  "Cycle Cam",       // 5
  "Cycle Ship -",    // 6
  "Cycle Ship +",    // 7
  "Reset Focus",     // 8
  "Screen Shot",     // 9
  "UI",              //10
  "DEBUG",           //11
  "",                //12 → unused or reserved
  "",                //13 → unused or reserved
  "",                //14 → unused or reserved
  ""                 //15 → unused or reserved
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

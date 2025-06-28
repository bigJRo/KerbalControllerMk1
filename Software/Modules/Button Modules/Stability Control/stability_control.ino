/********************************************************************************************************************************
  Stability Control Module for Kerbal Controller Mk1

  Adapted from the Vehicle Control Module by J. Rostoker for Jeb's Controller Works.
  Handles stability-related SAS commands, RGB LED feedback, and I2C communication with the host.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <ButtonModuleCore.h>

/***************************************************************************************
  Module-Specific Constants
****************************************************************************************/
constexpr uint8_t panel_addr = 0x2A;  // I2C slave address for Stability Control Module

/***************************************************************************************
  LED Color Mapping (Index-based from ColorIndex enum)
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  GREEN,  // 0: SAS Maneuver
  GREEN,  // 1: SAS Hold
  GREEN,  // 2: SAS Retrograde
  GREEN,  // 3: SAS Prograde
  GREEN,  // 4: SAS Antinormal
  GREEN,  // 5: SAS Normal
  GREEN,  // 6: SAS Radial Out
  GREEN,  // 7: SAS Radial In
  GREEN,  // 8: SAS Anti-Target
  GREEN,  // 9: SAS Target
  GREEN,  //10: SAS Invert
  BLACK   //11: Unused
};

/***************************************************************************************
  Button Command Names (for mapping and reference)
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "SAS Maneuver",   // 0
  "SAS Hold",       // 1
  "SAS Retrograde", // 2
  "SAS Prograde",   // 3
  "SAS Antinormal", // 4
  "SAS Normal",     // 5
  "SAS Rad Out",    // 6
  "SAS Rad In",     // 7
  "Anti-Target",    // 8
  "Target",         // 9
  "SAS Invert",     //10
  "Unused",         //11
  "SAS Enable",     //12 → LED13 (discrete output)
  "RCS Enable",     //13 → LED14 (discrete output
  "Unused/Reserved",//14 → LED15 (discrete output)
  "Unused/Reserved" //15 → LED16 (discrete output)
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

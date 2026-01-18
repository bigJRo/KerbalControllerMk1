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
  GREEN,  // 0: SAS Target
  GREEN,  // 1: SAS Anti-Target
  GREEN,  // 2: SAS Radial Out
  GREEN,  // 3: SAS Radial In
  GREEN,  // 4: SAS Normal
  GREEN,  // 5: SAS Antinormal
  GREEN,  // 6: SAS Prograde
  GREEN,  // 7: SAS Retrograde
  GREEN,  // 8: SAS Hold
  GREEN,  // 9: SAS Maneuver
  BLACK,  //10: Unused
  GREEN   //11: SAS Invert
};

/***************************************************************************************
  Button Command Names (for mapping and reference)
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "Target",          // 0
  "Anti-Target",     // 1
  "SAS Rad Out",     // 2
  "SAS Rad In",      // 3
  "SAS Normal",      // 4
  "SAS Antinormal",  // 5
  "SAS Prograde",    // 6
  "SAS Retrograde",  // 7
  "SAS Hold",        // 8
  "SAS Maneuver",    // 9
  "Unused",          //10
  "SAS Invert",      //11
  "SAS Enable",      //12
  "Unused/Reserved", //13
  "RCS Enable",      //14
  "Unused/Reserved"  //15
};

/***************************************************************************************
  Module LED Update Logic (ButtonModuleCore v1.1 priority)
****************************************************************************************/
void handle_ledUpdate() {
  // Priority:
  //  RGB LEDs (0–11):
  //    1) led_bits == 1           -> assigned color
  //    2) else button_active_bits -> DIM_GRAY
  //    3) else                    -> OFF (BLACK)
  //
  //  Discrete outputs (12–15): binary only
  //    led_bits == 1 -> HIGH, else LOW

  const uint16_t ledState        = led_bits;
  const uint16_t activeState     = button_active_bits;
  const uint16_t prevLedState    = prev_led_bits;
  const uint16_t prevActiveState = prev_button_active_bits;

  bool rgbUpdated = false;

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    const bool ledNow    = bitRead(ledState, i);
    const bool activeNow = bitRead(activeState, i);
    const bool ledPrev   = bitRead(prevLedState, i);
    const bool actPrev   = bitRead(prevActiveState, i);

    if (i < NUM_LEDS) {
      // Mode: 0=OFF, 1=DIM_GRAY, 2=ASSIGNED_COLOR
      const uint8_t modeNow  = ledNow ? 2 : (activeNow ? 1 : 0);
      const uint8_t modePrev = ledPrev ? 2 : (actPrev   ? 1 : 0);
      if (modeNow == modePrev) continue;

      buttonPixel px;
      if (modeNow == 2) {
        px = getColorFromTable(pixel_Array[i]);
      } else if (modeNow == 1) {
        px = getColorFromTable(DIM_GRAY);
      } else {
        px = getColorFromTable(BLACK);
      }

      leds.setPixelColor(i, px.r, px.g, px.b);
      rgbUpdated = true;

    } else {
      // Discrete outputs: led_bits only
      if (ledNow == ledPrev) continue;
      digitalWrite(discreteLEDs[i - NUM_LEDS], ledNow ? HIGH : LOW);
    }
  }

  if (rgbUpdated) leds.show();

  prev_led_bits = ledState;
  prev_button_active_bits = activeState;
}

/***************************************************************************************
  Setup
****************************************************************************************/
void setup() {
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

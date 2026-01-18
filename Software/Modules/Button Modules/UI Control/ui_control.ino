/********************************************************************************************************************************
  UI Control Module for Kerbal Controller Mk1

  Adapted from the Vehicle Control Module by J. Rostoker for Jeb's Controller Works.
  Uses shared ButtonModuleCore for common logic and I2C handling.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <ButtonModuleCore.h>

/***************************************************************************************
  Module-Specific Constants
****************************************************************************************/
constexpr uint8_t panel_addr = 0x20;  // I2C slave address for UI Control module

/***************************************************************************************
  LED Color Mapping (Index-based from ColorIndex enum)
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  MAGENTA,   //  0: Screen Shot
  RED,       //  1: DEBUG
  ORANGE,    //  2: UI
  AMBER,     //  3: Navball Enable
  GREEN,     //  4: Reset Map
  BLUE,      //  5: Navball Mode
  GREEN,     //  6: Cycle Map +
  GREEN,     //  7: Cycle Ship +
  GREEN,     //  8: Cycle Map -
  GREEN,     //  9: Cycle Ship -
  AMBER,     // 10: Map Enable
  SKY_BLUE   // 11: IVA
};

/***************************************************************************************
  Button Command Names (for mapping and reference)
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "Screen Shot",     //  0
  "DEBUG",           //  1
  "UI",              //  2
  "Navball Enable",  //  3
  "Reset Map",       //  4
  "Navball Mode",    //  5
  "Cycle Map +",     //  6
  "Cycle Ship +",    //  7
  "Cycle Map -",     //  8
  "Cycle Ship -",    //  9
  "Map Enable",      // 10
  "IVA",             // 11
  "",                // 12 → unused or reserved
  "",                // 13 → unused or reserved
  "",                // 14 → unused or reserved
  ""                 // 15 → unused or reserved
};

/***************************************************************************************
  Module LED Update Logic
****************************************************************************************/
void handle_ledUpdate() {
  // ButtonModuleCore v1.1 LED priority (per LED):
  //   1) led_bits bit == 1  -> assigned color
  //   2) else if button_active_bits bit == 1 -> DIM_GRAY
  //   3) else -> OFF (BLACK)

  const uint16_t ledState = led_bits;
  const uint16_t activeState = button_active_bits;
  const uint16_t prevLedState = prev_led_bits;
  const uint16_t prevActiveState = prev_button_active_bits;

  bool updated = false;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    const bool ledNow = bitRead(ledState, i);
    const bool activeNow = bitRead(activeState, i);
    const bool ledPrev = bitRead(prevLedState, i);
    const bool activePrev = bitRead(prevActiveState, i);

    // Mode encoding: 0=OFF, 1=DIM_GRAY, 2=ASSIGNED_COLOR
    const uint8_t modeNow = ledNow ? 2 : (activeNow ? 1 : 0);
    const uint8_t modePrev = ledPrev ? 2 : (activePrev ? 1 : 0);
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
    updated = true;
  }

  if (updated) leds.show();
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

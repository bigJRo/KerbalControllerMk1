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
  "SPC_MODE",      // 14: Spacecraft Control Mode
  "RVR_MODE"       // 15: Rover Control Mode
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

    // Determine current and previous output "modes" so we only update changed LEDs.
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

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
  GOLDEN_YELLOW,  // 0: Warp to ApA
  RED,            // 1: Cancel Warp
  GOLDEN_YELLOW,  // 2: Warp to PeA
  GOLDEN_YELLOW,  // 3: Physics Warp
  GOLDEN_YELLOW,  // 4: Warp to MNVR
  GOLDEN_YELLOW,  // 5: Warp +
  GOLDEN_YELLOW,  // 6: Warp to SOI
  GOLDEN_YELLOW,  // 7: Warp -
  GOLDEN_YELLOW,  // 8: Warp to Morn
  SKY_BLUE,       // 9: Load
  AMBER,          //10: Pause
  SKY_BLUE        //11: Save
};

/***************************************************************************************
  Button Command Names (for mapping and reference)
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "Warp to ApA",     // 0
  "Cancel Warp",     // 1
  "Warp to PeA",     // 2
  "Physics Warp",    // 3
  "Warp to MNVR",    // 4
  "Warp +",          // 5
  "Warp to SOI",     // 6
  "Warp -",          // 7
  "Warp to Morn",    // 8
  "Load",            // 9
  "Pause",           //10
  "Save",            //11
  "",                //12 → unused/reserved
  "",                //13 → unused/reserved
  "",                //14 → unused/reserved
  ""                 //15 → unused/reserved
};

/***************************************************************************************
  Module LED Update Logic (ButtonModuleCore v1.1 priority)
****************************************************************************************/
void handle_ledUpdate() {
  // Priority (per LED):
  //   1) led_bits bit == 1              -> assigned color
  //   2) else if button_active_bits == 1 -> DIM_GRAY
  //   3) else                            -> OFF (BLACK)

  const uint16_t ledState        = led_bits;
  const uint16_t activeState     = button_active_bits;
  const uint16_t prevLedState    = prev_led_bits;
  const uint16_t prevActiveState = prev_button_active_bits;

  bool updated = false;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    const bool ledNow    = bitRead(ledState, i);
    const bool activeNow = bitRead(activeState, i);
    const bool ledPrev   = bitRead(prevLedState, i);
    const bool actPrev   = bitRead(prevActiveState, i);

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

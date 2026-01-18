/********************************************************************************************************************************
  Vehicle Control Module for Kerbal Controller Mk1

  Uses shared ButtonModuleCore for common logic and I2C handling.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <ButtonModuleCore.h>

/***************************************************************************************
  Module-Specific Constants
****************************************************************************************/
constexpr uint8_t panel_addr = 0x23;  // I2C address for this module

/***************************************************************************************
  LED Color Mapping (Index-based from ColorIndex enum)
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  RED,            // 0: Brake
  GOLDEN_YELLOW,  // 1: Lights
  SKY_BLUE,       // 2: Solar
  GREEN,          // 3: Gear
  MINT,           // 4: Cargo
  MAGENTA,        // 5: Antenna
  CYAN,           // 6: Ladder
  LIME,           // 7: Radiator
  AMBER,          // 8: Main Deploy
  ORANGE,         // 9: Drogue Deploy
  RED,            //10: Main Cut
  RED             //11: Drogue Cut
};

/***************************************************************************************
  Button Command Names (for mapping and reference)
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "Brake",           // 0
  "Lights",          // 1
  "Solar",           // 2
  "Gear",            // 3
  "Cargo",           // 4
  "Antenna",         // 5
  "Ladder",          // 6
  "Radiator",        // 7
  "MainDep",         // 8
  "DrogueDep",       // 9
  "MainCut",         //10
  "DrogueCut",       //11
  "Brake Lock",      //12
  "Parachute Lock",  //13
  "Lights Lock",     //14
  "Gear Lock"        //15
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

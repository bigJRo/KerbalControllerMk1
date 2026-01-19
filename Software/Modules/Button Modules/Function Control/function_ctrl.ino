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
  BLUE,         // 0: Sci Collect
  AMBER,        // 1: Engine Alt
  CYAN,         // 2: Science Grp 1
  GREEN,        // 3: Engine Grp 1
  CYAN,         // 4: Science Grp 2
  GREEN,        // 5: Engine Grp 2
  SKY_BLUE,     // 6: Air Intake
  RED,          // 7: Launch Escape
  ORANGE,       // 8: Air Brake
  AMBER,        // 9: Lock Ctrl
  MINT,         //10: HtShield Rel
  RED           //11: HtShield Deploy
};

/***************************************************************************************
  Button Command Names (for mapping and reference)
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "Sci Collect",            // 0
  "Engine Alt",             // 1
  "Science Grp 1",          // 2
  "Engine Grp 1",           // 3
  "Science Grp 2",          // 4
  "Engine Grp 2",           // 5
  "Air Intake",             // 6
  "Launch Escape",          // 7
  "Air Brake",              // 8
  "Lock Ctrl",              // 9
  "Heat Shield Release",    //10
  "Heat Shield Deploy",     //11
  "Reserved/Unused",        //12 → discrete output
  "Reserved/Unused",        //13 → discrete output
  "Reserved/Unused",        //14 → discrete output
  "Reserved/Unused"         //15 → discrete output
};

/***************************************************************************************
  Module LED Update Logic (ButtonModuleCore v1.1 priority)
****************************************************************************************/
void handle_ledUpdate() {
  // Priority (per output):
  //   1) led_bits bit == 1            -> assigned color (RGB) / HIGH (discrete)
  //   2) else if button_active_bits   -> DIM_GRAY (RGB only)
  //   3) else                         -> OFF (BLACK for RGB) / LOW (discrete)

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

    // For RGB outputs, compare full "mode" so we only update when visible output changes.
    // Mode: 0=OFF, 1=DIM_GRAY, 2=ASSIGNED_COLOR
    const uint8_t modeNow  = ledNow ? 2 : (activeNow ? 1 : 0);
    const uint8_t modePrev = ledPrev ? 2 : (actPrev   ? 1 : 0);

    if (i < NUM_LEDS) {
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
      // Discrete outputs cannot dim: they follow led_bits only.
      // Only update if led_bits changed for this index.
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

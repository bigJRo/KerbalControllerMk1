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
  ORANGE,         // 8: Drogue Deploy
  AMBER,          // 9: Main Deploy
  RED,            //10: Drogue Cut
  RED             //11: Main Cut
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
  "DrogueDep",       // 8
  "MainDep",         // 9
  "DrogueCut",       //10
  "MainCut",         //11
  "Brake Lock",      //12
  "Parachute Lock",  //13
  "Lights Lock",     //14
  "Gear Lock"        //15
};

/***************************************************************************************
  Module LED Update Logic
****************************************************************************************/
void handle_ledUpdate() {
  uint16_t bits = led_bits;
  uint16_t prevBits = prev_led_bits;
  bool overlayEnabled = bitRead(bits, 13);
  bool updated = false;

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    bool newState = bitRead(bits, i);
    bool oldState = bitRead(prevBits, i);
    if (newState == oldState) continue;

    if (i < 8) {
      buttonPixel px = newState ? getColorFromTable(pixel_Array[i]) : getColorFromTable(DIM_GRAY);
      leds.setPixelColor(i, px.r, px.g, px.b);
      updated = true;

    } else if (i < NUM_LEDS) {
      bool localActive = bitRead(bits, i);
      bool modeActive = true;
      if (i == 10) modeActive = bitRead(bits, 8);
      if (i == 11) modeActive = bitRead(bits, 9);
      buttonPixel px = overlayColor(overlayEnabled, modeActive, localActive, pixel_Array[i]);
      leds.setPixelColor(i, px.r, px.g, px.b);
      updated = true;

    } else if (i < NUM_BUTTONS) {
      digitalWrite(discreteLEDs[i - NUM_LEDS], newState ? HIGH : LOW);
    }
  }

  if (updated) leds.show();
  prev_led_bits = bits;
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

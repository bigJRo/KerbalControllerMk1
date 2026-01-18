/********************************************************************************************************************************
  Assembly Test Module for Kerbal Controller Mk1

  Uses shared ButtonModuleCore for common logic and I2C handling.
  Adds an LED "rainbow cascade" post-bulb-test animation for assembly verification.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <ButtonModuleCore.h>

/***************************************************************************************
  Module-Specific Constants
****************************************************************************************/
constexpr uint8_t panel_addr = 0x2B;  // I2C slave address for Assembly Test module (adjust as needed)

/***************************************************************************************
  LED Color Mapping (Index-based from ColorIndex enum)
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  RED,            // 0
  AMBER,          // 1
  ORANGE,         // 2
  GOLDEN_YELLOW,  // 3
  SKY_BLUE,       // 4
  GREEN,          // 5
  MINT,           // 6
  MAGENTA,        // 7
  CYAN,           // 8
  LIME,           // 9
  YELLOW,         //10
  FUCHSIA         //11
};

/***************************************************************************************
  Button Command Names (for mapping and reference)
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "LED0",   // 0
  "LED1",   // 1
  "LED2",   // 2
  "LED3",   // 3
  "LED4",   // 4
  "LED5",   // 5
  "LED6",   // 6
  "LED7",   // 7
  "LED8",   // 8
  "LED9",   // 9
  "LED10",  // 10
  "LED11",  // 11
  "DOUT0",  // 12 -> discrete output 0
  "DOUT1",  // 13 -> discrete output 1
  "DOUT2",  // 14 -> discrete output 2
  "DOUT3"   // 15 -> discrete output 3
};

/***************************************************************************************
  Local Discrete Output Toggle State (Indices 12–15)
****************************************************************************************/
static uint8_t discreteToggleState = 0;  // bit0..bit3 map to discreteLEDs[0..3]

/***************************************************************************************
  Helpers
****************************************************************************************/
static inline buttonPixel wheel(uint8_t pos) {
  // Classic NeoPixel "wheel" (0-255) -> RGB
  // Produces a smooth rainbow: R->G->B->R
  buttonPixel c;
  if (pos < 85) {
    c.r = 255 - pos * 3;
    c.g = pos * 3;
    c.b = 0;
  } else if (pos < 170) {
    pos -= 85;
    c.r = 0;
    c.g = 255 - pos * 3;
    c.b = pos * 3;
  } else {
    pos -= 170;
    c.r = pos * 3;
    c.g = 0;
    c.b = 255 - pos * 3;
  }
  return c;
}

/***************************************************************************************
  Post Bulb-Test Assembly Animation
****************************************************************************************/
void runAssemblyRainbowTest() {
  // Total sequence: 10 seconds
  // Phase 1 (cascade): light LEDs one by one with changing hue (~40% time)
  // Phase 2 (keyboard rainbow): all LEDs lit and hue scrolls (~60% time)
  // End: all LEDs OFF

  constexpr uint32_t totalMs  = 10000;
  constexpr uint32_t phase1Ms = 4000;
  constexpr uint32_t phase2Ms = totalMs - phase1Ms;

  // Phase 1: cascade
  const uint32_t stepDelay = phase1Ms / NUM_LEDS;  // integer math ok
  uint8_t hue = 0;

  // Start with all off
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    buttonPixel off = getColorFromTable(BLACK);
    leds.setPixelColor(i, off.r, off.g, off.b);
  }
  leds.show();

  for (uint8_t litCount = 0; litCount < NUM_LEDS; litCount++) {
    // Illuminate LEDs [0..litCount] with a rainbow across indices
    for (uint8_t i = 0; i <= litCount; i++) {
      // Spread hues across indices so it looks like a cascade
      const uint8_t p = (uint8_t)(hue + i * 12);
      buttonPixel c = wheel(p);
      leds.setPixelColor(i, c.r, c.g, c.b);
    }
    leds.show();
    hue += 9;
    delay(stepDelay);
  }

  // Phase 2: keyboard-style rainbow scroll (~33 FPS)
  constexpr uint16_t frameDelay = 30;
  const uint32_t frames = phase2Ms / frameDelay;

  for (uint32_t f = 0; f < frames; f++) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      const uint8_t p = (uint8_t)(hue + i * 14);
      buttonPixel c = wheel(p);
      leds.setPixelColor(i, c.r, c.g, c.b);
    }
    leds.show();
    hue += 4;
    delay(frameDelay);
  }

  // End: all LEDs OFF
  buttonPixel off = getColorFromTable(BLACK);
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds.setPixelColor(i, off.r, off.g, off.b);
  }
  leds.show();
}

/***************************************************************************************
  Module LED Update Logic (ButtonModuleCore v1.1 priority)
****************************************************************************************/
void handle_ledUpdate() {
  // Priority (per RGB LED 0–11):
  //   1) led_bits bit == 1               -> assigned color
  //   2) else if button_active_bits == 1 -> DIM_GRAY
  //   3) else                             -> OFF (BLACK)

  const uint16_t ledState        = led_bits;
  const uint16_t activeState     = button_active_bits;
  const uint16_t prevLedState    = prev_led_bits;
  const uint16_t prevActiveState = prev_button_active_bits;

  bool rgbUpdated = false;

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
    rgbUpdated = true;
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

  // After standard bulb test, run the assembly rainbow test once
  runAssemblyRainbowTest();

  // Ensure discrete outputs start LOW
  for (uint8_t i = 0; i < 4; i++) {
    digitalWrite(discreteLEDs[i], LOW);
  }
}

/***************************************************************************************
  Main Loop
****************************************************************************************/
void loop() {
  // Standard LED update behavior (RGB LEDs controlled by host via core)
  if (updateLED) {
    handle_ledUpdate();
    updateLED = false;
  }

  // Sample buttons (core updates button_state / pressed / released bitfields)
  readButtonStates();

  // Toggle discrete outputs (buttons 12–15) on press
  // NOTE: This relies on ButtonModuleCore v1.1 providing button_pressed_bits.
  // If the core variable name differs, change it here to match the core header.
  const uint16_t pressed = button_pressed_bits;

  for (uint8_t idx = 12; idx < 16; idx++) {
    if (bitRead(pressed, idx)) {
      const uint8_t d = idx - 12;  // 0..3
      bitWrite(discreteToggleState, d, !bitRead(discreteToggleState, d));
      digitalWrite(discreteLEDs[d], bitRead(discreteToggleState, d) ? HIGH : LOW);
    }
  }
}

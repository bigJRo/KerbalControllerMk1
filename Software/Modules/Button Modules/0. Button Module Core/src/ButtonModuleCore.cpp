#include "ButtonModuleCore.h"
#include <util/atomic.h>

/********************************************************************************************************************************
  Button Module Core for Kerbal Controller

  Handles common functions for all Button Modules for use in Kerbal Controller Mk1
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/

/***************************************************************************************
  Lookup table of RGB color definitions for LED feedback
  Stored in PROGMEM to save precious SRAM on the ATtiny816
  Each entry is a custom-named color used to indicate status of individual functions
  Indexes are mapped via the ColorIndex enum for readability
****************************************************************************************/
const buttonPixel colorTable[] PROGMEM = {
  { 128, 0, 0 },      // RED: General warning or critical function
  { 128, 96, 0 },     // AMBER: Secondary status or transitional state
  { 128, 50, 0 },     // ORANGE: UI prompt or toggle
  { 128, 108, 0 },    // GOLDEN_YELLOW: Status indicator (e.g., deployed)
  { 64, 128, 160 },   // SKY_BLUE: Represents atmosphere or power
  { 0, 128, 0 },      // GREEN: Safe or confirmed
  { 64, 128, 96 },    // MINT: Passive/idle
  { 128, 0, 128 },    // MAGENTA: Communication
  { 0, 128, 128 },    // CYAN: Cooling or environmental
  { 64, 128, 0 },     // LIME: Active
  { 255, 255, 255 },  // WHITE: All-clear or generic
  { 0, 0, 0 },        // BLACK: Off or disabled
  { 32, 32, 32 },     // DIM_GRAY: Neutral, inactive but valid
  { 0, 0, 255 },      // BLUE: Default function
  { 255, 0, 0 },      // BRIGHT_RED: Emergency
  { 0, 255, 0 },      // BRIGHT_GREEN: Enabled
  { 0, 0, 255 },      // BRIGHT_BLUE: Attention
  { 255, 255, 0 },    // YELLOW: Alert
  { 0, 255, 255 },    // AQUA: Coolant/temperature
  { 255, 0, 255 }     // FUCHSIA: Comms or science
};


/***************************************************************************************
  Global variable setup
****************************************************************************************/
const uint8_t discreteLEDs[4] = { led_13, led_14, led_15, led_16 };  // Discrete output pins mapped to final 4 button functions

volatile bool updateLED = false;  // Flag from I2C input event
uint16_t button_state_bits = 0;   // Bitfield for all buttons
uint16_t button_pressed_bits = 0;   // Bitfield: buttons that transitioned 0->1 since last read
uint16_t button_released_bits = 0;  // Bitfield: buttons that transitioned 1->0 since last read

uint16_t led_bits = 0;            // Desired LED state
uint16_t button_active_bits = 0;  // Bitfield: buttons currently active (from master)
uint16_t prev_led_bits = 0;       // Previous LED state (to avoid redundant updates)
uint16_t prev_button_active_bits = 0;  // Previous active state (to avoid redundant updates)

static uint16_t prev_button_state_bits = 0;  // Internal: for press/release edge detection

/***************************************************************************************
  Setup for libary objects
****************************************************************************************/
ShiftIn<2> shift;                                                // 16-bit shift register interface
tinyNeoPixel leds = tinyNeoPixel(NUM_LEDS, neopixCmd, NEO_GRB);  // Neopixel LED object

/***************************************************************************************
  Module configuration helper function
****************************************************************************************/
void beginModule(uint8_t panel_addr) {
  pinMode(INT_OUT, OUTPUT);
  clearInterrupt();

  for (uint8_t i = 0; i < 4; i++) {
    pinMode(discreteLEDs[i], OUTPUT);
    digitalWrite(discreteLEDs[i], LOW);
  }

  shift.begin(load, clockEnable, dataInPin, clockIn);

  leds.begin();
  delay(10);

  bulbTest();  // <-- Run bulb test on startup

  Wire.begin(SDA, SCL, panel_addr);
  Wire.onReceive(handleReceiveEvent);
  Wire.onRequest(handleRequestEvent);
}

/***************************************************************************************
  Function to read button state using shift register if update is detected
****************************************************************************************/
void readButtonStates() {
  // Check for button state changes
  if (shift.update()) {
    uint16_t newState = 0;
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
      newState |= (shift.state(i) ? 1 : 0) << i;
    }
    // Edge detection (0->1 and 1->0)
    const uint16_t new_pressed  = (uint16_t)( newState & (uint16_t)~prev_button_state_bits);
    const uint16_t new_released = (uint16_t)(~newState & (uint16_t) prev_button_state_bits);

    // Snapshot/update shared 16-bit fields atomically.
    // Pressed/released are *latched* until the master reads them.
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      button_state_bits = newState;
      button_pressed_bits  |= new_pressed;
      button_released_bits |= new_released;
    }
    prev_button_state_bits = newState;
    setInterrupt();  // Signal to master that buttons have changed
  }
}

// Small HSV->RGB helper for bulbTest rainbow (8-bit hue)
static inline buttonPixel hueToRGB(uint8_t hue) {
  // Classic "wheel" mapping: 0..255
  // Keeps math simple and fast on tinyAVR.
  buttonPixel px;
  uint8_t region = hue / 85;         // 0..2
  uint8_t pos = (hue % 85) * 3;      // 0..252

  switch (region) {
    case 0:  // R->G
      px.r = 255 - pos;
      px.g = pos;
      px.b = 0;
      break;
    case 1:  // G->B
      px.r = 0;
      px.g = 255 - pos;
      px.b = pos;
      break;
    default: // B->R
      px.r = pos;
      px.g = 0;
      px.b = 255 - pos;
      break;
  }
  return px;
}

/***************************************************************************************
  Helper functions for interrupt processing
****************************************************************************************/
void clearInterrupt() {
  digitalWrite(INT_OUT, HIGH);
}

void setInterrupt() {
  digitalWrite(INT_OUT, LOW);
}

/***************************************************************************************
  Helper to fetch RGB color struct from PROGMEM
****************************************************************************************/
buttonPixel getColorFromTable(ColorIndex index) {
  buttonPixel px;
  const uint8_t* ptr = (const uint8_t*)colorTable + index * sizeof(buttonPixel);
  px.r = pgm_read_byte(ptr);
  px.g = pgm_read_byte(ptr + 1);
  px.b = pgm_read_byte(ptr + 2);
  return px;
}

/***************************************************************************************
  Overlay Color Helper: handles logic for LEDs 8–11
****************************************************************************************/
buttonPixel overlayColor(bool overlayEnabled, bool modeActive, bool localActive, uint8_t colorIndex) {
  if (!overlayEnabled || !modeActive) return getColorFromTable(BLACK);
  if (localActive) return getColorFromTable(static_cast<ColorIndex>(colorIndex));
  return getColorFromTable(DIM_GRAY);
}

/***************************************************************************************
  Bulb Test Function
  - Cycles through RED, GREEN, BLUE, WHITE on all pixels
  - Then displays a specific pattern on pixel_Array
  - Sets odd discrete outputs HIGH and even LOW during the pattern
  - Holds the pattern for 2 seconds before resetting
****************************************************************************************/
void bulbTest() {
  const ColorIndex sequence[] = { RED, GREEN, BLUE, WHITE };

  // Cycle all NeoPixels through the base color sequence
  for (ColorIndex color : sequence) {
    buttonPixel px = getColorFromTable(color);
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      leds.setPixelColor(i, px.r, px.g, px.b);
    }

    for (uint8_t j = 0; j < 4; j++) {
      digitalWrite(discreteLEDs[j], (color % 2 == j % 2) ? HIGH : LOW);  // Alternating pattern
    }

    leds.show();
    delay(150);
  }

  // Custom pixel pattern
  const ColorIndex finalPattern[NUM_LEDS] = {
    RED, WHITE, BLUE, GREEN,
    RED, BLUE, WHITE, RED,
    GREEN, BLUE, WHITE, GREEN
  };

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    buttonPixel px = getColorFromTable(finalPattern[i]);
    leds.setPixelColor(i, px.r, px.g, px.b);
  }

  // Set discrete LEDs: odd HIGH, even LOW
  for (uint8_t j = 0; j < 4; j++) {
    digitalWrite(discreteLEDs[j], (j % 2 == 1) ? HIGH : LOW);
  }

  leds.show();
  delay(2000);  // Hold final pattern for 2 seconds

  // Clear everything
  for (uint8_t i = 0; i < NUM_LEDS; i++) leds.setPixelColor(i, 0, 0, 0);
  leds.show();
  for (uint8_t j = 0; j < 4; j++) digitalWrite(discreteLEDs[j], LOW);
}

/***************************************************************************************
  I2C Event Handlers

  ATtiny816 acts as an I2C SLAVE device at address 0x23.
  Current protocol:

  - Master reads 6 bytes:
    [0] = button state bits 0–7
    [1] = button state bits 8–15
    [2] = button pressed edge bits 0–7   (0->1 since last readButtonStates update)
    [3] = button pressed edge bits 8–15
    [4] = button released edge bits 0–7  (1->0 since last readButtonStates update)
    [5] = button released edge bits 8–15

  - Master writes 4 bytes (or 2 bytes legacy):
    [0] = LED control bits LSB
    [1] = LED control bits MSB
    [2] = button_active_bits LSB (optional)
    [3] = button_active_bits MSB (optional)

  Note: LED interpretation is module-specific; this core library only receives/stores
  the control bitfields and sets updateLED when they change.

****************************************************************************************/
void handleRequestEvent() {
  // Respond to master read request with 6-byte status report
  // [0..1] button_state_bits (LSB, MSB)
  // [2..3] button_pressed_bits (LSB, MSB)   (latched since last read)
  // [4..5] button_released_bits (LSB, MSB)  (latched since last read)

  // These 16-bit values are shared with loop() code; snapshot atomically.
  uint16_t state = 0;
  uint16_t pressed = 0;
  uint16_t released = 0;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    state = button_state_bits;
    pressed = button_pressed_bits;
    released = button_released_bits;
    // One-shot behavior: once reported, clear the edge flags.
    button_pressed_bits = 0;
    button_released_bits = 0;
  }

  uint8_t response[6] = {
    (uint8_t)(state & 0xFF),
    (uint8_t)(state >> 8),
    (uint8_t)(pressed & 0xFF),
    (uint8_t)(pressed >> 8),
    (uint8_t)(released & 0xFF),
    (uint8_t)(released >> 8)
  };
  Wire.write(response, sizeof(response));
  clearInterrupt();
}

void handleReceiveEvent(int16_t howMany) {
  // Master has sent LED control words.
  // Expected payload: 4 bytes
  //   [0] led_bits LSB
  //   [1] led_bits MSB
  //   [2] button_active_bits LSB  (buttons currently active)
  //   [3] button_active_bits MSB
  // Backwards compatibility: if only 2 bytes arrive, update led_bits only.

  if (Wire.available() < 2) return;

  const uint8_t led_lsb = Wire.read();
  const uint8_t led_msb = Wire.read();
  const uint16_t new_led_bits = (uint16_t)((uint16_t)led_msb << 8) | led_lsb;

  uint16_t new_active_bits = button_active_bits;
  if (Wire.available() >= 2) {
    const uint8_t active_lsb = Wire.read();
    const uint8_t active_msb = Wire.read();
    new_active_bits = (uint16_t)((uint16_t)active_msb << 8) | active_lsb;
  }

  // Update + flag only if something changed
  if (new_led_bits != led_bits || new_active_bits != button_active_bits) {
    prev_led_bits = led_bits;
    prev_button_active_bits = button_active_bits;
    led_bits = new_led_bits;
    button_active_bits = new_active_bits;
    updateLED = true;
  }
}

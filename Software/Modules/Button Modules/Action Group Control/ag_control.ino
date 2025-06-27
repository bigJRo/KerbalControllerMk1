/********************************************************************************************************************************
  Action Group Control Module for Kerbal Controller

  Adapted from the UI Control Module by J. Rostoker for Jeb's Controller Works.
  Handles Action Group toggling, LED status feedback, and I2C communication with the host.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>          // I2C communication
#include <ShiftIn.h>       // Shift register input
#include <tinyNeoPixel.h>  // NeoPixel support for ATtiny series
#include <avr/pgmspace.h>  // PROGMEM access for ATtiny

/***************************************************************************************
  Pin Definitions (ATtiny816 w/ megaTinyCore Pin Mapping)
****************************************************************************************/
#define SDA PIN_PB1  // I2C data pin
#define SCL PIN_PB0  // I2C clock pin

#define INT_OUT PIN_PA4      // Interrupt output to notify host of button state change
#define load PIN_PA7         // Shift register load pin
#define clockEnable PIN_PA6  // Shift register clock enable (active low)
#define clockIn PIN_PB5      // Shift register clock input
#define dataInPin PIN_PA5    // Shift register data pin
#define neopixCmd PIN_PB4    // NeoPixel data output pin

#define led_13 PIN_PB3  // Lock LED 1
#define led_14 PIN_PC2  // Lock LED 2
#define led_15 PIN_PB2  // Lock LED 3
#define led_16 PIN_PC1  // Lock LED 4

/***************************************************************************************
  Constants and Globals
****************************************************************************************/
constexpr uint8_t panel_addr = 0x29;  // I2C slave address for Action Group Control
constexpr uint8_t NUM_LEDS = 12;      // Number of addressable NeoPixels
constexpr uint8_t NUM_BUTTONS = 16;   // Number of input buttons

tinyNeoPixel leds = tinyNeoPixel(NUM_LEDS, neopixCmd, NEO_GRB);  // LED object

struct buttonPixel {
  uint8_t r, g, b;  // RGB color values
};

/***************************************************************************************
  Lookup table of RGB color definitions for LED feedback
  Stored in PROGMEM to save precious SRAM on the ATtiny816
  Each entry is a custom-named color used to indicate status of individual functions
  Indexes are mapped via the ColorIndex enum for readability
****************************************************************************************/
const buttonPixel colorTable[] PROGMEM = {
  { 128, 0, 0 },      // RED
  { 128, 96, 0 },     // AMBER
  { 128, 50, 0 },     // ORANGE
  { 128, 108, 0 },    // GOLDEN_YELLOW
  { 64, 128, 160 },   // SKY_BLUE
  { 0, 128, 0 },      // GREEN
  { 64, 128, 96 },    // MINT
  { 128, 0, 128 },    // MAGENTA
  { 0, 128, 128 },    // CYAN
  { 64, 128, 0 },     // LIME
  { 255, 255, 255 },  // WHITE
  { 0, 0, 0 },        // BLACK
  { 32, 32, 32 },     // DIM_GRAY
  { 0, 0, 255 },      // BLUE
  { 255, 0, 0 },      // BRIGHT_RED
  { 0, 255, 0 },      // BRIGHT_GREEN
  { 0, 0, 255 },      // BRIGHT_BLUE
  { 255, 255, 0 },    // YELLOW
  { 0, 255, 255 },    // AQUA
  { 255, 0, 255 }     // FUCHSIA
};

// Human-readable color index enum
enum ColorIndex : uint8_t {
  RED,
  AMBER,
  ORANGE,
  GOLDEN_YELLOW,
  SKY_BLUE,
  GREEN,
  MINT,
  MAGENTA,
  CYAN,
  LIME,
  WHITE,
  BLACK,
  DIM_GRAY,
  BLUE,
  BRIGHT_RED,
  BRIGHT_GREEN,
  BRIGHT_BLUE,
  YELLOW,
  AQUA,
  FUCHSIA
};

// Helper to fetch RGB color struct from PROGMEM
buttonPixel getColorFromTable(ColorIndex index) {
  buttonPixel px;
  const uint8_t* ptr = (const uint8_t*)colorTable + index * sizeof(buttonPixel);
  px.r = pgm_read_byte(ptr);
  px.g = pgm_read_byte(ptr + 1);
  px.b = pgm_read_byte(ptr + 2);
  return px;
}

/***************************************************************************************
  Maps physical LED index (0–11) to a color index from colorTable[]
  Used to assign a default visual feedback color per functional button
  All 12 LEDs correspond to Action Group toggle buttons (AG1–AG12)
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
  Defines functional label for each button index (0–15)
  These strings are not used directly in logic, but help map indices to in-game actions
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
  "",      // 14 → unused
  ""       // 15 → unused
};

// Discrete output pins mapped to final 4 button functions
const uint8_t discreteLEDs[4] = { led_13, led_14, led_15, led_16 };

uint16_t button_state_bits = 0;  // Bitfield for all buttons
ShiftIn<2> shift;                // 16-bit shift register interface

volatile bool updateLED = false;  // Flag from I2C input event
uint16_t led_bits = 0;            // Desired LED state
uint16_t prev_led_bits = 0;       // Previous LED state (to avoid redundant updates)

/***************************************************************************************
  Overlay Color Helper: handles logic for LEDs 8–11
****************************************************************************************/
buttonPixel overlayColor(bool overlayEnabled, bool modeActive, bool localActive, uint8_t colorIndex) {
  if (!overlayEnabled || !modeActive) return getColorFromTable(BLACK);
  if (localActive) return getColorFromTable(colorIndex);
  return getColorFromTable(DIM_GRAY);
}


/***************************************************************************************
  Setup
****************************************************************************************/
void setup() {
  pinMode(INT_OUT, OUTPUT);
  digitalWrite(INT_OUT, HIGH);

  shift.begin(load, clockEnable, dataInPin, clockIn);

  leds.begin();
  delay(10);  // Ensure NeoPixel hardware has time to initialize

  // Set initial colors: white for active slots, black for others
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    buttonPixel px = (i < 8) ? getColorFromTable(WHITE) : getColorFromTable(BLACK);
    leds.setPixelColor(i, px.r, px.g, px.b);
  }
  leds.show();

  // Start I2C and register callbacks
  Wire.begin(SDA, SCL, panel_addr);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}

/***************************************************************************************
  Main Loop
****************************************************************************************/
void loop() {
  // Update LEDs if new data has been received over I2C
  if (updateLED) {
    handle_ledUpdate();
    updateLED = false;
  }

  // Check for button state changes
  if (shift.update()) {
    uint16_t newState = 0;
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
      newState |= (shift.state(i) ? 1 : 0) << i;
    }
    button_state_bits = newState;

    digitalWrite(INT_OUT, LOW);  // Notify master
  }
}

/***************************************************************************************
  LED Update Handler
****************************************************************************************/
void handle_ledUpdate() {
  uint16_t bits = led_bits;
  uint16_t prevBits = prev_led_bits;
  bool updated = false;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    bool newState = bitRead(bits, i);
    bool oldState = bitRead(prevBits, i);
    if (newState == oldState) continue;  // Skip unchanged states

    // Assign defined color if LED bit is set, or dim gray if not
    buttonPixel px = newState ? getColorFromTable(pixel_Array[i]) : getColorFromTable(DIM_GRAY);
    leds.setPixelColor(i, px.r, px.g, px.b);
    updated = true;
  }

  if (updated) leds.show();
  prev_led_bits = bits;
}

/***************************************************************************************
  I2C Event Handlers

  ATtiny816 acts as an I2C SLAVE device at address 0x23.
  The protocol between master and slave uses 4 bytes:

  - Master reads 4 bytes:
    [0] = button state bits 0–7
    [1] = button state bits 8–15
    [2] = LED control bits LSB (LED0–7)
    [3] = LED control bits MSB (LED8–15)

  - Master writes 2 bytes:
    [0] = LED control bits LSB
    [1] = LED control bits MSB

  The LED bits control color changes or status indication depending on the bit state.

****************************************************************************************/
void requestEvent() {
  // Respond to master read request with 4-byte status report
  uint8_t response[4] = {
    (uint8_t)(button_state_bits & 0xFF),  // Bits 0–7 of button state
    (uint8_t)(button_state_bits >> 8),    // Bits 8–15 of button state
    (uint8_t)(led_bits & 0xFF),           // LED LSB (control for LEDs 0–7)
    (uint8_t)(led_bits >> 8)              // LED MSB (control for LEDs 8–15)
  };
  Wire.write(response, sizeof(response));  // Send full report

  digitalWrite(INT_OUT, HIGH);  // Reset interrupt
}

void receiveEvent(int howMany) {
  // Master has sent LED state change request
  if (Wire.available() >= 2) {
    uint8_t lsb = Wire.read();  // Lower 8 bits of new LED bitfield
    uint8_t msb = Wire.read();  // Upper 8 bits of new LED bitfield
    led_bits = (msb << 8) | lsb;

    updateLED = true;  // Set flag to process LED changes in main loop
  }
}

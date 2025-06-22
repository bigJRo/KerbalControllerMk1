/********************************************************************************************************************************
  UI Control Module for Kerbal Controller

  Adapted from the Vehicle Control Module by J. Rostoker for Jeb's Controller Works.
  Handles UI-related commands, LED status feedback, and I2C communication with the host.
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
#define SDA 9  // PB1, I2C data pin
#define SCL 8  // PB0, I2C clock pin

#define INT_OUT 4      //  PA4, Interrupt output to notify host of button state change
#define load 7         // PA7, Shift register load pin
#define clockEnable 6  // PA6, Shift register clock enable (active low)
#define clockIn 13     // PB5, Shift register clock input
#define dataInPin 5    // PA5, Shift register data pin
#define neopixCmd 12   // PB4, NeoPixel data output pin

#define led_13 11  // PB3, Lock LED 1
#define led_14 15  // PC2, Lock LED 2
#define led_15 10  // PB2, Lock LED 3
#define led_16 14  // PC1, Lock LED 4

/***************************************************************************************
  Constants and Globals
****************************************************************************************/
constexpr uint8_t panel_addr = 0x20;  // I2C slave address for UIControl
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
buttonPixel getColorFromTable(ColorIndex  index) {
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
  All 12 LEDs correspond to direct command buttons
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  AMBER,     // 0: Map Enable
  GREEN,     // 1: Cycle Map -
  GREEN,     // 2: Cycle Map +
  BLUE,      // 3: Navball Mode
  AMBER,     // 4: IVA
  SKY_BLUE,  // 5: Cycle Cam
  GREEN,     // 6: Cycle Ship -
  GREEN,     // 7: Cycle Ship +
  GREEN,     // 8: Reset Focus
  MAGENTA,   // 9: Screen Shot
  AMBER,     //10: UI
  RED        //11: DEBUG
};

/***************************************************************************************
  Defines functional label for each button index (0–11)
  These strings are not used directly in logic, but help map indices to in-game actions
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "Map Enable",      // 0
  "Cycle Map -",     // 1
  "Cycle Map +",     // 2
  "Navball Mode",    // 3
  "IVA",             // 4
  "Cycle Cam",       // 5
  "Cycle Ship -",    // 6
  "Cycle Ship +",    // 7
  "Reset Focus",     // 8
  "Screen Shot",     // 9
  "UI",              //10
  "DEBUG",           //11
  "",                //12 → unused or reserved
  "",                //13 → unused or reserved
  "",                //14 → unused or reserved
  ""                 //15 → unused or reserved
};

/***************************************************************************************
  Global State Variables
****************************************************************************************/
uint16_t button_state_bits = 0;  // Bitfield for all buttons
ShiftIn<2> shift;                // 16-bit shift register interface

volatile bool updateLED = false;  // Flag from I2C input event
uint16_t led_bits = 0;            // Desired LED state
uint16_t prev_led_bits = 0;       // Previous LED state (to avoid redundant updates)


/***************************************************************************************
  Setup
****************************************************************************************/
void setup() {
  pinMode(INT_OUT, OUTPUT);
  digitalWrite(INT_OUT, HIGH);  // Idle high

  shift.begin(load, clockEnable, dataInPin, clockIn);  // Setup shift register

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

    digitalWrite(INT_OUT, LOW);  // Signal to master that buttons have changed
  }
}

/***************************************************************************************
  LED Update Handler
  - Compares new vs. previous LED state
  - Updates NeoPixels only if changes occurred
  - Special case: Cancel Warp (index 11) goes BLACK when off
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

/********************************************************************************************************************************
  Vehicle Control Module for Kerbal Controller Mk1

  References UntitledSpaceCraft module code written by CodapopKSP (https://github.com/CodapopKSP/UntitledSpaceCraft)
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
constexpr uint8_t panel_addr = 0x23;  // I2C slave address
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
  First 8 LEDs correspond to direct command buttons
  Last 4 may be overlays, indicators, or reserved for future use
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
  ORANGE,         // 8: Drogue Deploy (overlay zone)
  AMBER,          // 9: Main Deploy (overlay zone)
  RED,            //10: Drogue Cut (overlay zone)
  RED             //11: Main Cut (overlay zone)
};

/***************************************************************************************
  Defines functional label for each button index (0–15)
  These strings are not used directly in logic, but help map indices to in-game actions
  Example: button_state_bits bit 0 corresponds to "Brake" command
  Last four buttons (12–15) have associated discrete output LEDs on the controller
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
  "Brake Lock",      //12 → LED13 (discrete output)
  "Parachute Lock",  //13 → LED14 (discrete output)
  "Lights Lock",     //14 → LED15 (discrete output)
  "Gear Lock"        //15 → LED16 (discrete output)
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
  digitalWrite(INT_OUT, HIGH);  // Idle high

  // Initialize discrete output LEDs
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(discreteLEDs[i], OUTPUT);
    digitalWrite(discreteLEDs[i], LOW);
  }

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
****************************************************************************************/
void handle_ledUpdate() {
  uint16_t bits = led_bits;
  uint16_t prevBits = prev_led_bits;
  bool overlayEnabled = bitRead(bits, 13);
  bool updated = false;

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    bool newState = bitRead(bits, i);
    bool oldState = bitRead(prevBits, i);
    if (newState == oldState) continue;  // Skip unchanged states

    if (i < 8) {
      // Top 8 command buttons: set their assigned color or dim gray when off
      buttonPixel px = newState ? getColorFromTable(pixel_Array[i]) : getColorFromTable(DIM_GRAY);
      leds.setPixelColor(i, px.r, px.g, px.b);
      updated = true;

    } else if (i < NUM_LEDS) {
      // Parachute status LEDs with layered logic depending on Chutes enabled (LED bit 13) and local bit state
      bool localActive = bitRead(bits, i);
      bool modeActive = true;
      if (i == 10) modeActive = bitRead(bits, 8);  // Drogue Cut active only if Drogue deployed
      if (i == 11) modeActive = bitRead(bits, 9);  // Main Cut active only if Main deployed
      buttonPixel px = overlayColor(overlayEnabled, modeActive, localActive, pixel_Array[i]);
      leds.setPixelColor(i, px.r, px.g, px.b);
      updated = true;

    } else if (i < NUM_BUTTONS) {
      // Discrete LED outputs (lock states)
      digitalWrite(discreteLEDs[i - NUM_LEDS], newState ? HIGH : LOW);
    }
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

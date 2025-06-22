/********************************************************************************************************************************
  Stability Control Module for Kerbal Controller Mk1

  Adapted from the Function Control Module by J. Rostoker for Jeb's Controller Works.
  Handles stability-related SAS commands, RGB LED feedback, and I2C communication with the host.
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

#define INT_OUT 4      // PA4, Interrupt output to notify host of button state change
#define load 7         // PA7, Shift register load pin
#define clockEnable 6  // PA6, Shift register clock enable (active low)
#define clockIn 13     // PB5, Shift register clock input
#define dataInPin 5    // PA5, Shift register data pin
#define neopixCmd 12   // PB4, NeoPixel data output pin

#define led_13 11  // PB3, Discrete LED 1 (RCS Lock)
#define led_14 15  // PC2, Discrete LED 2 (SAS Lock)
#define led_15 10  // PB2, Discrete LED 3 (Pilot Assist)
#define led_16 14  // PC1, Discrete LED 4 (Reaction Ctrl)

/***************************************************************************************
  Constants and Globals
****************************************************************************************/
constexpr uint8_t panel_addr = 0x2A;  // I2C slave address for Stability Control Module
constexpr uint8_t NUM_LEDS = 12;      // Number of NeoPixels
constexpr uint8_t NUM_BUTTONS = 16;   // Total input buttons (via shift registers)

tinyNeoPixel leds = tinyNeoPixel(NUM_LEDS, neopixCmd, NEO_GRB);  // LED object

struct buttonPixel {
  uint8_t r, g, b;  // RGB color triplet
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
  All 12 LEDs correspond to direct command buttons
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  GREEN,  // 0: SAS Maneuver
  GREEN,  // 1: SAS Hold
  GREEN,  // 2: SAS Retrograde
  GREEN,  // 3: SAS Prograde
  GREEN,  // 4: SAS Antinormal
  GREEN,  // 5: SAS Normal
  GREEN,  // 6: SAS Radial Out
  GREEN,  // 7: SAS Radial In
  GREEN,  // 8: SAS Anti-Target
  GREEN,  // 9: SAS Target
  GREEN,  //10: SAS Invert
  BLACK   //11: Unused
};

/***************************************************************************************
  Defines functional label for each button index (0–15)
  These strings are not used directly in logic, but help map indices to in-game actions
  Example: button_state_bits bit 0 corresponds to "Brake" command
  Last four buttons (12–15) have associated discrete output LEDs on the controller
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "SAS Maneuver",   // 0
  "SAS Hold",       // 1
  "SAS Retrograde", // 2
  "SAS Prograde",   // 3
  "SAS Antinormal", // 4
  "SAS Normal",     // 5
  "SAS Rad Out",    // 6
  "SAS Rad In",     // 7
  "Anti-Target",    // 8
  "Target",         // 9
  "SAS Invert",     //10
  "Unused",         //11
  "SAS Enable",     //12 → LED13 (discrete output)
  "RCS Enable",     //13 → LED14 (discrete output
  "Unused/Reserved",//14 → LED15 (discrete output)
  "Unused/Reserved" //15 → LED16 (discrete output)
};

// Discrete output pins mapped to final 4 button functions
const uint8_t discreteLEDs[4] = { led_13, led_14, led_15, led_16 };

uint16_t button_state_bits = 0;  // Bitfield for all button states
ShiftIn<2> shift;                // 16-bit shift register interface

volatile bool updateLED = false;  // Flag from I2C input event
uint16_t led_bits = 0;            // Desired LED state bitfield
uint16_t prev_led_bits = 0;       // Cached LED state to avoid redundant updates


/***************************************************************************************
  Setup
****************************************************************************************/
void setup() {
  pinMode(INT_OUT, OUTPUT);
  digitalWrite(INT_OUT, HIGH);  // Idle high

  // Initialize discrete outputs
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(discreteLEDs[i], OUTPUT);
    digitalWrite(discreteLEDs[i], LOW);
  }

  shift.begin(load, clockEnable, dataInPin, clockIn);

  leds.begin();
  delay(10);  // Give NeoPixels time to power up

  // Show placeholder white for all LEDs
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    buttonPixel px = getColorFromTable(WHITE);
    leds.setPixelColor(i, px.r, px.g, px.b);
  }
  leds.show();

  // Initialize I2C and register callbacks
  Wire.begin(SDA, SCL, panel_addr);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}

/***************************************************************************************
  Main Loop
****************************************************************************************/
void loop() {
  // Handle incoming LED state update
  if (updateLED) {
    handle_ledUpdate();
    updateLED = false;
  }

  // Poll shift registers for button state changes
  if (shift.update()) {
    uint16_t newState = 0;
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
      newState |= (shift.state(i) ? 1 : 0) << i;
    }
    button_state_bits = newState;
    
    // Signal host that buttons have changed
    digitalWrite(INT_OUT, LOW);  // Signal host of change
  }
}

/***************************************************************************************
  LED Update Handler
  - Compares new LED state to previous
  - Updates NeoPixels and discrete LEDs accordingly
****************************************************************************************/
void handle_ledUpdate() {
  uint16_t bits = led_bits;
  uint16_t prevBits = prev_led_bits;
  bool updated = false;

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    bool newState = bitRead(bits, i);
    bool oldState = bitRead(prevBits, i);
    if (newState == oldState) continue;  // No change

    if (i < NUM_LEDS) {
      // RGB LED update
      buttonPixel px = newState ? getColorFromTable(pixel_Array[i]) : getColorFromTable(DIM_GRAY);
      leds.setPixelColor(i, px.r, px.g, px.b);
      updated = true;
    
    } else {
      // Discrete LED output (buttons 12–15)
      digitalWrite(discreteLEDs[i - NUM_LEDS], newState ? HIGH : LOW);
    }
  }

  if (updated) leds.show();
  prev_led_bits = bits;
}

/***************************************************************************************
  I2C Event Handlers

  ATtiny816 acts as an I2C SLAVE device at address 0x2A.
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
  Wire.write(response, sizeof(response));
}

void receiveEvent(int howMany) {
  if (Wire.available() >= 2) {
    uint8_t lsb = Wire.read();  // LED bits 0–7
    uint8_t msb = Wire.read();  // LED bits 8–15
    led_bits = (msb << 8) | lsb;
    updateLED = true;
  }
}

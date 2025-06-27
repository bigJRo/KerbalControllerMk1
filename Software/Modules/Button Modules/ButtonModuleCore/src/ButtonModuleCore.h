#ifndef BUTTON_MODULE_CORE_H
#define BUTTON_MODULE_CORE_H

/********************************************************************************************************************************
  Button Module Core for Kerbal Controller

  Handles common functions for all Button Modules for us in Kerbal Controller Mk1
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/

#include <Arduino.h>       // Include arduino namespace
#include <Wire.h>          // I2C communication
#include <ShiftIn.h>       // Shift register input
#include <tinyNeoPixel.h>  // NeoPixel support for ATtiny series
#include <avr/pgmspace.h>  // PROGMEM access for ATtiny

/***************************************************************************************
  Pin Definitions (ATtiny816 w/ megaTinyCore Pin Mapping)
****************************************************************************************/
#define SDA PIN_PB1  // I2C data pin
#define SCL PIN_PB0  // I2C clock pin

#define INT_OUT PIN_PA4      //  Interrupt output to notify host of button state change
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
  Structs and Enums
****************************************************************************************/
struct buttonPixel {  //  RGB color values
  uint8_t r, g, b;
};

enum ColorIndex : uint8_t {  //  // Human-readable color index enum
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

/***************************************************************************************
  Globals
****************************************************************************************/
constexpr uint8_t NUM_LEDS = 12;
constexpr uint8_t NUM_BUTTONS = 16;

extern const buttonPixel colorTable[] PROGMEM;  //  Lookup table of RGB color definitions for LED feedback
extern const uint8_t discreteLEDs[4];           // Discrete output pins mapped to final 4 button functions

extern volatile bool updateLED;     // Flag from I2C input event
extern uint16_t button_state_bits;  // Bitfield for all buttons
extern uint16_t led_bits;           // Desired LED state
extern uint16_t prev_led_bits;      // Previous LED state (to avoid redundant updates)

extern ShiftIn<2> shift;  // 16-bit shift register interface

/***************************************************************************************
  Core Function Prototypes
****************************************************************************************/
void beginModule(uint8_t panel_addr);  //  Function for setup in main sketch
void handleRequestEvent();             //  I2C function, responds to master read request with 4-byte status report
void handleReceiveEvent();             //  I2C function, reacts to master sent LED state change request
void readButtonStates();               // Function to check for button state changes
void setInterrupt();                   // Set interrupt to incidate to I2C master
void clearInterrupt();                 // Clears interrupt

buttonPixel getColorFromTable(ColorIndex index);  // Helper to fetch RGB color struct from PROGMEM
buttonPixel overlayColor(bool overlayEnabled, bool modeActive, bool localActive, uint8_t colorIndex);

#endif  // BUTTON_MODULE_CORE_H

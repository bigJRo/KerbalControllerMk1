#ifndef JOYSTICK_MODULE_CORE_H
#define JOYSTICK_MODULE_CORE_H

/********************************************************************************************************************************
  Joystick Module Core for Kerbal Controller

  Handles common functions for all Joystick Modules for use in Kerbal Controller Mk1
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Arduino.h>       // Include arduino namespace
#include <Wire.h>          // I2C communication
#include <tinyNeoPixel.h>  // NeoPixel support for ATtiny series
#include <avr/pgmspace.h>  // PROGMEM access for ATtiny

/***************************************************************************************
  Pin Definitions (ATtiny816 w/ megaTinyCore Pin Mapping)
****************************************************************************************/

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
constexpr uint8_t NUM_LEDS = 2;
constexpr uint8_t NUM_BUTTONS = 3;

constexpr unsigned long debounceDelay = 20;
constexpr int16_t JOY_DEADZONE = 10;

extern const buttonPixel colorTable[] PROGMEM;  //  Lookup table of RGB color definitions for LED feedback

extern volatile bool updateLED;
extern uint8_t buttonBits;
extern uint8_t led_bits;
extern uint8_t prev_led_bits;
extern int16_t axis1, axis2, axis3;

/***************************************************************************************
  Core Function Prototypes
****************************************************************************************/
void beginJoystickModule(uint8_t address, uint8_t neopixelPin, const buttonPixel* colorTable, size_t colorCount);
void readJoystickInputs(uint8_t buttonPins[NUM_BUTTONS], uint8_t analogPins[3]);
void handleRequestEvent();                  //  I2C function, responds to master read request with 4-byte status report
void handleReceiveEvent(int howMany);  //  I2C function, reacts to master sent LED state change request
void setInterrupt();                        // Set interrupt to incidate to I2C master
void clearInterrupt();                      // Clears interrupt

buttonPixel getColorFromTable(ColorIndex index);  // Helper to fetch RGB color struct from PROGMEM
buttonPixel overlayColor(bool overlayEnabled, bool modeActive, bool localActive, uint8_t colorIndex);

#endif  //  JOYSTICK_MODULE_CORE_H

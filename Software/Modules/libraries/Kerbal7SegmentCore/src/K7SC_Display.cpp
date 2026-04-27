/**
 * @file        K7SC_Display.cpp
 * @version     1.1.0
 * @date        2026-04-26
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       MAX7219 7-segment display driver implementation.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include "K7SC_Display.h"

// ============================================================
//  State
// ============================================================

static uint16_t _currentValue  = 0;
static uint8_t  _intensity     = K7SC_MAX_INTENSITY;

// ============================================================
//  Port-mapped SPI pin helpers
//
//  CLK (PA7), DATA (PA6), and LOAD (PA5) are all on Port A.
//  Direct register access replaces digitalWrite() in the bit-bang
//  loop, reducing a 4-digit display update from ~250μs to ~25μs
//  at 10MHz. This matters for encoder responsiveness — fast spin
//  triggers 100-step jumps, each requiring a full display refresh.
//
//  PORTA.OUT is the output data register for Port A on the
//  ATtiny816 (megaTinyCore). Bit positions match pin numbers:
//    PA7 = bit 7 (CLK)
//    PA6 = bit 6 (DATA)
//    PA5 = bit 5 (LOAD)
//
//  PORTA.OUTSET / PORTA.OUTCLR are single-cycle atomic set/clear
//  registers — no read-modify-write needed, no race conditions.
// ============================================================

#define _SPI_CLK_HI()   (PORTA.OUTSET = PIN7_bm)
#define _SPI_CLK_LO()   (PORTA.OUTCLR = PIN7_bm)
#define _SPI_DATA_HI()  (PORTA.OUTSET = PIN6_bm)
#define _SPI_DATA_LO()  (PORTA.OUTCLR = PIN6_bm)
#define _SPI_LOAD_HI()  (PORTA.OUTSET = PIN5_bm)
#define _SPI_LOAD_LO()  (PORTA.OUTCLR = PIN5_bm)

// ============================================================
//  Software SPI — _spiSend()
//
//  Sends a 16-bit MAX7219 packet (8-bit register + 8-bit data)
//  MSB first. LOAD is held LOW for the duration of the transfer
//  and pulsed HIGH at the end to latch data into the MAX7219.
//
//  CLK idles LOW. Data is set up before the rising edge.
//  MAX7219 maximum clock frequency: 10MHz — well within budget
//  since direct port writes at 10MHz CPU give ~4 cycles per bit.
// ============================================================

static void _spiSend(uint8_t reg, uint8_t data) {
    uint16_t packet = ((uint16_t)reg << 8) | data;

    _SPI_LOAD_LO();

    for (int8_t bit = 15; bit >= 0; bit--) {
        _SPI_CLK_LO();
        if ((packet >> bit) & 0x01) {
            _SPI_DATA_HI();
        } else {
            _SPI_DATA_LO();
        }
        _SPI_CLK_HI();
    }

    _SPI_CLK_LO();   // return CLK to idle state before latching
    _SPI_LOAD_HI();
}

// ============================================================
//  _writeValue() — format value as 4 digits, no leading zeros
// ============================================================

static void _writeValue(uint16_t value) {
    if (value > K7SC_VALUE_MAX) value = K7SC_VALUE_MAX;

    // Decompose into BCD digits without repeated division.
    uint8_t d[4] = {0, 0, 0, 0};
    while (value >= 1000) { d[3]++; value -= 1000; }
    while (value >= 100)  { d[2]++; value -= 100;  }
    while (value >= 10)   { d[1]++; value -= 10;   }
    d[0] = (uint8_t)value;

    // Physical display wiring (from schematic KC-01-1881/1882 v2.0):
    //   MAX7219 DIG0 (reg 1) -> G4 = leftmost  physical digit = thousands
    //   MAX7219 DIG1 (reg 2) -> G3             physical digit = hundreds
    //   MAX7219 DIG2 (reg 3) -> G2             physical digit = tens
    //   MAX7219 DIG3 (reg 4) -> G1 = rightmost physical digit = units
    //
    // Verified by hardware diagnostic on KC-01-1882 v2.0.
    // Leading zero suppression: blank regs 1-3 from left; reg 4 always shown.
    bool leading = true;

    if (leading && d[3] == 0) {
        _spiSend(K7SC_MAX_REG_DIGIT0,     K7SC_MAX_BLANK);  // reg 1
    } else {
        _spiSend(K7SC_MAX_REG_DIGIT0,     d[3]);
        leading = false;
    }

    if (leading && d[2] == 0) {
        _spiSend(K7SC_MAX_REG_DIGIT0 + 1, K7SC_MAX_BLANK);  // reg 2
    } else {
        _spiSend(K7SC_MAX_REG_DIGIT0 + 1, d[2]);
        leading = false;
    }

    if (leading && d[1] == 0) {
        _spiSend(K7SC_MAX_REG_DIGIT0 + 2, K7SC_MAX_BLANK);  // reg 3
    } else {
        _spiSend(K7SC_MAX_REG_DIGIT0 + 2, d[1]);
        leading = false;
    }

    _spiSend(K7SC_MAX_REG_DIGIT0 + 3, d[0]);  // reg 4 = units, always shown
}

// ============================================================
//  displayBegin()
// ============================================================

void displayBegin() {
    pinMode(K7SC_PIN_CLK,  OUTPUT);
    pinMode(K7SC_PIN_LOAD, OUTPUT);
    pinMode(K7SC_PIN_DATA, OUTPUT);

    // Set initial pin states via direct register — CLK idle LOW,
    // LOAD idle HIGH (deasserted), DATA LOW
    _SPI_CLK_LO();
    _SPI_LOAD_HI();
    _SPI_DATA_LO();

    // MAX7219 initialisation sequence
    _spiSend(K7SC_MAX_REG_DISPLAYTEST, 0x00);  // Display test off
    _spiSend(K7SC_MAX_REG_SHUTDOWN,    0x01);  // Normal operation
    _spiSend(K7SC_MAX_REG_SCANLIMIT,   0x03);  // Scan digits 0-3
    _spiSend(K7SC_MAX_REG_DECODE,      0xFF);  // BCD decode all digits
    _spiSend(K7SC_MAX_REG_INTENSITY,   _intensity);

    // Show 0 on startup
    _currentValue = 0;
    _writeValue(0);
}

// ============================================================
//  displaySetValue()
// ============================================================

void displaySetValue(uint16_t value) {
    if (value > K7SC_VALUE_MAX) value = K7SC_VALUE_MAX;
    _currentValue = value;
    _writeValue(_currentValue);
}

// ============================================================
//  displayGetValue()
// ============================================================

uint16_t displayGetValue() {
    return _currentValue;
}

// ============================================================
//  displaySetIntensity()
// ============================================================

void displaySetIntensity(uint8_t intensity) {
    if (intensity > 15) intensity = 15;
    _intensity = intensity;
    _spiSend(K7SC_MAX_REG_INTENSITY, _intensity);
}

// ============================================================
//  displayTest() / displayTestEnd()
//  Turns on all segments. Call displayTestEnd() to restore.
//  Non-blocking — does not delay.
// ============================================================

void displayTest() {
    _spiSend(K7SC_MAX_REG_DISPLAYTEST, 0x01);
}

void displayTestEnd() {
    _spiSend(K7SC_MAX_REG_DISPLAYTEST, 0x00);
    _writeValue(_currentValue);
}

// ============================================================
//  displayBlank()
// ============================================================

void displayBlank() {
    for (uint8_t i = 0; i < 4; i++) {
        _spiSend(K7SC_MAX_REG_DIGIT0 + i, K7SC_MAX_BLANK);
    }
}

// ============================================================
//  displayRestore()
// ============================================================

void displayRestore() {
    _writeValue(_currentValue);
}

// ============================================================
//  displayShutdown()
// ============================================================

void displayShutdown() {
    _spiSend(K7SC_MAX_REG_SHUTDOWN, 0x00);
}

// ============================================================
//  displayWake()
// ============================================================

void displayWake() {
    // Write the correct value into digit registers BEFORE exiting shutdown.
    // This prevents a brief flash of stale register contents when the
    // MAX7219 comes out of shutdown mode.
    _writeValue(_currentValue);
    _spiSend(K7SC_MAX_REG_SHUTDOWN, 0x01);
}

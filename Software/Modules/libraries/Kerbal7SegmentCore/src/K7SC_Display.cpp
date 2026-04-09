/**
 * @file        K7SC_Display.cpp
 * @version     1.0.0
 * @date        2026-04-08
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
//  Software SPI helpers
// ============================================================

static void _spiSend(uint8_t reg, uint8_t data) {
    uint16_t packet = ((uint16_t)reg << 8) | data;

    digitalWrite(K7SC_PIN_LOAD, LOW);

    // Send 16 bits MSB first
    for (int8_t bit = 15; bit >= 0; bit--) {
        digitalWrite(K7SC_PIN_CLK, LOW);
        digitalWrite(K7SC_PIN_DATA, (packet >> bit) & 0x01 ? HIGH : LOW);
        digitalWrite(K7SC_PIN_CLK, HIGH);
    }

    digitalWrite(K7SC_PIN_LOAD, HIGH);
}

// ============================================================
//  _writeValue() — format value as 4 digits, no leading zeros
// ============================================================

static void _writeValue(uint16_t value) {
    // Clamp
    if (value > K7SC_VALUE_MAX) value = K7SC_VALUE_MAX;

    uint8_t d[4];
    d[3] = value / 1000;
    d[2] = (value % 1000) / 100;
    d[1] = (value % 100) / 10;
    d[0] = value % 10;

    // Suppress leading zeros using MAX7219 BCD blank code (0x0F)
    // Always show at least the units digit
    bool leading = true;
    for (int8_t i = 3; i >= 1; i--) {
        if (leading && d[i] == 0) {
            _spiSend(K7SC_MAX_REG_DIGIT0 + i, K7SC_MAX_BLANK);
        } else {
            leading = false;
            _spiSend(K7SC_MAX_REG_DIGIT0 + i, d[i]);
        }
    }
    // Units digit always shown
    _spiSend(K7SC_MAX_REG_DIGIT0, d[0]);
}

// ============================================================
//  displayBegin()
// ============================================================

void displayBegin() {
    pinMode(K7SC_PIN_CLK,  OUTPUT);
    pinMode(K7SC_PIN_LOAD, OUTPUT);
    pinMode(K7SC_PIN_DATA, OUTPUT);

    digitalWrite(K7SC_PIN_CLK,  LOW);
    digitalWrite(K7SC_PIN_LOAD, HIGH);
    digitalWrite(K7SC_PIN_DATA, LOW);

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
//  displayTest()
// ============================================================

void displayTest(uint16_t durationMs) {
    _spiSend(K7SC_MAX_REG_DISPLAYTEST, 0x01);
    delay(durationMs);
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
    _spiSend(K7SC_MAX_REG_SHUTDOWN, 0x01);
    _writeValue(_currentValue);
}

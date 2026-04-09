/**
 * @file        Buttons.cpp
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Throttle Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Button input and discrete LED output implementation.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include "Buttons.h"

// ============================================================
//  Pin tables
// ============================================================

static const uint8_t _btnPins[4] = {
    THR_PIN_THRTL_100,
    THR_PIN_THRTL_UP,
    THR_PIN_THRTL_DOWN,
    THR_PIN_THRTL_00
};

static const uint8_t _ledPins[4] = {
    THR_PIN_LED_100,
    THR_PIN_LED_UP,
    THR_PIN_LED_DOWN,
    THR_PIN_LED_00
};

// ============================================================
//  State
// ============================================================

static uint8_t _state              = 0;
static uint8_t _eventMask          = 0;
static uint8_t _changeMask         = 0;
static bool    _intPending         = false;
static uint8_t _debounceCount[4]   = {0};
static uint8_t _debounceCandidate  = 0;

// ============================================================
//  buttonsBegin()
// ============================================================

void buttonsBegin() {
    for (uint8_t i = 0; i < 4; i++) {
        pinMode(_btnPins[i], INPUT);   // active high, hardware pull-downs
        pinMode(_ledPins[i], OUTPUT);
        digitalWrite(_ledPins[i], LOW);
        _debounceCount[i] = 0;
    }
    _state             = 0;
    _eventMask         = 0;
    _changeMask        = 0;
    _intPending        = false;
    _debounceCandidate = 0;
}

// ============================================================
//  buttonsPoll()
// ============================================================

bool buttonsPoll() {
    uint8_t raw = 0;
    for (uint8_t i = 0; i < 4; i++) {
        if (digitalRead(_btnPins[i])) raw |= (1 << i);
    }

    if (raw != _debounceCandidate) {
        _debounceCandidate = raw;
        memset(_debounceCount, 0, sizeof(_debounceCount));
        return false;
    }

    bool anyEvent = false;
    for (uint8_t i = 0; i < 4; i++) {
        bool rawBit  = (raw >> i) & 0x01;
        bool liveBit = (_state >> i) & 0x01;

        if (rawBit != liveBit) {
            _debounceCount[i]++;
            if (_debounceCount[i] >= THR_DEBOUNCE_COUNT) {
                if (rawBit) {
                    _state      |=  (1 << i);
                    _eventMask  |=  (1 << i);
                    _changeMask |=  (1 << i);
                    _intPending  = true;
                    anyEvent     = true;
                } else {
                    _state      &= ~(1 << i);
                    _changeMask |=  (1 << i);
                    _intPending  = true;
                    anyEvent     = true;
                }
                _debounceCount[i] = 0;
            }
        } else {
            _debounceCount[i] = 0;
        }
    }
    return anyEvent;
}

// ============================================================
//  buttonsIsIntPending()
// ============================================================

bool buttonsIsIntPending() {
    return _intPending;
}

// ============================================================
//  buttonsGetEvents()
// ============================================================

uint8_t buttonsGetEvents() {
    uint8_t events  = _eventMask;
    _eventMask      = 0;
    _changeMask     = 0;
    _intPending     = false;
    return events;
}

// ============================================================
//  buttonsLEDsOn() / buttonsLEDsOff()
// ============================================================

void buttonsLEDsOn() {
    for (uint8_t i = 0; i < 4; i++) {
        digitalWrite(_ledPins[i], HIGH);
    }
}

void buttonsLEDsOff() {
    for (uint8_t i = 0; i < 4; i++) {
        digitalWrite(_ledPins[i], LOW);
    }
}

// ============================================================
//  buttonsClearAll()
// ============================================================

void buttonsClearAll() {
    _state             = 0;
    _eventMask         = 0;
    _changeMask        = 0;
    _intPending        = false;
    _debounceCandidate = 0;
    memset(_debounceCount, 0, sizeof(_debounceCount));
}

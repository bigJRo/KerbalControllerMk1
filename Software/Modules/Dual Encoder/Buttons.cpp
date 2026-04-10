/**
 * @file        Buttons.cpp
 * @version     1.1.0
 * @date        2026-04-09
 * @project     Kerbal Controller Mk1 — Dual Encoder Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Encoder pushbutton input implementation.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include "Buttons.h"

static const uint8_t _btnPins[2] = {
    DEC_PIN_ENC1_SW,
    DEC_PIN_ENC2_SW
};

static uint8_t _state                    = 0;
static uint8_t _latchedState             = 0;
static uint8_t _changeMask               = 0;
static uint8_t _eventMask                = 0;
static bool    _intPending               = false;
static uint8_t _debounceCount[2]         = {0};
static uint8_t _debounceCandidate[2]     = {0};

void buttonsBegin() {
    for (uint8_t i = 0; i < 2; i++) {
        pinMode(_btnPins[i], INPUT);  // active high, hardware pull-downs
        _debounceCount[i]     = 0;
        _debounceCandidate[i] = 0;
    }
    _state        = 0;
    _latchedState = 0;
    _changeMask   = 0;
    _eventMask    = 0;
    _intPending   = false;
}

bool buttonsPoll() {
    uint8_t raw = 0;
    for (uint8_t i = 0; i < 2; i++) {
        if (digitalRead(_btnPins[i])) raw |= (1 << i);
    }

    // Per-button debounce with independent candidate tracking.
    bool anyEvent = false;
    for (uint8_t i = 0; i < 2; i++) {
        bool rawBit  = (raw >> i) & 0x01;
        bool liveBit = (_state >> i) & 0x01;

        if (rawBit != _debounceCandidate[i]) {
            _debounceCandidate[i] = rawBit;
            _debounceCount[i]     = 0;
        } else if (rawBit != liveBit) {
            _debounceCount[i]++;
            if (_debounceCount[i] >= DEC_BTN_DEBOUNCE_COUNT) {
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

bool buttonsIsIntPending()   { return _intPending; }
uint8_t buttonsGetState()    { return _state; }
uint8_t buttonsGetEvents()   { return _eventMask; }

uint8_t buttonsGetChangeMask() {
    uint8_t mask  = _changeMask;
    _latchedState = _state;    // snapshot live state at read time

    _changeMask = 0;
    _eventMask  = 0;
    _intPending = false;

    // Re-assert if live state diverged during this read transaction.
    if (_state != _latchedState) {
        _changeMask = _state ^ _latchedState;
        _intPending = true;
    }

    return mask;
}

void buttonsClearAll() {
    _state        = 0;
    _latchedState = 0;
    _changeMask   = 0;
    _eventMask    = 0;
    _intPending   = false;
    memset(_debounceCount,     0, sizeof(_debounceCount));
    memset(_debounceCandidate, 0, sizeof(_debounceCandidate));
}

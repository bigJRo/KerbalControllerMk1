/**
 * @file        Switches.cpp
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Switch Panel Input Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Switch input implementation with dual-buffer latching.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include "Switches.h"

// ============================================================
//  Switch pin table — indexed 0-9 (SW1-SW10)
// ============================================================

static const uint8_t _swPins[SWP_SWITCH_COUNT] = {
    SWP_PIN_SW1,   // bit 0
    SWP_PIN_SW2,   // bit 1
    SWP_PIN_SW3,   // bit 2
    SWP_PIN_SW4,   // bit 3
    SWP_PIN_SW5,   // bit 4
    SWP_PIN_SW6,   // bit 5
    SWP_PIN_SW7,   // bit 6
    SWP_PIN_SW8,   // bit 7
    SWP_PIN_SW9,   // bit 8
    SWP_PIN_SW10   // bit 9
};

// ============================================================
//  Dual-buffer state
// ============================================================

static uint16_t _liveState    = 0;
static uint16_t _latchedState = 0;
static uint16_t _changeMask   = 0;
static bool     _intPending   = false;

// ============================================================
//  Debounce state
// ============================================================

static uint8_t  _debounceCount[SWP_SWITCH_COUNT]     = {0};
static uint16_t _debounceCandidate[SWP_SWITCH_COUNT] = {0};

// ============================================================
//  switchesBegin()
// ============================================================

void switchesBegin() {
    for (uint8_t i = 0; i < SWP_SWITCH_COUNT; i++) {
        pinMode(_swPins[i], INPUT);  // active high, hardware pull-downs
        _debounceCount[i] = 0;
    }
    _liveState    = 0;
    _latchedState = 0;
    _changeMask   = 0;
    _intPending   = false;
    memset(_debounceCount,     0, sizeof(_debounceCount));
    memset(_debounceCandidate, 0, sizeof(_debounceCandidate));
}

// ============================================================
//  switchesPoll()
// ============================================================

bool switchesPoll() {
    // Read all 10 switch pins into a 16-bit bitmask
    uint16_t raw = 0;
    for (uint8_t i = 0; i < SWP_SWITCH_COUNT; i++) {
        if (digitalRead(_swPins[i])) raw |= (1u << i);
    }

    // Per-switch debounce with independent candidate tracking.
    bool anyChanged = false;
    for (uint8_t i = 0; i < SWP_SWITCH_COUNT; i++) {
        bool rawBit  = (raw >> i) & 0x01;
        bool liveBit = (_liveState >> i) & 0x01;

        if (rawBit != _debounceCandidate[i]) {
            _debounceCandidate[i] = rawBit;
            _debounceCount[i]     = 0;
        } else if (rawBit != liveBit) {
            _debounceCount[i]++;
            if (_debounceCount[i] >= SWP_DEBOUNCE_COUNT) {
                if (rawBit)
                    _liveState |=  (1u << i);
                else
                    _liveState &= ~(1u << i);

                _changeMask |= (1u << i);
                _debounceCount[i] = 0;
                _intPending  = true;
                anyChanged   = true;
            }
        } else {
            _debounceCount[i] = 0;
        }
    }

    return anyChanged;
}

// ============================================================
//  switchesIsIntPending()
// ============================================================

bool switchesIsIntPending() {
    return _intPending;
}

// ============================================================
//  switchesGetPacket()
// ============================================================

void switchesGetPacket(uint8_t* buf) {
    // Snapshot live state into latch
    _latchedState = _liveState;

    // Build standard 4-byte KBC-style packet
    buf[0] = (uint8_t)(_latchedState >> 8);    // state HIGH (bits 15-8)
    buf[1] = (uint8_t)(_latchedState & 0xFF);  // state LOW  (bits 7-0)
    buf[2] = (uint8_t)(_changeMask >> 8);       // change HIGH
    buf[3] = (uint8_t)(_changeMask & 0xFF);     // change LOW

    // Clear change mask and INT flag
    _changeMask = 0;
    _intPending = false;

    // Re-assert if live state diverged during this read
    if (_liveState != _latchedState) {
        _changeMask = _liveState ^ _latchedState;
        _intPending = true;
    }
}

// ============================================================
//  switchesGetState()
// ============================================================

uint16_t switchesGetState() {
    return _liveState;
}

// ============================================================
//  switchesClearAll()
// ============================================================

void switchesClearAll() {
    _liveState    = 0;
    _latchedState = 0;
    _changeMask   = 0;
    _intPending   = false;
    memset(_debounceCount,     0, sizeof(_debounceCount));
    memset(_debounceCandidate, 0, sizeof(_debounceCandidate));
}

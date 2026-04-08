/**
 * @file        Buttons.cpp
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — EVA Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Implementation of direct GPIO button input for the
 *              EVA module. Provides debounce and dual-buffer latching
 *              consistent with the KBC button protocol.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include "Buttons.h"

// ============================================================
//  Button pin table
// ============================================================

static const uint8_t _btnPins[EVA_BUTTON_COUNT] = EVA_BTN_PINS;

// ============================================================
//  Dual-buffer state
// ============================================================

static uint8_t  _liveState      = 0;   // current debounced state (bit per button)
static uint8_t  _latchedState   = 0;   // snapshot sent on last read
static uint8_t  _changeMask     = 0;   // accumulated changes since last read
static bool     _intPending     = false;

// ============================================================
//  Debounce state
// ============================================================

static uint8_t  _debounceCount[EVA_BUTTON_COUNT] = {0};
static uint8_t  _debounceCandidate = 0;

// ============================================================
//  buttonsBegin()
// ============================================================

void buttonsBegin() {
    for (uint8_t i = 0; i < EVA_BUTTON_COUNT; i++) {
        pinMode(_btnPins[i], INPUT);  // active high, hardware pull-downs fitted
        _debounceCount[i] = 0;
    }
    _liveState         = 0;
    _latchedState      = 0;
    _changeMask        = 0;
    _intPending        = false;
    _debounceCandidate = 0;
}

// ============================================================
//  buttonsPoll()
// ============================================================

bool buttonsPoll() {
    // Read all 6 button pins into a bitmask
    uint8_t raw = 0;
    for (uint8_t i = 0; i < EVA_BUTTON_COUNT; i++) {
        if (digitalRead(_btnPins[i])) {
            raw |= (1 << i);
        }
    }

    // If raw reading changed from candidate, reset debounce
    if (raw != _debounceCandidate) {
        _debounceCandidate = raw;
        memset(_debounceCount, 0, sizeof(_debounceCount));
        return false;
    }

    // Increment per-button counters for transitioning buttons
    bool anyChanged = false;
    for (uint8_t i = 0; i < EVA_BUTTON_COUNT; i++) {
        bool rawBit  = (raw >> i) & 0x01;
        bool liveBit = (_liveState >> i) & 0x01;

        if (rawBit != liveBit) {
            _debounceCount[i]++;
            if (_debounceCount[i] >= EVA_DEBOUNCE_COUNT) {
                // Commit state change
                if (rawBit)
                    _liveState |=  (1 << i);
                else
                    _liveState &= ~(1 << i);

                _changeMask |= (1 << i);
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
//  buttonsIsIntPending()
// ============================================================

bool buttonsIsIntPending() {
    return _intPending;
}

// ============================================================
//  buttonsGetPacket()
// ============================================================

void buttonsGetPacket(uint8_t* buf) {
    // Snapshot live state into latch
    _latchedState = _liveState;

    // Build 4-byte EVA packet
    buf[0] = _latchedState;      // button state (bits 0-5 active)
    buf[1] = _changeMask;        // change mask  (bits 0-5 active)
    buf[2] = 0;                  // ENC1 delta — injected by I2C.cpp
    buf[3] = 0;                  // ENC2 delta — injected by I2C.cpp

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
//  buttonsGetState()
// ============================================================

bool buttonsGetState(uint8_t index) {
    if (index >= EVA_BUTTON_COUNT) return false;
    return (_liveState >> index) & 0x01;
}

// ============================================================
//  buttonsClearAll()
// ============================================================

void buttonsClearAll() {
    _liveState    = 0;
    _latchedState = 0;
    _changeMask   = 0;
    _intPending   = false;
    memset(_debounceCount, 0, sizeof(_debounceCount));
}

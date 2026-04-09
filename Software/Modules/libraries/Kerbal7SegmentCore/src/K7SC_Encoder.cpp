/**
 * @file        K7SC_Encoder.cpp
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Rotary encoder implementation with acceleration.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include "K7SC_Encoder.h"
#include "K7SC_Display.h"

// ============================================================
//  State
// ============================================================

static uint8_t  _lastAB         = 0;
static uint32_t _lastClickTime  = 0;
static bool     _valueChanged   = false;

// Quadrature state transition table
// Index: (lastAB << 2) | currentAB
// Value: +1 = clockwise, -1 = counter-clockwise, 0 = invalid
static const int8_t _qTable[16] = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0
};

// ============================================================
//  _getStepSize() — acceleration based on click interval
// ============================================================

static uint16_t _getStepSize(uint32_t now) {
    uint32_t elapsed = now - _lastClickTime;
    if (elapsed < K7SC_ENC_FAST_MS) {
        return K7SC_STEP_FAST;
    } else if (elapsed < K7SC_ENC_SLOW_MS) {
        return K7SC_STEP_MEDIUM;
    } else {
        return K7SC_STEP_SLOW;
    }
}

// ============================================================
//  encoderBegin()
// ============================================================

void encoderBegin(uint16_t initialValue) {
    // Encoder channels have hardware pull-ups (R1/R2 10k)
    // and RC debounce caps (C1/C2 10nF) — INPUT only needed
    pinMode(K7SC_PIN_ENC_A, INPUT);
    pinMode(K7SC_PIN_ENC_B, INPUT);

    // Read initial state
    uint8_t a = digitalRead(K7SC_PIN_ENC_A) ? 1 : 0;
    uint8_t b = digitalRead(K7SC_PIN_ENC_B) ? 1 : 0;
    _lastAB = (a << 1) | b;

    _lastClickTime = millis();
    _valueChanged  = false;

    // Set initial display value
    uint16_t clamped = initialValue;
    if (clamped > K7SC_VALUE_MAX) clamped = K7SC_VALUE_MAX;
    displaySetValue(clamped);
}

// ============================================================
//  encoderPoll()
// ============================================================

bool encoderPoll() {
    uint32_t now = millis();

    uint8_t a = digitalRead(K7SC_PIN_ENC_A) ? 1 : 0;
    uint8_t b = digitalRead(K7SC_PIN_ENC_B) ? 1 : 0;
    uint8_t currentAB = (a << 1) | b;

    if (currentAB == _lastAB) return false;

    // Software debounce guard — hardware RC helps but we still
    // enforce a minimum interval to reject any residual glitches
    if ((now - _lastClickTime) < K7SC_ENC_DEBOUNCE_MS) {
        return false;
    }

    // Look up direction from quadrature table
    int8_t dir = _qTable[(_lastAB << 2) | currentAB];
    _lastAB = currentAB;

    if (dir == 0) return false;  // invalid transition

    // Determine step size from click interval
    uint16_t step = _getStepSize(now);
    _lastClickTime = now;

    // Apply step with clamping
    uint16_t current = displayGetValue();
    if (dir > 0) {
        // Clockwise — increment
        uint16_t next = current + step;
        if (next > K7SC_VALUE_MAX || next < current) {
            // Overflow or exceeded max
            next = K7SC_VALUE_MAX;
        }
        displaySetValue(next);
    } else {
        // Counter-clockwise — decrement
        if (step > current) {
            displaySetValue(K7SC_VALUE_MIN);
        } else {
            displaySetValue(current - step);
        }
    }

    _valueChanged = true;
    return true;
}

// ============================================================
//  encoderIsValueChanged()
// ============================================================

bool encoderIsValueChanged() {
    return _valueChanged;
}

// ============================================================
//  encoderClearChanged()
// ============================================================

void encoderClearChanged() {
    _valueChanged = false;
}

// ============================================================
//  encoderSetValue()
// ============================================================

void encoderSetValue(uint16_t value) {
    if (value > K7SC_VALUE_MAX) value = K7SC_VALUE_MAX;
    displaySetValue(value);
    _valueChanged = true;
}

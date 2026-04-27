/**
 * @file        K7SC_Encoder.cpp
 * @version     1.1.0
 * @date        2026-04-26
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Rotary encoder implementation with interrupt-driven
 *              edge detection and count-based acceleration.
 *
 *              ENC_A (PB4) triggers a rising-edge pin change interrupt.
 *              ENC_B (PB5) is sampled inside the ISR for direction.
 *              This ensures no edge is missed regardless of loop speed.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include "K7SC_Encoder.h"
#include "K7SC_Display.h"

// ============================================================
//  ISR-shared state  (volatile — written in ISR, read in poll)
// ============================================================

static volatile int16_t  _pendingDelta = 0;  // net clicks queued by ISR
static volatile int8_t   _isrLastDir   = 0;  // direction seen in ISR
static volatile uint8_t  _isrRunCount  = 0;  // consecutive same-dir count

// ============================================================
//  Poll-side state
// ============================================================

static bool    _valueChanged = false;

// ============================================================
//  _stepFromCount() — map consecutive click count to step size
// ============================================================

static uint16_t _stepFromCount(uint8_t count) {
    if      (count >= K7SC_ENC_TURBO_COUNT)  return K7SC_STEP_TURBO;
    else if (count >= K7SC_ENC_FAST_COUNT)   return K7SC_STEP_FAST;
    else if (count >= K7SC_ENC_MEDIUM_COUNT) return K7SC_STEP_MEDIUM;
    else                                      return K7SC_STEP_SLOW;
}

// ============================================================
//  ISR — fires on every rising edge of ENC_A (PB4)
//
//  ENC_B (PB5) is sampled immediately — it is stable at the
//  moment A rises on the PEC11R-4220F-S0024.
//  Hardware RC caps (C1/C2 10nF) suppress contact bounce.
//
//  We accumulate a signed _pendingDelta rather than applying
//  the step directly — displaySetValue() must not be called
//  from an ISR context.
// ============================================================

ISR(PORTB_PORT_vect)
{
    // Acknowledge the interrupt by clearing the flag
    PORTB.INTFLAGS = PIN4_bm;

    // Only handle rising edge (ISC configured for rising, but
    // guard anyway in case of spurious flags)
    if (!(PORTB.IN & PIN4_bm)) return;

    // Read ENC_B direction
    int8_t dir = (PORTB.IN & PIN5_bm) ? -1 : +1;

    // Update run count
    if (dir == _isrLastDir) {
        if (_isrRunCount < 255) _isrRunCount++;
    } else {
        _isrRunCount = 1;
        _isrLastDir  = dir;
    }

    // Accumulate delta — step applied in encoderPoll()
    _pendingDelta += dir;
}

// ============================================================
//  encoderBegin()
// ============================================================

void encoderBegin(uint16_t initialValue) {
    // Pins have hardware pull-ups (R1/R2 10k) and RC debounce
    // caps (C1/C2 10nF) — INPUT only needed
    pinMode(K7SC_PIN_ENC_A, INPUT);
    pinMode(K7SC_PIN_ENC_B, INPUT);

    // Configure rising-edge interrupt on ENC_A (PB4)
    PORTB.PIN4CTRL = PORT_ISC_RISING_gc;

    // Reset state
    _pendingDelta = 0;
    _isrLastDir   = 0;
    _isrRunCount  = 0;
    _valueChanged = false;

    uint16_t clamped = initialValue;
    if (clamped > K7SC_VALUE_MAX) clamped = K7SC_VALUE_MAX;
    displaySetValue(clamped);

    sei();  // ensure global interrupts are enabled
}

// ============================================================
//  encoderPoll()
//
//  Called from loop(). Drains _pendingDelta one click at a time,
//  applying the appropriate step size for each click based on
//  the run count accumulated in the ISR.
// ============================================================

bool encoderPoll() {
    // Atomically snapshot and clear pending delta
    noInterrupts();
    int16_t  delta    = _pendingDelta;
    uint8_t  runCount = _isrRunCount;
    _pendingDelta = 0;
    interrupts();

    if (delta == 0) return false;

    // Determine direction of the batch
    int8_t dir = (delta > 0) ? +1 : -1;

    // Step size from the run count accumulated in the ISR
    uint16_t step = _stepFromCount(runCount);

    // Apply total movement: abs(delta) clicks × step
    uint16_t moves    = (delta > 0) ? (uint16_t)delta : (uint16_t)(-delta);
    uint32_t total    = (uint32_t)moves * step;
    uint16_t current  = displayGetValue();

    if (dir > 0) {
        uint32_t next = (uint32_t)current + total;
        displaySetValue(next > K7SC_VALUE_MAX ? K7SC_VALUE_MAX : (uint16_t)next);
    } else {
        if (total >= current) {
            displaySetValue(K7SC_VALUE_MIN);
        } else {
            displaySetValue(current - (uint16_t)total);
        }
    }

    _valueChanged = true;
    return true;
}

// ============================================================
//  encoderIsValueChanged() / encoderClearChanged()
// ============================================================

bool encoderIsValueChanged() { return _valueChanged; }
void encoderClearChanged()   { _valueChanged = false; }

// ============================================================
//  encoderSetValue()
// ============================================================

void encoderSetValue(uint16_t value) {
    if (value > K7SC_VALUE_MAX) value = K7SC_VALUE_MAX;
    displaySetValue(value);
    _valueChanged = true;
}

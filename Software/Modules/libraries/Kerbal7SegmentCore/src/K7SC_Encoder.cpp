/**
 * @file        K7SC_Encoder.cpp
 * @version     2.0.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Rotary encoder driver — reports raw delta to inputState.
 *              No acceleration, no value tracking, no display coupling.
 *              All application logic belongs in the sketch.
 *
 * @license     GNU General Public License v3.0
 */

#include <Arduino.h>
#include "K7SC_Encoder.h"
#include "K7SC_State.h"

// ── ISR-shared state ──────────────────────────────────────────
static volatile int16_t _pendingDelta = 0;

// ── ISR — rising edge on ENC_A (PB4) ─────────────────────────
ISR(PORTB_PORT_vect) {
    PORTB.INTFLAGS = PIN4_bm;
    if (!(PORTB.IN & PIN4_bm)) return;
    int8_t dir = (PORTB.IN & PIN5_bm) ? -1 : +1;
    _pendingDelta += dir;
}

// ── encoderBegin() ────────────────────────────────────────────
void encoderBegin() {
    pinMode(K7SC_PIN_ENC_A, INPUT);
    pinMode(K7SC_PIN_ENC_B, INPUT);
    PORTB.PIN4CTRL = PORT_ISC_RISING_gc;
    _pendingDelta = 0;
    sei();
}

// ── encoderPoll() ─────────────────────────────────────────────
void encoderPoll() {
    noInterrupts();
    int16_t delta = _pendingDelta;
    _pendingDelta = 0;
    interrupts();

    if (delta == 0) return;

    inputState.encoderDelta   += delta;
    inputState.encoderChanged  = true;
}

// ── encoderClearDelta() ───────────────────────────────────────
void encoderClearDelta() {
    noInterrupts();
    _pendingDelta = 0;
    interrupts();
    inputState.encoderDelta  = 0;
    inputState.encoderChanged = false;
}

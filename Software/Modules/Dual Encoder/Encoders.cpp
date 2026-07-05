/**
 * @file        Encoders.cpp
 * @version     2.0
 * @date        2026-07-05
 * @project     Kerbal Controller Mk1 — Dual Encoder Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Quadrature encoder decoding implementation.
 *              1 detent = ±1 delta, no acceleration.
 *
 *              Decoding is interrupt-driven: a rising-edge pin interrupt on
 *              each encoder's channel A samples channel B to determine
 *              direction (B low = CW/+, B high = CCW/-). A detented
 *              PEC11R-4220F-S0024 produces exactly one channel-A rising edge
 *              per detent, so this yields ±1 per click and — unlike a polled
 *              decoder — cannot alias or reverse direction at high turn speed.
 *              Hardware RC debounce (10nF on A/B) suppresses contact bounce.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include "Encoders.h"

// ============================================================
//  ISR-shared state
//  _delta accumulates signed clicks between reads; it is read
//  (clamped to int8) and cleared by the I2C packet builder.
// ============================================================

static volatile int16_t _delta[2]   = {0, 0};
static volatile bool     _dataPending = false;

// ============================================================
//  Pin-change ISRs — rising edge on each channel A
//
//  Pins are fixed by the KC-01-1862 v2.0 schematic (see Config.h):
//    ENC1_A = PC3, ENC1_B = PC2   -> PORTC vector
//    ENC2_A = PA7, ENC2_B = PB5   -> PORTA vector
//  Each ISR clears only its own flag and confirms the sampled edge is
//  really high before counting, so residual glitches are ignored.
// ============================================================

ISR(PORTC_PORT_vect) {
    if (PORTC.INTFLAGS & PIN3_bm) {              // ENC1_A (PC3)
        PORTC.INTFLAGS = PIN3_bm;                // clear the flag
        if (PORTC.IN & PIN3_bm) {                // confirm real rising edge
            _delta[0]   += (PORTC.IN & PIN2_bm) ? -1 : +1;   // ENC1_B (PC2)
            _dataPending = true;
        }
    }
}

ISR(PORTA_PORT_vect) {
    if (PORTA.INTFLAGS & PIN7_bm) {              // ENC2_A (PA7)
        PORTA.INTFLAGS = PIN7_bm;                // clear the flag
        if (PORTA.IN & PIN7_bm) {                // confirm real rising edge
            _delta[1]   += (PORTB.IN & PIN5_bm) ? -1 : +1;   // ENC2_B (PB5)
            _dataPending = true;
        }
    }
}

// ============================================================
//  Helpers
// ============================================================

static int8_t _clamp8(int16_t v) {
    if (v > INT8_MAX) return INT8_MAX;
    if (v < INT8_MIN) return INT8_MIN;
    return (int8_t)v;
}

// ============================================================
//  encodersBegin()
// ============================================================

void encodersBegin() {
    pinMode(DEC_PIN_ENC1_A, INPUT);
    pinMode(DEC_PIN_ENC1_B, INPUT);
    pinMode(DEC_PIN_ENC2_A, INPUT);
    pinMode(DEC_PIN_ENC2_B, INPUT);

    // Enable a rising-edge interrupt on each channel A. pinMode() is called
    // first because it rewrites PINnCTRL; the interrupt sense is programmed
    // after so it sticks.
    PORTC.PIN3CTRL = PORT_ISC_RISING_gc;         // ENC1_A (PC3)
    PORTA.PIN7CTRL = PORT_ISC_RISING_gc;         // ENC2_A (PA7)

    _delta[0]    = 0;
    _delta[1]    = 0;
    _dataPending = false;

    sei();
}

// ============================================================
//  encodersPoll()
//
//  Decoding happens in the ISRs; there is nothing to poll. Kept so the
//  main loop's call site is unchanged. Returns whether movement has been
//  accumulated since the last read.
// ============================================================

bool encodersPoll() {
    return _dataPending;
}

// ============================================================
//  encodersIsDataPending()
// ============================================================

bool encodersIsDataPending() {
    return _dataPending;
}

// ============================================================
//  encodersGetDelta1() / encodersGetDelta2()
//  16-bit reads are non-atomic on this 8-bit MCU, so guard against an
//  encoder ISR updating the value mid-read. SREG is saved and restored
//  (rather than using sei()) because these run inside the I2C packet
//  builder, itself an ISR — the guard must never force interrupts back on.
// ============================================================

int8_t encodersGetDelta1() {
    uint8_t s = SREG; cli();
    int8_t d = _clamp8(_delta[0]);
    SREG = s;
    return d;
}

int8_t encodersGetDelta2() {
    uint8_t s = SREG; cli();
    int8_t d = _clamp8(_delta[1]);
    SREG = s;
    return d;
}

// ============================================================
//  encodersClearDeltas()
// ============================================================

void encodersClearDeltas() {
    uint8_t s = SREG; cli();
    _delta[0]    = 0;
    _delta[1]    = 0;
    _dataPending = false;
    SREG = s;
}

// ============================================================
//  encodersReset()
// ============================================================

void encodersReset() {
    encodersClearDeltas();
}

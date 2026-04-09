/**
 * @file        Encoders.cpp
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Dual Encoder Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Quadrature encoder decoding implementation.
 *              1 click = ±1 delta, no acceleration.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include "Encoders.h"

// ============================================================
//  Quadrature state transition table
//  Index: (lastAB << 2) | currentAB
//  Value: +1=CW, -1=CCW, 0=invalid/no movement
// ============================================================

static const int8_t _qTable[16] = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0
};

// ============================================================
//  Per-encoder state
// ============================================================

struct EncoderState {
    uint8_t  pin_a;
    uint8_t  pin_b;
    uint8_t  lastAB;
    uint32_t lastClickTime;
    volatile int8_t delta;
};

static EncoderState _enc[2];
static bool _dataPending = false;

// ============================================================
//  _pollOne() — poll a single encoder
// ============================================================

static bool _pollOne(EncoderState& enc) {
    uint8_t a = digitalRead(enc.pin_a) ? 1 : 0;
    uint8_t b = digitalRead(enc.pin_b) ? 1 : 0;
    uint8_t currentAB = (a << 1) | b;

    if (currentAB == enc.lastAB) return false;

    // Software debounce guard — hardware RC does most of the work
    uint32_t now = millis();
    if ((now - enc.lastClickTime) < DEC_ENC_DEBOUNCE_MS) {
        return false;
    }

    // Look up direction
    int8_t dir = _qTable[(enc.lastAB << 2) | currentAB];
    enc.lastAB = currentAB;

    if (dir == 0) return false;  // invalid transition

    // Accumulate delta — clamp to int8 limits to prevent overflow
    enc.lastClickTime = now;
    int16_t next = (int16_t)enc.delta + dir;
    if (next > INT8_MAX) next = INT8_MAX;
    if (next < INT8_MIN) next = INT8_MIN;
    enc.delta = (int8_t)next;

    return true;
}

// ============================================================
//  encodersBegin()
// ============================================================

void encodersBegin() {
    // ENC1
    _enc[0].pin_a        = DEC_PIN_ENC1_A;
    _enc[0].pin_b        = DEC_PIN_ENC1_B;
    _enc[0].delta        = 0;
    _enc[0].lastClickTime = 0;

    // ENC2
    _enc[1].pin_a        = DEC_PIN_ENC2_A;
    _enc[1].pin_b        = DEC_PIN_ENC2_B;
    _enc[1].delta        = 0;
    _enc[1].lastClickTime = 0;

    // Configure pins — hardware pull-ups fitted, no INPUT_PULLUP needed
    for (uint8_t i = 0; i < 2; i++) {
        pinMode(_enc[i].pin_a, INPUT);
        pinMode(_enc[i].pin_b, INPUT);

        // Read initial state
        uint8_t a = digitalRead(_enc[i].pin_a) ? 1 : 0;
        uint8_t b = digitalRead(_enc[i].pin_b) ? 1 : 0;
        _enc[i].lastAB = (a << 1) | b;
    }

    _dataPending = false;
}

// ============================================================
//  encodersPoll()
// ============================================================

bool encodersPoll() {
    bool moved = false;
    for (uint8_t i = 0; i < 2; i++) {
        if (_pollOne(_enc[i])) {
            moved       = true;
            _dataPending = true;
        }
    }
    return moved;
}

// ============================================================
//  encodersIsDataPending()
// ============================================================

bool encodersIsDataPending() {
    return _dataPending;
}

// ============================================================
//  encodersGetDelta1() / encodersGetDelta2()
// ============================================================

int8_t encodersGetDelta1() { return _enc[0].delta; }
int8_t encodersGetDelta2() { return _enc[1].delta; }

// ============================================================
//  encodersClearDeltas()
// ============================================================

void encodersClearDeltas() {
    _enc[0].delta = 0;
    _enc[1].delta = 0;
    _dataPending  = false;
}

// ============================================================
//  encodersReset()
// ============================================================

void encodersReset() {
    encodersClearDeltas();
}

/**
 * @file        Wiper.cpp
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Throttle Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Potentiometer wiper and touch sensor implementation.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include "Wiper.h"

// ============================================================
//  State
// ============================================================

static uint16_t _rawADC        = 0;
static uint16_t _lastRawADC    = 0;
static uint16_t _scaledOutput  = 0;
static bool     _touched       = false;
static bool     _valueChanged  = false;

// Precision mode
static bool     _precision     = false;
static uint16_t _anchor        = 0;   // output value at precision entry

// ============================================================
//  _scale() — convert raw ADC to throttle output
// ============================================================

static uint16_t _scale(uint16_t raw) {
    if (!_precision) {
        // Normal mode: full ADC range → 0 to INT16_MAX
        return (uint16_t)map((long)raw, 0L, 1023L, 0L, (long)INT16_MAX);
    } else {
        // Precision mode: full ADC range → anchor ± THR_PRECISION_RANGE
        // Physical center (512) = anchor
        int16_t delta = (int16_t)raw - (int16_t)THR_ADC_CENTER;

        // Map delta from ADC space to precision output space
        // delta range: -512 to +511
        // output delta range: -THR_PRECISION_RANGE to +THR_PRECISION_RANGE
        int32_t outputDelta = map((long)delta,
                                  -512L, 511L,
                                  -(long)THR_PRECISION_RANGE,
                                  (long)THR_PRECISION_RANGE);

        int32_t output = (int32_t)_anchor + outputDelta;

        // Clamp to valid range
        if (output < 0)           output = 0;
        if (output > INT16_MAX)   output = INT16_MAX;

        return (uint16_t)output;
    }
}

// ============================================================
//  wiperBegin()
// ============================================================

void wiperBegin() {
    pinMode(THR_PIN_WIPER,     INPUT);
    pinMode(THR_PIN_TOUCH_IND, INPUT);

    _rawADC       = (uint16_t)analogRead(THR_PIN_WIPER);
    _lastRawADC   = _rawADC;
    _scaledOutput = _scale(_rawADC);
    _touched      = false;
    _valueChanged = false;
    _precision    = false;
    _anchor       = 0;
}

// ============================================================
//  wiperPoll()
// ============================================================

bool wiperPoll() {
    // Read touch sensor
    _touched = digitalRead(THR_PIN_TOUCH_IND);

    // Read ADC
    uint16_t raw = (uint16_t)analogRead(THR_PIN_WIPER);
    _rawADC = raw;

    // Apply change threshold — ignore tiny fluctuations
    uint16_t absChange = (raw > _lastRawADC)
                       ? (raw - _lastRawADC)
                       : (_lastRawADC - raw);

    if (absChange < THR_WIPER_CHANGE_MIN) {
        return false;
    }

    // Scale to output
    uint16_t newScaled = _scale(raw);
    if (newScaled != _scaledOutput) {
        _scaledOutput  = newScaled;
        _lastRawADC    = raw;
        _valueChanged  = true;
        return true;
    }

    _lastRawADC = raw;
    return false;
}

// ============================================================
//  Accessors
// ============================================================

uint16_t wiperGetRaw() {
    return _rawADC;
}

uint16_t wiperGetScaled() {
    return _scaledOutput;
}

bool wiperIsTouched() {
    return _touched;
}

bool wiperIsValueChanged() {
    return _valueChanged;
}

void wiperClearChanged() {
    _valueChanged = false;
}

// ============================================================
//  wiperEnterPrecision()
// ============================================================

void wiperEnterPrecision(uint16_t currentOutput) {
    _anchor    = currentOutput;
    _precision = true;
}

// ============================================================
//  wiperExitPrecision()
// ============================================================

void wiperExitPrecision() {
    _precision    = false;
    // _scaledOutput already reflects the current precision value
    // which becomes the new normal mode position
    _valueChanged = true;
}

// ============================================================
//  wiperIsPrecision()
// ============================================================

bool wiperIsPrecision() {
    return _precision;
}

// ============================================================
//  wiperReset()
// ============================================================

void wiperReset() {
    _valueChanged = false;
    _precision    = false;
    _anchor       = 0;
}

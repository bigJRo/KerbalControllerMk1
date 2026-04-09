/**
 * @file        KJC_ADC.cpp
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Implementation of analog axis reading for the
 *              KerbalJoystickCore library.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include "KJC_ADC.h"

// ============================================================
//  Axis pin table
// ============================================================

static const uint8_t _axisPins[KJC_AXIS_COUNT] = {
    KJC_PIN_AXIS1,
    KJC_PIN_AXIS2,
    KJC_PIN_AXIS3
};

// ============================================================
//  Per-axis state
// ============================================================

static uint16_t _center[KJC_AXIS_COUNT]         = {512, 512, 512};
static uint16_t _lastSentRaw[KJC_AXIS_COUNT]     = {512, 512, 512};
static int16_t  _lastSentScaled[KJC_AXIS_COUNT]  = {0, 0, 0};
static bool     _dataPending                     = false;

// ============================================================
//  _readRaw() — single ADC reading
// ============================================================

static uint16_t _readRaw(uint8_t pin) {
    return (uint16_t)analogRead(pin);
}

// ============================================================
//  _calibrateAxis() — average KJC_CALIBRATION_SAMPLES reads
// ============================================================

static uint16_t _calibrateAxis(uint8_t pin) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < KJC_CALIBRATION_SAMPLES; i++) {
        sum += _readRaw(pin);
        delay(5);  // brief settling time between samples
    }
    return (uint16_t)(sum / KJC_CALIBRATION_SAMPLES);
}

// ============================================================
//  _processAxis() — full pipeline for one axis
//
//  Returns scaled INT16 value if reportable, KJC_AXIS_SUPPRESS
//  if the value should not trigger an update.
//
//  All threshold comparisons in raw ADC counts.
//  map() called only after both filters pass.
// ============================================================

static int16_t _processAxis(uint8_t axisIdx) {
    uint16_t raw    = _readRaw(_axisPins[axisIdx]);
    uint16_t center = _center[axisIdx];

    // Step 1: Deadzone — in raw ADC counts
    int16_t delta = (int16_t)raw - (int16_t)center;
    if (abs(delta) <= KJC_DEADZONE) {
        // Inside deadzone — report zero only if last sent was non-zero
        if (_lastSentRaw[axisIdx] != center) {
            _lastSentRaw[axisIdx]    = center;
            _lastSentScaled[axisIdx] = 0;
            return 0;
        }
        // Already at zero — suppress
        return KJC_AXIS_SUPPRESS;
    }

    // Step 2: Change threshold — in raw ADC counts
    int16_t rawDelta = (int16_t)raw - (int16_t)_lastSentRaw[axisIdx];
    if (abs(rawDelta) <= KJC_CHANGE_THRESHOLD) {
        return KJC_AXIS_SUPPRESS;
    }

    // Step 3: Both filters passed — apply split map to INT16
    int16_t scaled;
    if (raw >= center) {
        scaled = (int16_t)map((long)raw, (long)center, 1023L, 0L, 32767L);
    } else {
        scaled = (int16_t)map((long)raw, 0L, (long)center, -32768L, 0L);
    }

    // Update last-sent tracking
    _lastSentRaw[axisIdx]    = raw;
    _lastSentScaled[axisIdx] = scaled;

    return scaled;
}

// ============================================================
//  adcBegin()
// ============================================================

void adcBegin() {
    // Configure pins as analog inputs
    for (uint8_t i = 0; i < KJC_AXIS_COUNT; i++) {
        pinMode(_axisPins[i], INPUT);
    }

    // Startup calibration — average reads to find center
    for (uint8_t i = 0; i < KJC_AXIS_COUNT; i++) {
        _center[i]         = _calibrateAxis(_axisPins[i]);
        _lastSentRaw[i]    = _center[i];
        _lastSentScaled[i] = 0;
    }

    _dataPending = false;
}

// ============================================================
//  adcPoll()
// ============================================================

bool adcPoll() {
    bool anyChanged = false;

    for (uint8_t i = 0; i < KJC_AXIS_COUNT; i++) {
        int16_t result = _processAxis(i);
        if (result != KJC_AXIS_SUPPRESS) {
            anyChanged   = true;
            _dataPending = true;
        }
    }

    return anyChanged;
}

// ============================================================
//  adcIsDataPending()
// ============================================================

bool adcIsDataPending() {
    return _dataPending;
}

// ============================================================
//  adcClearPending()
// ============================================================

void adcClearPending() {
    _dataPending = false;
}

// ============================================================
//  adcGetScaled()
// ============================================================

int16_t adcGetScaled(uint8_t axis) {
    if (axis >= KJC_AXIS_COUNT) return 0;
    return _lastSentScaled[axis];
}

// ============================================================
//  adcGetCenter()
// ============================================================

uint16_t adcGetCenter(uint8_t axis) {
    if (axis >= KJC_AXIS_COUNT) return 512;
    return _center[axis];
}

// ============================================================
//  adcClearAll()
// ============================================================

void adcClearAll() {
    for (uint8_t i = 0; i < KJC_AXIS_COUNT; i++) {
        _lastSentRaw[i]    = _center[i];
        _lastSentScaled[i] = 0;
    }
    _dataPending = false;
}

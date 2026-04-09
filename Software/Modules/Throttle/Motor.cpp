/**
 * @file        Motor.cpp
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Throttle Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       L293D H-bridge motor control implementation.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include "Motor.h"

// ============================================================
//  State
// ============================================================

static uint16_t _target     = 0;
static bool     _hardLimit  = true;
static bool     _moving     = false;
static bool     _seeking    = false;

// ============================================================
//  motorBegin()
// ============================================================

void motorBegin() {
    pinMode(THR_PIN_MTR_FWD, OUTPUT);
    pinMode(THR_PIN_MTR_REV, OUTPUT);
    pinMode(THR_PIN_SPEED,   OUTPUT);

    digitalWrite(THR_PIN_MTR_FWD, LOW);
    digitalWrite(THR_PIN_MTR_REV, LOW);
    analogWrite(THR_PIN_SPEED, 0);

    _target    = 0;
    _hardLimit = true;
    _moving    = false;
    _seeking   = false;
}

// ============================================================
//  motorSetTarget()
// ============================================================

void motorSetTarget(uint16_t targetADC, bool hardLimit) {
    if (targetADC > THR_ADC_MAX) targetADC = THR_ADC_MAX;
    _target    = targetADC;
    _hardLimit = hardLimit;
    _seeking   = true;
}

// ============================================================
//  motorUpdate()
// ============================================================

void motorUpdate(uint16_t currentADC) {
    if (!_seeking) {
        if (_moving) motorStop();
        return;
    }

    int16_t error = (int16_t)_target - (int16_t)currentADC;
    uint16_t absError = (uint16_t)(error < 0 ? -error : error);
    uint16_t deadzone = _hardLimit ? THR_DEADZONE_HARD : THR_DEADZONE_SOFT;

    // Within arrival deadzone — stop
    if (absError <= deadzone) {
        motorStop();
        _seeking = false;
        return;
    }

    // Determine speed — slow near target, full elsewhere
    uint8_t speed = (absError <= THR_SLOWDOWN_ZONE)
                  ? THR_MOTOR_SPEED_SLOW
                  : THR_MOTOR_SPEED_FULL;

    // Drive toward target
    bool forward = (error > 0);
    motorDrive(speed, forward);
    _moving = true;
}

// ============================================================
//  motorStop()
// ============================================================

void motorStop() {
    digitalWrite(THR_PIN_MTR_FWD, LOW);
    digitalWrite(THR_PIN_MTR_REV, LOW);
    analogWrite(THR_PIN_SPEED, 0);
    _moving = false;
}

// ============================================================
//  motorIsMoving()
// ============================================================

bool motorIsMoving() {
    return _moving;
}

// ============================================================
//  motorGetTarget()
// ============================================================

uint16_t motorGetTarget() {
    return _target;
}

// ============================================================
//  motorDrive()
// ============================================================

void motorDrive(uint8_t speed, bool forward) {
    if (forward) {
        digitalWrite(THR_PIN_MTR_FWD, HIGH);
        digitalWrite(THR_PIN_MTR_REV, LOW);
    } else {
        digitalWrite(THR_PIN_MTR_FWD, LOW);
        digitalWrite(THR_PIN_MTR_REV, HIGH);
    }
    analogWrite(THR_PIN_SPEED, speed);
    _moving = true;
}

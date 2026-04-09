/**
 * @file        Motor.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Throttle Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       L293D H-bridge motor control for the Throttle Module.
 *
 *              Motor direction: MTR_FWD and MTR_REV are binary signals
 *              to the L293D INPUT pins. Only one should be HIGH at a time.
 *
 *              Motor speed: SPEED drives the L293D ENABLE pin via PWM.
 *              0 = stopped, 255 = full speed.
 *
 *              Position control: bang-bang with near-target slowdown.
 *                Outside THR_SLOWDOWN_ZONE: THR_MOTOR_SPEED_FULL
 *                Within THR_SLOWDOWN_ZONE:  THR_MOTOR_SPEED_SLOW
 *                Within arrival deadzone:   stopped
 *
 *              Deadzones:
 *                THR_DEADZONE_HARD — for target=0 and target=1023
 *                THR_DEADZONE_SOFT — for intermediate targets
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "Config.h"

// ============================================================
//  Public interface
// ============================================================

/**
 * @brief Initialise motor pins, stop motor, drive to 0 and hold.
 */
void motorBegin();

/**
 * @brief Set the target position in ADC counts (0-1023).
 *        motorUpdate() drives toward this position each call.
 * @param targetADC  Target wiper position.
 * @param hardLimit  If true, uses THR_DEADZONE_HARD (for 0 and 100%).
 */
void motorSetTarget(uint16_t targetADC, bool hardLimit);

/**
 * @brief Update motor drive based on current wiper position vs target.
 *        Call every loop iteration for responsive position control.
 * @param currentADC  Current wiper ADC reading.
 */
void motorUpdate(uint16_t currentADC);

/**
 * @brief Stop the motor immediately.
 */
void motorStop();

/**
 * @brief Returns true if motor is actively seeking a target.
 */
bool motorIsMoving();

/**
 * @brief Returns the current target ADC position.
 */
uint16_t motorGetTarget();

/**
 * @brief Drive motor at a specific speed and direction.
 *        For direct control — bypasses position controller.
 * @param speed    PWM value 0-255.
 * @param forward  true = forward, false = reverse.
 */
void motorDrive(uint8_t speed, bool forward);

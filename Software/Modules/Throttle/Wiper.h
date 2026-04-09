/**
 * @file        Wiper.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Throttle Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Potentiometer wiper ADC reading, touch detection,
 *              and throttle value scaling for the Throttle Module.
 *
 *              Position pipeline:
 *                1. Read raw ADC (0-1023) from potentiometer wiper
 *                2. Apply change threshold (THR_WIPER_CHANGE_MIN)
 *                3. Normal mode:    map(raw, 0, 1023, 0, INT16_MAX)
 *                   Precision mode: map relative to anchor ±10% of INT16_MAX
 *                4. Clamp to [0, INT16_MAX]
 *
 *              Touch detection:
 *                TOUCH_IND from AT42QT1010 — active high when pilot
 *                is touching the slider. When touched and enabled,
 *                motor stops and module follows wiper position.
 *                When touched and disabled, motor resists by holding 0.
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
 * @brief Initialise wiper ADC and touch sensor pins.
 */
void wiperBegin();

/**
 * @brief Poll wiper ADC and touch sensor.
 *        Call every THR_POLL_INTERVAL_MS.
 * @return true if throttle output value changed enough to report.
 */
bool wiperPoll();

/**
 * @brief Get current raw ADC wiper reading (0-1023).
 */
uint16_t wiperGetRaw();

/**
 * @brief Get current scaled throttle output value (0 to INT16_MAX).
 */
uint16_t wiperGetScaled();

/**
 * @brief Returns true if pilot is currently touching the slider.
 */
bool wiperIsTouched();

/**
 * @brief Returns true if throttle value changed since last cleared.
 */
bool wiperIsValueChanged();

/**
 * @brief Clear the value changed flag.
 */
void wiperClearChanged();

/**
 * @brief Enter precision mode.
 *        Records current output as anchor, sets precision mapping.
 * @param currentOutput  Current throttle output to anchor on.
 */
void wiperEnterPrecision(uint16_t currentOutput);

/**
 * @brief Exit precision mode.
 *        Returns to normal mapping from current precision output.
 */
void wiperExitPrecision();

/**
 * @brief Returns true if precision mode is active.
 */
bool wiperIsPrecision();

/**
 * @brief Reset wiper state — clears changed flag and precision mode.
 */
void wiperReset();

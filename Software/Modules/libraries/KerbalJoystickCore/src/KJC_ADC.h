/**
 * @file        KJC_ADC.h
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Analog axis reading, startup calibration, deadzone,
 *              change threshold, and INT16 scaling for the
 *              KerbalJoystickCore library.
 *
 *              Processing pipeline per axis (all thresholds in ADC counts):
 *                1. Read raw ADC (10-bit, 0-1023)
 *                2. Apply deadzone: if |raw - center| <= KJC_DEADZONE,
 *                   output is zero (check if last sent was also zero)
 *                3. Apply change threshold: if |raw - lastSentRaw|
 *                   <= KJC_CHANGE_THRESHOLD, suppress output
 *                4. Apply split map to convert to INT16:
 *                     if raw >= center: map(raw, center, 1023, 0, 32767)
 *                     if raw <  center: map(raw, 0, center, -32768, 0)
 *                5. Return scaled INT16 value and update tracking state
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "KJC_Config.h"

// ============================================================
//  Public interface
// ============================================================

/**
 * @brief Initialise ADC pins and run startup calibration.
 *        Reads KJC_CALIBRATION_SAMPLES on each axis and averages
 *        them to establish the center reference for each axis.
 *        Do not move the joystick during the first
 *        (KJC_CALIBRATION_SAMPLES * ~5ms) after power-on.
 */
void adcBegin();

/**
 * @brief Poll all three axes and update internal state.
 *        Applies deadzone and change threshold filtering.
 *        Call every KJC_POLL_INTERVAL_MS from the main loop.
 * @return true if any axis produced a reportable change.
 */
bool adcPoll();

/**
 * @brief Returns true if at least one axis has a pending value
 *        that has not yet been read by the controller.
 */
bool adcIsDataPending();

/**
 * @brief Clear the pending data flag.
 *        Called after axis data is included in a packet read.
 */
void adcClearPending();

/**
 * @brief Get the latest scaled INT16 value for an axis.
 *        Returns the last valid (non-suppressed) value.
 *        Returns 0 if the axis is within the deadzone.
 * @param axis  Axis index (KJC_AXIS1, KJC_AXIS2, KJC_AXIS3).
 */
int16_t adcGetScaled(uint8_t axis);

/**
 * @brief Get the calibrated center raw ADC value for an axis.
 * @param axis  Axis index (KJC_AXIS1, KJC_AXIS2, KJC_AXIS3).
 */
uint16_t adcGetCenter(uint8_t axis);

/**
 * @brief Reset all axis tracking state and clear pending flag.
 *        Called on CMD_RESET.
 */
void adcClearAll();

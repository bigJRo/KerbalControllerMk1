/**
 * @file        K7SC_Encoder.h
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Rotary encoder input with acceleration for the
 *              Kerbal7SegmentCore library.
 *
 *              Hardware: PEC11R-4220F-S0024 quadrature encoder
 *              with hardware RC debounce (10nF caps on ENC_A/B).
 *              Pull-up resistors R1/R2 (10k) on both channels.
 *
 *              Acceleration — step size based on click interval:
 *                Slow   (>K7SC_ENC_SLOW_MS)  → K7SC_STEP_SLOW   (1)
 *                Medium (between thresholds)  → K7SC_STEP_MEDIUM (10)
 *                Fast   (<K7SC_ENC_FAST_MS)   → K7SC_STEP_FAST   (100)
 *
 *              The encoder directly updates the display value.
 *              displaySetValue() and displayGetValue() are called
 *              internally. The caller only needs to check
 *              encoderIsValueChanged() each poll cycle.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "K7SC_Config.h"

// ============================================================
//  Public interface
// ============================================================

/**
 * @brief Initialise encoder pins and state.
 *        Reads initial A/B state as reference.
 * @param initialValue  Starting value for the display.
 */
void encoderBegin(uint16_t initialValue);

/**
 * @brief Poll encoder and update display value if changed.
 *        Call every K7SC_POLL_INTERVAL_MS from the main loop.
 * @return true if the display value changed this poll cycle.
 */
bool encoderPoll();

/**
 * @brief Returns true if the display value changed since last
 *        call to encoderClearChanged().
 */
bool encoderIsValueChanged();

/**
 * @brief Clear the value changed flag.
 *        Called after the new value is included in a packet.
 */
void encoderClearChanged();

/**
 * @brief Force-set the encoder's tracked value.
 *        Used for preset buttons and reset actions.
 *        Updates display immediately.
 * @param value  New value (clamped to 0-9999).
 */
void encoderSetValue(uint16_t value);

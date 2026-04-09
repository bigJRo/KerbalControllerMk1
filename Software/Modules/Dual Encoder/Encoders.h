/**
 * @file        Encoders.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Dual Encoder Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Quadrature encoder decoding and delta accumulation
 *              for both encoders on the Dual Encoder Module.
 *
 *              Both encoders are PEC11R-4220F-S0024 with hardware
 *              RC debounce (10nF caps on A/B channels) and 10k
 *              pull-ups. Software adds a minimum interval guard
 *              against any residual glitches.
 *
 *              Deltas accumulate between reads. If the controller
 *              is slow to respond, no clicks are lost — the delta
 *              count grows until the next packet read.
 *
 *              Delta sign convention:
 *                Positive = clockwise
 *                Negative = counter-clockwise
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "Config.h"

/**
 * @brief Initialise encoder pins and read initial A/B states.
 */
void encodersBegin();

/**
 * @brief Poll both encoders.
 *        Call every DEC_POLL_INTERVAL_MS from the main loop.
 * @return true if either encoder moved.
 */
bool encodersPoll();

/**
 * @brief Returns true if either encoder has an unread delta.
 */
bool encodersIsDataPending();

/**
 * @brief Get accumulated delta for ENC1 since last read.
 *        Does not clear the delta — call encodersClearDeltas() after use.
 */
int8_t encodersGetDelta1();

/**
 * @brief Get accumulated delta for ENC2 since last read.
 *        Does not clear the delta — call encodersClearDeltas() after use.
 */
int8_t encodersGetDelta2();

/**
 * @brief Clear both encoder deltas and pending flag.
 *        Call after deltas are consumed in a packet read.
 */
void encodersClearDeltas();

/**
 * @brief Reset all encoder state.
 *        Called on CMD_RESET.
 */
void encodersReset();

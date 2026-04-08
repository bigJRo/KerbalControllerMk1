/**
 * @file        Encoders.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — EVA Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Rotary encoder interface for the EVA module.
 *
 *              Two encoder headers (H1, H2) are present on the PCB.
 *              This subsystem is stubbed for future use — encoders
 *              are not currently connected or implemented.
 *
 *              When implemented, encoder deltas will be reported in
 *              bytes 4 and 5 of the extended button state packet as
 *              signed int8 values (relative clicks since last read).
 *
 *              Pin assignments:
 *                ENC1: PA3=A, PA2=B, PA1=SW
 *                ENC2: PC3=A, PC2=B, PC1=SW
 *
 *              Note: Encoders have no external pull-ups on the PCB.
 *              Use INPUT_PULLUP if common-ground encoders are fitted.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "Config.h"

/**
 * @brief Initialise encoder pins.
 *        Currently configures all encoder pins as inputs.
 *        No interrupt or polling logic active — future use only.
 */
void encodersBegin();

/**
 * @brief Returns the accumulated delta for ENC1 since last read.
 *        Always returns 0 in the current implementation.
 * @return Signed click count (positive = clockwise).
 */
int8_t encodersGetDelta1();

/**
 * @brief Returns the accumulated delta for ENC2 since last read.
 *        Always returns 0 in the current implementation.
 * @return Signed click count (positive = clockwise).
 */
int8_t encodersGetDelta2();

/**
 * @brief Clear accumulated encoder deltas.
 *        Called after deltas are consumed in a packet read.
 */
void encodersClearDeltas();

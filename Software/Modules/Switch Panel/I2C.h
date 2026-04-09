/**
 * @file        I2C.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Switch Panel Input Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler for the Switch Panel Input Module.
 *
 *              Uses the standard 4-byte KBC-style packet format.
 *              INT asserts on any debounced switch state change.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "Config.h"

/**
 * @brief Initialise I2C callbacks and INT pin.
 *        Call after Wire.begin() in setup().
 */
void i2cBegin();

/**
 * @brief Sync INT pin with pending switch state.
 *        Call every loop iteration.
 */
void i2cSyncINT();

/**
 * @brief Returns true if module is in sleep mode.
 */
bool i2cIsSleeping();

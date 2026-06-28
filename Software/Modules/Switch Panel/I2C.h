/**
 * @file        I2C.h
 * @version     2.0
 * @date        2026-06-28
 * @project     Kerbal Controller Mk1 — Switch Panel Input Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler for the Switch Panel Input Module.
 *
 *              Conformant with I2C Protocol Specification v2.4: standard
 *              button packet, 7 bytes (3-byte universal header + 4-byte
 *              state/change payload, spec §9.1). Powers on in BOOT_READY
 *              (INT held); full lifecycle state machine. INT asserts on any
 *              debounced switch state change while ACTIVE.
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

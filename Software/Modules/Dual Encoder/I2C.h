/**
 * @file        I2C.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Dual Encoder Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler for the Dual Encoder Module.
 *
 *              Data packet (module → controller, 4 bytes):
 *                Byte 0:  Button events  (bit0=ENC1_SW, bit1=ENC2_SW)
 *                Byte 1:  Change mask    (same bit layout)
 *                Byte 2:  ENC1 delta     (signed int8, +CW, -CCW)
 *                Byte 3:  ENC2 delta     (signed int8, +CW, -CCW)
 *
 *              Deltas accumulate between reads — no clicks are lost.
 *              Button events are rising-edge only, cleared after read.
 *              INT asserts on any encoder movement or button press.
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
 * @brief Sync INT pin with pending encoder and button state.
 *        Call every loop iteration.
 */
void i2cSyncINT();

/**
 * @brief Returns true if module is in sleep mode.
 */
bool i2cIsSleeping();

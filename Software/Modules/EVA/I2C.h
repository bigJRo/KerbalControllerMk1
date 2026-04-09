/**
 * @file        I2C.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — EVA Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler for the EVA module.
 *
 *              Implements the Kerbal Controller Mk1 I2C protocol
 *              command set independently of the KerbalButtonCore
 *              library. Wire callbacks use a static trampoline pattern.
 *
 *              Button read response is the extended 6-byte packet:
 *                Byte 0-1: current state bitmask
 *                Byte 2-3: change mask
 *                Byte 4:   ENC1 delta (signed int8)
 *                Byte 5:   ENC2 delta (signed int8)
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
 * @brief Sync INT pin with pending button state.
 *        Call every loop iteration.
 */
void i2cSyncINT();

/**
 * @brief Returns true if a pending LED command requires a render.
 */
bool i2cIsRenderPending();

/**
 * @brief Clear the render pending flag after ledsRender() is called.
 */
void i2cClearRenderPending();

/**
 * @brief Returns true if module is in sleep mode.
 */
bool i2cIsSleeping();

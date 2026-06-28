/**
 * @file        I2C.h
 * @version     2.0
 * @date        2026-06-28
 * @project     Kerbal Controller Mk1 — EVA Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler for the EVA module.
 *
 *              Conformant with I2C Protocol Specification v2.4. Implements
 *              the shared command set independently of the KerbalButtonCore
 *              library. Wire callbacks use a static trampoline pattern.
 *
 *              Data packet is the standard button format, 7 bytes (spec §9.1):
 *                Byte 0:   Status byte   (lifecycle/fault/data-changed)
 *                Byte 1:   Module Type ID
 *                Byte 2:   Transaction counter
 *                Byte 3:   Button events HI (bits 0-5 = buttons 0-5)
 *                Byte 4:   Button events LO (always 0)
 *                Byte 5:   Change mask  HI (bits 0-5)
 *                Byte 6:   Change mask  LO (always 0)
 *              Encoder hardware is unpopulated; no delta bytes are sent and
 *              the capability flags are 0x00. The module powers on in
 *              BOOT_READY (INT held) and runs the full lifecycle.
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

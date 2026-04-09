/**
 * @file        I2C.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Indicator Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler for the Indicator Module.
 *
 *              This is a pure output module — INT is never asserted
 *              during normal operation (no inputs to report). INT
 *              is reserved for future use when encoder headers are
 *              populated.
 *
 *              Commands:
 *                CMD_SET_LED_STATE (0x02) — primary runtime command.
 *                  8-byte nibble-packed payload, 1 nibble per pixel.
 *                  Pixel 0 = high nibble of byte 0, pixel 1 = low nibble, etc.
 *
 *              All other standard commands are supported per spec.
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
 * @brief Sync INT pin. Currently always deasserted (no inputs).
 *        Call every loop iteration.
 */
void i2cSyncINT();

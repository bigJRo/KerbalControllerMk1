/**
 * @file        I2C.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Throttle Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler for the Throttle Module.
 *
 *              Data packet (module → controller, 4 bytes):
 *                Byte 0:   Status flags  (bit0=enabled, bit1=precision,
 *                                         bit2=pilot touching, bit3=motor moving)
 *                Byte 1:   Button events (bit0=THRTL_100, bit1=THRTL_UP,
 *                                         bit2=THRTL_DOWN, bit3=THRTL_00)
 *                Byte 2-3: Throttle value (uint16, big-endian, 0 to INT16_MAX)
 *
 *              Commands (controller → module):
 *                0x01 CMD_GET_IDENTITY   — identity response
 *                0x02 CMD_SET_LED_STATE  — accepted, ignored (no NeoPixels)
 *                0x03 CMD_SET_BRIGHTNESS — accepted, ignored
 *                0x04 CMD_BULB_TEST      — flashes indicator LED
 *                0x05 CMD_SLEEP          — same as CMD_DISABLE
 *                0x06 CMD_WAKE           — same as CMD_ENABLE
 *                0x07 CMD_RESET          — disable, clear all state
 *                0x08 CMD_ACK_FAULT      — acknowledged, ignored
 *                0x09 CMD_ENABLE         — enable module, LEDs on
 *                0x0A CMD_DISABLE        — disable, motor to 0, LEDs off
 *                0x0B CMD_SET_THROTTLE   — 2 bytes: uint16 target position
 *                0x0C CMD_SET_PRECISION  — 1 byte: 0x00=normal, 0x01=precision
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
 * @brief Sync INT pin with pending state.
 *        Asserts INT on button events or throttle value changes.
 *        Call every loop iteration.
 */
void i2cSyncINT();

/**
 * @brief Returns true if module is enabled.
 */
bool i2cIsEnabled();

/**
 * @brief Returns true if precision mode is active.
 */
bool i2cIsPrecision();

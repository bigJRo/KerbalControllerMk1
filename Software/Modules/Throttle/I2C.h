/**
 * @file        I2C.h
 * @version     2.0
 * @date        2026-06-28
 * @project     Kerbal Controller Mk1 — Throttle Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler for the Throttle Module.
 *
 *              Conformant with I2C Protocol Specification v2.4. Data packet
 *              (module → controller, 7 bytes, spec §9.4):
 *                Byte 0:   Status byte   (lifecycle/fault/data-changed)
 *                Byte 1:   Module Type ID
 *                Byte 2:   Transaction counter
 *                Byte 3:   Status flags  (bit0=enabled, bit1=precision,
 *                                         bit2=pilot touching, bit3=motor moving)
 *                Byte 4:   Button events (bit0=THRTL_100, bit1=THRTL_UP,
 *                                         bit2=THRTL_DOWN, bit3=THRTL_00)
 *                Byte 5-6: Throttle value (uint16, big-endian, 0 to INT16_MAX)
 *
 *              The module powers on in BOOT_READY (INT asserted, held until
 *              the controller reads the boot packet and sends CMD_DISABLE),
 *              and remains DISABLED until CMD_ENABLE. Lifecycle: BOOT_READY →
 *              DISABLED → ACTIVE ↔ SLEEPING.
 *
 *              Commands (controller → module):
 *                0x01 CMD_GET_IDENTITY   — identity response
 *                0x02 CMD_SET_LED_STATE  — accepted, ignored (no NeoPixels)
 *                0x03 CMD_SET_BRIGHTNESS — accepted, ignored
 *                0x04 CMD_BULB_TEST      — flashes indicator LED
 *                0x05 CMD_SLEEP          — freeze state (SLEEPING), motor holds
 *                0x06 CMD_WAKE           — resume from SLEEPING to ACTIVE
 *                0x07 CMD_RESET          — reset to defaults, stay ACTIVE
 *                0x08 CMD_ACK_FAULT      — clear fault flag
 *                0x09 CMD_ENABLE         — enable module (ACTIVE), LEDs on
 *                0x0A CMD_DISABLE        — disable (DISABLED), motor to 0, LEDs off
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
 * @brief Sync INT pin and process touch/button events each loop.
 *
 *        Handles touch detection, button event processing, and
 *        motor target-setting in response to inputs. Asserts or
 *        deasserts the INT pin based on pending state.
 *
 *        Does NOT call motorUpdate(). The caller (loop()) is
 *        responsible for calling motorUpdate(wiperGetRaw()) once
 *        per iteration after this function returns.
 *
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

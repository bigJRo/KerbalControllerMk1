/**
 * @file        KJC_I2C.h
 * @version     2.0.0
 * @date        2026-06-28
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler for KerbalJoystickCore modules.
 *
 *              Conformant with I2C Protocol Specification v2.4. The
 *              joystick data packet is 12 bytes (3-byte universal header
 *              + 9-byte payload, spec §9.2):
 *
 *                Byte 0:    Status byte   (lifecycle/fault/data-changed)
 *                Byte 1:    Module Type ID
 *                Byte 2:    Transaction counter
 *                Byte 3:    Button events (rising edges; bit0=BTN_JOY, ...)
 *                Byte 4:    Change mask   (same bit layout)
 *                Byte 5:    Button state  (persistent; same bit layout)
 *                Byte 6-7:  AXIS1         (int16, signed, big-endian)
 *                Byte 8-9:  AXIS2         (int16, signed, big-endian)
 *                Byte 10-11:AXIS3         (int16, signed, big-endian)
 *
 *              The module powers on in BOOT_READY with INT asserted,
 *              held until the controller reads the boot packet and sends
 *              CMD_DISABLE. Full lifecycle: BOOT_READY → DISABLED →
 *              ACTIVE ↔ SLEEPING.
 *
 *              INT assertion follows Option E hybrid strategy:
 *                - Buttons: always immediate, bypass all throttling
 *                - Axes: asserted only when outside deadzone, change
 *                  exceeds threshold, and quiet period has elapsed
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "KJC_Config.h"

/**
 * @brief Initialise I2C callbacks, INT pin, and quiet period timer.
 *        Call after Wire.begin() in setup().
 * @param typeId    Module Type ID reported in identity response.
 * @param capFlags  Capability flags reported in identity response.
 */
void kjcI2CBegin(uint8_t typeId, uint8_t capFlags);

/**
 * @brief Sync INT pin with pending button and axis state.
 *        Enforces KJC_QUIET_PERIOD_MS between axis-driven INT assertions.
 *        Button changes bypass the quiet period entirely.
 *        Call every loop iteration.
 */
void kjcI2CSyncINT();

/**
 * @brief Returns true if a pending LED command requires a render.
 */
bool kjcI2CIsRenderPending();

/**
 * @brief Clear the render pending flag after buttonsRender() is called.
 */
void kjcI2CClearRenderPending();

/**
 * @brief Returns true if module is in sleep mode.
 */
bool kjcI2CIsSleeping();

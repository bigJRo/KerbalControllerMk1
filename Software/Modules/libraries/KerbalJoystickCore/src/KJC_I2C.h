/**
 * @file        KJC_I2C.h
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler for KerbalJoystickCore modules.
 *
 *              Implements the Kerbal Controller Mk1 I2C protocol
 *              command set. Joystick data packet is 8 bytes:
 *
 *                Byte 0:   Button state  (bit0=BTN_JOY, bit1=BTN01, bit2=BTN02)
 *                Byte 1:   Change mask   (same bit layout)
 *                Byte 2-3: AXIS1         (int16, signed, -32768 to +32767)
 *                Byte 4-5: AXIS2         (int16, signed, -32768 to +32767)
 *                Byte 6-7: AXIS3         (int16, signed, -32768 to +32767)
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

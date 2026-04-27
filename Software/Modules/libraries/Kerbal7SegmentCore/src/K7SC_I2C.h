/**
 * @file        K7SC_I2C.h
 * @version     1.1.0
 * @date        2026-04-26
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler for Kerbal7SegmentCore modules.
 *
 *              Data packet (6 bytes, module → controller):
 *                Byte 0:   Button events  (bit0=BTN01, bit1=BTN02,
 *                                          bit2=BTN03, bit3=BTN_EN)
 *                Byte 1:   Change mask    (same bit layout)
 *                Byte 2:   Module state   (bits 0-1 = BTN01 cycle,
 *                                          bit 2 = BTN02 active,
 *                                          bit 3 = BTN03 active)
 *                Byte 3:   Reserved
 *                Byte 4-5: Display value  (uint16, big-endian, 0-9999)
 *
 *              Additional command:
 *                K7SC_CMD_SET_VALUE (0x0D) — controller sets display value
 *                  Payload: 2 bytes (uint16, big-endian)
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "K7SC_Config.h"

/**
 * @brief Initialise I2C as target, register callbacks, and configure INT pin.
 *        Called internally by k7scBegin() — do not call directly.
 * @param i2cAddress  I2C target address for this module.
 * @param typeId      Module Type ID.
 * @param capFlags    Capability flags.
 */
void k7scI2CBegin(uint8_t i2cAddress, uint8_t typeId, uint8_t capFlags);

/**
 * @brief Sync INT pin with pending state.
 *        Asserts INT when button events or display value changes pending.
 *        Call every loop iteration.
 */
void k7scI2CSyncINT();

/**
 * @brief Poll bulb test timer. Call every loop iteration via k7scUpdate().
 *        Ends bulb test and restores display/LEDs after BULB_DURATION_MS.
 */
void k7scBulbTestPoll();

/**
 * @brief Returns true if the module is in sleep mode.
 */
bool k7scI2CIsSleeping();

/**
 * @brief Returns the pending button event bitmask for use in loop().
 *
 *        This is the same event snapshot captured when INT was last
 *        asserted. Sketch code should call this after k7scUpdate() to
 *        act on button presses (e.g. preset jumps) without racing
 *        against the I2C snapshot capture.
 *
 *        The snapshot is consumed (cleared) when the controller reads
 *        the data packet, not by this call. It is safe to call this
 *        function multiple times in the same loop iteration.
 *
 *        Bit layout: bit0=BTN01, bit1=BTN02, bit2=BTN03, bit3=BTN_EN.
 *        Each bit is set only for the loop iteration in which the press
 *        was first captured.
 *
 *        Do NOT call buttonsGetEvents() directly in sketch loop() code —
 *        it clears _eventMask and will race with k7scI2CSyncINT().
 *        Use this function instead.
 */
uint8_t k7scGetPendingEvents();

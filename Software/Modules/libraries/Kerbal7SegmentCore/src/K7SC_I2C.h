/**
 * @file        K7SC_I2C.h
 * @version     1.0.0
 * @date        2026-04-08
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
 *                K7SC_CMD_SET_VALUE (0x09) — controller sets display value
 *                  Payload: 2 bytes (uint16, big-endian)
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "K7SC_Config.h"

/**
 * @brief Initialise I2C callbacks and INT pin.
 *        Call after Wire.begin() in setup().
 * @param typeId    Module Type ID.
 * @param capFlags  Capability flags.
 */
void k7scI2CBegin(uint8_t typeId, uint8_t capFlags);

/**
 * @brief Sync INT pin with pending state.
 *        Asserts INT when button events or display value changes pending.
 *        Call every loop iteration.
 */
void k7scI2CSyncINT();

/**
 * @brief Returns true if the module is in sleep mode.
 */
bool k7scI2CIsSleeping();

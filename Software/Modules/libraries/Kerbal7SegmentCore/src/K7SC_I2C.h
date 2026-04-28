/**
 * @file        K7SC_I2C.h
 * @version     2.0.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler — thin protocol framer.
 *
 *              Receives commands from master → updates cmdState.
 *              Transmits packets built by sketch → prepends 3-byte header.
 *
 *              Library handles silently (no cmdState flag):
 *                GET_IDENTITY   — sends identity response
 *                ACK_FAULT      — clears fault flag
 *                SET_BRIGHTNESS — writes MAX7219 intensity register
 *
 *              Library sets cmdState flags (sketch acts):
 *                ENABLE, DISABLE, SLEEP, WAKE → cmdState.lifecycle
 *                BULB_TEST                    → cmdState.isBulbTest
 *                RESET                        → cmdState.isReset (one-shot)
 *                SET_VALUE                    → cmdState.hasNewValue + newValue
 *                SET_LED_STATE                → cmdState.hasLEDState + ledState
 *
 * @license     GNU General Public License v3.0
 */

#pragma once
#include "K7SC_Config.h"
#include "K7SC_State.h"

/**
 * @brief Initialise I2C target, assert BOOT_READY INT.
 *        Called internally by k7scBegin().
 */
void k7scI2CBegin(uint8_t i2cAddress, uint8_t typeId, uint8_t capFlags);

/**
 * @brief Queue a packet for transmission to master and assert INT.
 *        Library prepends the 3-byte universal header. Sketch provides
 *        the module-specific payload bytes per the protocol spec.
 *        If called while a packet is already pending, the new packet
 *        overwrites the previous one (last-write-wins).
 *
 * @param payload   Module-specific payload bytes (bytes 3+ of packet).
 * @param length    Number of payload bytes.
 */
void k7scQueuePacket(const uint8_t* payload, uint8_t length);

/**
 * @brief Sync I2C state each loop. Clears one-shot cmdState flags
 *        that were set in the previous loop. Called by k7scUpdate().
 */
void k7scI2CPoll();

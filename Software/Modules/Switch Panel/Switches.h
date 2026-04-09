/**
 * @file        Switches.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Switch Panel Input Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       10-switch GPIO input with debounce and dual-buffer
 *              state latching for the Switch Panel Input Module.
 *
 *              All switches are active high with hardware 10k pull-downs.
 *              Supports both latching and momentary toggle switches —
 *              the dual-buffer strategy guarantees every state change
 *              edge is reported to the controller regardless of how
 *              quickly a momentary switch returns to its default position.
 *
 *              Switch-to-bit mapping in the 16-bit state word:
 *                Bit 0  = SW1   (PB4)
 *                Bit 1  = SW2   (PC3)
 *                Bit 2  = SW3   (PB5)
 *                Bit 3  = SW4   (PC2)
 *                Bit 4  = SW5   (PA7)
 *                Bit 5  = SW6   (PC1)
 *                Bit 6  = SW7   (PA6)
 *                Bit 7  = SW8   (PC0)
 *                Bit 8  = SW9   (PA5)
 *                Bit 9  = SW10  (PB3)
 *                Bits 10-15 = unused (always 0)
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "Config.h"

/**
 * @brief Initialise all switch GPIO pins as inputs.
 */
void switchesBegin();

/**
 * @brief Poll all 10 switch inputs with debounce.
 *        Call every SWP_POLL_INTERVAL_MS from the main loop.
 * @return true if any debounced state change occurred.
 */
bool switchesPoll();

/**
 * @brief Returns true if unread state changes are pending.
 */
bool switchesIsIntPending();

/**
 * @brief Build the 4-byte response packet and clear pending state.
 *        Byte 0: current state HIGH (bits 15-8)
 *        Byte 1: current state LOW  (bits 7-0)
 *        Byte 2: change mask HIGH   (bits 15-8)
 *        Byte 3: change mask LOW    (bits 7-0)
 * @param buf  Destination buffer, must be SWP_PACKET_SIZE bytes.
 */
void switchesGetPacket(uint8_t* buf);

/**
 * @brief Returns the current debounced state as a 16-bit bitmask.
 */
uint16_t switchesGetState();

/**
 * @brief Clear all latched state and deassert INT pending.
 *        Called on CMD_RESET.
 */
void switchesClearAll();

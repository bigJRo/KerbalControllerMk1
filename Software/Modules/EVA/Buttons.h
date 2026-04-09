/**
 * @file        Buttons.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — EVA Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Direct GPIO button input with debounce and dual-buffer
 *              state latching for the EVA module.
 *
 *              6 buttons read directly from ATtiny816 GPIO pins.
 *              No shift registers used on this module.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "Config.h"

// ============================================================
//  Public interface
// ============================================================

/**
 * @brief Initialise button GPIO pins as inputs.
 *        All pins are active high with hardware pull-downs —
 *        no INPUT_PULLUP needed.
 */
void buttonsBegin();

/**
 * @brief Poll all 6 button inputs with debounce.
 *        Call every EVA_POLL_INTERVAL_MS from the main loop.
 *        Updates live state and sets INT pending on change.
 * @return true if any debounced state change occurred.
 */
bool buttonsPoll();

/**
 * @brief Returns true if unread button state is pending.
 */
bool buttonsIsIntPending();

/**
 * @brief Build the 4-byte EVA response packet and clear pending state.
 *        Byte 0:  Button state  (bits 0-5 = buttons 0-5)
 *        Byte 1:  Change mask   (bits 0-5 = buttons 0-5)
 *        Byte 2:  ENC1 delta    (signed int8, always 0 — future use)
 *        Byte 3:  ENC2 delta    (signed int8, always 0 — future use)
 * @param buf  Destination buffer, must be EVA_PACKET_SIZE bytes.
 */
void buttonsGetPacket(uint8_t* buf);

/**
 * @brief Returns the current debounced state of button at index.
 * @param index  Button index (0-5).
 * @return true if pressed.
 */
bool buttonsGetState(uint8_t index);

/**
 * @brief Clear all latched state and deassert INT.
 *        Called on CMD_RESET.
 */
void buttonsClearAll();

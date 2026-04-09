/**
 * @file        Buttons.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Throttle Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Button input and discrete LED output for the Throttle Module.
 *
 *              Four buttons: THRTL_100, THRTL_UP, THRTL_DOWN, THRTL_00
 *              Four discrete LEDs (2N3904 NPN switched):
 *                THRTL_100_LED, THRTL_UP_LED, THRTL_DOWN_LED, THRTL_00_LED
 *
 *              All buttons active high with hardware pull-downs (RN1 10k).
 *              All LEDs active high through 2N3904 transistors.
 *
 *              LED behavior:
 *                All ON  — throttle enabled
 *                All OFF — throttle disabled
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
 * @brief Initialise button GPIO and LED pins.
 *        All LEDs off on start (module starts disabled).
 */
void buttonsBegin();

/**
 * @brief Poll all four buttons with debounce.
 *        Call every THR_POLL_INTERVAL_MS.
 * @return true if any button event occurred.
 */
bool buttonsPoll();

/**
 * @brief Returns true if unread button events are pending.
 */
bool buttonsIsIntPending();

/**
 * @brief Get and clear the button event bitmask.
 *        bit0=THRTL_100, bit1=THRTL_UP, bit2=THRTL_DOWN, bit3=THRTL_00
 *        Clears INT pending flag.
 */
uint8_t buttonsGetEvents();

/**
 * @brief Set all LEDs on (throttle enabled state).
 */
void buttonsLEDsOn();

/**
 * @brief Set all LEDs off (throttle disabled state).
 */
void buttonsLEDsOff();

/**
 * @brief Clear all button state and events.
 *        Called on CMD_RESET.
 */
void buttonsClearAll();

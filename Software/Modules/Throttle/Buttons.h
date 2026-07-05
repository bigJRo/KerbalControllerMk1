/**
 * @file        Buttons.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Throttle Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Button input and switch-backlight output for the Throttle Module.
 *
 *              Four command buttons: THRTL_100, THRTL_UP, THRTL_DOWN, THRTL_00.
 *              (A fifth panel switch, THRTL_ENA, exists in hardware on PB5 but
 *              is not read here — see Config.h.)
 *
 *              All buttons active high with hardware pull-downs.
 *
 *              Switch backlights: a SINGLE shared enable line (LED_ENA, PC3)
 *              gates all five illuminated switches together through one 2N3904
 *              per switch — there are no independent per-button LEDs.
 *
 *              Backlight behavior:
 *                On  — throttle enabled
 *                Off — throttle disabled
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
 * @brief Initialise button GPIO and the shared backlight-enable pin.
 *        Backlight off on start (module starts disabled).
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
 * @brief Turn the shared switch backlight on (throttle enabled state).
 */
void buttonsLEDsOn();

/**
 * @brief Turn the shared switch backlight off (throttle disabled state).
 */
void buttonsLEDsOff();

/**
 * @brief Clear all button state and events.
 *        Called on CMD_RESET.
 */
void buttonsClearAll();

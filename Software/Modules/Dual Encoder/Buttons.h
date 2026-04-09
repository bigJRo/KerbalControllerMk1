/**
 * @file        Buttons.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Dual Encoder Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Encoder pushbutton input for the Dual Encoder Module.
 *
 *              ENC1_SW (PC3) and ENC2_SW (PA4) — both active high
 *              with hardware 10k pull-down resistors (R10, R11).
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "Config.h"

/**
 * @brief Initialise pushbutton GPIO pins.
 */
void buttonsBegin();

/**
 * @brief Poll both pushbuttons with debounce.
 *        Call every DEC_POLL_INTERVAL_MS from the main loop.
 * @return true if any button event occurred.
 */
bool buttonsPoll();

/**
 * @brief Returns true if unread button events are pending.
 */
bool buttonsIsIntPending();

/**
 * @brief Get current button state bitmask.
 *        bit0=ENC1_SW, bit1=ENC2_SW. 1=pressed.
 */
uint8_t buttonsGetState();

/**
 * @brief Get and clear the button change mask.
 *        Clears INT pending flag after read.
 */
uint8_t buttonsGetChangeMask();

/**
 * @brief Get the button event mask (rising edges only).
 *        Set on press, cleared after packet read.
 */
uint8_t buttonsGetEvents();

/**
 * @brief Clear all button state and events.
 *        Called on CMD_RESET.
 */
void buttonsClearAll();

/**
 * @file        K7SC_Buttons.h
 * @version     2.0.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Button input and SK6812MINI-EA NeoPixel driver.
 *
 *              Reports rising/falling edges to inputState.
 *              No state machine — sketch owns all LED and button logic.
 *
 *              Bitmask layout (all edge/state fields):
 *                bit 0 = BTN01  (PA1)
 *                bit 1 = BTN02  (PC2)
 *                bit 2 = BTN03  (PC1)
 *                bit 3 = BTN_EN (PB3, encoder pushbutton)
 *
 * @license     GNU General Public License v3.0
 */

#pragma once
#include "K7SC_Config.h"
#include <KerbalModuleCommon.h>

/** @brief Initialise button GPIO pins and NeoPixel chain. */
void buttonsBegin();

/** @brief Poll all buttons with debounce. Accumulates edges into inputState.
 *         Called by k7scUpdate() each loop. */
void buttonsPoll();

/** @brief Set a single NeoPixel colour. Call buttonsShow() to render. */
void buttonSetPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/** @brief Render current pixel buffer to NeoPixel hardware. */
void buttonsShow();

/** @brief Turn off all NeoPixels immediately. */
void buttonsClearAll();

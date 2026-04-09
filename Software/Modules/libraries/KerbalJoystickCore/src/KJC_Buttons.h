/**
 * @file        KJC_Buttons.h
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Button input and NeoPixel LED control for the
 *              KerbalJoystickCore library.
 *
 *              Manages three button inputs:
 *                BTN_JOY — joystick pushbutton (PA6, direct GPIO,
 *                          active high, hardware pull-down, no LED)
 *                BTN01   — NeoPixel RGB button 1 (PC0, WS2811)
 *                BTN02   — NeoPixel RGB button 2 (PB3, WS2811)
 *
 *              NeoPixel chain on PC1 (Port C — set in IDE).
 *
 *              Button state is reported in the joystick data packet:
 *                Byte 0: state  (bit0=BTN_JOY, bit1=BTN01, bit2=BTN02)
 *                Byte 1: change (bit0=BTN_JOY, bit1=BTN01, bit2=BTN02)
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "KJC_Config.h"
#include "KJC_Colors.h"

// ============================================================
//  Public interface — initialisation
// ============================================================

/**
 * @brief Initialise button GPIO pins and NeoPixel chain.
 *        All NeoPixel buttons start in ENABLED (dim white) state.
 *        BTN_JOY has no LED — always OFF in color terms.
 * @param activeColors  Array of KJC_NEO_COUNT active colors
 *                      indexed as [BTN01, BTN02].
 * @param brightness    Initial ENABLED state brightness (0-255).
 */
void buttonsBegin(const RGBColor* activeColors, uint8_t brightness);

// ============================================================
//  Public interface — runtime
// ============================================================

/**
 * @brief Poll all three buttons with debounce.
 *        Call every KJC_POLL_INTERVAL_MS from the main loop.
 * @return true if any debounced button state changed.
 */
bool buttonsPoll();

/**
 * @brief Returns true if unread button state changes are pending.
 */
bool buttonsIsIntPending();

/**
 * @brief Get the current button state bitmask.
 *        bit0=BTN_JOY, bit1=BTN01, bit2=BTN02.
 */
uint8_t buttonsGetState();

/**
 * @brief Get and clear the button change mask.
 *        bit0=BTN_JOY, bit1=BTN01, bit2=BTN02.
 *        Clears INT pending flag after read.
 */
uint8_t buttonsGetChangeMask();

/**
 * @brief Clear all button state and change tracking.
 *        Called on CMD_RESET.
 */
void buttonsClearAll();

// ============================================================
//  Public interface — LED control
// ============================================================

/**
 * @brief Apply full LED state from nibble-packed payload.
 *        Only nibbles for BTN01 (index 0) and BTN02 (index 1)
 *        are used. BTN_JOY has no LED.
 *        Does not render — call buttonsRender() after.
 * @param payload  8-byte nibble-packed LED state payload.
 */
void buttonsSetLEDs(const uint8_t* payload);

/**
 * @brief Set LED state for a single NeoPixel button.
 *        Does not render — call buttonsRender() after.
 * @param index  NeoPixel button index (0=BTN01, 1=BTN02).
 * @param state  LED state nibble (KJC_LED_*).
 */
void buttonsSetLED(uint8_t index, uint8_t state);

/**
 * @brief Set ENABLED state brightness (0-255).
 * @param brightness  Brightness value 0-255.
 */
void buttonsSetBrightness(uint8_t brightness);

/**
 * @brief Set all NeoPixel LEDs to OFF state.
 *        Does not render — call buttonsRender() after.
 */
void buttonsClearLEDs();

/**
 * @brief Push current LED state to NeoPixel hardware.
 */
void buttonsRender();

/**
 * @brief Light all NeoPixels full white for bulb test then restore.
 * @param durationMs  Duration in milliseconds.
 */
void buttonsBulbTest(uint16_t durationMs);

/**
 * @brief Get current LED state nibble for a NeoPixel button.
 * @param index  NeoPixel button index (0=BTN01, 1=BTN02).
 */
uint8_t buttonsGetLEDState(uint8_t index);

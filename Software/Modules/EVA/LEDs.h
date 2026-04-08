/**
 * @file        LEDs.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — EVA Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       NeoPixel LED control for the EVA module.
 *              Manages 6 WS2811 RGB buttons via tinyNeoPixel_Static
 *              on PIN_PC0 (Port C).
 *
 *              LED states match the Kerbal Controller Mk1 I2C protocol:
 *                LED_OFF      — unlit
 *                LED_ENABLED  — dim white (scaled by brightness)
 *                LED_ACTIVE   — full brightness, EVA palette color
 *                LED_WARNING  — flashing amber (future use)
 *                LED_ALERT    — flashing red   (future use)
 *                LED_ARMED    — static cyan    (future use)
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "Config.h"

/**
 * @brief Initialise NeoPixel chain and set all LEDs off.
 *        Must be called in setup() after Wire.begin().
 */
void ledsBegin();

/**
 * @brief Apply full 6-button LED state from nibble-packed payload.
 *        Does not render — call ledsRender() after.
 * @param payload  8-byte nibble-packed LED state payload.
 */
void ledsSetAll(const uint8_t* payload);

/**
 * @brief Set LED state for a single button.
 *        Does not render — call ledsRender() after.
 * @param index  Button index (0-5).
 * @param state  LED state nibble (LED_*).
 */
void ledSetState(uint8_t index, uint8_t state);

/**
 * @brief Set ENABLED state brightness (0-255).
 *        Applied via RGB scaling at render time.
 * @param brightness  Brightness value 0-255.
 */
void ledsSetBrightness(uint8_t brightness);

/**
 * @brief Set all buttons to OFF state.
 *        Does not render — call ledsRender() after.
 */
void ledsClearAll();

/**
 * @brief Push current LED state to hardware.
 *        Writes all colors to tinyNeoPixel buffer and calls show().
 */
void ledsRender();

/**
 * @brief Light all LEDs full white for bulb test then restore state.
 * @param durationMs  Duration in milliseconds.
 */
void ledsBulbTest(uint16_t durationMs);

/**
 * @brief Returns current LED state nibble for a button.
 * @param index  Button index (0-5).
 */
uint8_t ledsGetState(uint8_t index);

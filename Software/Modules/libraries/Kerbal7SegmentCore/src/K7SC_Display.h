/**
 * @file        K7SC_Display.h
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       MAX7219 7-segment display driver for the
 *              Kerbal7SegmentCore library.
 *
 *              Drives a 4-digit FJ4401AG common-cathode display
 *              via the MAX7219CWG+T controller IC using software
 *              SPI (bit-banged) on PA7 (CLK), PA6 (LOAD), PA5 (DATA).
 *
 *              Display behavior:
 *                - BCD decode mode on all digits
 *                - No leading zeros (blank character for unused digits)
 *                - Range: 0-9999, clamped
 *                - Digit 0 = rightmost position (MAX7219 register 1)
 *                - Integer values only, no decimal point
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "K7SC_Config.h"

// ============================================================
//  Public interface
// ============================================================

/**
 * @brief Initialise MAX7219 and display.
 *        Configures BCD decode mode, scan limit, intensity,
 *        and exits shutdown mode. Shows 0 on startup.
 */
void displayBegin();

/**
 * @brief Set the displayed value (0-9999).
 *        Applies no-leading-zeros formatting automatically.
 *        Value is clamped to K7SC_VALUE_MIN/MAX.
 * @param value  Value to display.
 */
void displaySetValue(uint16_t value);

/**
 * @brief Get the currently displayed value.
 */
uint16_t displayGetValue();

/**
 * @brief Set display intensity (0-15).
 * @param intensity  Intensity level 0 (dimmest) to 15 (brightest).
 */
void displaySetIntensity(uint8_t intensity);

/**
 * @brief Run display test — all segments on for durationMs.
 *        Restores previous value after test.
 * @param durationMs  Test duration in milliseconds.
 */
void displayTest(uint16_t durationMs);

/**
 * @brief Blank all digits (display test off, all segments off).
 */
void displayBlank();

/**
 * @brief Restore display from blank with current value.
 */
void displayRestore();

/**
 * @brief Put MAX7219 into shutdown mode (display off, low power).
 */
void displayShutdown();

/**
 * @brief Wake MAX7219 from shutdown mode.
 */
void displayWake();

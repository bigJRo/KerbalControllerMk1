/**
 * @file        Kerbal7SegmentCore.h
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Top-level include for the Kerbal7SegmentCore library.
 *
 *              Kerbal7SegmentCore manages a 4-digit 7-segment display
 *              via MAX7219, rotary encoder input with acceleration,
 *              three SK6812MINI-EA NeoPixel (GRBW) buttons with
 *              configurable behavior modes, encoder pushbutton, and
 *              I2C target communication for Kerbal Controller Mk1
 *              display modules (KC-01-1881/1882).
 *
 *              Minimum sketch:
 *
 *                #include <Wire.h>
 *                #include <Kerbal7SegmentCore.h>
 *
 *                // Configure button behavior for each module type
 *                const ButtonConfig btnConfigs[K7SC_NEO_COUNT] = {
 *                    // BTN01 — cycle mode: off → state1 → state2 → off
 *                    { BTN_MODE_CYCLE,
 *                      {K7SC_OFF, K7SC_GREEN, K7SC_AMBER},
 *                      3, 0 },
 *                    // BTN02 — toggle
 *                    { BTN_MODE_TOGGLE,
 *                      {K7SC_OFF, K7SC_GREEN, K7SC_OFF},
 *                      2, 0 },
 *                    // BTN03 — toggle
 *                    { BTN_MODE_TOGGLE,
 *                      {K7SC_OFF, K7SC_GREEN, K7SC_OFF},
 *                      2, 0 },
 *                };
 *
 *                void setup() {
 *                    Wire.begin(0x2A);
 *                    k7scBegin(0x0B, K7SC_CAP_DISPLAY, btnConfigs, 200);
 *                }
 *
 *                void loop() {
 *                    k7scUpdate();
 *                    // Handle encoder button action
 *                    if (buttonsGetEncoderPress()) {
 *                        encoderSetValue(200);  // reset to default
 *                    }
 *                }
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware: KC-01-1881/1882 7-Segment Display Module v2.0
 *              IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             10 MHz or higher
 *                tinyNeoPixel Port: Port C
 */

#pragma once

#include "K7SC_Config.h"
#include "K7SC_Colors.h"
#include "K7SC_Display.h"
#include "K7SC_Encoder.h"
#include "K7SC_Buttons.h"
#include "K7SC_I2C.h"

// ============================================================
//  Top-level API
// ============================================================

/**
 * @brief Initialise all Kerbal7SegmentCore subsystems.
 *        Call after Wire.begin() in setup().
 *
 * @param typeId        Module Type ID.
 * @param capFlags      Capability flags (use K7SC_CAP_DISPLAY).
 * @param btnConfigs    Array of K7SC_NEO_COUNT ButtonConfig structs.
 * @param initialValue  Starting display value (0-9999).
 */
void k7scBegin(uint8_t typeId, uint8_t capFlags,
               const ButtonConfig* btnConfigs,
               uint16_t initialValue = 0);

/**
 * @brief Update all subsystems. Call every loop iteration.
 *
 *        Performs on each call:
 *          1. Button polling, debounce, state transitions, flash timing
 *          2. Encoder polling with acceleration
 *          3. INT pin sync
 */
void k7scUpdate();

/**
 * @file        Kerbal7SegmentCore.h
 * @version     1.1.0
 * @date        2026-04-26
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
 *                    k7scBegin(0x2A, KMC_TYPE_GPWS_INPUT,
 *                              KMC_CAP_DISPLAY, btnConfigs, 200);
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
 *                Clock:             20 MHz internal
 *                tinyNeoPixel Port: Port C
 */

#pragma once

#include <Wire.h>
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
 *        Calls Wire.begin() internally — do NOT call Wire.begin()
 *        in the sketch.
 *
 * @param i2cAddress    I2C target address for this module.
 * @param typeId        Module Type ID (KMC_TYPE_* from KerbalModuleCommon).
 * @param capFlags      Capability flags (use KMC_CAP_DISPLAY).
 * @param btnConfigs    Array of K7SC_NEO_COUNT ButtonConfig structs.
 * @param initialValue  Starting display value (0-9999). Default 0.
 */
void k7scBegin(uint8_t i2cAddress,
               uint8_t typeId, uint8_t capFlags,
               const ButtonConfig* btnConfigs,
               uint16_t initialValue = 0);

/**
 * @brief Update all subsystems. Call every loop iteration.
 *
 *        Performs on each call:
 *          1. Button polling, debounce, state transitions, flash timing
 *          2. Encoder polling with acceleration
 *          3. INT pin sync — captures button event snapshot if asserting INT
 */
void k7scUpdate();

/**
 * @brief Returns the pending button event bitmask for sketch loop() code.
 *
 *        Call this after k7scUpdate() to check for button presses that
 *        require sketch-level action (e.g. preset value jumps). This is
 *        the correct replacement for calling buttonsGetEvents() directly
 *        in loop(), which races against the internal I2C snapshot capture.
 *
 *        The returned bitmask is the same snapshot sent to the controller
 *        in the data packet. It is cleared when the controller reads the
 *        packet, not by this call — safe to read multiple times per loop.
 *
 *        Bit layout: bit0=BTN01, bit1=BTN02, bit2=BTN03, bit3=BTN_EN.
 *        Returns 0 if no button events are pending.
 *
 *        Example (Pre-Warp Time module):
 *          uint8_t events = k7scGetPendingEvents();
 *          if (events & (1 << K7SC_BIT_BTN01)) encoderSetValue(PRESET_5MIN);
 */
uint8_t k7scGetPendingEvents();

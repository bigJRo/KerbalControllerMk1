/**
 * @file        Kerbal7SegmentCore.h
 * @version     2.0.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Top-level include for the Kerbal7SegmentCore library.
 *
 *              Manages KC-01-1881/1882 hardware:
 *                - MAX7219 4-digit 7-segment display
 *                - PEC11R rotary encoder with pushbutton
 *                - 3× SK6812MINI-EA NeoPixel buttons
 *                - I2C target communication
 *
 *              Design philosophy:
 *                The library is a hardware interface layer only.
 *                All application logic (lifecycle, LED states, value
 *                tracking, button behavior) belongs in the sketch.
 *                The library reports what the hardware did via
 *                inputState and cmdState. The sketch decides what
 *                it means and what to do about it.
 *
 *              Minimum sketch:
 *
 *                #include <KerbalModuleCommon.h>
 *                #include <Kerbal7SegmentCore.h>
 *
 *                void setup() {
 *                    k7scBegin(0x2A, KMC_TYPE_GPWS_INPUT, KMC_CAP_DISPLAY);
 *                    // Initial LED state — all off
 *                    buttonsClearAll();
 *                }
 *
 *                void loop() {
 *                    k7scUpdate();
 *
 *                    // Handle master commands
 *                    if (cmdState.lifecycle == K7SC_ACTIVE) { ... }
 *                    if (cmdState.isReset)                  { ... }
 *                    if (cmdState.hasNewValue)               { ... }
 *
 *                    // Handle local input
 *                    if (inputState.buttonPressed & (1 << K7SC_BIT_BTN01)) { ... }
 *                    if (inputState.encoderChanged) {
 *                        myValue += inputState.encoderDelta;
 *                        inputState.encoderDelta   = 0;
 *                        inputState.encoderChanged = false;
 *                        displaySetValue(myValue);
 *                    }
 *
 *                    // Clear consumed button events
 *                    inputState.buttonPressed  = 0;
 *                    inputState.buttonReleased = 0;
 *                    inputState.buttonChanged  = 0;
 *
 *                    // Queue packet when state changes
 *                    if (somethingChanged) {
 *                        uint8_t payload[5] = { ... };
 *                        k7scQueuePacket(payload, 5);
 *                    }
 *                }
 *
 * @license     GNU General Public License v3.0
 */

#pragma once

#include <KerbalModuleCommon.h>
#include "K7SC_Config.h"
#include "K7SC_State.h"
#include "K7SC_Display.h"
#include "K7SC_Encoder.h"
#include "K7SC_Buttons.h"
#include "K7SC_I2C.h"

/**
 * @brief Initialise all hardware subsystems and assert BOOT_READY INT.
 *        Calls Wire.begin() internally.
 *
 * @param i2cAddress  I2C target address.
 * @param typeId      Module Type ID (KMC_TYPE_*).
 * @param capFlags    Capability flags (KMC_CAP_*).
 */
void k7scBegin(uint8_t i2cAddress, uint8_t typeId, uint8_t capFlags);

/**
 * @brief Update all subsystems. Call every loop iteration.
 *        - Polls buttons and encoder (on interval)
 *        - Accumulates edges into inputState
 *        - Clears one-shot cmdState flags from previous loop
 */
void k7scUpdate();

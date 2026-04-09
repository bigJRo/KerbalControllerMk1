/**
 * @file        KerbalJoystickCore.h
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Top-level include for the KerbalJoystickCore library.
 *
 *              KerbalJoystickCore manages analog joystick axis reading,
 *              startup calibration, deadzone and change threshold
 *              filtering, INT16 scaling, NeoPixel LED control, button
 *              input debounce, and I2C target communication for
 *              Kerbal Controller Mk1 joystick modules (KC-01-1831/1832).
 *
 *              This is not a KerbalButtonCore (KBC) module. It shares
 *              the same Kerbal Controller Mk1 I2C wire protocol but
 *              uses a device-specific 8-byte data packet defined in
 *              the protocol specification Section 9.
 *
 *              Minimum sketch:
 *
 *                #include <Wire.h>
 *                #include <KerbalJoystickCore.h>
 *
 *                // Active colors for [BTN01, BTN02]
 *                const RGBColor activeColors[KJC_NEO_COUNT] = {
 *                    KJC_GREEN,  // BTN01
 *                    KJC_AMBER,  // BTN02
 *                };
 *
 *                void setup() {
 *                    Wire.begin(0x28);
 *                    kjcBegin(0x09, KJC_CAP_JOYSTICK, activeColors);
 *                }
 *
 *                void loop() {
 *                    kjcUpdate();
 *                }
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware: KC-01-1831/1832 Joystick Module v1.0
 *              IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             10 MHz or higher
 *                tinyNeoPixel Port: Port C
 */

#pragma once

#include "KJC_Config.h"
#include "KJC_Colors.h"
#include "KJC_ADC.h"
#include "KJC_Buttons.h"
#include "KJC_I2C.h"

// ============================================================
//  Top-level API
// ============================================================

/**
 * @brief Initialise all KerbalJoystickCore subsystems.
 *        Call after Wire.begin() in setup().
 *
 *        Performs startup calibration — reads center position on
 *        all three axes. Do not touch the joystick during the
 *        first ~80ms after power-on.
 *
 * @param typeId       Module Type ID (e.g. 0x09 for Rotation).
 * @param capFlags     Capability flags (use KJC_CAP_JOYSTICK).
 * @param activeColors Array of KJC_NEO_COUNT RGBColor values
 *                     [BTN01 color, BTN02 color].
 * @param brightness   Initial ENABLED state brightness (default 32).
 */
void kjcBegin(uint8_t typeId, uint8_t capFlags,
              const RGBColor* activeColors,
              uint8_t brightness = KJC_ENABLED_BRIGHTNESS);

/**
 * @brief Update all subsystems. Call every loop iteration.
 *
 *        Performs on each call:
 *          1. Button polling and debounce (gated by poll interval)
 *          2. ADC polling with deadzone and change threshold filtering
 *             (gated by poll interval)
 *          3. Render pending LED changes from I2C commands
 *          4. INT pin sync with Option E hybrid strategy
 */
void kjcUpdate();

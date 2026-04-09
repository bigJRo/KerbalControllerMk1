/**
 * @file        KerbalButtonCore.h
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Top-level include and main class for the KerbalButtonCore
 *              library. This is the only file that module sketches need
 *              to include.
 *
 *              KerbalButtonCore ties together all library subsystems:
 *                KBCShiftReg   — 74HC165 button input with debounce
 *                KBCLEDControl — NeoPixel and discrete LED state machine
 *                KBCi2C        — I2C target command handler
 *
 *              Module sketch usage pattern:
 *
 *                #include <KerbalButtonCore.h>
 *
 *                // Define per-button active colors for this module
 *                const RGBColor activeColors[KBC_BUTTON_COUNT] = {
 *                    KBC_GREEN,   // Button 0
 *                    KBC_RED,     // Button 1
 *                    // ...
 *                };
 *
 *                // Instantiate the library with this module's identity
 *                KerbalButtonCore kbc(
 *                    KBC_TYPE_UI_CONTROL,   // module type ID
 *                    0,                     // capability flags
 *                    activeColors           // per-button active colors
 *                );
 *
 *                void setup() {
 *                    Wire.begin(0x20);      // set I2C address first
 *                    kbc.begin();
 *                }
 *
 *                void loop() {
 *                    kbc.update();
 *                }
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the KerbalButtonCore (KBC) library.
 *              Hardware: KC-01-1822 v1.1
 *              Protocol: KBC_Protocol_Spec.md v1.1
 */

#ifndef KERBAL_BUTTON_CORE_H
#define KERBAL_BUTTON_CORE_H

#include <Arduino.h>
#include <Wire.h>

// Pull in all library components
#include "KBC_Config.h"
#include "KBC_Protocol.h"
#include "KBC_Colors.h"
#include "KBC_ShiftReg.h"
#include "KBC_LEDControl.h"
#include "KBC_I2C.h"

// ============================================================
//  KerbalButtonCore class
// ============================================================

/**
 * @class  KerbalButtonCore
 * @brief  Top-level library class for a KBC button input module.
 *
 *         Instantiate once per sketch. Owns KBCShiftReg,
 *         KBCLEDControl, and KBCi2C subsystem instances and
 *         coordinates their operation.
 *
 *         The sketch is responsible for:
 *           1. Calling Wire.begin(address) before kbc.begin()
 *           2. Calling kbc.update() in every loop() iteration
 *           3. Defining the per-button active color array
 *
 *         The library handles all I2C communication, button
 *         polling, debouncing, LED state management, INT pin
 *         assertion, flash timing, and sleep/wake management
 *         automatically once begin() and update() are called.
 */
class KerbalButtonCore {
public:

    // --------------------------------------------------------
    //  Construction
    // --------------------------------------------------------

    /**
     * @brief  Construct a KerbalButtonCore instance.
     *
     * @param  moduleTypeId  Module Type ID (KBC_TYPE_* from KBC_Protocol.h).
     *                       Must match the registry entry for this module.
     * @param  capFlags      Capability flags bitmask (KBC_CAP_*).
     *                       Pass KBC_CAP_EXTENDED_STATES if this module
     *                       implements WARNING/ALERT/ARMED/PARTIAL_DEPLOY.
     * @param  activeColors  Pointer to array of KBC_BUTTON_COUNT RGBColor
     *                       values defining the ACTIVE color for each button.
     *                       Must remain valid for the lifetime of this object.
     *                       Typically declared as a file-scope const array
     *                       in the module sketch.
     */
    KerbalButtonCore(uint8_t         moduleTypeId,
                     uint8_t         capFlags,
                     const RGBColor* activeColors);

    // --------------------------------------------------------
    //  Initialisation
    // --------------------------------------------------------

    /**
     * @brief  Initialise all library subsystems.
     *
     *         Must be called once in setup() AFTER Wire.begin(address).
     *         Initialises shift registers, LED hardware, and I2C
     *         callbacks in the correct dependency order.
     *         Sets all LEDs to ENABLED state and performs initial render.
     *
     * @param  brightness  Initial ENABLED state brightness (0-255).
     *                     Defaults to KBC_ENABLED_BRIGHTNESS from config.
     */
    void begin(uint8_t brightness = KBC_ENABLED_BRIGHTNESS);

    // --------------------------------------------------------
    //  Main loop update
    // --------------------------------------------------------

    /**
     * @brief  Service all library tasks. Call once per loop() iteration.
     *
     *         Performs the following on each call:
     *           1. Button polling with debounce (on KBC_POLL_INTERVAL_MS
     *              timer — skips poll if interval has not elapsed)
     *           2. LED flash timing update (WARNING/ALERT states)
     *           3. Render pending LED changes from I2C commands
     *           4. INT pin synchronisation
     *
     *         All timing is non-blocking via millis(). Safe to call
     *         at any frequency — expensive operations are gated.
     */
    void update();

    // --------------------------------------------------------
    //  Direct subsystem access
    //
    //  Provided for advanced module sketches that need direct
    //  access to subsystem state. Most sketches should not need
    //  these — update() handles all normal operation.
    // --------------------------------------------------------

    /** @brief Direct access to shift register subsystem. */
    KBCShiftReg&   shiftReg();

    /** @brief Direct access to LED control subsystem. */
    KBCLEDControl& ledControl();

    /** @brief Direct access to I2C subsystem. */
    KBCi2C&        i2c();

    // --------------------------------------------------------
    //  Fault reporting
    // --------------------------------------------------------

    /**
     * @brief  Report a module fault condition.
     *         Sets the fault flag visible to the controller via
     *         CMD_GET_IDENTITY capability flags.
     *         Call from module sketch when a hardware anomaly is detected.
     */
    void reportFault();

private:

    // --------------------------------------------------------
    //  Subsystem instances
    // --------------------------------------------------------

    KBCShiftReg   _shiftReg;
    KBCLEDControl _ledControl;
    KBCi2C        _i2c;

    // --------------------------------------------------------
    //  Active color array pointer
    // --------------------------------------------------------

    const RGBColor* _activeColors;

    // --------------------------------------------------------
    //  Poll timing
    // --------------------------------------------------------

    uint32_t _lastPollTime;

    // --------------------------------------------------------
    //  Sleep mode poll timing
    // --------------------------------------------------------

    uint32_t _lastSleepPollTime;
};

#endif // KERBAL_BUTTON_CORE_H

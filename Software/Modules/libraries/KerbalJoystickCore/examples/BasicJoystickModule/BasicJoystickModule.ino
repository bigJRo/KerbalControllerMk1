/**
 * @file        BasicJoystickModule.ino
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       BasicJoystickModule — minimal KerbalJoystickCore example.
 *
 *              Demonstrates the simplest possible joystick module
 *              configuration. Set the I2C address, Type ID, and
 *              button colors for the specific module being flashed.
 *
 *              Do not touch the joystick during the first ~80ms
 *              after power-on — calibration reads center position
 *              at startup.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware: KC-01-1831/1832 v1.0 (ATtiny816)
 *              IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             10 MHz or higher
 *                tinyNeoPixel Port: Port C
 */

#include <Wire.h>
#include <KerbalJoystickCore.h>

// ============================================================
//  Module identity — set these per module being flashed
// ============================================================

/** @brief I2C address. 0x28 = Rotation, 0x29 = Translation. */
#define I2C_ADDRESS     0x28

/** @brief Module Type ID. 0x09 = Rotation, 0x0A = Translation. */
#define MODULE_TYPE_ID  0x09

// ============================================================
//  NeoPixel button active colors
//
//  Index 0 = BTN01, Index 1 = BTN02
//  BTN_JOY has no LED — not represented here.
//
//  Rotation:    BTN01 = Reset Trim (GREEN), BTN02 = Trim Set (AMBER)
//  Translation: BTN01 = Cycle Camera (MAGENTA), BTN02 = Camera Reset (GREEN)
// ============================================================

const RGBColor activeColors[KJC_NEO_COUNT] = {
    KJC_GREEN,   // BTN01 — Reset Trim
    KJC_AMBER,   // BTN02 — Trim Set
};

// ============================================================
//  setup()
// ============================================================

void setup() {
    // Wire.begin() must be called before kjcBegin()
    Wire.begin(I2C_ADDRESS);

    // Initialise all subsystems.
    // Startup calibration runs here — ~80ms, joystick at rest.
    kjcBegin(MODULE_TYPE_ID, KJC_CAP_JOYSTICK, activeColors);
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    // kjcUpdate() handles everything each iteration:
    //   - Button polling and debounce
    //   - ADC polling with deadzone and change threshold filtering
    //   - LED render on pending I2C commands
    //   - INT pin sync (Option E hybrid strategy)
    kjcUpdate();
}

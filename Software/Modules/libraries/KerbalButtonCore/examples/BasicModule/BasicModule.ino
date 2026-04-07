/**
 * @file        BasicModule.ino
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       BasicModule — minimal KerbalButtonCore sketch example.
 *
 *              Demonstrates the simplest possible module configuration
 *              using only core LED states (OFF, ENABLED, ACTIVE).
 *              No extended states (WARNING/ALERT/ARMED/PARTIAL_DEPLOY).
 *
 *              This sketch is a suitable starting template for any
 *              module that does not require extended state behavior.
 *
 *              To adapt for a specific module:
 *                1. Set I2C_ADDRESS to the module's assigned address
 *                2. Set MODULE_TYPE_ID to the module's KBC_TYPE_* constant
 *                3. Fill in activeColors[] with the per-button active colors
 *                   from the KBC Button Color Assignments reference
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Requires: KerbalButtonCore library
 *              Hardware: KC-01-1822 v1.1 (ATtiny816)
 *              IDE settings:
 *                Board: ATtiny816 (megaTinyCore)
 *                Clock: 10 MHz or higher
 *                tinyNeoPixel Port: Port A
 */

#include <Wire.h>
#include <KerbalButtonCore.h>

// ============================================================
//  Module identity — set these for each module
// ============================================================

/** @brief I2C address for this module. Range 0x20-0x2E. */
#define I2C_ADDRESS     0x20

/** @brief Module type ID. See KBC_Protocol.h KBC_TYPE_* constants. */
#define MODULE_TYPE_ID  KBC_TYPE_UI_CONTROL

// ============================================================
//  Per-button active colors
//
//  Define the ACTIVE state color for each of the 16 buttons.
//  Index matches KBC button index (0-15).
//  Use named colors from KBC_Colors.h or KBC_COLOR(r, g, b).
//
//  Buttons 12-15 are discrete LEDs — color is used for ON/OFF
//  only (any non-zero color = ON).
// ============================================================

const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_GREEN,   // B0  — Screenshot
    KBC_RED,     // B1  — Debug
    KBC_GREEN,   // B2  — UI Toggle
    KBC_AMBER,   // B3  — Nav Ball
    KBC_SKY,     // B4  — Map Reset
    KBC_GREEN,   // B5  — Cycle Nav
    KBC_SKY,     // B6  — Map Forward
    KBC_TEAL,    // B7  — Ship Forward
    KBC_SKY,     // B8  — Map Back
    KBC_TEAL,    // B9  — Ship Back
    KBC_SKY,     // B10 — Map Enable
    KBC_CORAL,   // B11 — IVA
    KBC_GREEN,   // B12 — discrete (unused on UI module)
    KBC_GREEN,   // B13 — discrete (unused on UI module)
    KBC_GREEN,   // B14 — discrete (unused on UI module)
    KBC_GREEN,   // B15 — discrete (unused on UI module)
};

// ============================================================
//  Library instantiation
//
//  No capability flags — this module uses core states only.
// ============================================================

KerbalButtonCore kbc(MODULE_TYPE_ID, 0, activeColors);

// ============================================================
//  setup()
// ============================================================

void setup() {
    // Wire.begin() must be called before kbc.begin()
    Wire.begin(I2C_ADDRESS);

    // Initialise all library subsystems.
    // All buttons start in ENABLED (dim white) state.
    kbc.begin();
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    // update() handles everything:
    //   - Button polling and debounce
    //   - LED flash timing (not used in this sketch)
    //   - Rendering pending LED changes from I2C commands
    //   - INT pin synchronisation
    kbc.update();
}

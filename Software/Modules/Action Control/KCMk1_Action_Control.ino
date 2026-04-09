/**
 * @file        KCMk1_Action_Control.ino
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Action Control module sketch for the Kerbal Controller Mk1.
 *
 *              This module provides action group toggles (AG1-AG10) and
 *              control point selection (CP PRI, CP ALT) for Kerbal Space
 *              Program. It also carries two discrete input signals for
 *              control mode detection (spacecraft and rover modes).
 *
 *              I2C Address: 0x22
 *              Module Type: KBC_TYPE_ACTION_CONTROL (0x03)
 *              Extended States: No
 *
 *              NeoPixel button layout (2 rows × 6 columns):
 *
 *                Col:  6(left)    5        4        3        2        1(right)
 *                Row1: B10        B8       B6       B4       B2       B0
 *                      CP PRI     AG 1     AG 2     AG 3     AG 4     AG 5
 *                      ROSE       GREEN    GREEN    GREEN    GREEN    GREEN
 *
 *                Row2: B11        B9       B7       B5       B3       B1
 *                      CP ALT     AG 6     AG 7     AG 8     AG 9     AG 10
 *                      ROSE       GREEN    GREEN    GREEN    GREEN    GREEN
 *
 *              Discrete inputs (outside NeoPixel grid, input only, no LED):
 *                B12 — CTRL_MODE_SPC — HIGH when spacecraft control mode selected
 *                B13 — Not installed
 *                B14 — CTRL_MODE_RVR — HIGH when rover control mode selected
 *                B15 — Not installed
 *
 *              Button-to-KBC index mapping:
 *                BUTTON01 (PCB) → KBC index 0  → AG 5
 *                BUTTON02 (PCB) → KBC index 1  → AG 10
 *                BUTTON03 (PCB) → KBC index 2  → AG 4
 *                BUTTON04 (PCB) → KBC index 3  → AG 9
 *                BUTTON05 (PCB) → KBC index 4  → AG 3
 *                BUTTON06 (PCB) → KBC index 5  → AG 8
 *                BUTTON07 (PCB) → KBC index 6  → AG 2
 *                BUTTON08 (PCB) → KBC index 7  → AG 7
 *                BUTTON09 (PCB) → KBC index 8  → AG 1
 *                BUTTON10 (PCB) → KBC index 9  → AG 6
 *                BUTTON11 (PCB) → KBC index 10 → CP PRI
 *                BUTTON12 (PCB) → KBC index 11 → CP ALT
 *                BUTTON13 (PCB) → KBC index 12 → CTRL_MODE_SPC (input only)
 *                BUTTON14 (PCB) → KBC index 13 → Not installed
 *                BUTTON15 (PCB) → KBC index 14 → CTRL_MODE_RVR (input only)
 *                BUTTON16 (PCB) → KBC index 15 → Not installed
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware:  KC-01-1822 v1.1 (ATtiny816)
 *              Library:   KerbalButtonCore v1.0.0
 *              IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             10 MHz or higher
 *                tinyNeoPixel Port: Port A
 *              Dependencies:
 *                ShiftIn (InfectedBytes/ArduinoShiftIn)
 *                tinyNeoPixel_Static (megaTinyCore)
 */

#include <Wire.h>
#include <KerbalButtonCore.h>

// ============================================================
//  Module identity
// ============================================================

/** @brief I2C address for the Action Control module. */
#define I2C_ADDRESS     0x22

/** @brief Module type identifier. */
#define MODULE_TYPE_ID  KBC_TYPE_ACTION_CONTROL

// ============================================================
//  Per-button active colors
//
//  Indexed by KBC button index (0-15).
//
//  B0-B9  : Action Groups — all GREEN (uniform configurable bank)
//  B10-B11: Control Points — ROSE (vessel configuration, distinct
//           from action groups and all other modules)
//  B12    : CTRL_MODE_SPC — input only, no LED hardware. KBC_OFF
//           ensures no LED drive attempt on this position.
//  B13    : Not installed — KBC_OFF
//  B14    : CTRL_MODE_RVR — input only, no LED hardware. KBC_OFF
//           ensures no LED drive attempt on this position.
//  B15    : Not installed — KBC_OFF
// ============================================================

const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_GREEN,  // B0  — AG 5    (Col 1, Row 1)
    KBC_GREEN,  // B1  — AG 10   (Col 1, Row 2)
    KBC_GREEN,  // B2  — AG 4    (Col 2, Row 1)
    KBC_GREEN,  // B3  — AG 9    (Col 2, Row 2)
    KBC_GREEN,  // B4  — AG 3    (Col 3, Row 1)
    KBC_GREEN,  // B5  — AG 8    (Col 3, Row 2)
    KBC_GREEN,  // B6  — AG 2    (Col 4, Row 1)
    KBC_GREEN,  // B7  — AG 7    (Col 4, Row 2)
    KBC_GREEN,  // B8  — AG 1    (Col 5, Row 1)
    KBC_GREEN,  // B9  — AG 6    (Col 5, Row 2)
    KBC_ROSE,   // B10 — CP PRI  (Col 6, Row 1) — control point
    KBC_ROSE,   // B11 — CP ALT  (Col 6, Row 2) — control point
    KBC_OFF,    // B12 — CTRL_MODE_SPC — input only, no LED
    KBC_OFF,    // B13 — Not installed
    KBC_OFF,    // B14 — CTRL_MODE_RVR — input only, no LED
    KBC_OFF,    // B15 — Not installed
};

// ============================================================
//  Library instantiation
//
//  No extended states on this module — capability flags = 0.
// ============================================================

KerbalButtonCore kbc(MODULE_TYPE_ID, 0, activeColors);

// ============================================================
//  setup()
// ============================================================

void setup() {
    // Wire.begin() must be called before kbc.begin()
    Wire.begin(I2C_ADDRESS);

    // Initialise all library subsystems.
    // NeoPixel buttons B0-B11 start in ENABLED (dim white) state.
    // B12-B15 start OFF — B12 and B14 are input-only with no LED
    // hardware; B13 and B15 are not installed.
    kbc.begin();
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    // update() handles all library tasks each iteration:
    //   - Button polling and debounce (every KBC_POLL_INTERVAL_MS)
    //     including CTRL_MODE_SPC (B12) and CTRL_MODE_RVR (B14)
    //     which are reported to the controller via normal button
    //     state packets when their signal changes
    //   - Render pending LED changes from I2C commands
    //   - INT pin synchronisation with button state
    //
    // No extended state flash timing on this module.
    kbc.update();
}

/**
 * @file        KC_UI_Control.ino
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       UI Control module sketch for the Kerbal Controller Mk1.
 *
 *              This module provides cinematic, navigation, and UI
 *              control functions for Kerbal Space Program. It occupies
 *              12 NeoPixel RGB button positions (KBC indices 0-11).
 *              The four discrete LED button positions (KBC indices 12-15)
 *              are not populated on this module.
 *
 *              I2C Address: 0x20
 *              Module Type: KBC_TYPE_UI_CONTROL (0x01)
 *              Extended States: No
 *
 *              Panel Layout (2 rows × 6 columns):
 *
 *                Col:  6(left)    5          4          3          2          1(right)
 *                Row1: B10        B8         B6         B4         B2         B0
 *                      Map Enable Map Back   Map Fwd    Map Reset  UI Toggle  Screenshot
 *                      SKY        SKY        SKY        SKY        GREEN      GREEN
 *
 *                Row2: B11        B9         B7         B5         B3         B1
 *                      IVA        Ship Back  Ship Fwd   Cycle Nav  Nav Ball   Debug
 *                      CORAL      TEAL       TEAL       GREEN      AMBER      RED
 *
 *              Button-to-KBC index mapping:
 *                BUTTON01 (PCB) → KBC index 0  → Screenshot
 *                BUTTON02 (PCB) → KBC index 1  → Debug
 *                BUTTON03 (PCB) → KBC index 2  → UI Toggle
 *                BUTTON04 (PCB) → KBC index 3  → Nav Ball Toggle
 *                BUTTON05 (PCB) → KBC index 4  → Map Reset
 *                BUTTON06 (PCB) → KBC index 5  → Cycle Nav
 *                BUTTON07 (PCB) → KBC index 6  → Map Forward
 *                BUTTON08 (PCB) → KBC index 7  → Ship Forward
 *                BUTTON09 (PCB) → KBC index 8  → Map Back
 *                BUTTON10 (PCB) → KBC index 9  → Ship Back
 *                BUTTON11 (PCB) → KBC index 10 → Map Enable
 *                BUTTON12 (PCB) → KBC index 11 → IVA
 *                BUTTON13-16    → KBC index 12-15 → Not installed
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

/** @brief I2C address for the UI Control module. */
#define I2C_ADDRESS     0x20

/** @brief Module type identifier. */
#define MODULE_TYPE_ID  KBC_TYPE_UI_CONTROL

// ============================================================
//  Per-button active colors
//
//  Indexed by KBC button index (0-15).
//  Buttons 12-15 are not installed on this module — set to
//  KBC_OFF so they never illuminate if a stray state is sent.
//
//  Color family reference:
//    Map family   : KBC_SKY   — navigation context
//    Ship family  : KBC_TEAL  — vessel cycling
//    General UI   : KBC_GREEN — standard active state
//    Nav Ball     : KBC_AMBER — awareness (visible display toggle)
//    IVA          : KBC_CORAL — mode shift (full perspective change)
//    Debug        : KBC_RED   — caution (developer/debug access)
// ============================================================

const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_GREEN,  // B0  — Screenshot       (Col 1, Row 1)
    KBC_RED,    // B1  — Debug            (Col 1, Row 2)
    KBC_GREEN,  // B2  — UI Toggle        (Col 2, Row 1)
    KBC_AMBER,  // B3  — Nav Ball Toggle  (Col 2, Row 2)
    KBC_SKY,    // B4  — Map Reset        (Col 3, Row 1) — map family
    KBC_GREEN,  // B5  — Cycle Nav        (Col 3, Row 2)
    KBC_SKY,    // B6  — Map Forward      (Col 4, Row 1) — map family
    KBC_TEAL,   // B7  — Ship Forward     (Col 4, Row 2) — ship family
    KBC_SKY,    // B8  — Map Back         (Col 5, Row 1) — map family
    KBC_TEAL,   // B9  — Ship Back        (Col 5, Row 2) — ship family
    KBC_SKY,    // B10 — Map Enable       (Col 6, Row 1) — map family
    KBC_CORAL,  // B11 — IVA             (Col 6, Row 2)
    KBC_OFF,    // B12 — Not installed
    KBC_OFF,    // B13 — Not installed
    KBC_OFF,    // B14 — Not installed
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
    // All installed buttons start in ENABLED (dim white) state.
    // Buttons 12-15 start OFF (no hardware present).
    kbc.begin();
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    // update() handles all library tasks each iteration:
    //   - Button polling and debounce (every KBC_POLL_INTERVAL_MS)
    //   - Render pending LED changes from I2C commands
    //   - INT pin synchronisation with button state
    //
    // No extended state flash timing on this module.
    kbc.update();
}

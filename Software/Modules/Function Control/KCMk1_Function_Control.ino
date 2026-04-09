/**
 * @file        KCMk1_Function_Control.ino
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Function Control module sketch for the Kerbal Controller Mk1.
 *
 *              This module provides science experiment, engine group,
 *              atmosphere, and thermal control functions for Kerbal
 *              Space Program. It occupies 12 NeoPixel RGB button
 *              positions (KBC indices 0-11). The four discrete LED
 *              button positions (KBC indices 12-15) are not populated
 *              on this module.
 *
 *              I2C Address: 0x21
 *              Module Type: KBC_TYPE_FUNCTION_CONTROL (0x02)
 *              Extended States: No
 *
 *              Panel Layout (6 rows × 2 columns):
 *
 *                        Col 1 (left)          Col 2 (right)
 *                Row 1:  B1                    B0
 *                        Engine Alt Mode       Science Collect
 *                        ORANGE                PURPLE
 *
 *                Row 2:  B3                    B2
 *                        Engine Group 1        Science Group 1
 *                        YELLOW                INDIGO
 *
 *                Row 3:  B5                    B4
 *                        Engine Group 2        Science Group 2
 *                        CHARTREUSE            BLUE
 *
 *                Row 4:  B7                    B6
 *                        LES                   Air Intake
 *                        RED                   TEAL
 *
 *                Row 5:  B9                    B8
 *                        Lock Surfaces         Air Brake
 *                        AMBER                 CYAN
 *
 *                Row 6:  B11                   B10
 *                        Heat Shield Deploy    Heat Shield Release
 *                        GREEN                 RED
 *
 *              Button-to-KBC index mapping:
 *                BUTTON01 (PCB) → KBC index 0  → Science Collect
 *                BUTTON02 (PCB) → KBC index 1  → Engine Alt Mode
 *                BUTTON03 (PCB) → KBC index 2  → Science Group 1
 *                BUTTON04 (PCB) → KBC index 3  → Engine Group 1
 *                BUTTON05 (PCB) → KBC index 4  → Science Group 2
 *                BUTTON06 (PCB) → KBC index 5  → Engine Group 2
 *                BUTTON07 (PCB) → KBC index 6  → Air Intake
 *                BUTTON08 (PCB) → KBC index 7  → LES
 *                BUTTON09 (PCB) → KBC index 8  → Air Brake
 *                BUTTON10 (PCB) → KBC index 9  → Lock Surfaces
 *                BUTTON11 (PCB) → KBC index 10 → Heat Shield Release
 *                BUTTON12 (PCB) → KBC index 11 → Heat Shield Deploy
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

/** @brief I2C address for the Function Control module. */
#define I2C_ADDRESS     0x21

/** @brief Module type identifier. */
#define MODULE_TYPE_ID  KBC_TYPE_FUNCTION_CONTROL

// ============================================================
//  Per-button active colors
//
//  Indexed by KBC button index (0-15).
//  Buttons 12-15 are not installed on this module.
//
//  Color family reference:
//    Science family    : PURPLE → INDIGO → BLUE (collect to group 2)
//    Engine family     : ORANGE → YELLOW → CHARTREUSE (alt to group 2)
//    Atmosphere family : TEAL (intake), CYAN (airbrake)
//    Thermal           : GREEN (deploy), RED (release — irreversible)
//    Irreversible      : RED (LES, heat shield release)
//    Awareness         : AMBER (lock surfaces)
// ============================================================

const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_PURPLE,      // B0  — Science Collect     (Col 2, Row 1) — science family
    KBC_ORANGE,      // B1  — Engine Alt Mode      (Col 1, Row 1) — engine family
    KBC_INDIGO,      // B2  — Science Group 1      (Col 2, Row 2) — science family
    KBC_YELLOW,      // B3  — Engine Group 1       (Col 1, Row 2) — engine family
    KBC_BLUE,        // B4  — Science Group 2      (Col 2, Row 3) — science family
    KBC_CHARTREUSE,  // B5  — Engine Group 2       (Col 1, Row 3) — engine family
    KBC_TEAL,        // B6  — Air Intake           (Col 2, Row 4) — atmosphere family
    KBC_RED,         // B7  — LES                  (Col 1, Row 4) — irreversible
    KBC_CYAN,        // B8  — Air Brake            (Col 2, Row 5) — atmosphere family
    KBC_AMBER,       // B9  — Lock Surfaces        (Col 1, Row 5) — awareness
    KBC_RED,         // B10 — Heat Shield Release  (Col 2, Row 6) — irreversible
    KBC_GREEN,       // B11 — Heat Shield Deploy   (Col 1, Row 6) — thermal
    KBC_OFF,         // B12 — Not installed
    KBC_OFF,         // B13 — Not installed
    KBC_OFF,         // B14 — Not installed
    KBC_OFF,         // B15 — Not installed
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

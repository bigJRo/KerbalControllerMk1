/**
 * @file        KCMk1_Function_Control.ino
 * @version     2.0
 * @date        2026-06-28
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Function Control module sketch for the Kerbal Controller Mk1.
 *
 *              This module provides launch, flight-system, aerodynamic, and
 *              engine/science group functions for Kerbal Space Program over
 *              12 NeoPixel RGB buttons (KBC indices 0-11), plus the eight
 *              Switch Group 1 panel switches read as discrete inputs at KBC
 *              indices 16-23 via a third shift register (U16). The four
 *              discrete LED positions (KBC indices 12-15) are no-connects.
 *
 *              (Resolves Module UI Reference Open Item #7/TODO #7: button
 *              layout updated to the v5.x panel design and Switch Group 1
 *              added via 24-input / 3-byte shift-register reads.)
 *
 *              I2C Address: 0x21
 *              Module Type: KBC_TYPE_FUNCTION_CONTROL (0x02)
 *              Extended States: No
 *              Inputs: 24 (KBC_INPUT_COUNT) via 3 shift registers
 *
 *              NeoPixel Layout (6 rows × 2 columns):
 *
 *                        Col 1 (left)          Col 2 (right)
 *                Row 1:  B0 LES                B1 Fairing Jettison
 *                        RED                   AMBER
 *                Row 2:  B2 Air Intake         B3 Lock Surfaces
 *                        TEAL                  SKY
 *                Row 3:  B4 Airbrake           B5 Reaction Wheel Disable
 *                        CYAN                  AMBER
 *                Row 4:  B6 Engine Alt Mode    B7 Science Collect
 *                        ORANGE                PURPLE
 *                Row 5:  B8 Engine Group 1     B9 Science Group 1
 *                        YELLOW                INDIGO
 *                Row 6:  B10 Engine Group 2    B11 Science Group 2
 *                        CHARTREUSE            BLUE
 *
 *              Switch Group 1 — discrete inputs (no LED), reported in the
 *              button-event payload bytes for inputs 16-23:
 *                KBC index 16 → MSTR     (OPER / RESET)
 *                KBC index 17 → DISPL    (OPER / CLR)
 *                KBC index 18 → ENGINE   (SAFE / ARM)
 *                KBC index 19 → THROTTLE (INOP / ACT)
 *                KBC index 20 → SCE      (NORM / AUX)
 *                KBC index 21 → UPTLM    (INHIB / XMIT)
 *                KBC index 22 → LTG      (NORM / TEST)
 *                KBC index 23 → THRTL    (STD / FINE)
 *              Switch semantics and controller actions are resolved on the
 *              main controller; this module reports raw input state only.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware:  KC-01-1812 v1.1 (ATtiny816)
 *              Library:   KerbalButtonCore v2.0.0
 *              IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             10 MHz or higher
 *                tinyNeoPixel Port: Port A
 *              Dependencies:
 *                ShiftIn (InfectedBytes/ArduinoShiftIn)
 *                tinyNeoPixel_Static (megaTinyCore)
 */

#include <Wire.h>

// ============================================================
//  Input width — 24 inputs (Switch Group 1)
//
//  Function Control adds a third 74HC165 (U16) for the eight Switch
//  Group 1 panel switches at KBC indices 16-23. These overrides MUST be
//  defined before including KerbalButtonCore.h so the library widens its
//  input path and response packet (9-byte packet). See KBC_Config.h.
// ============================================================

#define KBC_INPUT_COUNT     24
#define KBC_SHIFTREG_COUNT  3

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
//  Indexed by KBC button index (0-15) — the 16 LED positions. Buttons
//  12-15 are no-connects; switch inputs 16-23 have no LED.
//
//  Color family reference:
//    Science family    : PURPLE → INDIGO → BLUE (collect to group 2)
//    Engine family     : ORANGE → YELLOW → CHARTREUSE (alt to group 2)
//    Atmosphere family : TEAL (intake), CYAN (airbrake)
//    Irreversible      : RED (LES)
//    Awareness         : AMBER (fairing jettison, RW disable), SKY (lock surfaces)
// ============================================================

const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_RED,         // B0  — LES                  (Col 1, Row 1) — irreversible
    KBC_AMBER,       // B1  — Fairing Jettison     (Col 2, Row 1) — awareness
    KBC_TEAL,        // B2  — Air Intake           (Col 1, Row 2) — atmosphere family
    KBC_SKY,         // B3  — Lock Surfaces        (Col 2, Row 2) — awareness
    KBC_CYAN,        // B4  — Airbrake             (Col 1, Row 3) — atmosphere family
    KBC_AMBER,       // B5  — Reaction Wheel Disable (Col 2, Row 3) — awareness
    KBC_ORANGE,      // B6  — Engine Alt Mode      (Col 1, Row 4) — engine family
    KBC_PURPLE,      // B7  — Science Collect      (Col 2, Row 4) — science family
    KBC_YELLOW,      // B8  — Engine Group 1       (Col 1, Row 5) — engine family
    KBC_INDIGO,      // B9  — Science Group 1      (Col 2, Row 5) — science family
    KBC_CHARTREUSE,  // B10 — Engine Group 2       (Col 1, Row 6) — engine family
    KBC_BLUE,        // B11 — Science Group 2      (Col 2, Row 6) — science family
    KBC_OFF,         // B12 — No-connect (PCB)
    KBC_OFF,         // B13 — No-connect (PCB)
    KBC_OFF,         // B14 — No-connect (PCB)
    KBC_OFF,         // B15 — No-connect (PCB)
    // KBC indices 16-23 are Switch Group 1 discrete inputs (U16) with no
    // LED hardware — they have no entry in this colour array (sized to the
    // 16 LED positions, KBC_BUTTON_COUNT).
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

    // Initialise all library subsystems. The module powers on dark
    // (BOOT_READY → DISABLED); NeoPixel buttons B0-B11 light to ENABLED
    // on CMD_ENABLE. Buttons 12-15 are no-connects. The library reads all
    // 24 inputs (3 shift registers) and reports Switch Group 1 (16-23) in
    // the button-event payload.
    kbc.begin();
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    // update() handles all library tasks each iteration:
    //   - Button/switch polling and debounce across all 24 inputs
    //   - Render pending LED changes from I2C commands
    //   - INT pin synchronisation with input state
    //
    // No extended state flash timing on this module.
    kbc.update();
}

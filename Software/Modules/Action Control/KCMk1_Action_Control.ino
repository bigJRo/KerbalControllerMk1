/**
 * @file        KCMk1_Action_Control.ino
 * @version     2.0
 * @date        2026-06-28
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Action Control module sketch for the Kerbal Controller Mk1.
 *
 *              This module provides twelve custom action group toggles
 *              (AG1–AG12) for Kerbal Space Program. All twelve positions
 *              are uniform GREEN action-group buttons. (Resolves Module UI
 *              Reference Open Item #7/TODO #8: the former CP PRI / CP ALT
 *              control-point buttons at B10/B11 are now AG1/AG7 — the
 *              control-point functions moved to the AUX CTRL module, and the
 *              CTRL_MODE discrete inputs moved to the direct-wired Ctrl Mode
 *              Toggle on the joystick.)
 *
 *              I2C Address: 0x22
 *              Module Type: KBC_TYPE_ACTION_CONTROL (0x03)
 *              Extended States: No
 *
 *              NeoPixel button layout (2 rows × 6 columns), reads
 *              right-to-left (AG1 top-right, AG12 bottom-left):
 *
 *                Col:  1(left)    2        3        4        5        6(right)
 *                Row1: B0         B2       B4       B6       B8       B10
 *                      AG 6       AG 5     AG 4     AG 3     AG 2     AG 1
 *                      GREEN      GREEN    GREEN    GREEN    GREEN    GREEN
 *
 *                Row2: B1         B3       B5       B7       B9       B11
 *                      AG 12      AG 11    AG 10    AG 9     AG 8     AG 7
 *                      GREEN      GREEN    GREEN    GREEN    GREEN    GREEN
 *
 *                B12–B15 are no-connects on the PCB (not installed).
 *
 *              Button-to-KBC index mapping (AG number is resolved
 *              controller-side; the module reports button events only):
 *                BUTTON01 (PCB) → KBC index 0  → AG 6
 *                BUTTON02 (PCB) → KBC index 1  → AG 12
 *                BUTTON03 (PCB) → KBC index 2  → AG 5
 *                BUTTON04 (PCB) → KBC index 3  → AG 11
 *                BUTTON05 (PCB) → KBC index 4  → AG 4
 *                BUTTON06 (PCB) → KBC index 5  → AG 10
 *                BUTTON07 (PCB) → KBC index 6  → AG 3
 *                BUTTON08 (PCB) → KBC index 7  → AG 9
 *                BUTTON09 (PCB) → KBC index 8  → AG 2
 *                BUTTON10 (PCB) → KBC index 9  → AG 8
 *                BUTTON11 (PCB) → KBC index 10 → AG 1
 *                BUTTON12 (PCB) → KBC index 11 → AG 7
 *                BUTTON13–16    → KBC index 12-15 → Not installed
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware:  KC-01-1802 v1.1 (ATtiny816)
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
//  B0-B11 : Action Groups AG1–AG12 — all GREEN (uniform configurable
//           bank; the specific AG number is resolved controller-side)
//  B12-B15: Not installed (no-connect on the PCB) — KBC_OFF
// ============================================================

const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_GREEN,  // B0  — AG 6    (Col 1, Row 1)
    KBC_GREEN,  // B1  — AG 12   (Col 1, Row 2)
    KBC_GREEN,  // B2  — AG 5    (Col 2, Row 1)
    KBC_GREEN,  // B3  — AG 11   (Col 2, Row 2)
    KBC_GREEN,  // B4  — AG 4    (Col 3, Row 1)
    KBC_GREEN,  // B5  — AG 10   (Col 3, Row 2)
    KBC_GREEN,  // B6  — AG 3    (Col 4, Row 1)
    KBC_GREEN,  // B7  — AG 9    (Col 4, Row 2)
    KBC_GREEN,  // B8  — AG 2    (Col 5, Row 1)
    KBC_GREEN,  // B9  — AG 8    (Col 5, Row 2)
    KBC_GREEN,  // B10 — AG 1    (Col 6, Row 1) — was CP PRI (moved to AUX CTRL)
    KBC_GREEN,  // B11 — AG 7    (Col 6, Row 2) — was CP ALT (moved to AUX CTRL)
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

    // Initialise all library subsystems. The module powers on dark
    // (BOOT_READY → DISABLED); buttons light to ENABLED on CMD_ENABLE.
    // B12-B15 are not installed (no-connect on the PCB).
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

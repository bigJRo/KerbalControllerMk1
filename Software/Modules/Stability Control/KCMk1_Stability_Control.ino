/**
 * @file        KCMk1_Stability_Control.ino
 * @version     2.0
 * @date        2026-06-28
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Stability Control module sketch for the Kerbal Controller Mk1.
 *
 *              This module provides ten SAS mode buttons, an RCS toggle,
 *              and an Invert modifier for Kerbal Space Program, plus two
 *              discrete staging inputs. The staging enable position (B15)
 *              has both a discrete input and a discrete LED output acting
 *              as a safety interlock indicator.
 *
 *              (Resolves Module UI Reference Open Item #10/TODO #10: RCS
 *              is now a NeoPixel button at B10; the former SAS_ENA / RCS_ENA
 *              discrete inputs at B12/B13 are removed — those switches moved
 *              to Switch Group 2 on the Vehicle Control module.)
 *
 *              I2C Address: 0x23
 *              Module Type: KBC_TYPE_STABILITY_CONTROL (0x04)
 *              Extended States: No
 *
 *              NeoPixel button layout (2 rows x 6 columns):
 *
 *                Col:  6(left)    5          4          3          2          1(right)
 *                Row1: B10        B8         B6         B4         B2         B0
 *                      RCS        Stab Asst  Prograde   Normal     Radial In  Target
 *                      GREEN      GREEN      GREEN      GREEN      GREEN      GREEN
 *
 *                Row2: B11        B9         B7         B5         B3         B1
 *                      Invert     Maneuver   Retrograde Anti-Norm  Radial Out Anti-Tgt
 *                      AMBER      GREEN      GREEN      GREEN      GREEN      GREEN
 *
 *              Discrete positions (outside NeoPixel grid):
 *                B12 - Not installed (SAS_ENA moved to Switch Group 2)
 *                B13 - Not installed (RCS_ENA moved to Switch Group 2)
 *                B14 - Stage        - discrete input, latching, no LED
 *                B15 - Stage Enable - discrete input + discrete LED interlock
 *
 *              Button-to-KBC index mapping:
 *                BUTTON01 (PCB) -> KBC index 0  -> Target
 *                BUTTON02 (PCB) -> KBC index 1  -> Anti-Target
 *                BUTTON03 (PCB) -> KBC index 2  -> Radial In
 *                BUTTON04 (PCB) -> KBC index 3  -> Radial Out
 *                BUTTON05 (PCB) -> KBC index 4  -> Normal
 *                BUTTON06 (PCB) -> KBC index 5  -> Anti-Normal
 *                BUTTON07 (PCB) -> KBC index 6  -> Prograde
 *                BUTTON08 (PCB) -> KBC index 7  -> Retrograde
 *                BUTTON09 (PCB) -> KBC index 8  -> Stab Assist
 *                BUTTON10 (PCB) -> KBC index 9  -> Maneuver
 *                BUTTON11 (PCB) -> KBC index 10 -> RCS
 *                BUTTON12 (PCB) -> KBC index 11 -> Invert
 *                BUTTON13 (PCB) -> KBC index 12 -> Not installed
 *                BUTTON14 (PCB) -> KBC index 13 -> Not installed
 *                BUTTON15 (PCB) -> KBC index 14 -> Stage (input only)
 *                BUTTON16 (PCB) -> KBC index 15 -> Stage Enable (input + LED)
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

/** @brief I2C address for the Stability Control module. */
#define I2C_ADDRESS     0x23

/** @brief Module type identifier. */
#define MODULE_TYPE_ID  KBC_TYPE_STABILITY_CONTROL

// ============================================================
//  Per-button active colors
//
//  Indexed by KBC button index (0-15).
//
//  B0-B9  : SAS modes - all GREEN (one active at a time; position
//           identifies the mode, color uniformity is intentional)
//  B10    : RCS - GREEN (RCS toggle)
//  B11    : Invert - AMBER (modifier, draws attention)
//  B12-B13: Not installed - KBC_OFF (SAS_ENA/RCS_ENA moved to Switch Group 2)
//  B14    : Stage - discrete input, no LED - KBC_OFF
//  B15    : Stage Enable - discrete input + discrete LED interlock.
//           KBC_DISCRETE_ON so the ACTIVE state lights the discrete LED.
// ============================================================

const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_GREEN,        // B0  - Target       (Col 1, Row 1) - SAS mode
    KBC_GREEN,        // B1  - Anti-Target  (Col 1, Row 2) - SAS mode
    KBC_GREEN,        // B2  - Radial In    (Col 2, Row 1) - SAS mode
    KBC_GREEN,        // B3  - Radial Out   (Col 2, Row 2) - SAS mode
    KBC_GREEN,        // B4  - Normal       (Col 3, Row 1) - SAS mode
    KBC_GREEN,        // B5  - Anti-Normal  (Col 3, Row 2) - SAS mode
    KBC_GREEN,        // B6  - Prograde     (Col 4, Row 1) - SAS mode
    KBC_GREEN,        // B7  - Retrograde   (Col 4, Row 2) - SAS mode
    KBC_GREEN,        // B8  - Stab Assist  (Col 5, Row 1) - SAS mode
    KBC_GREEN,        // B9  - Maneuver     (Col 5, Row 2) - SAS mode
    KBC_GREEN,        // B10 - RCS          (Col 6, Row 1) - RCS toggle
    KBC_AMBER,        // B11 - Invert       (Col 6, Row 2) - modifier
    KBC_OFF,          // B12 - Not installed (SAS_ENA moved to Switch Group 2)
    KBC_OFF,          // B13 - Not installed (RCS_ENA moved to Switch Group 2)
    KBC_OFF,          // B14 - Stage (latching toggle, discrete input, no LED)
    KBC_DISCRETE_ON,  // B15 - Stage Enable (discrete input + discrete LED)
};

// ============================================================
//  Library instantiation
//
//  No extended states on this module - capability flags = 0.
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
    // on CMD_ENABLE. B12-B13 are not installed; B14/B15 are discrete
    // staging inputs (B15 also drives a discrete interlock LED, set by
    // the controller via CMD_SET_LED_STATE).
    kbc.begin();
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    // update() handles all library tasks each iteration:
    //   - Button polling and debounce (every KBC_POLL_INTERVAL_MS)
    //     including the B14/B15 discrete staging inputs, reported to
    //     the controller via normal button state packets when they change
    //   - Render pending LED changes from I2C commands
    //   - INT pin synchronisation with button state
    //
    // No extended state flash timing on this module.
    kbc.update();
}

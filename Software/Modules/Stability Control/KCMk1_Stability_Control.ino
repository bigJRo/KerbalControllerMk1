/**
 * @file        KCMk1_Stability_Control.ino
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Stability Control module sketch for the Kerbal Controller Mk1.
 *
 *              This module provides SAS mode selection and stability
 *              control functions for Kerbal Space Program. It also
 *              carries discrete inputs for SAS enable, RCS enable,
 *              staging, and staging enable signals. The staging enable
 *              position (B15) has both a button input and a discrete
 *              LED output acting as a safety interlock indicator.
 *
 *              I2C Address: 0x23
 *              Module Type: KBC_TYPE_STABILITY_CONTROL (0x04)
 *              Extended States: No
 *
 *              NeoPixel button layout (2 rows x 6 columns):
 *
 *                Col:  6(left)    5          4          3          2          1(right)
 *                Row1: B10        B8         B6         B4         B2         B0
 *                      N/A        Stab Asst  Prograde   Normal     Radial In  Target
 *                      (empty)    GREEN      GREEN      GREEN      GREEN      GREEN
 *
 *                Row2: B11        B9         B7         B5         B3         B1
 *                      Invert     Maneuver   Retrograde Anti-Norm  Radial Out Anti-Tgt
 *                      AMBER      GREEN      GREEN      GREEN      GREEN      GREEN
 *
 *              Discrete positions (outside NeoPixel grid):
 *                B12 - SAS_ENA  - input only, no LED
 *                B13 - RCS_ENA  - input only, no LED
 *                B14 - Not installed
 *                B15 - Not installed
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
 *                BUTTON11 (PCB) -> KBC index 10 -> N/A (not installed)
 *                BUTTON12 (PCB) -> KBC index 11 -> Invert
 *                BUTTON13 (PCB) -> KBC index 12 -> SAS_ENA (input only)
 *                BUTTON14 (PCB) -> KBC index 13 -> RCS_ENA (input only)
 *                BUTTON15 (PCB) -> KBC index 14 -> Not installed
 *                BUTTON16 (PCB) -> KBC index 15 -> Not installed
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
//  B10    : Not installed - KBC_OFF
//  B11    : Invert - AMBER (modifier, draws attention)
//  B12    : SAS_ENA - input only, no LED - KBC_OFF
//  B13    : RCS_ENA - input only, no LED - KBC_OFF
//  B14    : Not installed - KBC_OFF
//  B15    : Not installed - KBC_OFF
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
    KBC_OFF,          // B10 - Not installed
    KBC_AMBER,        // B11 - Invert       (Col 6, Row 2) - modifier
    KBC_OFF,          // B12 - SAS_ENA - input only, no LED
    KBC_OFF,          // B13 - RCS_ENA - input only, no LED
    KBC_OFF,          // B14 - Not installed
    KBC_OFF,          // B15 - Not installed
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

    // Initialise all library subsystems.
    // NeoPixel buttons B0-B9, B11 start in ENABLED (dim white) state.
    // B10 starts OFF (not installed).
    // B12-B14 start OFF (input only, no LED hardware).
    // B15 starts OFF (controller manages staging interlock state).
    kbc.begin();
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    // update() handles all library tasks each iteration:
    //   - Button polling and debounce (every KBC_POLL_INTERVAL_MS)
    //     including SAS_ENA (B12) and RCS_ENA (B13) which are
    //     reported to the controller via normal button state packets
    //     when their signals change
    //   - Render pending LED changes from I2C commands
    //   - INT pin synchronisation with button state
    //
    // No extended state flash timing on this module.
    kbc.update();
}

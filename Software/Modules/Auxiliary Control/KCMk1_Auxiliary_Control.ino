/**
 * @file        KCMk1_Auxiliary_Control.ino
 * @version     1.0
 * @date        2026-06-28
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Auxiliary Control (AUX CTRL) module sketch for the
 *              Kerbal Controller Mk1.
 *
 *              Twelve NeoPixel buttons combining EVA (kerbal) controls and
 *              general flight Control-Point selection. This module replaces
 *              the former standalone 6-button EVA module: it is now a
 *              standard 12-button KerbalButtonCore target (KC-01-1802),
 *              identical in structure to the other button modules.
 *
 *              All button *functions* are resolved controller-side; the
 *              module only reports button events and renders LED states:
 *                - EVA functions (B0-B7, B9) are active only when the
 *                  controller has isEVA = true (KEYBOARD_EMULATOR keys).
 *                - Cruise Control (B8) is a controller-side hold for rover
 *                  mode.
 *                - Control-Point selection (B10 CP Toggle PRI/ALT, B11 CP
 *                  Docking Port) fires custom action groups (base CAG 35/36
 *                  /37) and is active in all modes.
 *
 *              I2C Address: 0x26
 *              Module Type: KBC_TYPE_AUX_CTRL (0x07)
 *              Extended States: No
 *
 *              NeoPixel button layout (2 rows x 6 columns), Col 6 left to
 *              Col 1 right (B0 top-right), matching the panel convention:
 *
 *                Col:  6(left)    5        4        3        2        1(right)
 *                Row1: B10        B8       B6       B4       B2       B0
 *                      CP Toggle  Cruise   EVA      Jump/    Board    EVA
 *                                 Control  Chute    Let Go   Craft    Lights
 *                      ROSE*      GOLD     AMBER    CHARTRSE GREEN    MINT
 *
 *                Row2: B11        B9       B7       B5       B3       B1
 *                      CP Dock    Plant    Helmet   Grab     EVA      Jetpack
 *                      Port       Flag     Toggle            Constr.  Enable
 *                      PINK       SEAFOAM  SKY      SEAFOAM  TEAL     LIME
 *
 *                * B10 CP Toggle: KBC_LED_ACTIVE renders ROSE (Primary);
 *                  the controller sends KBC_LED_ACTIVE_ALT to render CORAL
 *                  (Alternate) via the altColors array below. CP Docking
 *                  Port (B11) is a separate button. See README for the
 *                  mutually-exclusive PRI/ALT/Docking-Port logic.
 *
 *                B12-B15 are no-connects on the PCB (not installed).
 *
 *              Button-to-KBC index mapping (function resolved controller-side;
 *              the module reports button events only):
 *                BUTTON01 (PCB) -> KBC index 0  -> EVA Lights
 *                BUTTON02 (PCB) -> KBC index 1  -> Jetpack Enable
 *                BUTTON03 (PCB) -> KBC index 2  -> Board Craft
 *                BUTTON04 (PCB) -> KBC index 3  -> EVA Construction
 *                BUTTON05 (PCB) -> KBC index 4  -> Jump / Let Go
 *                BUTTON06 (PCB) -> KBC index 5  -> Grab
 *                BUTTON07 (PCB) -> KBC index 6  -> EVA Chute
 *                BUTTON08 (PCB) -> KBC index 7  -> Helmet Toggle
 *                BUTTON09 (PCB) -> KBC index 8  -> Cruise Control
 *                BUTTON10 (PCB) -> KBC index 9  -> Plant Flag
 *                BUTTON11 (PCB) -> KBC index 10 -> CP Toggle (PRI/ALT)
 *                BUTTON12 (PCB) -> KBC index 11 -> CP Docking Port
 *                BUTTON13-16    -> KBC index 12-15 -> Not installed
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware:  KC-01-1802 v1.1 (ATtiny816)
 *              Library:   KerbalButtonCore v2.2.0
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

/** @brief I2C address for the Auxiliary Control module. */
#define I2C_ADDRESS     0x26

/** @brief Module type identifier. */
#define MODULE_TYPE_ID  KBC_TYPE_AUX_CTRL

// ============================================================
//  Per-button active colors
//
//  Indexed by KBC button index (0-15). Colours per Module UI
//  Reference 0x26 (canonical). Functions are resolved controller-side.
//
//  B0  EVA Lights       - MINT
//  B1  Jetpack Enable   - LIME
//  B2  Board Craft      - GREEN
//  B3  EVA Construction - TEAL
//  B4  Jump / Let Go    - CHARTREUSE
//  B5  Grab             - SEAFOAM
//  B6  EVA Chute        - AMBER
//  B7  Helmet Toggle    - SKY
//  B8  Cruise Control   - GOLD
//  B9  Plant Flag       - SEAFOAM (controller flashes briefly on press)
//  B10 CP Toggle        - ROSE  (Primary; controller uses CORAL for Alt)  CAG 35/36
//  B11 CP Docking Port  - PINK                                            CAG 37
//  B12-B15              - OFF (no-connect on the PCB)
// ============================================================

const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_MINT,        // B0  - EVA Lights       (Col 1, Row 1)
    KBC_LIME,        // B1  - Jetpack Enable   (Col 1, Row 2)
    KBC_GREEN,       // B2  - Board Craft      (Col 2, Row 1)
    KBC_TEAL,        // B3  - EVA Construction (Col 2, Row 2)
    KBC_CHARTREUSE,  // B4  - Jump / Let Go    (Col 3, Row 1)
    KBC_SEAFOAM,     // B5  - Grab             (Col 3, Row 2)
    KBC_AMBER,       // B6  - EVA Chute        (Col 4, Row 1)
    KBC_SKY,         // B7  - Helmet Toggle    (Col 4, Row 2)
    KBC_GOLD,        // B8  - Cruise Control   (Col 5, Row 1)
    KBC_SEAFOAM,     // B9  - Plant Flag       (Col 5, Row 2)
    KBC_ROSE,        // B10 - CP Toggle (PRI)  (Col 6, Row 1)
    KBC_PINK,        // B11 - CP Docking Port  (Col 6, Row 2)
    KBC_OFF,         // B12 - Not installed
    KBC_OFF,         // B13 - Not installed
    KBC_OFF,         // B14 - Not installed
    KBC_OFF,         // B15 - Not installed
};

// ============================================================
//  Per-button ALTERNATE active colors (KBC_LED_ACTIVE_ALT)
//
//  Only B10 (CP Toggle) uses a second active colour: CORAL for the
//  Alternate control point. ACTIVE renders ROSE (Primary); the
//  controller sends KBC_LED_ACTIVE_ALT to switch B10 to CORAL. All
//  other positions are unused here (KBC_OFF).
// ============================================================

const RGBColor altColors[KBC_BUTTON_COUNT] = {
    KBC_OFF,         // B0
    KBC_OFF,         // B1
    KBC_OFF,         // B2
    KBC_OFF,         // B3
    KBC_OFF,         // B4
    KBC_OFF,         // B5
    KBC_OFF,         // B6
    KBC_OFF,         // B7
    KBC_OFF,         // B8
    KBC_OFF,         // B9
    KBC_CORAL,       // B10 - CP Toggle ALTERNATE control point
    KBC_OFF,         // B11
    KBC_OFF,         // B12
    KBC_OFF,         // B13
    KBC_OFF,         // B14
    KBC_OFF,         // B15
};

// ============================================================
//  Library instantiation
//
//  No extended states — capability flags = 0. The altColors array
//  supplies the CP Toggle Alternate colour (KBC_LED_ACTIVE_ALT, B10).
// ============================================================

KerbalButtonCore kbc(MODULE_TYPE_ID, 0, activeColors, altColors);

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

/**
 * @file        KCMk1_Time_Control.ino
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Time Control module sketch for the Kerbal Controller Mk1.
 *
 *              This module provides time warp control, warp target
 *              selection, physics warp, game pause, and quicksave /
 *              quickload functions for Kerbal Space Program.
 *
 *              The panel uses a yellow family color theme reflecting
 *              the unusual and consequential nature of time manipulation.
 *              Warp targets share YELLOW, warp rate controls use
 *              WARM WHITE, physics warp uses CHARTREUSE, and game
 *              state actions use AMBER (pause/stop), LIME (save),
 *              and MINT (load).
 *
 *              This module uses 12 NeoPixel RGB button positions
 *              (KBC indices 0-11). The four discrete LED button
 *              positions (KBC indices 12-15) are not populated.
 *
 *              I2C Address: 0x25
 *              Module Type: KBC_TYPE_TIME_CONTROL (0x06)
 *              Extended States: No
 *
 *              NeoPixel button layout (2 rows x 6 columns):
 *
 *                Col:  6(left)    5        4        3        2        1(right)
 *                Row1: B10        B8       B6       B4       B2       B0
 *                      Pause      Morn     SOI      MNVR     PeA      ApA
 *                      AMBER      YELLOW   YELLOW   YELLOW   YELLOW   YELLOW
 *
 *                Row2: B11        B9       B7       B5       B3       B1
 *                      Save       Load     Back     Fwd      PHYS     Stop
 *                      LIME       MINT     WARM WHT WARM WHT CHARTRS  AMBER
 *
 *              Button-to-KBC index mapping:
 *                BUTTON01 (PCB) -> KBC index 0  -> ApA
 *                BUTTON02 (PCB) -> KBC index 1  -> Stop
 *                BUTTON03 (PCB) -> KBC index 2  -> PeA
 *                BUTTON04 (PCB) -> KBC index 3  -> PHYS
 *                BUTTON05 (PCB) -> KBC index 4  -> MNVR
 *                BUTTON06 (PCB) -> KBC index 5  -> Fwd
 *                BUTTON07 (PCB) -> KBC index 6  -> SOI
 *                BUTTON08 (PCB) -> KBC index 7  -> Back
 *                BUTTON09 (PCB) -> KBC index 8  -> Morn
 *                BUTTON10 (PCB) -> KBC index 9  -> Load
 *                BUTTON11 (PCB) -> KBC index 10 -> Pause
 *                BUTTON12 (PCB) -> KBC index 11 -> Save
 *                BUTTON13-16    -> KBC index 12-15 -> Not installed
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

/** @brief I2C address for the Time Control module. */
#define I2C_ADDRESS     0x25

/** @brief Module type identifier. */
#define MODULE_TYPE_ID  KBC_TYPE_TIME_CONTROL

// ============================================================
//  Per-button active colors
//
//  Indexed by KBC button index (0-15).
//
//  Color family — yellow theme (time is unusual to manipulate):
//    Warp targets  : YELLOW      (ApA, PeA, MNVR, SOI, Morn)
//    Warp rate     : WARM WHITE  (Fwd, Back — neutral rate controls)
//    Physics warp  : CHARTREUSE  (PHYS — different warp type)
//    Stop / Pause  : AMBER       (awareness — interrupting time flow)
//    Save          : LIME        (positive reinforcement)
//    Load          : MINT        (restoration / recovery)
//    Not installed : KBC_OFF
// ============================================================

const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_YELLOW,      // B0  - ApA    (Col 1, Row 1) - warp target
    KBC_AMBER,       // B1  - Stop   (Col 1, Row 2) - awareness
    KBC_YELLOW,      // B2  - PeA    (Col 2, Row 1) - warp target
    KBC_CHARTREUSE,  // B3  - PHYS   (Col 2, Row 2) - physics warp
    KBC_YELLOW,      // B4  - MNVR   (Col 3, Row 1) - warp target
    KBC_WHITE_WARM,  // B5  - Fwd    (Col 3, Row 2) - warp rate
    KBC_YELLOW,      // B6  - SOI    (Col 4, Row 1) - warp target
    KBC_WHITE_WARM,  // B7  - Back   (Col 4, Row 2) - warp rate
    KBC_YELLOW,      // B8  - Morn   (Col 5, Row 1) - warp target
    KBC_MINT,        // B9  - Load   (Col 5, Row 2) - restoration
    KBC_AMBER,       // B10 - Pause  (Col 6, Row 1) - awareness
    KBC_LIME,        // B11 - Save   (Col 6, Row 2) - positive
    KBC_OFF,         // B12 - Not installed
    KBC_OFF,         // B13 - Not installed
    KBC_OFF,         // B14 - Not installed
    KBC_OFF,         // B15 - Not installed
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

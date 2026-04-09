/**
 * @file        KCMk1_Vehicle_Control.ino
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Vehicle Control module sketch for the Kerbal Controller Mk1.
 *
 *              This module provides vehicle systems control including
 *              brakes, lights, gear, solar arrays, antenna, cargo door,
 *              ladder, radiators, and parachute deployment/cut functions
 *              for Kerbal Space Program. Parachute buttons (B8-B11)
 *              support extended LED states for pre-deployment sequencing.
 *
 *              Four discrete input positions carry auxiliary vehicle
 *              state signals (parking brake, parachutes armed, lights
 *              lock, gear lock). None have LED outputs.
 *
 *              I2C Address: 0x24
 *              Module Type: KBC_TYPE_VEHICLE_CONTROL (0x05)
 *              Extended States: Yes (KBC_CAP_EXTENDED_STATES)
 *
 *              NeoPixel button layout (6 rows x 2 columns):
 *
 *                        Col 1 (left)      Col 2 (right)
 *                Row 1:  B1                B0
 *                        Lights            Brakes
 *                        YELLOW            RED
 *
 *                Row 2:  B3                B2
 *                        Gear              Solar Array
 *                        GREEN             GOLD
 *
 *                Row 3:  B5                B4
 *                        Antenna           Cargo Door
 *                        PINK              TEAL
 *
 *                Row 4:  B7                B6
 *                        Radiator          Ladder
 *                        ORANGE            LIME
 *
 *                Row 5:  B9                B8
 *                        Drogue Deploy     Main Deploy
 *                        GREEN             GREEN
 *
 *                Row 6:  B11               B10
 *                        Drogue Cut        Main Cut
 *                        RED               RED
 *
 *              Discrete positions (outside NeoPixel grid, input only):
 *                B12 - Parking Brake    - input only, no LED
 *                B13 - Parachutes Armed - input only, no LED
 *                B14 - Lights Lock      - input only, no LED
 *                B15 - Gear Lock        - input only, no LED
 *
 *              Button-to-KBC index mapping:
 *                BUTTON01 (PCB) -> KBC index 0  -> Brakes
 *                BUTTON02 (PCB) -> KBC index 1  -> Lights
 *                BUTTON03 (PCB) -> KBC index 2  -> Solar Array
 *                BUTTON04 (PCB) -> KBC index 3  -> Gear
 *                BUTTON05 (PCB) -> KBC index 4  -> Cargo Door
 *                BUTTON06 (PCB) -> KBC index 5  -> Antenna
 *                BUTTON07 (PCB) -> KBC index 6  -> Ladder
 *                BUTTON08 (PCB) -> KBC index 7  -> Radiator
 *                BUTTON09 (PCB) -> KBC index 8  -> Main Deploy
 *                BUTTON10 (PCB) -> KBC index 9  -> Drogue Deploy
 *                BUTTON11 (PCB) -> KBC index 10 -> Main Cut
 *                BUTTON12 (PCB) -> KBC index 11 -> Drogue Cut
 *                BUTTON13 (PCB) -> KBC index 12 -> Parking Brake (input only)
 *                BUTTON14 (PCB) -> KBC index 13 -> Parachutes Armed (input only)
 *                BUTTON15 (PCB) -> KBC index 14 -> Lights Lock (input only)
 *                BUTTON16 (PCB) -> KBC index 15 -> Gear Lock (input only)
 *
 *              Extended LED state usage (parachute buttons B8-B11):
 *                WARNING        - parachute deployment window approaching
 *                ALERT          - deploy immediately
 *                ARMED          - parachute armed and ready
 *                PARTIAL_DEPLOY - drogue deployed, main pending
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

/** @brief I2C address for the Vehicle Control module. */
#define I2C_ADDRESS     0x24

/** @brief Module type identifier. */
#define MODULE_TYPE_ID  KBC_TYPE_VEHICLE_CONTROL

// ============================================================
//  Per-button active colors
//
//  Indexed by KBC button index (0-15).
//
//  B0   Brakes        - RED    (irreversible-feel, significant action)
//  B1   Lights        - YELLOW (natural light association)
//  B2   Solar Array   - GOLD   (power generation, sunlight)
//  B3   Gear          - GREEN  (terrain family)
//  B4   Cargo Door    - TEAL   (terrain family)
//  B5   Antenna       - PINK   (comms family)
//  B6   Ladder        - LIME   (terrain family)
//  B7   Radiator      - ORANGE (thermal management)
//  B8   Main Deploy   - GREEN  (parachute deploy — positive action)
//  B9   Drogue Deploy - GREEN  (parachute deploy — positive action)
//  B10  Main Cut      - RED    (parachute cut — irreversible)
//  B11  Drogue Cut    - RED    (parachute cut — irreversible)
//  B12  Parking Brake - OFF    (input only, no LED hardware)
//  B13  Para Armed    - OFF    (input only, no LED hardware)
//  B14  Lights Lock   - OFF    (input only, no LED hardware)
//  B15  Gear Lock     - OFF    (input only, no LED hardware)
// ============================================================

const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_RED,     // B0  - Brakes         (Col 2, Row 1)
    KBC_YELLOW,  // B1  - Lights         (Col 1, Row 1)
    KBC_GOLD,    // B2  - Solar Array    (Col 2, Row 2)
    KBC_GREEN,   // B3  - Gear           (Col 1, Row 2) - terrain family
    KBC_TEAL,    // B4  - Cargo Door     (Col 2, Row 3) - terrain family
    KBC_PINK,    // B5  - Antenna        (Col 1, Row 3) - comms
    KBC_LIME,    // B6  - Ladder         (Col 2, Row 4) - terrain family
    KBC_ORANGE,  // B7  - Radiator       (Col 1, Row 4) - thermal
    KBC_GREEN,   // B8  - Main Deploy    (Col 2, Row 5) - parachute deploy
    KBC_GREEN,   // B9  - Drogue Deploy  (Col 1, Row 5) - parachute deploy
    KBC_RED,     // B10 - Main Cut       (Col 2, Row 6) - irreversible
    KBC_RED,     // B11 - Drogue Cut     (Col 1, Row 6) - irreversible
    KBC_OFF,     // B12 - Parking Brake  - input only, no LED
    KBC_OFF,     // B13 - Para Armed     - input only, no LED
    KBC_OFF,     // B14 - Lights Lock    - input only, no LED
    KBC_OFF,     // B15 - Gear Lock      - input only, no LED
};

// ============================================================
//  Library instantiation
//
//  KBC_CAP_EXTENDED_STATES declared — this module supports
//  WARNING, ALERT, ARMED, and PARTIAL_DEPLOY states for
//  the parachute buttons (B8-B11). The system controller
//  uses these to sequence pre-deployment status.
// ============================================================

KerbalButtonCore kbc(MODULE_TYPE_ID, KBC_CAP_EXTENDED_STATES, activeColors);

// ============================================================
//  setup()
// ============================================================

void setup() {
    // Wire.begin() must be called before kbc.begin()
    Wire.begin(I2C_ADDRESS);

    // Initialise all library subsystems.
    // NeoPixel buttons B0-B11 start in ENABLED (dim white) state.
    // B12-B15 start OFF (input only, no LED hardware).
    kbc.begin();
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    // update() handles all library tasks each iteration:
    //   - Button polling and debounce (every KBC_POLL_INTERVAL_MS)
    //     including discrete inputs B12-B15 which are reported
    //     to the controller via normal button state packets
    //   - LED flash timing for WARNING and ALERT extended states
    //     on parachute buttons B8-B11
    //   - Render pending LED changes from I2C commands
    //   - INT pin synchronisation with button state
    kbc.update();
}

/**
 * @file        KCMk1_Vehicle_Control.ino
 * @version     2.2
 * @date        2026-06-28
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Vehicle Control module sketch for the Kerbal Controller Mk1.
 *
 *              This module provides vehicle systems control including
 *              brakes, lights, gear, antenna, fuel cell, solar array,
 *              cargo door, radiator, ladder, heat shield, and parachute
 *              functions for Kerbal Space Program. The heat shield (B8)
 *              and parachute (B10/B11) buttons are single-button state
 *              machines (deploy -> cut/release) driven by the controller
 *              via extended LED states.
 *
 *              The eight Switch Group 2 panel switches are read as discrete
 *              inputs at KBC indices 16-23 via a third shift register (U16).
 *              (Resolves Module UI Reference Open Item #9/TODO #9: the old
 *              B12-B15 discrete-input handling is removed and Switch Group 2
 *              is added via 24-input / 3-byte shift-register reads.)
 *
 *              I2C Address: 0x24
 *              Module Type: KBC_TYPE_VEHICLE_CONTROL (0x05)
 *              Extended States: Yes (KBC_CAP_EXTENDED_STATES)
 *              Inputs: 24 (KBC_INPUT_COUNT) via 3 shift registers
 *
 *              NeoPixel button layout (6 rows x 2 columns), per Module UI
 *              Reference v5.4 (canonical):
 *
 *                        Col 1 (left)      Col 2 (right)
 *                Row 1:  B1                B0
 *                        Lights            Brakes
 *                        YELLOW            RED
 *
 *                Row 2:  B3                B2
 *                        Gear              Antenna
 *                        GREEN             PINK
 *
 *                Row 3:  B5                B4
 *                        Solar Array       Fuel Cell
 *                        GOLD              CYAN
 *
 *                Row 4:  B7                B6
 *                        Radiator          Cargo Door
 *                        ORANGE            TEAL
 *
 *                Row 5:  B9                B8
 *                        Ladder            Heat Shield *
 *                        LIME              GREEN/RED
 *
 *                Row 6:  B11               B10
 *                        Drogue Chute *    Main Chute *
 *                        GREEN/RED         GREEN/RED
 *
 *                * Single-button state machine (deploy GREEN -> cut/release
 *                  RED). The deploy colour is the button's ACTIVE colour;
 *                  the cut/release transition is sequenced controller-side.
 *
 *              B12-B15 are no-connects on the PCB (the former parking
 *              brake / parachutes-armed / lights-lock / gear-lock discrete
 *              inputs are removed — those switches are now Switch Group 2).
 *
 *              Switch Group 2 — discrete inputs (no LED), reported in the
 *              button-event payload bytes for inputs 16-23:
 *                KBC index 16 → CHUTE   (SAFE / ARM) — parachute interlock
 *                KBC index 17 → GEAR    (UP / DOWN)
 *                KBC index 18 → BRAKE   (REL / LOCK)
 *                KBC index 19 → EXT LT  (OFF / ILLUM)
 *                KBC index 20 → SAS     (OFF / ENAB)
 *                KBC index 21 → RCS     (OFF / ENAB)
 *                KBC index 22 → THC/RHC (NORM / PREC)
 *                KBC index 23 → AUDIO   (MUTE / LIVE)
 *              Switch semantics and controller actions are resolved on the
 *              main controller; this module reports raw input state only.
 *
 *              Button-to-KBC index mapping:
 *                BUTTON01 (PCB) -> KBC index 0  -> Brakes
 *                BUTTON02 (PCB) -> KBC index 1  -> Lights
 *                BUTTON03 (PCB) -> KBC index 2  -> Antenna       (CAG 13)
 *                BUTTON04 (PCB) -> KBC index 3  -> Gear
 *                BUTTON05 (PCB) -> KBC index 4  -> Fuel Cell     (CAG 14)
 *                BUTTON06 (PCB) -> KBC index 5  -> Solar Array   (CAG 15)
 *                BUTTON07 (PCB) -> KBC index 6  -> Cargo Door    (CAG 16)
 *                BUTTON08 (PCB) -> KBC index 7  -> Radiator      (CAG 17)
 *                BUTTON09 (PCB) -> KBC index 8  -> Heat Shield   (CAG 19/20)
 *                BUTTON10 (PCB) -> KBC index 9  -> Ladder        (CAG 18)
 *                BUTTON11 (PCB) -> KBC index 10 -> Main Chute    (CAG 21/22)
 *                BUTTON12 (PCB) -> KBC index 11 -> Drogue Chute  (CAG 23/24)
 *                BUTTON13-16    -> KBC index 12-15 -> No-connect (PCB)
 *                Switch Group 2 -> KBC index 16-23 -> discrete inputs (U16)
 *
 *              Extended LED state usage (state-machine buttons B8/B10/B11):
 *                ACTIVE         - deployed (GREEN, the button's active colour)
 *                CUT            - cut / released (static RED, terminal state)
 *                WARNING        - deployment window approaching (amber flash)
 *                ALERT          - deploy/cut immediately (red flash)
 *                ARMED          - parachute armed and ready (cyan)
 *                PARTIAL_DEPLOY - drogue deployed, main pending (amber)
 *              Main/Drogue Chute (B10/B11) require the Switch Group 2 CHUTE
 *              switch in ARM before the controller deploys. Deploy renders
 *              GREEN (KBC_LED_ACTIVE); the cut/release terminal state renders
 *              static red via KBC_LED_CUT (KerbalButtonCore v2.1).
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
//  Input width — 24 inputs (Switch Group 2)
//
//  Vehicle Control adds a third 74HC165 (U16) for the eight Switch
//  Group 2 panel switches at KBC indices 16-23. These overrides MUST be
//  defined before including KerbalButtonCore.h so the library widens its
//  input path and response packet (9-byte packet). See KBC_Config.h.
// ============================================================

#define KBC_INPUT_COUNT     24
#define KBC_SHIFTREG_COUNT  3

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
//  Indexed by KBC button index (0-15). Layout per Module UI Reference
//  v5.4 (canonical). For the state-machine buttons (B8 Heat Shield,
//  B10 Main Chute, B11 Drogue Chute) this is the DEPLOY colour (GREEN);
//  the cut/release terminal state is sequenced controller-side.
//
//  B0   Brakes       - RED    (high-consequence, attention)
//  B1   Lights       - YELLOW (natural light association)
//  B2   Antenna      - PINK   (comms family)              CAG 13
//  B3   Gear         - GREEN  (terrain family)
//  B4   Fuel Cell    - CYAN   (power generation)          CAG 14
//  B5   Solar Array  - GOLD   (power generation, sun)     CAG 15
//  B6   Cargo Door   - TEAL   (terrain family)            CAG 16
//  B7   Radiator     - ORANGE (thermal management)        CAG 17
//  B8   Heat Shield  - GREEN  (deploy; release ctrl-side) CAG 19/20
//  B9   Ladder       - LIME   (terrain family)            CAG 18
//  B10  Main Chute   - GREEN  (deploy; cut ctrl-side)     CAG 21/22
//  B11  Drogue Chute - GREEN  (deploy; cut ctrl-side)     CAG 23/24
//  B12-B15           - OFF    (no-connect on the PCB)
//  16-23 Switch Group 2       (discrete inputs, no LED — not in this array)
// ============================================================

const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_RED,     // B0  - Brakes        (Col 2, Row 1)
    KBC_YELLOW,  // B1  - Lights        (Col 1, Row 1)
    KBC_PINK,    // B2  - Antenna       (Col 2, Row 2) - comms          CAG 13
    KBC_GREEN,   // B3  - Gear          (Col 1, Row 2) - terrain family
    KBC_CYAN,    // B4  - Fuel Cell     (Col 2, Row 3) - power          CAG 14
    KBC_GOLD,    // B5  - Solar Array   (Col 1, Row 3) - power          CAG 15
    KBC_TEAL,    // B6  - Cargo Door    (Col 2, Row 4) - terrain family CAG 16
    KBC_ORANGE,  // B7  - Radiator      (Col 1, Row 4) - thermal        CAG 17
    KBC_GREEN,   // B8  - Heat Shield   (Col 2, Row 5) - deploy         CAG 19/20
    KBC_LIME,    // B9  - Ladder        (Col 1, Row 5) - terrain family CAG 18
    KBC_GREEN,   // B10 - Main Chute    (Col 2, Row 6) - deploy         CAG 21/22
    KBC_GREEN,   // B11 - Drogue Chute  (Col 1, Row 6) - deploy         CAG 23/24
    KBC_OFF,     // B12 - No-connect (PCB)
    KBC_OFF,     // B13 - No-connect (PCB)
    KBC_OFF,     // B14 - No-connect (PCB)
    KBC_OFF,     // B15 - No-connect (PCB)
    // KBC indices 16-23 are Switch Group 2 discrete inputs (U16) with no
    // LED hardware — they have no entry in this colour array (sized to the
    // 16 LED positions, KBC_BUTTON_COUNT).
};

// ============================================================
//  Library instantiation
//
//  KBC_CAP_EXTENDED_STATES declared — this module supports
//  WARNING, ALERT, ARMED, and PARTIAL_DEPLOY states for the
//  state-machine buttons (B8 Heat Shield, B10 Main Chute,
//  B11 Drogue Chute). The system controller uses these to
//  sequence deploy / cut-release status.
// ============================================================

KerbalButtonCore kbc(MODULE_TYPE_ID, KBC_CAP_EXTENDED_STATES, activeColors);

// ============================================================
//  setup()
// ============================================================

void setup() {
    // Wire.begin() must be called before kbc.begin()
    Wire.begin(I2C_ADDRESS);

    // Initialise all library subsystems. The module powers on dark
    // (BOOT_READY → DISABLED); NeoPixel buttons B0-B11 light to ENABLED
    // on CMD_ENABLE. B12-B15 are no-connects. The library reads all 24
    // inputs (3 shift registers) and reports Switch Group 2 (16-23) in
    // the button-event payload.
    kbc.begin();
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    // update() handles all library tasks each iteration:
    //   - Button/switch polling and debounce across all 24 inputs,
    //     including Switch Group 2 (16-23) reported to the controller
    //     via the button-event payload
    //   - LED flash timing for WARNING and ALERT extended states
    //     on the state-machine buttons B8/B10/B11
    //   - Render pending LED changes from I2C commands
    //   - INT pin synchronisation with input state
    kbc.update();
}

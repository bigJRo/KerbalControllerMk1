/**
 * @file        ExtendedModule.ino
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       ExtendedModule — KerbalButtonCore sketch example with
 *              extended LED state support.
 *
 *              Demonstrates a module that supports all LED states
 *              including WARNING, ALERT, ARMED, and PARTIAL_DEPLOY.
 *              Models the Vehicle Control module which has parachute
 *              deploy/cut buttons that use extended states during flight.
 *
 *              Extended state behavior:
 *                WARNING        — slow amber flash (500ms on/off)
 *                                 e.g. parachute deployment available
 *                ALERT          — fast red flash (150ms on/off)
 *                                 e.g. chute deployment critical
 *                ARMED          — static cyan
 *                                 e.g. chute armed and ready
 *                PARTIAL_DEPLOY — static amber
 *                                 e.g. drogue deployed, main pending
 *
 *              The master controller sends these state values via
 *              CMD_SET_LED_STATE. This module only defines how they
 *              look — the master owns when to use them.
 *
 *              Custom ACTIVE colors can also be defined per-button
 *              using KBC_COLOR(r, g, b) for non-palette colors.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Requires: KerbalButtonCore library
 *              Hardware: KC-01-1822 v1.1 (ATtiny816)
 *              IDE settings:
 *                Board: ATtiny816 (megaTinyCore)
 *                Clock: 10 MHz or higher
 *                tinyNeoPixel Port: Port A
 */

#include <Wire.h>
#include <KerbalButtonCore.h>

// ============================================================
//  Module identity
// ============================================================

#define I2C_ADDRESS     0x24   // Vehicle Control module address
#define MODULE_TYPE_ID  KBC_TYPE_VEHICLE_CONTROL

// ============================================================
//  Per-button active colors — Vehicle Control module
// ============================================================

const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_RED,     // B0  — Brakes
    KBC_YELLOW,  // B1  — Lights
    KBC_GOLD,    // B2  — Solar Array
    KBC_GREEN,   // B3  — Gear
    KBC_TEAL,    // B4  — Cargo Door
    KBC_PINK,    // B5  — Antenna
    KBC_LIME,    // B6  — Ladder
    KBC_ORANGE,  // B7  — Radiator
    KBC_GREEN,   // B8  — Main Deploy   ← extended states active in flight
    KBC_GREEN,   // B9  — Drogue Deploy ← extended states active in flight
    KBC_RED,     // B10 — Main Cut      ← extended states active in flight
    KBC_RED,     // B11 — Drogue Cut    ← extended states active in flight
    KBC_GREEN,   // B12 — discrete (unused)
    KBC_GREEN,   // B13 — discrete (unused)
    KBC_GREEN,   // B14 — discrete (unused)
    KBC_GREEN,   // B15 — discrete (unused)
};

// ============================================================
//  Library instantiation
//
//  KBC_CAP_EXTENDED_STATES tells the master this module
//  supports WARNING/ALERT/ARMED/PARTIAL_DEPLOY states.
// ============================================================

KerbalButtonCore kbc(MODULE_TYPE_ID, KBC_CAP_EXTENDED_STATES, activeColors);

// ============================================================
//  setup()
// ============================================================

void setup() {
    Wire.begin(I2C_ADDRESS);
    kbc.begin();
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    // update() automatically services flash timing for
    // WARNING and ALERT states when they are active.
    // No additional code needed in the module sketch.
    kbc.update();
}

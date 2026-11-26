/**
 * @file        KBC_DevBoard_Target.ino
 * @version     1.1.0
 * @date        2026-11-26
 * @project     Kerbal Controller Mk1 — KBC Library Test Suite
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       KBC library test target for KC-01-9101 Development Board.
 *
 *              Runs on the ATtiny816 on the KC-01-9101 dev board.
 *              Acts as a standard KBC I2C target — the KBC library
 *              under test responds to commands from the XIAO SAMD21
 *              controller running KBC_Tester_Controller.ino.
 *
 *              No special test logic lives here. This sketch is
 *              intentionally minimal — the library itself is what's
 *              under test, exercised end-to-end over real I2C wire
 *              transactions.
 *
 *              Identity (I2C spec v2.22 §6, 8 bytes): type 0x01 (UI
 *              Control), capability flags 0x01 (extended states), the
 *              KerbalButtonCore version at bytes 1/2/4 and this sketch's own
 *              version (SKETCH_VERSION_* below) at bytes 5..7 — what
 *              KBC_Tester_Controller's identity test checks.
 *
 * @note        Hardware:  KC-01-9101 Module Development Board v1.0
 *              Tester:    KC-01-9001 Module Tester (XIAO SAMD21)
 *              Library:   KerbalButtonCore v2.5.0 or later (the identity
 *                         carries this sketch's version)
 *
 * @note        IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             10 MHz internal
 *                tinyNeoPixel Port: Port B  ← CRITICAL: must be Port B, not Port A
 *                millis/micros:     Enabled (default timer)
 *
 * ============================================================
 *  KC-01-9101 Dev Board vs KC-01-1822 Production Pin Differences
 * ============================================================
 *
 *  Pin       Dev Board       Production    Notes
 *  ───────   ─────────────   ───────────   ──────────────────────────────
 *  PA4       INT             NEOPIX_CMD    INT moved to PA4 on dev board
 *  PB4       NEOPIX_CMD      No connect    NeoPixel moved to PB4 (Port B)
 *  PB2       LED15           No connect    LED15 moved to PB2
 *  PC2       LED14           LED15         LED14 moved to PC2
 *  PC0       No connect      LED14         PC0 unused on dev board
 *
 *  NeoPixel: SK6812mini-012 (GRB order) vs WS2811 (RGB order)
 *
 *  INT, the NeoPixel pin and the colour order are handled by the
 *  overrides below. The discrete LEDs (LED14, LED15) need none:
 *  KerbalButtonCore no longer drives discrete LEDs (indices 12-15 are
 *  inputs only), so they stay dark on both boards.
 *  Remove the overrides when building for KC-01-1822 production hardware
 *  and change tinyNeoPixel Port back to Port A.
 *
 * @par Revision history
 *   1.1.0  2026-11-26  Ported to the current KerbalButtonCore API (2.5.0):
 *                      the identity and capability flags go to the
 *                      constructor, begin() takes no arguments, and the
 *                      sketch's own version goes in with
 *                      kbc.setSketchVersion(). The colour array is
 *                      KBC_BUTTON_COUNT (16) long, as the library reads.
 *                      The pin overrides now take effect (see below); the
 *                      discrete-LED overrides are gone with the driver.
 *   1.0.0  2026-04-09  Initial release.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

// ============================================================
//  Includes, with the KC-01-9101 Dev Board hardware overrides
//
//  KBC_Config.h defines the pins without #ifndef guards, so a
//  #define placed before the include would be replaced by the
//  production value. Include KBC_Config.h first, replace the
//  values, then include KerbalButtonCore.h: its library code is
//  compiled in this sketch (the .ipp files) and uses the dev
//  board values; KBC_Config.h is not read a second time.
//  Remove the overrides when building for KC-01-1822 production
//  hardware.
// ============================================================

#include <Wire.h>
#include <KBC_Config.h>

#undef  KBC_PIN_INT
#define KBC_PIN_INT         PIN_PA4   // PA1 on production
#undef  KBC_PIN_NEO
#define KBC_PIN_NEO         PIN_PB4   // PA4 on production — also change
                                      // IDE: Tools → tinyNeoPixel Port → Port B
#undef  KBC_NEO_COLOR_ORDER
#define KBC_NEO_COLOR_ORDER NEO_GRB   // NEO_RGB on production (WS2811)

#include <KerbalButtonCore.h>

// ============================================================
//  Module identity
//
//  Using UI_CONTROL type and address as a convenient default.
//  The tester controller will use this address and type.
//  Change address if testing a different slot in isolation.
// ============================================================

#define TEST_I2C_ADDRESS    0x20
#define TEST_MODULE_TYPE    KBC_TYPE_UI_CONTROL

/** @brief This sketch's own version, MAJOR.MINOR.PATCH: the single source of
 *         the sketch version reported at identity bytes 5..7 (I2C spec §6),
 *         next to the KerbalButtonCore version at bytes 1/2/4. Keep @version
 *         and the README in step with it. */
#define SKETCH_VERSION_MAJOR  1
#define SKETCH_VERSION_MINOR  1
#define SKETCH_VERSION_PATCH  0

// ============================================================
//  Button active colors
//
//  A simple distinct color per button makes it easy to visually
//  confirm correct NeoPixel addressing during LED tests.
//  Buttons 0-11 cycle through hues; indices 12-15 are inputs
//  with no LED (the slots are unused, kept so the array is
//  KBC_BUTTON_COUNT long).
// ============================================================

static const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_RED,        // BTN01
    KBC_GREEN,      // BTN02
    KBC_BLUE,       // BTN03
    KBC_AMBER,      // BTN04
    KBC_CYAN,       // BTN05
    KBC_MAGENTA,    // BTN06
    KBC_WHITE_COOL, // BTN07
    KBC_ORANGE,     // BTN08
    KBC_LIME,       // BTN09
    KBC_SKY,        // BTN10
    KBC_PURPLE,     // BTN11
    KBC_TEAL,       // BTN12
    KBC_OFF,        // index 12 — input, no LED
    KBC_OFF,        // index 13 — input, no LED
    KBC_OFF,        // index 14 — input, no LED
    KBC_OFF,        // index 15 — input, no LED
};

// ============================================================
//  KerbalButtonCore instance
//
//  KBC_CAP_EXTENDED_STATES: the tester exercises WARNING, ALERT,
//  ARMED and PARTIAL_DEPLOY, and checks capability flags 0x01.
// ============================================================

KerbalButtonCore kbc(TEST_MODULE_TYPE, KBC_CAP_EXTENDED_STATES, activeColors);

// ============================================================
//  setup()
// ============================================================

void setup() {
    // Report this sketch's version in the identity, next to the library's.
    kbc.setSketchVersion(SKETCH_VERSION_MAJOR, SKETCH_VERSION_MINOR, SKETCH_VERSION_PATCH);

    // Wire.begin() must be called before kbc.begin()
    Wire.begin(TEST_I2C_ADDRESS);

    kbc.begin();   // ENABLED brightness: KBC_ENABLED_BRIGHTNESS

    // Module starts disabled — controller must send CMD_ENABLE
    // to activate, matching production module behaviour.
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    kbc.update();
}

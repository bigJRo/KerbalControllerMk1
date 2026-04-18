/**
 * @file        KBC_DevBoard_Target.ino
 * @version     1.0.0
 * @date        2026-04-09
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
 * @note        Hardware:  KC-01-9101 Module Development Board v1.0
 *              Tester:    KC-01-9001 Module Tester (XIAO SAMD21)
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
 *  All five differences are handled by the #define overrides below.
 *  Remove these overrides when building for KC-01-1822 production hardware
 *  and change tinyNeoPixel Port back to Port A.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

// ============================================================
//  KC-01-9101 Dev Board hardware overrides
//  Must appear BEFORE KerbalButtonCore.h include so that
//  KBC_Config.h #ifndef guards pick up these values.
//  Remove when building for KC-01-1822 production hardware.
// ============================================================

#define KBC_PIN_INT         PIN_PA4   // PA1 on production
#define KBC_PIN_NEO         PIN_PB4   // PA4 on production — also change
                                      // IDE: Tools → tinyNeoPixel Port → Port B
#define KBC_PIN_LED_13      PIN_PC2   // PC0 on production (LED14)
#define KBC_PIN_LED_14      PIN_PB2   // PC2 on production (LED15)
#define KBC_NEO_COLOR_ORDER NEO_GRB   // NEO_RGB on production (WS2811)

// ============================================================
//  Includes
// ============================================================

#include <Wire.h>
#include <KerbalButtonCore.h>

// ============================================================
//  Module identity
//
//  Using UI_CONTROL type and address as a convenient default.
//  The tester controller will use this address and type.
//  Change address if testing a different slot in isolation.
// ============================================================

#define TEST_I2C_ADDRESS    0x20
#define TEST_MODULE_TYPE    KMC_TYPE_UI_CONTROL
#define TEST_FW_MAJOR       1
#define TEST_FW_MINOR       0

// ============================================================
//  Button active colors
//
//  A simple distinct color per button makes it easy to visually
//  confirm correct NeoPixel addressing during LED tests.
//  Buttons 0-11 cycle through hues; discrete LEDs 12-15 are
//  ON/OFF only so color doesn't apply.
// ============================================================

static const RGBColor activeColors[KBC_NEO_COUNT] = {
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
};

// ============================================================
//  KerbalButtonCore instance
// ============================================================

KerbalButtonCore kbc;

// ============================================================
//  setup()
// ============================================================

void setup() {
    Wire.begin(TEST_I2C_ADDRESS);

    kbc.begin(
        TEST_MODULE_TYPE,
        TEST_FW_MAJOR,
        TEST_FW_MINOR,
        KBC_CAP_EXTENDED_STATES,
        activeColors,
        KBC_ENABLED_BRIGHTNESS
    );

    // Module starts disabled — controller must send CMD_ENABLE
    // to activate, matching production module behaviour.
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    kbc.update();
}

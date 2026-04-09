/**
 * @file        KCMk1_PreWarp_Time.ino
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Pre-Warp Time module for the Kerbal Controller Mk1.
 *
 *              Allows the operator to set a pre-warp time duration
 *              in minutes (0-9999) using the rotary encoder. Three
 *              preset buttons jump to commonly used durations. The
 *              current value is transmitted to the main controller
 *              via I2C for use in time warp sequencing.
 *
 *              I2C Address:    0x2B
 *              Module Type ID: 0x0C
 *              Capability:     0x10 (K7SC_CAP_DISPLAY)
 *
 *              Physical layout (top to bottom, center justified):
 *                [  4-digit 7-segment display  ]
 *                [      Rotary encoder         ]
 *                [ BTN01 ][ BTN02 ][ BTN03 ]
 *
 *              Button behavior:
 *                BTN01 — 5 min  preset — flashes GOLD on press,
 *                         sets display to 5
 *                BTN02 — 1 hour preset — flashes GOLD on press,
 *                         sets display to 60
 *                BTN03 — 1 day  preset — flashes GOLD on press,
 *                         sets display to 1440
 *                BTN_EN — Resets display to 0
 *
 *              Display:
 *                Pre-warp duration in minutes (0-9999)
 *                Default: 0
 *                No leading zeros
 *
 *              Encoder acceleration:
 *                Slow  (>150ms) → ±1
 *                Medium (50-150ms) → ±10
 *                Fast  (<50ms) → ±100
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware:  KC-01-1881/1882 v2.0 (ATtiny816)
 *              Library:   Kerbal7SegmentCore v1.0.0
 *              IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             10 MHz or higher
 *                tinyNeoPixel Port: Port C
 */

#include <Wire.h>
#include <Kerbal7SegmentCore.h>

// ============================================================
//  Module identity
// ============================================================

/** @brief I2C address for the Pre-Warp Time module. */
#define I2C_ADDRESS     0x2B

/** @brief Module type identifier. */
#define MODULE_TYPE_ID  0x0C

// ============================================================
//  Preset values (minutes)
// ============================================================

#define PRESET_5MIN     5       // BTN01
#define PRESET_1HOUR    60      // BTN02
#define PRESET_1DAY     1440    // BTN03
#define DEFAULT_VALUE   0       // encoder button reset

// ============================================================
//  Button configuration
//
//  All three buttons use FLASH mode — they illuminate GOLD
//  briefly on press then return to dim white backlight.
//  The preset jump is handled in loop() when the button
//  event is detected.
// ============================================================

const ButtonConfig btnConfigs[K7SC_NEO_COUNT] = {

    // BTN01 — 5 min preset — GOLD flash
    {
        BTN_MODE_FLASH,
        { K7SC_GOLD, K7SC_OFF, K7SC_OFF },
        1,    // numStates: not used in flash mode
        150   // flashMs: 150ms flash duration
    },

    // BTN02 — 1 hour preset — GOLD flash
    {
        BTN_MODE_FLASH,
        { K7SC_GOLD, K7SC_OFF, K7SC_OFF },
        1,
        150
    },

    // BTN03 — 1 day preset — GOLD flash
    {
        BTN_MODE_FLASH,
        { K7SC_GOLD, K7SC_OFF, K7SC_OFF },
        1,
        150
    },
};

// ============================================================
//  setup()
// ============================================================

void setup() {
    // Wire.begin() must be called before k7scBegin()
    Wire.begin(I2C_ADDRESS);

    // Initialise all subsystems.
    // Display starts at DEFAULT_VALUE (0 minutes).
    // All buttons start in dim white backlight.
    k7scBegin(MODULE_TYPE_ID, K7SC_CAP_DISPLAY, btnConfigs, DEFAULT_VALUE);
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    // k7scUpdate() handles all library tasks each iteration:
    //   - Button polling, debounce, flash timing
    //   - Encoder polling with 3-tier acceleration
    //   - INT pin sync
    k7scUpdate();

    // Handle preset button presses — jump display to preset value
    // buttonsGetEvents() returns a bitmask of buttons pressed
    // this cycle. We read it here to catch the press edge.
    uint8_t events = buttonsGetEvents();

    if (events & (1 << K7SC_BIT_BTN01)) {
        encoderSetValue(PRESET_5MIN);
    }
    if (events & (1 << K7SC_BIT_BTN02)) {
        encoderSetValue(PRESET_1HOUR);
    }
    if (events & (1 << K7SC_BIT_BTN03)) {
        encoderSetValue(PRESET_1DAY);
    }

    // Encoder button resets to 0
    if (buttonsGetEncoderPress()) {
        encoderSetValue(DEFAULT_VALUE);
    }
}

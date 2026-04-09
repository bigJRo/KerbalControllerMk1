/**
 * @file        KCMk1_GPWS_Input.ino
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       GPWS Input Panel module for the Kerbal Controller Mk1.
 *
 *              Provides ground proximity warning system configuration
 *              for Kerbal Space Program. The operator sets an altitude
 *              or distance threshold on the 4-digit display using the
 *              rotary encoder. When the vessel crosses this threshold
 *              the main controller triggers the configured warning behavior.
 *
 *              I2C Address:    0x2A
 *              Module Type ID: 0x0B
 *              Capability:     0x10 (K7SC_CAP_DISPLAY)
 *
 *              Physical layout (top to bottom, center justified):
 *                [  4-digit 7-segment display  ]
 *                [      Rotary encoder         ]
 *                [ BTN01 ][ BTN02 ][ BTN03 ]
 *
 *              Button behavior (all module-managed):
 *                BTN01 — GPWS Enable — 3-state cycle
 *                         State 0: OFF       — dim white backlight
 *                         State 1: GREEN     — full GPWS active
 *                         State 2: AMBER     — proximity tone only
 *                BTN02 — Proximity Alarm — toggle (off ↔ GREEN)
 *                BTN03 — Rendezvous Radar Enabled — toggle (off ↔ GREEN)
 *                BTN_EN — Reset altitude threshold to DEFAULT_VALUE (200)
 *
 *              State byte (Byte 2 of data packet):
 *                bits 0-1: BTN01 state (0=off, 1=full GPWS, 2=proximity only)
 *                bit 2:    BTN02 active (proximity alarm enabled)
 *                bit 3:    BTN03 active (rendezvous radar enabled)
 *
 *              Display:
 *                Altitude/distance threshold in meters (0-9999)
 *                Default: 200m
 *                No leading zeros — shows "200" not "0200"
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

/** @brief I2C address for the GPWS Input Panel. */
#define I2C_ADDRESS     0x2A

/** @brief Module type identifier. */
#define MODULE_TYPE_ID  0x0B

// ============================================================
//  Default altitude threshold (meters)
//
//  This value is displayed at power-on and restored when
//  the encoder button is pressed. Set this to the most
//  commonly used proximity warning altitude for your
//  typical mission profile.
// ============================================================

#define DEFAULT_VALUE   200

// ============================================================
//  Button configuration
//
//  BTN01 — GPWS Enable — 3-state cycle:
//             State 0: off     → dim white (K7SC_ENABLED_COLOR applied by library)
//             State 1: GREEN   → full GPWS active
//             State 2: AMBER   → proximity tone only
//
//  BTN02 — Proximity Alarm — binary toggle (off ↔ GREEN)
//
//  BTN03 — Rendezvous Radar — binary toggle (off ↔ GREEN)
//
//  colors[0] in CYCLE mode = state 0 color (off → use K7SC_OFF,
//  library applies ENABLED dim white for state 0 automatically)
// ============================================================

const ButtonConfig btnConfigs[K7SC_NEO_COUNT] = {

    // BTN01 — GPWS Enable — 3-state cycle
    {
        BTN_MODE_CYCLE,
        { K7SC_OFF, K7SC_GREEN, K7SC_AMBER },
        3,    // numStates: off → state1 → state2 → off
        0     // flashMs: not used in cycle mode
    },

    // BTN02 — Proximity Alarm — toggle
    {
        BTN_MODE_TOGGLE,
        { K7SC_OFF, K7SC_GREEN, K7SC_OFF },
        2,    // numStates: not used in toggle mode
        0
    },

    // BTN03 — Rendezvous Radar Enabled — toggle
    {
        BTN_MODE_TOGGLE,
        { K7SC_OFF, K7SC_GREEN, K7SC_OFF },
        2,
        0
    },
};

// ============================================================
//  setup()
// ============================================================

void setup() {
    // Wire.begin() must be called before k7scBegin()
    Wire.begin(I2C_ADDRESS);

    // Initialise all subsystems.
    // Display starts at DEFAULT_VALUE.
    // All buttons start in off state (dim white backlight).
    k7scBegin(MODULE_TYPE_ID, K7SC_CAP_DISPLAY, btnConfigs, DEFAULT_VALUE);
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    // k7scUpdate() handles all library tasks each iteration:
    //   - Button polling, debounce, state transitions (cycle/toggle)
    //   - Encoder polling with 3-tier acceleration
    //   - INT pin sync — asserts on any button state change or
    //     display value change
    k7scUpdate();

    // Encoder button resets altitude threshold to default
    if (buttonsGetEncoderPress()) {
        encoderSetValue(DEFAULT_VALUE);
    }
}

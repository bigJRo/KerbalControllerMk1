/**
 * @file        BasicDisplayModule.ino
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       BasicDisplayModule — minimal Kerbal7SegmentCore example.
 *
 *              Template for a GPWS-style module with:
 *                - BTN01: 3-state cycle (off → green → amber → off)
 *                - BTN02: toggle (off ↔ green)
 *                - BTN03: toggle (off ↔ green)
 *                - Encoder: increment/decrement value with acceleration
 *                - Encoder button: reset value to default
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware: KC-01-1881/1882 v2.0 (ATtiny816)
 *              IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             10 MHz or higher
 *                tinyNeoPixel Port: Port C
 */

#include <Wire.h>
#include <Kerbal7SegmentCore.h>

#define I2C_ADDRESS     0x2A
#define MODULE_TYPE_ID  0x0B
#define DEFAULT_VALUE   200

const ButtonConfig btnConfigs[K7SC_NEO_COUNT] = {
    // BTN01 — 3-state cycle: off → GREEN → AMBER → off
    { BTN_MODE_CYCLE,
      {K7SC_OFF, K7SC_GREEN, K7SC_AMBER},
      3, 0 },

    // BTN02 — toggle: off ↔ GREEN
    { BTN_MODE_TOGGLE,
      {K7SC_OFF, K7SC_GREEN, K7SC_OFF},
      2, 0 },

    // BTN03 — toggle: off ↔ GREEN
    { BTN_MODE_TOGGLE,
      {K7SC_OFF, K7SC_GREEN, K7SC_OFF},
      2, 0 },
};

void setup() {
    Wire.begin(I2C_ADDRESS);
    k7scBegin(MODULE_TYPE_ID, K7SC_CAP_DISPLAY, btnConfigs, DEFAULT_VALUE);
}

void loop() {
    k7scUpdate();

    // Encoder button resets display to default value
    if (buttonsGetEncoderPress()) {
        encoderSetValue(DEFAULT_VALUE);
    }
}

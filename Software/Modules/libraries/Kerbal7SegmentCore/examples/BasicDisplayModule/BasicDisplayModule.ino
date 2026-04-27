/**
 * @file        BasicDisplayModule.ino
 * @version     1.1.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Minimal Kerbal7SegmentCore example — GPWS Input Panel template.
 *
 *              Button configuration:
 *                BTN01: 3-state cycle (ENABLED → GREEN → AMBER → ENABLED)
 *                BTN02: toggle (ENABLED ↔ BLUE)
 *                BTN03: flash RED on press, return to ENABLED
 *                BTN_EN: reset display to default value
 *
 * @note        Hardware: KC-01-1881/1882 v2.0 (ATtiny816)
 *              IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             20 MHz internal
 *                tinyNeoPixel Port: Port C
 *                Programmer:        serialUPDI
 */

#include <KerbalModuleCommon.h>
#include <Kerbal7SegmentCore.h>

#define I2C_ADDRESS    0x2A
#define DEFAULT_VALUE  200

const ButtonConfig btnConfigs[K7SC_NEO_COUNT] = {
    // BTN01 — 3-state cycle: ENABLED → GREEN → AMBER → ENABLED
    { BTN_MODE_CYCLE,
      { K7SC_ENABLED_COLOR, toGRBW(KMC_GREEN), toGRBW(KMC_AMBER) },
      3, 0 },

    // BTN02 — toggle: ENABLED ↔ BLUE
    { BTN_MODE_TOGGLE,
      { K7SC_ENABLED_COLOR, toGRBW(KMC_BLUE), K7SC_OFF },
      2, 0 },

    // BTN03 — flash RED 300ms on press, return to ENABLED
    { BTN_MODE_FLASH,
      { toGRBW(KMC_RED), K7SC_OFF, K7SC_OFF },
      0, 300 },
};

void setup() {
    k7scBegin(I2C_ADDRESS, KMC_TYPE_GPWS_INPUT,
              KMC_CAP_DISPLAY, btnConfigs, DEFAULT_VALUE);
}

void loop() {
    k7scUpdate();

    // Encoder button resets display to default value
    if (buttonsGetEncoderPress()) {
        encoderSetValue(DEFAULT_VALUE);
    }
}

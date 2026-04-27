/*
 * KC-01-1880  7-Segment Display Module — Full Test Firmware
 * Board:      ATtiny816  (megaTinyCore, 20 MHz internal)
 *
 * Loads the module with all three button modes active so the
 * Xiao RA4M1 tester can exercise every I2C command and verify
 * all responses. The module runs a display self-test on boot.
 *
 * Button configuration:
 *   BTN01 — CYCLE   (3 states: dim white → green → amber)
 *   BTN02 — TOGGLE  (dim white ↔ blue)
 *   BTN03 — FLASH   (flashes red 300ms on press, returns to dim white)
 *   BTN_EN — MOMENTARY (resets display to 0, no LED)
 *
 * I2C address : 0x2A
 * Type ID     : KMC_TYPE_GPWS_INPUT (0x0B)
 * Packet size : 6 bytes
 *
 * Libraries required:
 *   - Kerbal7SegmentCore  (v1.1.0)
 *   - KerbalModuleCommon
 *   - tinyNeoPixel        (bundled with megaTinyCore)
 */

#include <KerbalModuleCommon.h>
#include <Kerbal7SegmentCore.h>

#define I2C_ADDRESS     0x2A
#define MODULE_TYPE_ID  KMC_TYPE_GPWS_INPUT

static GRBWColor rgb(RGBColor c) { return toGRBW(c); }

const ButtonConfig btnConfigs[K7SC_NEO_COUNT] = {
    // BTN01: CYCLE — dim white → green → amber → (back to dim white)
    {
        BTN_MODE_CYCLE,
        { K7SC_ENABLED_COLOR, rgb(KMC_GREEN), rgb(KMC_AMBER) },
        3, 0
    },
    // BTN02: TOGGLE — dim white ↔ blue
    {
        BTN_MODE_TOGGLE,
        { K7SC_ENABLED_COLOR, rgb(KMC_BLUE), K7SC_OFF },
        2, 0
    },
    // BTN03: FLASH — red 300ms on press, returns to dim white
    {
        BTN_MODE_FLASH,
        { rgb(KMC_RED), K7SC_OFF, K7SC_OFF },
        0, 300
    },
};

void setup()
{
    k7scBegin(I2C_ADDRESS, MODULE_TYPE_ID, KMC_CAP_DISPLAY, btnConfigs, 0);

    // Boot display self-test: sweep 0→9 on rightmost digit
    for (uint8_t d = 0; d <= 9; d++) {
        displaySetValue(d);
        delay(120);
    }
    displaySetValue(0);
}

void loop()
{
    k7scUpdate();

    if (buttonsGetEncoderPress()) {
        encoderSetValue(0);
    }
}

/*
 * KC-01-1880  7-Segment Display Module — Demo Sketch
 * Board:      ATtiny816  (megaTinyCore, 20 MHz internal)
 *
 * Uses the Kerbal7SegmentCore library for display, encoder, and buttons.
 * NeoPixel colour rotation is handled here in loop() as application-level
 * behaviour — it uses buttonsSetPixelColor() / buttonsShow() to drive
 * the library's internal NeoPixel instance directly.
 *
 * Behaviour:
 *   - 7-segment shows 0-9999, starts at 0
 *   - Encoder CW  = increment, CCW = decrement (with acceleration)
 *   - Encoder button (BUTTON_EN) resets display to 0
 *   - NeoPixels rotate R/G/B across the 3 pixels once per second
 *
 * Libraries required:
 *   - Kerbal7SegmentCore  (install from zip)
 *   - KerbalModuleCommon  (dependency)
 *   - tinyNeoPixel        (bundled with megaTinyCore)
 */

#include <Kerbal7SegmentCore.h>

// Dummy I2C address — no master connected in this standalone demo
#define I2C_ADDRESS    0x2A
#define MODULE_TYPE_ID 0x0B

// All three panel buttons set to MOMENTARY — no LED state needed in demo
const ButtonConfig btnConfigs[K7SC_NEO_COUNT] = {
    { BTN_MODE_MOMENTARY, {K7SC_OFF, K7SC_OFF, K7SC_OFF}, 0, 0 },
    { BTN_MODE_MOMENTARY, {K7SC_OFF, K7SC_OFF, K7SC_OFF}, 0, 0 },
    { BTN_MODE_MOMENTARY, {K7SC_OFF, K7SC_OFF, K7SC_OFF}, 0, 0 },
};

// Colour table for NeoPixel rotation: index 0=R, 1=G, 2=B
static const uint8_t ct[3][3] = {
    {255,   0,   0},  // RED
    {  0, 255,   0},  // GREEN
    {  0,   0, 255},  // BLUE
};

// Push a colour rotation phase to the NeoPixels via the library
static void neo_show(uint8_t phase)
{
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++) {
        uint8_t c = (i + phase) % 3;
        buttonsSetPixelColor(i, ct[c][0], ct[c][1], ct[c][2]);
    }
    buttonsShow();
}

void setup()
{
    k7scBegin(I2C_ADDRESS, MODULE_TYPE_ID, KMC_CAP_DISPLAY, btnConfigs, 0);

    // Set initial pixel colours: px0=R, px1=G, px2=B
    neo_show(0);
}

void loop()
{
    static uint32_t lastNeo  = 0;
    static uint8_t  neoPhase = 0;

    // Library handles encoder polling, display updates, button debounce
    k7scUpdate();

    // Encoder button resets display to 0
    if (buttonsGetEncoderPress()) {
        encoderSetValue(0);
    }

    // NeoPixel colour rotation — once per second
    uint32_t now = millis();
    if (now - lastNeo >= 1000UL) {
        lastNeo  = now;
        neoPhase = (neoPhase + 1) % 3;
        neo_show(neoPhase);
    }
}

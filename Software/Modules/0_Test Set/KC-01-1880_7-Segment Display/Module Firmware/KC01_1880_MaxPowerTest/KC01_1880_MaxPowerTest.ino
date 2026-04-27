/*
 * KC-01-1880  Maximum Power Draw Test
 * Board:      ATtiny816  (megaTinyCore, 20 MHz internal)
 *
 * Holds the module at maximum current draw:
 *   - All 4 x 7-segment digits showing 8 (all segments lit)
 *   - MAX7219 intensity at maximum (15)
 *   - All 3 NeoPixels at full white (255, 255, 255)
 *   - NeoPixel brightness at maximum (255)
 *
 * Flash this sketch, connect ammeter in series with VCC,
 * and record the current reading.
 *
 * Libraries required:
 *   - Kerbal7SegmentCore  (v1.1.0)
 *   - KerbalModuleCommon
 *   - tinyNeoPixel        (bundled with megaTinyCore)
 */

#include <KerbalModuleCommon.h>
#include <Kerbal7SegmentCore.h>

#define I2C_ADDRESS    0x2A
#define MODULE_TYPE_ID KMC_TYPE_GPWS_INPUT

const ButtonConfig btnConfigs[K7SC_NEO_COUNT] = {
    { BTN_MODE_MOMENTARY, {K7SC_OFF, K7SC_OFF, K7SC_OFF}, 0, 0 },
    { BTN_MODE_MOMENTARY, {K7SC_OFF, K7SC_OFF, K7SC_OFF}, 0, 0 },
    { BTN_MODE_MOMENTARY, {K7SC_OFF, K7SC_OFF, K7SC_OFF}, 0, 0 },
};

void setup()
{
    k7scBegin(I2C_ADDRESS, MODULE_TYPE_ID, KMC_CAP_DISPLAY, btnConfigs, 0);

    // MAX7219: all segments on (digit 8 = all segments in Code-B)
    // and maximum intensity
    displaySetIntensity(15);
    displaySetValue(8888);

    // NeoPixels: full white at maximum brightness
    buttonsSetBrightness(255);
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++) {
        buttonsSetPixelColor(i, 255, 255, 255);
    }
    buttonsShow();
}

void loop()
{
    // Nothing — hold steady state for power measurement
}

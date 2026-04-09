/**
 * @file        LEDs.cpp
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — EVA Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Implementation of NeoPixel LED control for the EVA module.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include "LEDs.h"

// ============================================================
//  tinyNeoPixel_Static pixel buffer
//  3 bytes per pixel x 6 pixels = 18 bytes
// ============================================================

static uint8_t _pixelBuffer[EVA_NEO_COUNT * 3];
static tinyNeoPixel _pixels(EVA_NEO_COUNT, EVA_NEO_PIN,
                             EVA_NEO_COLOR_ORDER, _pixelBuffer);

// ============================================================
//  Per-button LED state
// ============================================================

static uint8_t  _state[EVA_BUTTON_COUNT];
static uint8_t  _brightness = EVA_ENABLED_BRIGHTNESS;

// ============================================================
//  ledsBegin()
// ============================================================

void ledsBegin() {
    pinMode(EVA_NEO_PIN, OUTPUT);
    memset(_pixelBuffer, 0, sizeof(_pixelBuffer));
    memset(_state, LED_OFF, sizeof(_state));
    _brightness = EVA_ENABLED_BRIGHTNESS;
}

// ============================================================
//  ledsSetAll()
// ============================================================

void ledsSetAll(const uint8_t* payload) {
    for (uint8_t i = 0; i < EVA_BUTTON_COUNT; i++) {
        _state[i] = ledPackGet(payload, i);
    }
}

// ============================================================
//  ledSetState()
// ============================================================

void ledSetState(uint8_t index, uint8_t state) {
    if (index >= EVA_BUTTON_COUNT) return;
    _state[index] = state;
}

// ============================================================
//  ledsSetBrightness()
// ============================================================

void ledsSetBrightness(uint8_t brightness) {
    _brightness = brightness;
}

// ============================================================
//  ledsClearAll()
// ============================================================

void ledsClearAll() {
    memset(_state, LED_OFF, sizeof(_state));
}

// ============================================================
//  ledsRender()
// ============================================================

void ledsRender() {
    for (uint8_t i = 0; i < EVA_BUTTON_COUNT; i++) {
        RGBColor color = EVA_OFF;

        switch (_state[i]) {
            case LED_OFF:
                color = EVA_OFF;
                break;

            case LED_ENABLED:
                // Dim white scaled by brightness setting
                color = evaScaleColor({255, 255, 255}, _brightness);
                break;

            case LED_ACTIVE:
                // Full brightness EVA palette color
                color = EVA_ACTIVE_COLORS[i];
                break;

            case LED_WARNING:
                // Amber — reserved for future use
                color = {245, 158, 11};
                break;

            case LED_ALERT:
                // Red — reserved for future use
                color = {239, 68, 68};
                break;

            case LED_ARMED:
                // Cyan — reserved for future use
                color = {6, 182, 212};
                break;

            default:
                color = EVA_OFF;
                break;
        }

        _pixels.setPixelColor(i, color.r, color.g, color.b);
    }

    _pixels.show();
}

// ============================================================
//  ledsBulbTest()
// ============================================================

void ledsBulbTest(uint16_t durationMs) {
    for (uint8_t i = 0; i < EVA_NEO_COUNT; i++) {
        _pixels.setPixelColor(i, 255, 255, 255);
    }
    _pixels.show();
    delay(durationMs);
    ledsRender();
}

// ============================================================
//  ledsGetState()
// ============================================================

uint8_t ledsGetState(uint8_t index) {
    if (index >= EVA_BUTTON_COUNT) return LED_OFF;
    return _state[index];
}

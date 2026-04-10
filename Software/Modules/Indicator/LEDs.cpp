/**
 * @file        LEDs.cpp
 * @version     2.0
 * @date        2026-04-09
 * @project     Kerbal Controller Mk1 — Indicator Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       NeoPixel LED control implementation (18 pixels).
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include <tinyNeoPixel_Static.h>
#include "LEDs.h"

// ============================================================
//  NeoPixel — SK6812mini-012 RGB, Port A
// ============================================================

static uint8_t _pixelBuffer[IND_NEO_COUNT * 3];
static tinyNeoPixel _pixels(IND_NEO_COUNT, IND_PIN_NEOPIX,
                             IND_NEO_COLOR_ORDER, _pixelBuffer);

// ============================================================
//  State
// ============================================================

static uint8_t  _state[IND_NEO_COUNT]   = {0};
static bool     _flashPhase             = false;
static uint32_t _lastFlashToggle        = 0;
static bool     _sleeping               = false;

// ============================================================
//  _resolveColor() — map state to physical color for a pixel
// ============================================================

static RGBColor _resolveColor(uint8_t pixelIdx, uint8_t state) {
    switch (state) {
        case LED_STATE_OFF:
            return IND_OFF;

        case LED_STATE_ENABLED:
            return IND_WHITE_DIM;

        case LED_STATE_ACTIVE:
            return IND_ACTIVE_COLORS[pixelIdx];

        case LED_STATE_WARNING:
            // Amber flash — color driven by _flashPhase
            return _flashPhase ? IND_AMBER : IND_OFF;

        case LED_STATE_ALERT:
            // Red flash — color driven by _flashPhase
            return _flashPhase ? IND_RED : IND_OFF;

        case LED_STATE_ARMED:
            return IND_CYAN;

        case LED_STATE_PARTIAL:
            return IND_AMBER;

        default:
            return IND_OFF;
    }
}

// ============================================================
//  _render() — push current state to NeoPixel hardware
// ============================================================

static void _render() {
    for (uint8_t i = 0; i < IND_NEO_COUNT; i++) {
        RGBColor c = _resolveColor(i, _state[i]);
        _pixels.setPixelColor(i, c.r, c.g, c.b);
    }
    _pixels.show();
}

// ============================================================
//  ledsBegin()
// ============================================================

void ledsBegin() {
    pinMode(IND_PIN_NEOPIX, OUTPUT);
    memset(_pixelBuffer, 0, sizeof(_pixelBuffer));
    memset(_state, LED_STATE_OFF, sizeof(_state));
    _flashPhase      = false;
    _lastFlashToggle = millis();
    _sleeping        = false;
    _render();
}

// ============================================================
//  ledsUpdate()
// ============================================================

void ledsUpdate() {
    if (_sleeping) return;

    uint32_t now = millis();
    bool hasWarning = false;
    bool hasAlert   = false;

    for (uint8_t i = 0; i < IND_NEO_COUNT; i++) {
        if (_state[i] == LED_STATE_WARNING) hasWarning = true;
        if (_state[i] == LED_STATE_ALERT)   hasAlert   = true;
    }

    // Determine active flash interval
    // Alert takes priority over warning if both present
    bool needsFlash = hasWarning || hasAlert;
    if (needsFlash) {
        uint16_t interval = hasAlert
                          ? (_flashPhase ? IND_ALERT_ON_MS  : IND_ALERT_OFF_MS)
                          : (_flashPhase ? IND_WARNING_ON_MS: IND_WARNING_OFF_MS);

        if ((now - _lastFlashToggle) >= interval) {
            _flashPhase      = !_flashPhase;
            _lastFlashToggle = now;
            _render();
        }
    }
}

// ============================================================
//  ledsSetFromPayload()
// ============================================================

void ledsSetFromPayload(const uint8_t* payload) {
    for (uint8_t i = 0; i < IND_NEO_COUNT; i++) {
        _state[i] = kmcLedPackGet(payload, i);
    }
    _render();
}

// ============================================================
//  ledsSetState()
// ============================================================

void ledsSetState(uint8_t index, uint8_t state) {
    if (index >= IND_NEO_COUNT) return;
    _state[index] = state;
    _render();
}

// ============================================================
//  ledsClearAll()
// ============================================================

void ledsClearAll() {
    memset(_state, LED_STATE_OFF, sizeof(_state));
    _render();
}

// ============================================================
//  ledsBulbTest()
// ============================================================

void ledsBulbTest(uint16_t durationMs) {
    for (uint8_t i = 0; i < IND_NEO_COUNT; i++) {
        _pixels.setPixelColor(i, 255, 255, 255);
    }
    _pixels.show();
    delay(durationMs);
    _render();
}

// ============================================================
//  ledsSetEnabled()
// ============================================================

void ledsSetEnabled() {
    for (uint8_t i = 0; i < IND_NEO_COUNT; i++) {
        _state[i] = LED_STATE_ENABLED;
    }
    _render();
}

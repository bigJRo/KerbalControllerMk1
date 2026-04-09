/**
 * @file        K7SC_Buttons.cpp
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Button input and SK6812MINI-EA NeoPixel LED implementation.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include <tinyNeoPixel_Static.h>
#include "K7SC_Buttons.h"

// ============================================================
//  Button pin table (BTN01, BTN02, BTN03, BTN_EN)
// ============================================================

static const uint8_t _btnPins[K7SC_BUTTON_COUNT] = {
    K7SC_PIN_BTN01,
    K7SC_PIN_BTN02,
    K7SC_PIN_BTN03,
    K7SC_PIN_BTN_EN
};

// ============================================================
//  NeoPixel — SK6812MINI-EA GRBW, 4 bytes per pixel
// ============================================================

static uint8_t _pixelBuffer[K7SC_NEO_COUNT * 4];
static tinyNeoPixel _pixels(K7SC_NEO_COUNT, K7SC_PIN_NEOPIX,
                             K7SC_NEO_COLOR_ORDER, _pixelBuffer);

// ============================================================
//  Button configuration
// ============================================================

static ButtonConfig _configs[K7SC_NEO_COUNT];

// ============================================================
//  Button state
// ============================================================

static uint8_t  _rawState          = 0;   // current debounced raw state
static uint8_t  _changeMask        = 0;
static uint8_t  _eventMask         = 0;   // edges this cycle
static bool     _intPending        = false;
static bool     _encoderPressed    = false;

// Debounce
static uint8_t  _debounceCount[K7SC_BUTTON_COUNT]     = {0};
static uint8_t  _debounceCandidate[K7SC_BUTTON_COUNT] = {0};

// Per-NeoPixel button logical state
static uint8_t  _cycleState[K7SC_NEO_COUNT]   = {0, 0, 0}; // for CYCLE mode
static bool     _toggleActive[K7SC_NEO_COUNT] = {false, false, false};

// Flash timing
static uint32_t _flashStart[K7SC_NEO_COUNT]   = {0, 0, 0};
static bool     _flashing[K7SC_NEO_COUNT]     = {false, false, false};

// ============================================================
//  _renderButton() — set NeoPixel color for button i
// ============================================================

static void _renderButton(uint8_t i) {
    GRBWColor color = K7SC_ENABLED_COLOR;

    switch (_configs[i].mode) {
        case BTN_MODE_CYCLE:
            if (_cycleState[i] == 0) {
                color = K7SC_ENABLED_COLOR;
            } else {
                color = _configs[i].colors[_cycleState[i]];
            }
            break;

        case BTN_MODE_TOGGLE:
            color = _toggleActive[i]
                ? _configs[i].colors[1]
                : K7SC_ENABLED_COLOR;
            break;

        case BTN_MODE_FLASH:
            color = _flashing[i]
                ? _configs[i].colors[0]
                : K7SC_ENABLED_COLOR;
            break;

        case BTN_MODE_MOMENTARY:
            color = K7SC_OFF;
            break;
    }

    _pixels.setPixelColor(i, color.g, color.r, color.b, color.w);
}

// ============================================================
//  buttonsBegin()
// ============================================================

void buttonsBegin(const ButtonConfig* configs) {
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++) {
        _configs[i] = configs[i];
    }

    // GPIO init — all active high with hardware pull-downs
    for (uint8_t i = 0; i < K7SC_BUTTON_COUNT; i++) {
        pinMode(_btnPins[i], INPUT);
        _debounceCount[i] = 0;
    }

    // NeoPixel init
    pinMode(K7SC_PIN_NEOPIX, OUTPUT);
    memset(_pixelBuffer, 0, sizeof(_pixelBuffer));

    // Reset state
    memset(_cycleState,   0, sizeof(_cycleState));
    memset(_toggleActive, 0, sizeof(_toggleActive));
    memset(_flashing,     0, sizeof(_flashing));
    _rawState       = 0;
    _changeMask     = 0;
    _eventMask      = 0;
    _intPending     = false;
    _encoderPressed = false;
    memset(_debounceCount,     0, sizeof(_debounceCount));
    memset(_debounceCandidate, 0, sizeof(_debounceCandidate));

    buttonsRender();
}

// ============================================================
//  buttonsPoll()
// ============================================================

bool buttonsPoll() {
    uint32_t now = millis();

    // Service flash timeouts
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++) {
        if (_flashing[i]) {
            uint16_t dur = _configs[i].flashMs
                         ? _configs[i].flashMs
                         : K7SC_FLASH_DURATION_MS;
            if ((now - _flashStart[i]) >= dur) {
                _flashing[i] = false;
                _renderButton(i);
            }
        }
    }

    // Read all button pins
    uint8_t raw = 0;
    for (uint8_t i = 0; i < K7SC_BUTTON_COUNT; i++) {
        if (digitalRead(_btnPins[i])) raw |= (1 << i);
    }

    // Per-button debounce with independent candidate tracking.
    bool anyEvent = false;

    for (uint8_t i = 0; i < K7SC_BUTTON_COUNT; i++) {
        bool rawBit  = (raw >> i) & 0x01;
        bool liveBit = (_rawState >> i) & 0x01;

        if (rawBit != _debounceCandidate[i]) {
            _debounceCandidate[i] = rawBit;
            _debounceCount[i]     = 0;
        } else if (rawBit != liveBit) {
            _debounceCount[i]++;
            if (_debounceCount[i] >= K7SC_BTN_DEBOUNCE_COUNT) {
                if (rawBit) {
                    // Rising edge — button pressed
                    _rawState |= (1 << i);
                    _changeMask |= (1 << i);
                    _eventMask  |= (1 << i);
                    _intPending  = true;
                    anyEvent     = true;

                    // Handle NeoPixel button logic
                    if (i < K7SC_NEO_COUNT) {
                        switch (_configs[i].mode) {
                            case BTN_MODE_CYCLE:
                                _cycleState[i]++;
                                if (_cycleState[i] >= _configs[i].numStates) {
                                    _cycleState[i] = 0;
                                }
                                _renderButton(i);
                                break;

                            case BTN_MODE_TOGGLE:
                                _toggleActive[i] = !_toggleActive[i];
                                _renderButton(i);
                                break;

                            case BTN_MODE_FLASH:
                                _flashing[i]   = true;
                                _flashStart[i] = now;
                                _renderButton(i);
                                break;

                            case BTN_MODE_MOMENTARY:
                                break;
                        }
                    } else {
                        // BTN_EN — encoder pushbutton
                        _encoderPressed = true;
                    }
                } else {
                    // Falling edge — button released
                    _rawState &= ~(1 << i);
                    _changeMask |= (1 << i);
                    _intPending  = true;
                    anyEvent     = true;
                }
                _debounceCount[i] = 0;
            }
        } else {
            _debounceCount[i] = 0;
        }
    }

    if (anyEvent) _pixels.show();
    return anyEvent;
}

// ============================================================
//  buttonsIsIntPending()
// ============================================================

bool buttonsIsIntPending() {
    return _intPending;
}

// ============================================================
//  buttonsGetEvents()
// ============================================================

uint8_t buttonsGetEvents() {
    return _eventMask;
}

// ============================================================
//  buttonsGetChangeMask()
// ============================================================

uint8_t buttonsGetChangeMask() {
    uint8_t mask    = _changeMask;
    uint8_t latch   = _rawState;   // snapshot live state at read time

    _changeMask = 0;
    _eventMask  = 0;
    _intPending = false;

    // Re-assert if live state diverged during this read transaction.
    // Guarantees every state change edge generates at least one INT,
    // even if a button transitions while the controller is reading.
    if (_rawState != latch) {
        _changeMask = _rawState ^ latch;
        _intPending = true;
    }

    return mask;
}

// ============================================================
//  buttonsGetStateByte()
// ============================================================

uint8_t buttonsGetStateByte() {
    uint8_t state = 0;

    // BTN01: bits 0-1 carry cycle state
    state |= (_cycleState[0] & K7SC_STATE_BTN01_MASK);

    // BTN02: bit 2
    if (_toggleActive[1]) state |= (1 << K7SC_STATE_BTN02_BIT);

    // BTN03: bit 3
    if (_toggleActive[2]) state |= (1 << K7SC_STATE_BTN03_BIT);

    return state;
}

// ============================================================
//  buttonsGetEncoderPress()
// ============================================================

bool buttonsGetEncoderPress() {
    bool val = _encoderPressed;
    _encoderPressed = false;
    return val;
}

// ============================================================
//  buttonsClearAll()
// ============================================================

void buttonsClearAll() {
    _rawState       = 0;
    _changeMask     = 0;
    _eventMask      = 0;
    _intPending     = false;
    _encoderPressed = false;
    memset(_debounceCount,     0, sizeof(_debounceCount));
    memset(_debounceCandidate, 0, sizeof(_debounceCandidate));
    memset(_cycleState,        0, sizeof(_cycleState));
    memset(_toggleActive,      0, sizeof(_toggleActive));
    memset(_flashing,          0, sizeof(_flashing));
    buttonsRender();
}

// ============================================================
//  buttonsClearLEDs()
// ============================================================

void buttonsClearLEDs() {
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++) {
        _pixels.setPixelColor(i, 0, 0, 0, 0);
    }
    _pixels.show();
}

// ============================================================
//  buttonsRestoreLEDs()
// ============================================================

void buttonsRestoreLEDs() {
    buttonsRender();
}

// ============================================================
//  buttonsRender()
// ============================================================

void buttonsRender() {
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++) {
        _renderButton(i);
    }
    _pixels.show();
}

// ============================================================
//  buttonsBulbTest()
// ============================================================

void buttonsBulbTest(uint16_t durationMs) {
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++) {
        _pixels.setPixelColor(i, 0, 0, 0, 255);  // Full white channel
    }
    _pixels.show();
    delay(durationMs);
    buttonsRender();
}

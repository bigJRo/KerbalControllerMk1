/**
 * @file        KJC_Buttons.cpp
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Implementation of button input and NeoPixel LED
 *              control for the KerbalJoystickCore library.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include <tinyNeoPixel_Static.h>
#include "KJC_Buttons.h"

// ============================================================
//  Button pin table
//  Index 0 = BTN_JOY, 1 = BTN01, 2 = BTN02
// ============================================================

static const uint8_t _btnPins[KJC_BUTTON_COUNT] = {
    KJC_PIN_BTN_JOY,
    KJC_PIN_BTN01,
    KJC_PIN_BTN02
};

// ============================================================
//  Button state
// ============================================================

static uint8_t  _state          = 0;
static uint8_t  _latchedState   = 0;
static uint8_t  _changeMask     = 0;
static bool     _intPending     = false;

// ============================================================
//  Debounce state
// ============================================================

#define BTN_DEBOUNCE_COUNT      4
static uint8_t  _debounceCount[KJC_BUTTON_COUNT] = {0};
static uint8_t  _debounceCandidate = 0;

// ============================================================
//  NeoPixel LED state
// ============================================================

static uint8_t  _ledState[KJC_NEO_COUNT]  = {KJC_LED_OFF, KJC_LED_OFF};
static uint8_t  _brightness               = KJC_ENABLED_BRIGHTNESS;
static RGBColor _activeColors[KJC_NEO_COUNT];

static uint8_t  _pixelBuffer[KJC_NEO_COUNT * 3];
static tinyNeoPixel _pixels(KJC_NEO_COUNT, KJC_PIN_NEOPIX,
                             KJC_NEO_COLOR_ORDER, _pixelBuffer);

// ============================================================
//  buttonsBegin()
// ============================================================

void buttonsBegin(const RGBColor* activeColors, uint8_t brightness) {
    // Store active colors for BTN01 and BTN02
    for (uint8_t i = 0; i < KJC_NEO_COUNT; i++) {
        _activeColors[i] = activeColors[i];
    }
    _brightness = brightness;

    // Button GPIO pins — all active high with hardware pull-downs
    for (uint8_t i = 0; i < KJC_BUTTON_COUNT; i++) {
        pinMode(_btnPins[i], INPUT);
        _debounceCount[i] = 0;
    }

    // NeoPixel init
    pinMode(KJC_PIN_NEOPIX, OUTPUT);
    memset(_pixelBuffer, 0, sizeof(_pixelBuffer));

    // All NeoPixel buttons start ENABLED
    // BTN_JOY has no LED — index 0 in button array, not in LED array
    _ledState[0] = KJC_LED_ENABLED;  // BTN01
    _ledState[1] = KJC_LED_ENABLED;  // BTN02

    _state             = 0;
    _latchedState      = 0;
    _changeMask        = 0;
    _intPending        = false;
    _debounceCandidate = 0;

    buttonsRender();
}

// ============================================================
//  buttonsPoll()
// ============================================================

bool buttonsPoll() {
    // Read all button pins into a bitmask
    uint8_t raw = 0;
    for (uint8_t i = 0; i < KJC_BUTTON_COUNT; i++) {
        if (digitalRead(_btnPins[i])) {
            raw |= (1 << i);
        }
    }

    // Reset debounce on candidate change
    if (raw != _debounceCandidate) {
        _debounceCandidate = raw;
        memset(_debounceCount, 0, sizeof(_debounceCount));
        return false;
    }

    bool anyChanged = false;
    for (uint8_t i = 0; i < KJC_BUTTON_COUNT; i++) {
        bool rawBit  = (raw >> i) & 0x01;
        bool liveBit = (_state >> i) & 0x01;

        if (rawBit != liveBit) {
            _debounceCount[i]++;
            if (_debounceCount[i] >= BTN_DEBOUNCE_COUNT) {
                if (rawBit)
                    _state |=  (1 << i);
                else
                    _state &= ~(1 << i);

                _changeMask |= (1 << i);
                _debounceCount[i] = 0;
                _intPending  = true;
                anyChanged   = true;
            }
        } else {
            _debounceCount[i] = 0;
        }
    }

    return anyChanged;
}

// ============================================================
//  buttonsIsIntPending()
// ============================================================

bool buttonsIsIntPending() {
    return _intPending;
}

// ============================================================
//  buttonsGetState()
// ============================================================

uint8_t buttonsGetState() {
    return _state;
}

// ============================================================
//  buttonsGetChangeMask()
// ============================================================

uint8_t buttonsGetChangeMask() {
    uint8_t mask = _changeMask;
    _changeMask  = 0;
    _intPending  = false;

    // Re-assert if state changed again during this read
    if (_state != _latchedState) {
        _changeMask = _state ^ _latchedState;
        _intPending = true;
    }
    _latchedState = _state;
    return mask;
}

// ============================================================
//  buttonsClearAll()
// ============================================================

void buttonsClearAll() {
    _state             = 0;
    _latchedState      = 0;
    _changeMask        = 0;
    _intPending        = false;
    _debounceCandidate = 0;
    memset(_debounceCount, 0, sizeof(_debounceCount));
}

// ============================================================
//  buttonsSetLEDs()
// ============================================================

void buttonsSetLEDs(const uint8_t* payload) {
    // BTN01 is at nibble index 0, BTN02 at nibble index 1
    for (uint8_t i = 0; i < KJC_NEO_COUNT; i++) {
        _ledState[i] = kjcPackGet(payload, i);
    }
}

// ============================================================
//  buttonsSetLED()
// ============================================================

void buttonsSetLED(uint8_t index, uint8_t state) {
    if (index >= KJC_NEO_COUNT) return;
    _ledState[index] = state;
}

// ============================================================
//  buttonsSetBrightness()
// ============================================================

void buttonsSetBrightness(uint8_t brightness) {
    _brightness = brightness;
}

// ============================================================
//  buttonsClearLEDs()
// ============================================================

void buttonsClearLEDs() {
    for (uint8_t i = 0; i < KJC_NEO_COUNT; i++) {
        _ledState[i] = KJC_LED_OFF;
    }
}

// ============================================================
//  buttonsRender()
// ============================================================

void buttonsRender() {
    for (uint8_t i = 0; i < KJC_NEO_COUNT; i++) {
        RGBColor color = KJC_OFF;

        switch (_ledState[i]) {
            case KJC_LED_OFF:
                color = KJC_OFF;
                break;
            case KJC_LED_ENABLED:
                color = kjcScaleColor({255, 255, 255}, _brightness);
                break;
            case KJC_LED_ACTIVE:
                color = _activeColors[i];
                break;
            case KJC_LED_WARNING:
                color = KJC_AMBER;
                break;
            case KJC_LED_ALERT:
                color = KJC_RED;
                break;
            case KJC_LED_ARMED:
                color = KJC_CYAN;
                break;
            default:
                color = KJC_OFF;
                break;
        }

        _pixels.setPixelColor(i, color.r, color.g, color.b);
    }
    _pixels.show();
}

// ============================================================
//  buttonsBulbTest()
// ============================================================

void buttonsBulbTest(uint16_t durationMs) {
    for (uint8_t i = 0; i < KJC_NEO_COUNT; i++) {
        _pixels.setPixelColor(i, 255, 255, 255);
    }
    _pixels.show();
    delay(durationMs);
    buttonsRender();
}

// ============================================================
//  buttonsGetLEDState()
// ============================================================

uint8_t buttonsGetLEDState(uint8_t index) {
    if (index >= KJC_NEO_COUNT) return KJC_LED_OFF;
    return _ledState[index];
}

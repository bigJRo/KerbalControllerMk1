/**
 * @file        K7SC_Buttons.cpp
 * @version     1.1.0
 * @date        2026-04-26
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

// Forward declaration — defined later in this file
static void _renderButton(uint8_t i);
void buttonsRender();

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
//  NeoPixel — SK6812MINI-EA, NEO_GRB 3 bytes/pixel
//  tinyNeoPixel_Static requires a pre-allocated pixel buffer.
//  Buffer size = pixel count × bytes per pixel.
// ============================================================

static uint8_t _pixelBuffer[K7SC_NEO_COUNT * K7SC_NEO_BYTES_PER_PIXEL];
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

// Debounce — time-based: track when each button last changed state
static uint32_t _debounceTime[K7SC_BUTTON_COUNT]      = {0};
static uint8_t  _debounceCandidate[K7SC_BUTTON_COUNT] = {0xFF, 0xFF, 0xFF, 0xFF};

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
            color = (_cycleState[i] == 0)
                ? K7SC_ENABLED_COLOR
                : _configs[i].colors[_cycleState[i]];
            break;
        case BTN_MODE_TOGGLE:
            color = _toggleActive[i] ? _configs[i].colors[1] : K7SC_ENABLED_COLOR;
            break;
        case BTN_MODE_FLASH:
            color = _flashing[i] ? _configs[i].colors[0] : K7SC_ENABLED_COLOR;
            break;
        case BTN_MODE_MOMENTARY:
            color = K7SC_OFF;
            break;
    }

    // NEO_GRB 3-byte pixel — R/G/B only, no W channel
    _pixels.setPixelColor(i, color.r, color.g, color.b);
}

// ============================================================
//  buttonsBegin()
// ============================================================

void buttonsBegin(const ButtonConfig* configs) {
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++) {
        _configs[i] = configs[i];
    }

    // GPIO init
    // All buttons: active HIGH, hardware pull-downs — INPUT only
    for (uint8_t i = 0; i < K7SC_BUTTON_COUNT; i++) {
        pinMode(_btnPins[i], INPUT);
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
    memset(_debounceTime,      0,    sizeof(_debounceTime));
    for (uint8_t i = 0; i < K7SC_BUTTON_COUNT; i++)
        _debounceCandidate[i] = 0xFF;  // uninitialised sentinel

    buttonsRender();
}

// ============================================================
//  buttonsPoll()
// ============================================================

bool buttonsPoll() {
    uint32_t now = millis();

    // Service flash timeouts
    bool flashExpired = false;
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++) {
        if (_flashing[i]) {
            uint16_t dur = _configs[i].flashMs
                         ? _configs[i].flashMs
                         : K7SC_FLASH_DURATION_MS;
            if ((now - _flashStart[i]) >= dur) {
                _flashing[i] = false;
                _renderButton(i);
                flashExpired = true;
            }
        }
    }
    if (flashExpired) _pixels.show();

    // Read all button pins — all active HIGH with hardware pull-downs
    uint8_t raw = 0;
    for (uint8_t i = 0; i < K7SC_BUTTON_COUNT; i++) {
        if (digitalRead(_btnPins[i])) raw |= (1 << i);
    }

    // Per-button time-based debounce.
    // A state change is accepted only after the new state has been
    // stable for K7SC_BTN_DEBOUNCE_MS without reverting.
    bool anyEvent = false;

    for (uint8_t i = 0; i < K7SC_BUTTON_COUNT; i++) {
        uint8_t rawBit  = (raw >> i) & 0x01;
        uint8_t liveBit = (_rawState >> i) & 0x01;

        if (_debounceCandidate[i] == 0xFF) {
            // First call — initialise candidate to current raw state
            _debounceCandidate[i] = rawBit;
            _debounceTime[i]      = now;
            continue;
        }

        if (rawBit != _debounceCandidate[i]) {
            // State changed — restart the debounce timer
            _debounceCandidate[i] = rawBit;
            _debounceTime[i]      = now;
        } else if (rawBit != liveBit &&
                   (now - _debounceTime[i]) >= K7SC_BTN_DEBOUNCE_MS) {
            // Candidate has been stable long enough — accept the change
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
                    _eventMask  |= (1 << i);
                    _intPending  = true;
                }
            } else {
                // Falling edge — button released.
                // Update raw state and change mask only. Do NOT set _intPending
                // or anyEvent — release edges would consume a waitForINT() call
                // before the press event packet is read.
                _rawState   &= ~(1 << i);
                _changeMask |=  (1 << i);
            }
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
    memset(_debounceTime,      0,    sizeof(_debounceTime));
    for (uint8_t i = 0; i < K7SC_BUTTON_COUNT; i++)
        _debounceCandidate[i] = 0xFF;
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
        _pixels.setPixelColor(i, 0, 0, 0);
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
//  buttonsSetPixelColor()
// ============================================================

void buttonsSetPixelColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= K7SC_NEO_COUNT) return;
    _pixels.setPixelColor(index, r, g, b);
}

// ============================================================
//  buttonsShow()
// ============================================================

void buttonsShow() {
    _pixels.show();
}

// ============================================================
//  buttonsBulbTest()
//  Lights all pixels white. Call buttonsBulbTestEnd() to restore.
//  Non-blocking — does not delay.
// ============================================================

void buttonsSetBrightness(uint8_t brightness) {
    _pixels.setBrightness(brightness);
    buttonsRender();
}

void buttonsBulbTest() {
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++) {
        _pixels.setPixelColor(i, 255, 255, 255);
    }
    _pixels.show();
}

void buttonsBulbTestEnd() {
    buttonsRender();
}

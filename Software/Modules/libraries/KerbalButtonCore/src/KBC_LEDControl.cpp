/**
 * @file        KBC_LEDControl.cpp
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Implementation of the KBCLEDControl LED state machine
 *              for the KerbalButtonCore library.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the KerbalButtonCore (KBC) library.
 *              Requires: tinyNeoPixel_Static library (megaTinyCore)
 *              Hardware: KC-01-1822 v1.1
 *              Protocol: KBC_Protocol_Spec.md v1.1
 */

#include "KBC_LEDControl.h"

// ============================================================
//  Discrete LED pin table (static, matches KBC_DISCRETE_PINS macro)
//
//  Indexed by (kbcIndex - KBC_DISCRETE_INDEX_FIRST).
//  Note PC2/PC1 swap for led_15/led_16 is handled here.
// ============================================================

const uint8_t KBCLEDControl::_discretePins[KBC_DISCRETE_COUNT] = {
    KBC_PIN_LED_12,   // KBC index 12 — PCB: led_13, PIN_PB3
    KBC_PIN_LED_13,   // KBC index 13 — PCB: led_14, PIN_PC0
    KBC_PIN_LED_14,   // KBC index 14 — PCB: led_15, PIN_PC2
    KBC_PIN_LED_15    // KBC index 15 — PCB: led_16, PIN_PC1
};

// ============================================================
//  Constructor
// ============================================================

KBCLEDControl::KBCLEDControl()
    : _pixels(KBC_NEO_COUNT, KBC_PIN_NEO, KBC_NEO_COLOR_ORDER, _pixelBuffer)
    , _activeColors(nullptr)
    , _brightness(KBC_ENABLED_BRIGHTNESS)
    , _flashOn(true)
    , _flashLastToggle(0)
{
    memset(_pixelBuffer, 0, sizeof(_pixelBuffer));
    memset(_state, KBC_LED_OFF, sizeof(_state));
}

// ============================================================
//  begin()
// ============================================================

void KBCLEDControl::begin(const RGBColor* activeColors, uint8_t brightness) {
    _activeColors = activeColors;
    _brightness   = brightness;
    _flashOn      = true;
    _flashLastToggle = millis();

    // Initialise discrete LED pins as outputs, all off
    for (uint8_t i = 0; i < KBC_DISCRETE_COUNT; i++) {
        pinMode(_discretePins[i], OUTPUT);
        digitalWrite(_discretePins[i], LOW);
    }

    // Initialise NeoPixel chain
    // Note: tinyNeoPixel_Static does not call begin() — pixel buffer
    // and pinMode are managed by the constructor and here directly.
    pinMode(KBC_PIN_NEO, OUTPUT);

    // Clear all button states and render
    clearAll();
    render();
}

// ============================================================
//  setLEDState()
// ============================================================

void KBCLEDControl::setLEDState(const uint8_t* payload) {
    for (uint8_t i = 0; i < KBC_BUTTON_COUNT; i++) {
        _state[i] = KBC_ledPackGet(payload, i);
    }
}

// ============================================================
//  setButtonState()
// ============================================================

void KBCLEDControl::setButtonState(uint8_t index, uint8_t state) {
    if (index >= KBC_BUTTON_COUNT) return;
    _state[index] = state;
}

// ============================================================
//  setBrightness()
// ============================================================

void KBCLEDControl::setBrightness(uint8_t brightness) {
    _brightness = brightness;
}

// ============================================================
//  clearAll()
// ============================================================

void KBCLEDControl::clearAll() {
    memset(_state, KBC_LED_OFF, sizeof(_state));
}

// ============================================================
//  update()
// ============================================================

bool KBCLEDControl::update() {
    // Only service flash timing if any button needs it
    if (!hasExtendedState()) return false;

    uint32_t now = millis();
    uint32_t elapsed = now - _flashLastToggle;

    // Determine the interval for the current flash phase
    uint32_t interval;
    if (_flashOn) {
        // Check both WARNING and ALERT on times — use the shorter
        // interval if both are active so neither is starved
        bool hasWarning = false;
        bool hasAlert   = false;
        for (uint8_t i = 0; i < KBC_BUTTON_COUNT; i++) {
            if (_state[i] == KBC_LED_WARNING) hasWarning = true;
            if (_state[i] == KBC_LED_ALERT)   hasAlert   = true;
        }
        if (hasAlert) {
            interval = KBC_ALERT_ON_MS;
        } else if (hasWarning) {
            interval = KBC_WARNING_ON_MS;
        } else {
            return false;
        }
    } else {
        bool hasWarning = false;
        bool hasAlert   = false;
        for (uint8_t i = 0; i < KBC_BUTTON_COUNT; i++) {
            if (_state[i] == KBC_LED_WARNING) hasWarning = true;
            if (_state[i] == KBC_LED_ALERT)   hasAlert   = true;
        }
        if (hasAlert) {
            interval = KBC_ALERT_OFF_MS;
        } else if (hasWarning) {
            interval = KBC_WARNING_OFF_MS;
        } else {
            return false;
        }
    }

    if (elapsed >= interval) {
        _flashOn = !_flashOn;
        _flashLastToggle = now;
        render();
        return true;
    }

    return false;
}

// ============================================================
//  render()
// ============================================================

void KBCLEDControl::render() {
    // Render NeoPixel buttons (KBC indices 0-11)
    for (uint8_t i = 0; i < KBC_NEO_COUNT; i++) {
        RGBColor color = _resolveColor(i, _flashOn);
        _setNeoPixel(i, color);
    }
    _pixels.show();

    // Render discrete LED buttons (KBC indices 12-15)
    for (uint8_t i = KBC_DISCRETE_INDEX_FIRST; i < KBC_BUTTON_COUNT; i++) {
        RGBColor color = _resolveColor(i, _flashOn);
        _setDiscrete(i, color);
    }
}

// ============================================================
//  getButtonState()
// ============================================================

uint8_t KBCLEDControl::getButtonState(uint8_t index) const {
    if (index >= KBC_BUTTON_COUNT) return KBC_LED_OFF;
    return _state[index];
}

// ============================================================
//  hasExtendedState()
// ============================================================

bool KBCLEDControl::hasExtendedState() const {
    for (uint8_t i = 0; i < KBC_BUTTON_COUNT; i++) {
        if (_state[i] >= KBC_LED_WARNING && _state[i] <= KBC_LED_PARTIAL_DEPLOY) {
            return true;
        }
    }
    return false;
}

// ============================================================
//  bulbTest()
// ============================================================

void KBCLEDControl::bulbTest(uint16_t durationMs) {
    // Set all NeoPixels to full white
    for (uint8_t i = 0; i < KBC_NEO_COUNT; i++) {
        _pixels.setPixelColor(i, 255, 255, 255);
    }
    _pixels.show();

    // Set all discrete LEDs on
    for (uint8_t i = 0; i < KBC_DISCRETE_COUNT; i++) {
        digitalWrite(_discretePins[i], HIGH);
    }

    delay(durationMs);

    // Restore previous state
    render();
}

// ============================================================
//  _resolveColor() — internal
//
//  Returns the RGB color to display for a button at the given
//  flash phase. Encapsulates all state-to-color mapping logic.
// ============================================================

RGBColor KBCLEDControl::_resolveColor(uint8_t index, bool flashOn) const {
    uint8_t state = _state[index];

    switch (state) {

        case KBC_LED_OFF:
            return KBC_OFF;

        case KBC_LED_ENABLED:
            // NeoPixel: scale white by brightness setting
            // Discrete: full ON (no dimming available)
            if (index < KBC_DISCRETE_INDEX_FIRST) {
                return KBC_scaleColor(KBC_WHITE_COOL, _brightness);
            } else {
                return KBC_WHITE_COOL;  // discrete: full on
            }

        case KBC_LED_ACTIVE:
            // Full brightness active color from per-button array
            if (_activeColors != nullptr) {
                return _activeColors[index];
            }
            return KBC_GREEN;  // safe fallback if no colors provided

        case KBC_LED_WARNING:
            // Flashing amber — honor flash phase
            return flashOn ? KBC_AMBER : KBC_OFF;

        case KBC_LED_ALERT:
            // Flashing red — honor flash phase
            return flashOn ? KBC_RED : KBC_OFF;

        case KBC_LED_ARMED:
            // Static cyan
            return KBC_STATE_ARMED;

        case KBC_LED_PARTIAL_DEPLOY:
            // Static amber
            return KBC_AMBER;

        default:
            // Reserved or unknown state — treat as off
            return KBC_OFF;
    }
}

// ============================================================
//  _setNeoPixel() — internal
// ============================================================

void KBCLEDControl::_setNeoPixel(uint8_t index, RGBColor color) {
    _pixels.setPixelColor(index, color.r, color.g, color.b);
}

// ============================================================
//  _setDiscrete() — internal
// ============================================================

void KBCLEDControl::_setDiscrete(uint8_t index, RGBColor color) {
    if (index < KBC_DISCRETE_INDEX_FIRST || index >= KBC_BUTTON_COUNT) return;

    uint8_t pinIndex = index - KBC_DISCRETE_INDEX_FIRST;
    bool on = (color.r > 0 || color.g > 0 || color.b > 0);
    digitalWrite(_discretePins[pinIndex], on ? HIGH : LOW);
}

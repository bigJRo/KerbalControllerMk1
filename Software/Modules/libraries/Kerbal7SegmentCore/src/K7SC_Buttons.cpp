/**
 * @file        K7SC_Buttons.cpp
 * @version     2.0.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Button and NeoPixel driver — reports edges to inputState.
 *              No state machine. Sketch owns all LED and button logic.
 *
 * @license     GNU General Public License v3.0
 */

#include <Arduino.h>
#include <tinyNeoPixel.h>
#include "K7SC_Buttons.h"
#include "K7SC_State.h"

// ── NeoPixel ──────────────────────────────────────────────────
static tinyNeoPixel _pixels(K7SC_NEO_COUNT, K7SC_PIN_NEOPIX,
                             K7SC_NEO_COLOR_ORDER);

// ── Button debounce state ─────────────────────────────────────
static uint8_t  _rawState   = 0;   // current debounced state
static uint8_t  _candidate  = 0;   // pending new state (not yet stable)
static uint32_t _debounceTime[K7SC_BUTTON_COUNT] = {0};

// Button pins in bitmask order
static const uint8_t _pins[K7SC_BUTTON_COUNT] = {
    K7SC_PIN_BTN01,
    K7SC_PIN_BTN02,
    K7SC_PIN_BTN03,
    K7SC_PIN_BTN_EN
};

// ── buttonsBegin() ────────────────────────────────────────────
void buttonsBegin() {
    for (uint8_t i = 0; i < K7SC_BUTTON_COUNT; i++) {
        pinMode(_pins[i], INPUT);
    }
    _rawState  = 0;
    _candidate = 0;
    _pixels.begin();
    _pixels.show();
}

// ── buttonsPoll() ─────────────────────────────────────────────
void buttonsPoll() {
    uint32_t now = millis();
    uint8_t  reading = 0;

    for (uint8_t i = 0; i < K7SC_BUTTON_COUNT; i++) {
        if (digitalRead(_pins[i])) reading |= (1 << i);
    }

    // Debounce — any bit that differs from current state starts a timer
    uint8_t changed = reading ^ _candidate;
    if (changed) {
        _candidate = reading;
        for (uint8_t i = 0; i < K7SC_BUTTON_COUNT; i++) {
            if (changed & (1 << i)) _debounceTime[i] = now;
        }
    }

    // Apply any bits that have been stable for the debounce window
    for (uint8_t i = 0; i < K7SC_BUTTON_COUNT; i++) {
        if ((now - _debounceTime[i]) >= K7SC_BTN_DEBOUNCE_MS) {
            uint8_t bit = (1 << i);
            uint8_t newBit = _candidate & bit;
            uint8_t oldBit = _rawState  & bit;

            if (newBit != oldBit) {
                // State changed — record edge
                if (newBit) {
                    inputState.buttonPressed  |= bit;
                } else {
                    inputState.buttonReleased |= bit;
                }
                inputState.buttonChanged |= bit;
                _rawState = (_rawState & ~bit) | newBit;
            }
        }
    }
}

// ── buttonSetPixel() ──────────────────────────────────────────
void buttonSetPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= K7SC_NEO_COUNT) return;
    _pixels.setPixelColor(index, r, g, b);
}

// ── buttonsShow() ─────────────────────────────────────────────
void buttonsShow() {
    _pixels.show();
}

// ── buttonsClearAll() ─────────────────────────────────────────
void buttonsClearAll() {
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++) {
        _pixels.setPixelColor(i, 0, 0, 0);
    }
    _pixels.show();
}

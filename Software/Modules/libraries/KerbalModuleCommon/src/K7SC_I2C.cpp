/**
 * @file        K7SC_I2C.cpp
 * @version     1.1.0
 * @date        2026-04-26
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler implementation for Kerbal7SegmentCore.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Wire.h>
#include <Arduino.h>
#include "K7SC_I2C.h"
#include "K7SC_Buttons.h"
#include "K7SC_Display.h"
#include "K7SC_Encoder.h"

// ============================================================
//  Module identity
// ============================================================

static uint8_t _typeId   = 0xFF;
static uint8_t _capFlags = 0;

// ============================================================
//  Internal state
// ============================================================

static bool _intAsserted   = false;
static bool _sleeping      = false;
static bool _bulbActive    = false;
static uint32_t _bulbEnd   = 0;
static const uint16_t BULB_DURATION_MS = 2000;

// Atomic event snapshot — captured together in k7scI2CSyncINT() when
// INT is asserted, consumed atomically in _sendDataPacket(). Prevents
// buttonsGetEvents() and buttonsGetChangeMask() being called in separate
// Wire ISR callbacks where a button event between the two calls could
// produce inconsistent buf[0]/buf[1] values in the data packet.
static uint8_t _snapshotEvents = 0;
static uint8_t _snapshotChange = 0;
static bool    _snapshotValid  = false;

static const uint8_t _CMD_BUF_SIZE = 3;  // cmd + 2 payload bytes max
static uint8_t _cmdBuf[_CMD_BUF_SIZE];
static uint8_t _cmdLen = 0;

enum ResponseType : uint8_t {
    RESP_NONE     = 0,
    RESP_DATA     = 1,
    RESP_IDENTITY = 2
};
static volatile ResponseType _pendingResponse = RESP_NONE;

// ============================================================
//  INT helpers
// ============================================================

static void _assertINT() {
    digitalWrite(K7SC_PIN_INT, LOW);
    _intAsserted = true;
}

static void _clearINT() {
    digitalWrite(K7SC_PIN_INT, HIGH);
    _intAsserted = false;
}

// ============================================================
//  Packet builders
// ============================================================

static void _sendDataPacket() {
    uint8_t buf[K7SC_PACKET_SIZE];
    uint16_t val = displayGetValue();

    // Read button state directly — no snapshot needed.
    // buttonsGetChangeMask() clears _intPending as a side effect,
    // which is correct: once we've read the state into the packet,
    // the pending flag should be cleared.
    buf[0] = buttonsGetEvents();
    buf[1] = buttonsGetChangeMask();  // also clears _intPending
    buf[2] = buttonsGetStateByte();
    buf[3] = 0;
    buf[4] = (uint8_t)(val >> 8);
    buf[5] = (uint8_t)(val & 0xFF);

    encoderClearChanged();
    _snapshotEvents = 0;
    _snapshotChange = 0;
    _snapshotValid  = false;
    Wire.write(buf, K7SC_PACKET_SIZE);
    _clearINT();
}

static void _sendIdentityPacket() {
    uint8_t buf[K7SC_IDENTITY_SIZE];
    buf[0] = _typeId;
    buf[1] = K7SC_FIRMWARE_MAJOR;
    buf[2] = K7SC_FIRMWARE_MINOR;
    buf[3] = _capFlags;
    Wire.write(buf, K7SC_IDENTITY_SIZE);
}

// ============================================================
//  Command dispatch
// ============================================================

static void _dispatch() {
    if (_cmdLen == 0) return;
    uint8_t cmd = _cmdBuf[0];

    switch (cmd) {
        case K7SC_CMD_GET_IDENTITY:
            _pendingResponse = RESP_IDENTITY;
            break;

        case K7SC_CMD_SET_LED_STATE:
            // This module manages its own LED states — ignore
            break;

        case K7SC_CMD_SET_BRIGHTNESS:
            // Top nibble (0-15) maps to MAX7219 display intensity 0-15
            // Full byte (0-255) maps to NeoPixel brightness 0-255
            if (_cmdLen >= 2) {
                uint8_t brightness = _cmdBuf[1];
                displaySetIntensity(brightness >> 4);  // 0-255 → 0-15
                buttonsSetBrightness(brightness);       // 0-255 directly
            }
            break;

        case K7SC_CMD_BULB_TEST:
            buttonsBulbTest();
            displayTest();
            _bulbActive = true;
            _bulbEnd    = millis() + BULB_DURATION_MS;
            break;

        case K7SC_CMD_SLEEP:
            _sleeping = true;
            buttonsClearLEDs();
            displayShutdown();
            _clearINT();
            break;

        case K7SC_CMD_WAKE:
            _sleeping = false;
            buttonsRestoreLEDs();
            displayWake();
            break;

        case K7SC_CMD_RESET:
            buttonsClearAll();
            encoderSetValue(0);
            _sleeping       = false;
            _snapshotEvents = 0;
            _snapshotChange = 0;
            _snapshotValid  = false;
            _pendingResponse = RESP_NONE;
            _clearINT();
            break;

        case K7SC_CMD_ACK_FAULT:
            // No fault tracking — acknowledge silently
            break;

        case K7SC_CMD_ENABLE:
            _sleeping = false;
            buttonsRestoreLEDs();
            displayWake();
            break;

        case K7SC_CMD_DISABLE:
            _sleeping = true;
            buttonsClearLEDs();
            displayShutdown();
            _clearINT();
            break;

        case K7SC_CMD_SET_VALUE:
            if (_cmdLen >= 3) {
                uint16_t val = ((uint16_t)_cmdBuf[1] << 8) | _cmdBuf[2];
                encoderSetValue(val);
            }
            break;

        default:
            break;
    }

    // For SET_VALUE/RESET: assert INT immediately so tester doesn't have
    // to wait for the next k7scUpdate() loop iteration.
    // Skip GET_IDENTITY — must not overwrite RESP_IDENTITY with RESP_DATA.
    if (!_sleeping && cmd != K7SC_CMD_GET_IDENTITY) {
        if (encoderIsValueChanged()) {
            _pendingResponse = RESP_DATA;
            _assertINT();
        }
    }
}

// ============================================================
//  Wire callbacks
// ============================================================

static void _onReceive(int numBytes) {
    _cmdLen = 0;
    while (Wire.available() && _cmdLen < _CMD_BUF_SIZE) {
        _cmdBuf[_cmdLen++] = Wire.read();
    }
    while (Wire.available()) Wire.read();
    _dispatch();
}

static void _onRequest() {
    switch (_pendingResponse) {
        case RESP_DATA:
            _sendDataPacket();
            break;
        case RESP_IDENTITY:
            _sendIdentityPacket();
            break;
        default:
            for (uint8_t i = 0; i < K7SC_PACKET_SIZE; i++) {
                Wire.write((uint8_t)0);
            }
            break;
    }
    _pendingResponse = RESP_NONE;
}

// ============================================================
//  k7scI2CBegin()
// ============================================================

void k7scI2CBegin(uint8_t i2cAddress, uint8_t typeId, uint8_t capFlags) {
    _typeId   = typeId;
    _capFlags = capFlags;

    Wire.begin(i2cAddress);
    Wire.onReceive(_onReceive);
    Wire.onRequest(_onRequest);

    pinMode(K7SC_PIN_INT, OUTPUT);
    _clearINT();
}

// ============================================================
//  k7scI2CSyncINT()
// ============================================================

void k7scI2CSyncINT() {
    if (_sleeping) {
        if (_intAsserted) _clearINT();
        return;
    }

    bool btnPending = buttonsIsIntPending();
    bool encPending = encoderIsValueChanged();

    if (btnPending || encPending) {
        _pendingResponse = RESP_DATA;
        if (!_intAsserted) _assertINT();
    } else {
        if (_intAsserted) _clearINT();
    }
}

// ============================================================
//  k7scBulbTestPoll()
// ============================================================

void k7scBulbTestPoll() {
    if (_bulbActive && millis() >= _bulbEnd) {
        _bulbActive = false;
        buttonsBulbTestEnd();
        displayTestEnd();
    }
}

// ============================================================
//  k7scI2CIsSleeping()
// ============================================================

bool k7scI2CIsSleeping() {
    return _sleeping;
}

// ============================================================
//  k7scGetPendingEvents()
// ============================================================

uint8_t k7scGetPendingEvents() {
    // Returns pending button events for sketch-level handling.
    // buttonsGetEvents() is read-only — does not clear _eventMask.
    return buttonsIsIntPending() ? buttonsGetEvents() : 0;
}

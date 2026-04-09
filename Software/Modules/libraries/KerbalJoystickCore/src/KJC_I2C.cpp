/**
 * @file        KJC_I2C.cpp
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Implementation of I2C target handler for
 *              KerbalJoystickCore modules.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Wire.h>
#include <Arduino.h>
#include "KJC_I2C.h"
#include "KJC_ADC.h"
#include "KJC_Buttons.h"

// ============================================================
//  Module identity (set by kjcI2CBegin)
// ============================================================

static uint8_t _typeId   = 0xFF;
static uint8_t _capFlags = 0;

// ============================================================
//  Internal state
// ============================================================

static bool     _intAsserted    = false;
static bool     _renderPending  = false;
static bool     _sleeping       = false;
static uint32_t _lastINTTime    = 0;

static const uint8_t _CMD_BUF_SIZE = 1 + KJC_LED_PAYLOAD_SIZE;
static uint8_t  _cmdBuf[_CMD_BUF_SIZE];
static uint8_t  _cmdLen = 0;

enum ResponseType : uint8_t {
    RESP_NONE     = 0,
    RESP_DATA     = 1,
    RESP_IDENTITY = 2
};
static volatile ResponseType _pendingResponse = RESP_NONE;

// ============================================================
//  INT pin helpers
// ============================================================

static void _assertINT() {
    digitalWrite(KJC_PIN_INT, LOW);
    _intAsserted  = true;
    _lastINTTime  = millis();
}

static void _clearINT() {
    digitalWrite(KJC_PIN_INT, HIGH);
    _intAsserted = false;
}

// ============================================================
//  Packet build and send
// ============================================================

static void _sendDataPacket() {
    uint8_t buf[KJC_PACKET_SIZE];

    // Bytes 0-1: button state and change mask
    buf[0] = buttonsGetState();
    buf[1] = buttonsGetChangeMask();

    // Bytes 2-7: axis data (int16, big-endian)
    for (uint8_t i = 0; i < KJC_AXIS_COUNT; i++) {
        int16_t val = adcGetScaled(i);
        buf[2 + (i * 2)]     = (uint8_t)(val >> 8);    // high byte
        buf[2 + (i * 2) + 1] = (uint8_t)(val & 0xFF);  // low byte
    }

    adcClearPending();
    Wire.write(buf, KJC_PACKET_SIZE);
    _clearINT();
}

static void _sendIdentityPacket() {
    uint8_t buf[KJC_IDENTITY_SIZE];
    buf[0] = _typeId;
    buf[1] = KJC_FIRMWARE_MAJOR;
    buf[2] = KJC_FIRMWARE_MINOR;
    buf[3] = _capFlags;
    Wire.write(buf, KJC_IDENTITY_SIZE);
}

// ============================================================
//  Command dispatch
// ============================================================

static void _dispatch() {
    if (_cmdLen == 0) return;
    uint8_t cmd = _cmdBuf[0];

    switch (cmd) {

        case KJC_CMD_GET_IDENTITY:
            _pendingResponse = RESP_IDENTITY;
            break;

        case KJC_CMD_SET_LED_STATE:
            if (_cmdLen >= 1 + KJC_LED_PAYLOAD_SIZE) {
                buttonsSetLEDs(&_cmdBuf[1]);
                _renderPending = true;
            }
            break;

        case KJC_CMD_SET_BRIGHTNESS:
            if (_cmdLen >= 2) {
                buttonsSetBrightness(_cmdBuf[1]);
                _renderPending = true;
            }
            break;

        case KJC_CMD_BULB_TEST:
            buttonsBulbTest(2000);
            break;

        case KJC_CMD_SLEEP:
            _sleeping = true;
            buttonsClearLEDs();
            _renderPending = true;
            _clearINT();
            break;

        case KJC_CMD_WAKE:
            _sleeping = false;
            buttonsSetLED(0, KJC_LED_ENABLED);
            buttonsSetLED(1, KJC_LED_ENABLED);
            _renderPending = true;
            break;

        case KJC_CMD_RESET:
            buttonsClearLEDs();
            buttonsClearAll();
            adcClearAll();
            _renderPending   = false;
            _sleeping        = false;
            _pendingResponse = RESP_NONE;
            _clearINT();
            break;

        case KJC_CMD_ACK_FAULT:
            // No fault tracking — acknowledge silently
            break;

        case KJC_CMD_ENABLE:
            _sleeping = false;
            buttonsSetLED(0, KJC_LED_ENABLED);
            buttonsSetLED(1, KJC_LED_ENABLED);
            _renderPending = true;
            break;

        case KJC_CMD_DISABLE:
            _sleeping = true;
            buttonsSetLED(0, KJC_LED_OFF);
            buttonsSetLED(1, KJC_LED_OFF);
            _renderPending = true;
            _clearINT();
            break;

        default:
            break;
    }
}

// ============================================================
//  Wire callback trampolines
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
            for (uint8_t i = 0; i < KJC_PACKET_SIZE; i++) {
                Wire.write((uint8_t)0);
            }
            break;
    }
    _pendingResponse = RESP_NONE;
}

// ============================================================
//  kjcI2CBegin()
// ============================================================

void kjcI2CBegin(uint8_t typeId, uint8_t capFlags) {
    _typeId   = typeId;
    _capFlags = capFlags;

    pinMode(KJC_PIN_INT, OUTPUT);
    _clearINT();

    Wire.onReceive(_onReceive);
    Wire.onRequest(_onRequest);
}

// ============================================================
//  kjcI2CSyncINT()
//
//  Option E hybrid INT strategy:
//    - Button changes: always immediate, no quiet period
//    - Axis changes: only if quiet period has elapsed
// ============================================================

void kjcI2CSyncINT() {
    if (_sleeping) {
        if (_intAsserted) _clearINT();
        return;
    }

    bool btnPending  = buttonsIsIntPending();
    bool axisPending = adcIsDataPending();

    if (btnPending) {
        // Buttons always get immediate INT — bypass quiet period
        _pendingResponse = RESP_DATA;
        if (!_intAsserted) _assertINT();
        return;
    }

    if (axisPending) {
        // Axis data respects quiet period
        uint32_t now = millis();
        if ((now - _lastINTTime) >= KJC_QUIET_PERIOD_MS) {
            _pendingResponse = RESP_DATA;
            if (!_intAsserted) _assertINT();
        }
        return;
    }

    // Nothing pending
    if (_intAsserted) _clearINT();
}

// ============================================================
//  Accessors
// ============================================================

bool kjcI2CIsRenderPending() {
    return _renderPending;
}

void kjcI2CClearRenderPending() {
    _renderPending = false;
}

bool kjcI2CIsSleeping() {
    return _sleeping;
}

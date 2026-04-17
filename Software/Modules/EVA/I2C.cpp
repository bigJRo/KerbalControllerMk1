/**
 * @file        I2C.cpp
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — EVA Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Implementation of I2C target handler for the EVA module.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Wire.h>
#include "I2C.h"
#include "Buttons.h"
#include "Encoders.h"
#include "LEDs.h"

// ============================================================
//  Internal state
// ============================================================

static bool     _intAsserted    = false;
static bool     _renderPending  = false;
static bool     _sleeping       = false;

static const uint8_t _CMD_BUF_SIZE = 1 + EVA_LED_PAYLOAD_SIZE;
static uint8_t  _cmdBuf[_CMD_BUF_SIZE];
static uint8_t  _cmdLen         = 0;

enum ResponseType : uint8_t {
    RESP_NONE     = 0,
    RESP_BUTTONS  = 1,
    RESP_IDENTITY = 2
};
static volatile ResponseType _pendingResponse = RESP_NONE;

// ============================================================
//  INT pin helpers
// ============================================================

static void _assertINT() {
    digitalWrite(EVA_INT_PIN, LOW);
    _intAsserted = true;
}

static void _clearINT() {
    digitalWrite(EVA_INT_PIN, HIGH);
    _intAsserted = false;
}

// ============================================================
//  Command dispatch
// ============================================================

static void _dispatch() {
    if (_cmdLen == 0) return;
    uint8_t cmd = _cmdBuf[0];

    switch (cmd) {

        case CMD_GET_IDENTITY:
            _pendingResponse = RESP_IDENTITY;
            break;

        case CMD_SET_LED_STATE:
            if (_cmdLen >= 1 + EVA_LED_PAYLOAD_SIZE) {
                ledsSetAll(&_cmdBuf[1]);
                _renderPending = true;
            }
            break;

        case CMD_SET_BRIGHTNESS:
            if (_cmdLen >= 2) {
                ledsSetBrightness(_cmdBuf[1]);
                _renderPending = true;
            }
            break;

        case CMD_BULB_TEST:
            ledsBulbTest(2000);
            break;

        case CMD_SLEEP:
            _sleeping = true;
            ledsClearAll();
            _renderPending = true;
            _clearINT();
            break;

        case CMD_WAKE:
            _sleeping = false;
            for (uint8_t i = 0; i < EVA_BUTTON_COUNT; i++) {
                ledSetState(i, LED_ENABLED);
            }
            _renderPending = true;
            break;

        case CMD_RESET:
            ledsClearAll();
            _renderPending  = true;
            _sleeping       = false;
            _pendingResponse = RESP_NONE;
            buttonsClearAll();
            encodersClearDeltas();
            _clearINT();
            break;

        case CMD_ACK_FAULT:
            // No fault tracking on this module — acknowledge silently
            break;

        case CMD_ENABLE:
            _sleeping = false;
            for (uint8_t i = 0; i < EVA_BUTTON_COUNT; i++) {
                ledSetState(i, LED_ENABLED);
            }
            _renderPending = true;
            break;

        case CMD_DISABLE:
            _sleeping = true;
            for (uint8_t i = 0; i < EVA_BUTTON_COUNT; i++) {
                ledSetState(i, LED_OFF);
            }
            _renderPending = true;
            _clearINT();
            break;

        default:
            break;
    }
}

// ============================================================
//  Send button packet
// ============================================================

static void _sendButtonPacket() {
    uint8_t buf[EVA_PACKET_SIZE];
    buttonsGetPacket(buf);

    // Inject encoder deltas into bytes 2 and 3
    buf[2] = (uint8_t)encodersGetDelta1();
    buf[3] = (uint8_t)encodersGetDelta2();
    encodersClearDeltas();

    Wire.write(buf, EVA_PACKET_SIZE);
    _clearINT();
}

// ============================================================
//  Send identity packet
// ============================================================

static void _sendIdentityPacket() {
    uint8_t buf[EVA_IDENTITY_SIZE];
    buf[0] = EVA_MODULE_TYPE_ID;
    buf[1] = EVA_FIRMWARE_MAJOR;
    buf[2] = EVA_FIRMWARE_MINOR;
    // Report 0x00 capability flags while encoders are not yet implemented.
    // EVA_CAP_ENCODERS (bit 2) must be restored here once encoder ISRs
    // are active and bytes 2-3 of the data packet carry live delta data.
    buf[3] = 0x00;
    Wire.write(buf, EVA_IDENTITY_SIZE);
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
        case RESP_BUTTONS:
            _sendButtonPacket();
            break;
        case RESP_IDENTITY:
            _sendIdentityPacket();
            break;
        default:
            // Unexpected read — send zeros
            for (uint8_t i = 0; i < EVA_PACKET_SIZE; i++) {
                Wire.write((uint8_t)0);
            }
            break;
    }
    _pendingResponse = RESP_NONE;
}

// ============================================================
//  Public interface
// ============================================================

void i2cBegin() {
    pinMode(EVA_INT_PIN, OUTPUT);
    _clearINT();
    Wire.onReceive(_onReceive);
    Wire.onRequest(_onRequest);
}

void i2cSyncINT() {
    if (_sleeping) {
        if (_intAsserted) _clearINT();
        return;
    }
    if (buttonsIsIntPending()) {
        // Queue button response for next onRequest
        _pendingResponse = RESP_BUTTONS;
        if (!_intAsserted) _assertINT();
    } else {
        if (_intAsserted) _clearINT();
    }
}

bool i2cIsRenderPending() {
    return _renderPending;
}

void i2cClearRenderPending() {
    _renderPending = false;
}

bool i2cIsSleeping() {
    return _sleeping;
}

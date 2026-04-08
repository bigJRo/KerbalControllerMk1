/**
 * @file        K7SC_I2C.cpp
 * @version     1.0.0
 * @date        2026-04-08
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

    buf[0] = buttonsGetEvents();
    buf[1] = buttonsGetChangeMask();
    buf[2] = buttonsGetStateByte();
    buf[3] = 0;                           // reserved
    buf[4] = (uint8_t)(val >> 8);         // display value high byte
    buf[5] = (uint8_t)(val & 0xFF);       // display value low byte

    encoderClearChanged();
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
            // Not applicable — SK6812 brightness managed internally
            break;

        case K7SC_CMD_BULB_TEST:
            buttonsBulbTest(2000);
            displayTest(2000);
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
            _sleeping        = false;
            _pendingResponse = RESP_NONE;
            _clearINT();
            break;

        case K7SC_CMD_ACK_FAULT:
            // No fault tracking — acknowledge silently
            break;

        case K7SC_CMD_SET_VALUE:
            // Controller sets display value directly
            if (_cmdLen >= 3) {
                uint16_t val = ((uint16_t)_cmdBuf[1] << 8) | _cmdBuf[2];
                encoderSetValue(val);
            }
            break;

        default:
            break;
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

void k7scI2CBegin(uint8_t typeId, uint8_t capFlags) {
    _typeId   = typeId;
    _capFlags = capFlags;

    pinMode(K7SC_PIN_INT, OUTPUT);
    _clearINT();

    Wire.onReceive(_onReceive);
    Wire.onRequest(_onRequest);
}

// ============================================================
//  k7scI2CSyncINT()
// ============================================================

void k7scI2CSyncINT() {
    if (_sleeping) {
        if (_intAsserted) _clearINT();
        return;
    }

    bool pending = buttonsIsIntPending() || encoderIsValueChanged();

    if (pending) {
        _pendingResponse = RESP_DATA;
        if (!_intAsserted) _assertINT();
    } else {
        if (_intAsserted) _clearINT();
    }
}

// ============================================================
//  k7scI2CIsSleeping()
// ============================================================

bool k7scI2CIsSleeping() {
    return _sleeping;
}

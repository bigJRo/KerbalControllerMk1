/**
 * @file        I2C.cpp
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Switch Panel Input Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler implementation for the Switch Panel.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Wire.h>
#include <Arduino.h>
#include "I2C.h"
#include "Switches.h"

// ============================================================
//  State
// ============================================================

static bool _intAsserted = false;
static bool _sleeping    = false;

static const uint8_t _CMD_BUF_SIZE = 2;
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
    digitalWrite(SWP_PIN_INT, LOW);
    _intAsserted = true;
}

static void _clearINT() {
    digitalWrite(SWP_PIN_INT, HIGH);
    _intAsserted = false;
}

// ============================================================
//  Packet builders
// ============================================================

static void _sendDataPacket() {
    uint8_t buf[SWP_PACKET_SIZE];
    switchesGetPacket(buf);
    Wire.write(buf, SWP_PACKET_SIZE);
    _clearINT();
}

static void _sendIdentityPacket() {
    uint8_t buf[SWP_IDENTITY_SIZE];
    buf[0] = SWP_MODULE_TYPE_ID;
    buf[1] = SWP_FIRMWARE_MAJOR;
    buf[2] = SWP_FIRMWARE_MINOR;
    buf[3] = SWP_CAP_FLAGS;
    Wire.write(buf, SWP_IDENTITY_SIZE);
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
        case CMD_SET_BRIGHTNESS:
        case CMD_BULB_TEST:
            // No LEDs — accept and ignore
            break;

        case CMD_SLEEP:
        case CMD_DISABLE:
            _sleeping = true;
            _clearINT();
            break;

        case CMD_WAKE:
        case CMD_ENABLE:
            _sleeping = false;
            break;

        case CMD_RESET:
            switchesClearAll();
            _sleeping        = false;
            _pendingResponse = RESP_NONE;
            _clearINT();
            break;

        case CMD_ACK_FAULT:
            // No fault tracking — acknowledge silently
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
            for (uint8_t i = 0; i < SWP_PACKET_SIZE; i++) {
                Wire.write((uint8_t)0);
            }
            break;
    }
    _pendingResponse = RESP_NONE;
}

// ============================================================
//  i2cBegin()
// ============================================================

void i2cBegin() {
    pinMode(SWP_PIN_INT, OUTPUT);
    _clearINT();
    Wire.onReceive(_onReceive);
    Wire.onRequest(_onRequest);
}

// ============================================================
//  i2cSyncINT()
// ============================================================

void i2cSyncINT() {
    if (_sleeping) {
        if (_intAsserted) _clearINT();
        return;
    }

    if (switchesIsIntPending()) {
        _pendingResponse = RESP_DATA;
        if (!_intAsserted) _assertINT();
    } else {
        if (_intAsserted) _clearINT();
    }
}

// ============================================================
//  i2cIsSleeping()
// ============================================================

bool i2cIsSleeping() {
    return _sleeping;
}

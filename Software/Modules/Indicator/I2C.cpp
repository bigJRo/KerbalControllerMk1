/**
 * @file        I2C.cpp
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Indicator Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler implementation for the Indicator Module.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Wire.h>
#include <Arduino.h>
#include "I2C.h"
#include "LEDs.h"

// ============================================================
//  State
// ============================================================

static bool _sleeping = false;

static const uint8_t _CMD_BUF_MAX = 9;  // cmd + 8 payload bytes
static uint8_t _cmdBuf[_CMD_BUF_MAX];
static uint8_t _cmdLen = 0;

enum ResponseType : uint8_t {
    RESP_NONE     = 0,
    RESP_IDENTITY = 1
};
static volatile ResponseType _pendingResponse = RESP_NONE;

// ============================================================
//  INT helpers — reserved, always deasserted currently
// ============================================================

static void _clearINT() {
    digitalWrite(IND_PIN_INT, HIGH);
}

// ============================================================
//  Identity packet
// ============================================================

static void _sendIdentityPacket() {
    uint8_t buf[IND_IDENTITY_SIZE];
    buf[0] = IND_MODULE_TYPE_ID;
    buf[1] = IND_FIRMWARE_MAJOR;
    buf[2] = IND_FIRMWARE_MINOR;
    buf[3] = IND_CAP_FLAGS;
    Wire.write(buf, IND_IDENTITY_SIZE);
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
            // Primary runtime command — 8-byte nibble-packed payload
            if (_cmdLen >= 9 && !_sleeping) {
                ledsSetFromPayload(&_cmdBuf[1]);
            }
            break;

        case CMD_SET_BRIGHTNESS: {
            // Scale all active colors by brightness factor
            // Simple approach: store brightness and apply in render
            // For now accepted and ignored — future enhancement
            break;
        }

        case CMD_BULB_TEST:
            ledsBulbTest(IND_BULB_TEST_MS);
            break;

        case CMD_SLEEP:
        case CMD_DISABLE:
            _sleeping = true;
            ledsClearAll();
            break;

        case CMD_WAKE:
        case CMD_ENABLE:
            _sleeping = false;
            // Restore LEDs to ENABLED dim white on wake
            // Controller will send CMD_SET_LED_STATE to set actual state
            ledsSetEnabled();
            break;

        case CMD_RESET:
            _sleeping        = false;
            _pendingResponse = RESP_NONE;
            ledsClearAll();
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
    while (Wire.available() && _cmdLen < _CMD_BUF_MAX) {
        _cmdBuf[_cmdLen++] = Wire.read();
    }
    while (Wire.available()) Wire.read();
    _dispatch();
}

static void _onRequest() {
    if (_pendingResponse == RESP_IDENTITY) {
        _sendIdentityPacket();
    } else {
        // No data packet — send zeros
        for (uint8_t i = 0; i < IND_IDENTITY_SIZE; i++) {
            Wire.write((uint8_t)0);
        }
    }
    _pendingResponse = RESP_NONE;
}

// ============================================================
//  i2cBegin()
// ============================================================

void i2cBegin() {
    pinMode(IND_PIN_INT, OUTPUT);
    _clearINT();
    Wire.onReceive(_onReceive);
    Wire.onRequest(_onRequest);
}

// ============================================================
//  i2cSyncINT()
// ============================================================

void i2cSyncINT() {
    // No inputs currently — INT always deasserted.
    // When encoder headers are populated this will assert
    // on encoder movement and button presses.
    _clearINT();
}

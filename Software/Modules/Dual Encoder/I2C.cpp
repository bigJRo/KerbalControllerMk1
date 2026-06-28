/**
 * @file        I2C.cpp
 * @version     2.0
 * @date        2026-06-28
 * @project     Kerbal Controller Mk1 — Dual Encoder Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler implementation for the Dual Encoder Module.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Wire.h>
#include <Arduino.h>
#include "I2C.h"
#include "Encoders.h"
#include "Buttons.h"

// ============================================================
//  State
// ============================================================

static bool _intAsserted = false;

// Lifecycle (KMC_STATUS_*), transaction counter, and fault flag for
// I2C Protocol v2.4 conformance.
static uint8_t _lifecycle = KMC_STATUS_BOOT_READY;
static uint8_t _txCounter = 0;
static bool    _fault     = false;

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
    digitalWrite(DEC_PIN_INT, LOW);
    _intAsserted = true;
    // Transaction counter increments on every INT assertion (wraps 255→0).
    _txCounter++;
}

static void _clearINT() {
    digitalWrite(DEC_PIN_INT, HIGH);
    _intAsserted = false;
}

// ============================================================
//  Packet builders
// ============================================================

static void _sendDataPacket() {
    uint8_t buf[DEC_PACKET_SIZE];

    // Byte 0-2: universal 3-byte header
    uint8_t status = _lifecycle & KMC_STATUS_LIFECYCLE_MASK;
    if (_fault) status |= KMC_STATUS_FAULT;
    status |= KMC_STATUS_DATA_CHANGED;
    buf[0] = status;
    buf[1] = DEC_MODULE_TYPE_ID;
    buf[2] = _txCounter;

    // Byte 3-6: button events, change mask, encoder deltas
    buf[3] = buttonsGetEvents();
    buf[4] = buttonsGetChangeMask();
    buf[5] = (uint8_t)encodersGetDelta1();
    buf[6] = (uint8_t)encodersGetDelta2();
    encodersClearDeltas();

    Wire.write(buf, DEC_PACKET_SIZE);
    _clearINT();
}

static void _sendIdentityPacket() {
    uint8_t buf[DEC_IDENTITY_SIZE];
    buf[0] = DEC_MODULE_TYPE_ID;
    buf[1] = DEC_FIRMWARE_MAJOR;
    buf[2] = DEC_FIRMWARE_MINOR;
    buf[3] = DEC_CAP_ENCODERS;
    Wire.write(buf, DEC_IDENTITY_SIZE);
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
            // No LEDs — accept and ignore
            break;

        case CMD_BULB_TEST:
            // No user-visible LEDs to test — acknowledge silently
            break;

        case CMD_SLEEP:
            // SLEEPING freezes state and suppresses INT until CMD_WAKE.
            _lifecycle = KMC_STATUS_SLEEPING;
            _clearINT();
            break;

        case CMD_DISABLE:
            // DISABLED — inputs suppressed. Completes BOOT_READY init.
            _lifecycle = KMC_STATUS_DISABLED;
            _clearINT();
            break;

        case CMD_WAKE:
            // Resume from SLEEPING to ACTIVE; send a fresh state packet.
            _lifecycle = KMC_STATUS_ACTIVE;
            _assertINT();
            break;

        case CMD_ENABLE:
            // Enter ACTIVE.
            _lifecycle = KMC_STATUS_ACTIVE;
            break;

        case CMD_RESET:
            // Reset to defaults, stay ACTIVE.
            buttonsClearAll();
            encodersReset();
            _lifecycle       = KMC_STATUS_ACTIVE;
            _pendingResponse = RESP_NONE;
            _clearINT();
            break;

        case CMD_ACK_FAULT:
            _fault = false;
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
    // Identity read takes priority and does not disturb a pending INT.
    // Every other read returns the current data packet.
    if (_pendingResponse == RESP_IDENTITY) {
        _sendIdentityPacket();
        _pendingResponse = RESP_NONE;
        return;
    }
    _sendDataPacket();
    _pendingResponse = RESP_NONE;
}

// ============================================================
//  i2cBegin()
// ============================================================

void i2cBegin() {
    pinMode(DEC_PIN_INT, OUTPUT);
    _clearINT();
    Wire.onReceive(_onReceive);
    Wire.onRequest(_onRequest);

    // Power-on lifecycle: BOOT_READY. Assert INT and hold until the
    // controller reads the boot packet and sends CMD_DISABLE (spec §3).
    _lifecycle       = KMC_STATUS_BOOT_READY;
    _pendingResponse = RESP_DATA;
    _assertINT();
}

// ============================================================
//  i2cSyncINT()
// ============================================================

void i2cSyncINT() {
    // Input-driven INT only applies in ACTIVE. In BOOT_READY the INT is
    // held from i2cBegin(); in SLEEPING/DISABLED inputs are suppressed.
    if (_lifecycle != KMC_STATUS_ACTIVE) {
        return;
    }

    bool pending = buttonsIsIntPending() || encodersIsDataPending();

    if (pending) {
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
    return _lifecycle == KMC_STATUS_SLEEPING;
}

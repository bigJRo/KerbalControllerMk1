/**
 * @file        I2C.cpp
 * @version     2.0
 * @date        2026-06-28
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

// Lifecycle (KMC_STATUS_*), transaction counter, and fault flag for
// I2C Protocol v2.4 conformance.
static uint8_t  _lifecycle      = KMC_STATUS_BOOT_READY;
static uint8_t  _txCounter      = 0;
static bool     _fault          = false;

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
    // Transaction counter increments on every INT assertion (wraps 255→0).
    _txCounter++;
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
            // SLEEPING freezes current state exactly as-is — no visual
            // change. INT suppressed until CMD_WAKE.
            _lifecycle = KMC_STATUS_SLEEPING;
            _clearINT();
            break;

        case CMD_WAKE:
            // Resume from SLEEPING to ACTIVE; send a fresh state packet.
            _lifecycle = KMC_STATUS_ACTIVE;
            _assertINT();
            break;

        case CMD_RESET:
            // Reset to defaults, stay ACTIVE. Buttons return to backlit.
            _lifecycle = KMC_STATUS_ACTIVE;
            for (uint8_t i = 0; i < EVA_BUTTON_COUNT; i++) {
                ledSetState(i, LED_ENABLED);
            }
            _renderPending   = true;
            _pendingResponse = RESP_NONE;
            buttonsClearAll();
            encodersClearDeltas();
            _clearINT();
            break;

        case CMD_ACK_FAULT:
            _fault = false;
            break;

        case CMD_ENABLE:
            // Enter ACTIVE and light all buttons to ENABLED.
            _lifecycle = KMC_STATUS_ACTIVE;
            for (uint8_t i = 0; i < EVA_BUTTON_COUNT; i++) {
                ledSetState(i, LED_ENABLED);
            }
            _renderPending = true;
            break;

        case CMD_DISABLE:
            // Enter DISABLED — outputs dark, inputs suppressed. Also
            // completes BOOT_READY init (clears the boot INT).
            _lifecycle = KMC_STATUS_DISABLED;
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
    // buttonsGetPacket() returns [state, change, 0, 0] and clears pending.
    uint8_t tmp[4];
    buttonsGetPacket(tmp);

    uint8_t buf[EVA_PACKET_SIZE];

    // Byte 0-2: universal 3-byte header
    uint8_t status = _lifecycle & KMC_STATUS_LIFECYCLE_MASK;
    if (_fault) status |= KMC_STATUS_FAULT;
    status |= KMC_STATUS_DATA_CHANGED;
    buf[0] = status;
    buf[1] = EVA_MODULE_TYPE_ID;
    buf[2] = _txCounter;

    // Byte 3-6: standard button payload (events HI/LO, change HI/LO).
    // Only buttons 0-5 exist, so the LO bytes are always 0. Encoder delta
    // bytes are dropped — encoder hardware is unpopulated.
    buf[3] = tmp[0];   // events HI — current state, buttons 0-5
    buf[4] = 0x00;     // events LO — buttons 8-15 unused
    buf[5] = tmp[1];   // change HI — buttons 0-5
    buf[6] = 0x00;     // change LO — unused

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
    // Identity read takes priority and does not disturb a pending INT.
    // Every other read returns the current data packet.
    if (_pendingResponse == RESP_IDENTITY) {
        _sendIdentityPacket();
        _pendingResponse = RESP_NONE;
        return;
    }
    _sendButtonPacket();
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

    // Power-on lifecycle: BOOT_READY. Assert INT and hold until the
    // controller reads the boot packet and sends CMD_DISABLE (spec §3).
    _lifecycle       = KMC_STATUS_BOOT_READY;
    _pendingResponse = RESP_BUTTONS;
    _assertINT();
}

void i2cSyncINT() {
    // Input-driven INT only applies in ACTIVE. In BOOT_READY the INT is
    // held from i2cBegin(); in SLEEPING/DISABLED inputs are suppressed.
    if (_lifecycle != KMC_STATUS_ACTIVE) {
        return;
    }
    if (buttonsIsIntPending()) {
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
    return _lifecycle == KMC_STATUS_SLEEPING;
}

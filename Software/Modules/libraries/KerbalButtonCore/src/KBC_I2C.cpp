/**
 * @file        KBC_I2C.cpp
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Implementation of the KBCi2C I2C target handler
 *              for the KerbalButtonCore library.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the KerbalButtonCore (KBC) library.
 *              Requires: Wire library (megaTinyCore)
 *              Hardware: KC-01-1822 v1.1
 *              Protocol: KBC_Protocol_Spec.md v1.1
 */

#include "KBC_I2C.h"

// ============================================================
//  Global instance pointer for Wire callback trampolines
// ============================================================

KBCi2C* KBCi2C::_instance = nullptr;

// ============================================================
//  Constructor
// ============================================================

KBCi2C::KBCi2C(KBCShiftReg&   shiftReg,
               KBCLEDControl& ledControl,
               uint8_t        moduleTypeId,
               uint8_t        capFlags)
    : _shiftReg(shiftReg)
    , _ledControl(ledControl)
    , _moduleTypeId(moduleTypeId)
    , _capFlags(capFlags)
    , _intAsserted(false)
    , _cmdLen(0)
    , _sleeping(false)
    , _fault(false)
    , _renderPending(false)
    , _pendingResponse(RESPONSE_NONE)
{
    memset(_cmdBuf, 0, sizeof(_cmdBuf));
}

// ============================================================
//  begin()
// ============================================================

void KBCi2C::begin() {
    // Store global instance pointer for trampoline callbacks
    _instance = this;

    // Configure INT pin as output, deasserted (high)
    pinMode(KBC_PIN_INT, OUTPUT);
    _clearINT();

    // Register Wire callbacks via static trampolines.
    // Wire.begin(address) must have been called in the sketch
    // before this method is called.
    Wire.onReceive(_onReceiveTrampoline);
    Wire.onRequest(_onRequestTrampoline);
}

// ============================================================
//  syncINT()
// ============================================================

void KBCi2C::syncINT() {
    if (_sleeping) {
        // Do not assert INT while sleeping
        if (_intAsserted) _clearINT();
        return;
    }

    if (_shiftReg.isIntPending()) {
        if (!_intAsserted) _assertINT();
    } else {
        if (_intAsserted) _clearINT();
    }
}

// ============================================================
//  State queries
// ============================================================

bool KBCi2C::isSleeping() const {
    return _sleeping;
}

bool KBCi2C::hasFault() const {
    return _fault;
}

void KBCi2C::setFault() {
    _fault = true;
    // Set fault bit in capability flags so controller sees it
    // on next CMD_GET_IDENTITY
    _capFlags |= KBC_CAP_FAULT;
}

bool KBCi2C::isRenderPending() const {
    return _renderPending;
}

void KBCi2C::clearRenderPending() {
    _renderPending = false;
}

// ============================================================
//  INT pin helpers
// ============================================================

void KBCi2C::_assertINT() {
    digitalWrite(KBC_PIN_INT, LOW);
    _intAsserted = true;
}

void KBCi2C::_clearINT() {
    digitalWrite(KBC_PIN_INT, HIGH);
    _intAsserted = false;
}

// ============================================================
//  Static trampoline — onReceive
//
//  Called by Wire ISR when the controller sends data.
//  Reads bytes into _cmdBuf then calls _dispatch().
// ============================================================

void KBCi2C::_onReceiveTrampoline(int numBytes) {
    if (_instance == nullptr) return;

    // Read all available bytes into the command buffer
    _instance->_cmdLen = 0;
    while (Wire.available() && _instance->_cmdLen < _CMD_BUF_SIZE) {
        _instance->_cmdBuf[_instance->_cmdLen++] = Wire.read();
    }
    // Drain any unexpected overflow bytes
    while (Wire.available()) Wire.read();

    _instance->_dispatch();
}

// ============================================================
//  Static trampoline — onRequest
//
//  Called by Wire ISR when the controller requests data.
//  Routes to the appropriate send method based on pending
//  response type.
// ============================================================

void KBCi2C::_onRequestTrampoline() {
    if (_instance == nullptr) return;

    switch (_instance->_pendingResponse) {
        case RESPONSE_BUTTONS:
            _instance->_sendButtonPacket();
            break;
        case RESPONSE_IDENTITY:
            _instance->_sendIdentityPacket();
            break;
        default:
            // Unexpected read — send zeros to satisfy the bus
            for (uint8_t i = 0; i < KBC_BUTTON_PACKET_SIZE; i++) {
                Wire.write((uint8_t)0);
            }
            break;
    }

    _instance->_pendingResponse = RESPONSE_NONE;
}

// ============================================================
//  _dispatch()
// ============================================================

void KBCi2C::_dispatch() {
    if (_cmdLen == 0) return;

    uint8_t cmd = _cmdBuf[0];

    switch (cmd) {
        case KBC_CMD_GET_IDENTITY:
            _handleGetIdentity();
            break;
        case KBC_CMD_SET_LED_STATE:
            _handleSetLEDState();
            break;
        case KBC_CMD_SET_BRIGHTNESS:
            _handleSetBrightness();
            break;
        case KBC_CMD_BULB_TEST:
            _handleBulbTest();
            break;
        case KBC_CMD_SLEEP:
            _handleSleep();
            break;
        case KBC_CMD_WAKE:
            _handleWake();
            break;
        case KBC_CMD_RESET:
            _handleReset();
            break;
        case KBC_CMD_ACK_FAULT:
            _handleAckFault();
            break;
        default:
            // Unknown command — ignore silently
            break;
    }
}

// ============================================================
//  Command handlers
// ============================================================

void KBCi2C::_handleGetIdentity() {
    // Queue identity response for the next onRequest callback
    _pendingResponse = RESPONSE_IDENTITY;
}

void KBCi2C::_handleSetLEDState() {
    // Payload must be exactly KBC_LED_PAYLOAD_SIZE bytes after
    // the command byte
    if (_cmdLen < 1 + KBC_LED_PAYLOAD_SIZE) return;

    _ledControl.setLEDState(&_cmdBuf[1]);
    _renderPending = true;
}

void KBCi2C::_handleSetBrightness() {
    // Payload: 1 byte brightness value
    if (_cmdLen < 2) return;

    _ledControl.setBrightness(_cmdBuf[1]);
    _renderPending = true;
}

void KBCi2C::_handleBulbTest() {
    // Payload interpretation per protocol spec:
    //   No payload or 0x01 : start bulb test, block for KBC_BULB_TEST_MS
    //   0x00               : stop bulb test / no-op (restore previous state)
    //
    // The blocking call is acceptable here — CMD_BULB_TEST is a
    // maintenance command used at startup or during servicing, never
    // during normal operation. The main loop render pending flag is not
    // set since bulbTest() calls render() internally and restores state.

    if (_cmdLen >= 2 && _cmdBuf[1] == 0x00) {
        // Stop payload — restore previous state without running the test.
        // Since we don't track whether a bulb test is in progress
        // (the blocking implementation makes that impossible), treat
        // 0x00 as a safe restore: render the current LED state.
        _renderPending = true;
        return;
    }

    // No payload or 0x01 — run the bulb test
    _ledControl.bulbTest(KBC_BULB_TEST_MS);
}

void KBCi2C::_handleSleep() {
    _sleeping = true;
    _ledControl.clearAll();
    _renderPending = true;
    _clearINT();
}

void KBCi2C::_handleWake() {
    _sleeping = false;
    // Restore all buttons to ENABLED state as a safe default.
    // The controller should follow CMD_WAKE with CMD_SET_LED_STATE
    // to restore the correct operating state.
    for (uint8_t i = 0; i < KBC_BUTTON_COUNT; i++) {
        _ledControl.setButtonState(i, KBC_LED_ENABLED);
    }
    _renderPending = true;
}

void KBCi2C::_handleReset() {
    // Clear all LEDs
    _ledControl.clearAll();
    _renderPending = true;

    // Clear sleeping state
    _sleeping = false;

    // Deassert INT and clear pending response
    _clearINT();
    _pendingResponse = RESPONSE_NONE;

    // Note: button state in KBCShiftReg is not cleared here since
    // it reflects actual hardware state. The change mask is cleared
    // via getButtonPacket() on the next read. The controller should
    // send CMD_SET_LED_STATE after CMD_RESET to establish state.
}

void KBCi2C::_handleAckFault() {
    _fault = false;
    // Clear fault bit from capability flags
    _capFlags &= ~KBC_CAP_FAULT;
}

// ============================================================
//  _sendButtonPacket()
// ============================================================

void KBCi2C::_sendButtonPacket() {
    // Get the latched button state packet from shift register
    KBCButtonPacket pkt;
    _shiftReg.getButtonPacket(pkt);

    // Serialize to wire format
    uint8_t buf[KBC_BUTTON_PACKET_SIZE];
    KBC_serializeButtonPacket(&pkt, buf);

    // Send over I2C
    Wire.write(buf, KBC_BUTTON_PACKET_SIZE);

    // INT clears on read completion — syncINT() in the main loop
    // will re-assert if live state has already diverged
    _clearINT();
}

// ============================================================
//  _sendIdentityPacket()
// ============================================================

void KBCi2C::_sendIdentityPacket() {
    KBCIdentityPacket pkt;
    pkt.typeId          = _moduleTypeId;
    pkt.firmwareMajor   = KBC_FIRMWARE_MAJOR;
    pkt.firmwareMinor   = KBC_FIRMWARE_MINOR;
    pkt.capabilityFlags = _capFlags;

    uint8_t buf[KBC_IDENTITY_PACKET_SIZE];
    KBC_serializeIdentityPacket(&pkt, buf);

    Wire.write(buf, KBC_IDENTITY_PACKET_SIZE);
}

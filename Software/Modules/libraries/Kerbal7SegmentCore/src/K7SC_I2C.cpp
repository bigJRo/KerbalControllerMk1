/**
 * @file        K7SC_I2C.cpp
 * @version     2.0.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler implementation.
 *
 * @license     GNU General Public License v3.0
 */

#include <Arduino.h>
#include <Wire.h>
#include "K7SC_I2C.h"
#include "K7SC_Display.h"

// ── Global state structs ──────────────────────────────────────
K7SCCommandState cmdState  = {K7SC_BOOT_READY, false, false, false, 0, false, 0};
K7SCInputState   inputState = {0, 0, 0, 0, false};

// ── Module identity ───────────────────────────────────────────
static uint8_t _typeId   = 0xFF;
static uint8_t _capFlags = 0;

// ── Packet state ──────────────────────────────────────────────
static bool    _fault       = false;
static uint8_t _txCounter   = 0;

// Pending outgoing packet (built by sketch, header prepended on send)
static uint8_t  _payload[16];   // max payload bytes
static uint8_t  _payloadLen = 0;
static bool     _packetPending = false;

// Pending identity response
static bool _identityPending = false;

// One-shot flag clear tracking (cleared one loop after being set)
static bool _clearReset    = false;
static bool _clearNewValue = false;
static bool _clearLEDState = false;

// ── INT pin helpers ───────────────────────────────────────────
static bool _intAsserted = false;

static void _assertINT() {
    digitalWrite(K7SC_PIN_INT, LOW);
    _intAsserted = true;
    _txCounter++;
}

static void _clearINT() {
    digitalWrite(K7SC_PIN_INT, HIGH);
    _intAsserted = false;
}

// ── Identity packet ───────────────────────────────────────────
static void _sendIdentity() {
    Wire.write(_typeId);
    Wire.write(K7SC_FIRMWARE_MAJOR);
    Wire.write(K7SC_FIRMWARE_MINOR);
    Wire.write(_capFlags);
}

// ── Data packet ───────────────────────────────────────────────
static void _sendDataPacket() {
    // Build status byte
    uint8_t status = (uint8_t)cmdState.lifecycle & KMC_STATUS_LIFECYCLE_MASK;
    if (_fault) status |= KMC_STATUS_FAULT;
    status |= KMC_STATUS_DATA_CHANGED;  // sketch only calls this when data changed

    // Header
    Wire.write(status);
    Wire.write(_typeId);
    Wire.write(_txCounter);

    // Sketch payload
    Wire.write(_payload, _payloadLen);

    _packetPending = false;
    _clearINT();
}

// ── Wire callbacks ────────────────────────────────────────────
static void _onReceive(int numBytes) {
    if (numBytes == 0) return;

    uint8_t cmd = Wire.read();
    numBytes--;

    // Read remaining bytes into temp buffer
    uint8_t buf[4] = {0};
    uint8_t n = 0;
    while (Wire.available() && n < 4) buf[n++] = Wire.read();
    while (Wire.available()) Wire.read();

    switch (cmd) {

        case KMC_CMD_GET_IDENTITY:
            _identityPending = true;
            break;

        case KMC_CMD_SET_LED_STATE:
            // Pass raw first byte to sketch — sketch interprets
            if (n >= 1) {
                cmdState.hasLEDState = true;
                cmdState.ledState    = buf[0];
            }
            break;

        case KMC_CMD_SET_BRIGHTNESS:
            // Apply directly — pure hardware, no sketch logic needed
            if (n >= 1) displaySetIntensity(buf[0] >> 4);
            break;

        case KMC_CMD_BULB_TEST:
            // No payload or 0x01 = start, 0x00 = stop
            if (n == 0 || buf[0] == 0x01) cmdState.isBulbTest = true;
            else                           cmdState.isBulbTest = false;
            break;

        case KMC_CMD_SLEEP:
            cmdState.lifecycle = K7SC_SLEEPING;
            break;

        case KMC_CMD_WAKE:
            cmdState.lifecycle = K7SC_ACTIVE;
            break;

        case KMC_CMD_RESET:
            cmdState.isReset = true;
            break;

        case KMC_CMD_ACK_FAULT:
            _fault = false;
            break;

        case KMC_CMD_ENABLE:
            cmdState.lifecycle = K7SC_ACTIVE;
            break;

        case KMC_CMD_DISABLE:
            cmdState.lifecycle = K7SC_DISABLED;
            break;

        case KMC_CMD_SET_VALUE:
            if (n >= 2) {
                cmdState.hasNewValue = true;
                cmdState.newValue    = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
            }
            break;

        default:
            break;
    }
}

static void _onRequest() {
    if (_identityPending) {
        _sendIdentity();
        _identityPending = false;
        // If INT still asserted (e.g. BOOT_READY pending), leave it —
        // master will read the data packet on the next request.
        return;
    }
    if (_packetPending || _intAsserted) {
        _sendDataPacket();
    } else {
        // Unexpected read with no pending packet — send a zeroed header.
        // Master should not be reading without a prior INT assertion.
        for (uint8_t i = 0; i < K7SC_HEADER_SIZE; i++) Wire.write((uint8_t)0);
    }
}

// ── k7scI2CBegin() ────────────────────────────────────────────
void k7scI2CBegin(uint8_t i2cAddress, uint8_t typeId, uint8_t capFlags) {
    _typeId   = typeId;
    _capFlags = capFlags;

    Wire.begin(i2cAddress);
    Wire.onReceive(_onReceive);
    Wire.onRequest(_onRequest);

    pinMode(K7SC_PIN_INT, OUTPUT);
    _clearINT();

    // Assert BOOT_READY — held until master reads packet
    cmdState.lifecycle = K7SC_BOOT_READY;
    // Queue an empty payload boot packet
    _payloadLen    = 0;
    _packetPending = true;
    _assertINT();
}

// ── k7scQueuePacket() ─────────────────────────────────────────
void k7scQueuePacket(const uint8_t* payload, uint8_t length) {
    if (length > sizeof(_payload)) length = sizeof(_payload);
    memcpy(_payload, payload, length);
    _payloadLen    = length;
    _packetPending = true;
    if (!_intAsserted) _assertINT();
}

// ── k7scI2CPoll() ─────────────────────────────────────────────
// Called each loop — clears one-shot flags that were set last loop
void k7scI2CPoll() {
    if (_clearReset)    { cmdState.isReset    = false; _clearReset    = false; }
    if (_clearNewValue) { cmdState.hasNewValue = false; cmdState.newValue = 0;
                          _clearNewValue = false; }
    if (_clearLEDState) { cmdState.hasLEDState = false; cmdState.ledState = 0;
                          _clearLEDState = false; }

    // Schedule clears for flags set this loop
    if (cmdState.isReset)    _clearReset    = true;
    if (cmdState.hasNewValue) _clearNewValue = true;
    if (cmdState.hasLEDState) _clearLEDState = true;
}

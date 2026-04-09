/**
 * @file        I2C.cpp
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Throttle Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler implementation for the Throttle Module.
 *
 *              This tab owns the module-level state machine:
 *                enabled/disabled, precision mode, motor seek commands.
 *              It coordinates Motor, Wiper, and Buttons subsystems
 *              in response to I2C commands and button events.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Wire.h>
#include <Arduino.h>
#include "I2C.h"
#include "Motor.h"
#include "Wiper.h"
#include "Buttons.h"

// ============================================================
//  Module state
// ============================================================

static bool _enabled   = false;
static bool _precision = false;
static bool _intAsserted = false;

static const uint8_t _CMD_BUF_SIZE = 3;  // cmd + up to 2 payload bytes
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
    digitalWrite(THR_PIN_INT, LOW);
    _intAsserted = true;
}

static void _clearINT() {
    digitalWrite(THR_PIN_INT, HIGH);
    _intAsserted = false;
}

// ============================================================
//  _enable() / _disable() — module state transitions
// ============================================================

static void _enable() {
    _enabled = true;
    buttonsLEDsOn();
    // Motor resumes normal operation — was seeking 0 while disabled
    // No target set yet — motor stays where it is
    motorStop();
}

static void _disable() {
    _enabled   = false;
    _precision = false;
    buttonsLEDsOff();
    wiperExitPrecision();
    // Drive motor to 0 and hold there
    motorSetTarget(THR_ADC_MIN, true);
}

// ============================================================
//  _buildStatusByte()
// ============================================================

static uint8_t _buildStatusByte() {
    uint8_t flags = 0;
    if (_enabled)            flags |= THR_FLAG_ENABLED;
    if (_precision)          flags |= THR_FLAG_PRECISION;
    if (wiperIsTouched())    flags |= THR_FLAG_TOUCH;
    if (motorIsMoving())     flags |= THR_FLAG_MOTOR_MOVING;
    return flags;
}

// ============================================================
//  Packet senders
// ============================================================

static void _sendDataPacket() {
    uint8_t buf[THR_PACKET_SIZE];
    uint16_t val = wiperGetScaled();

    buf[0] = _buildStatusByte();
    buf[1] = buttonsGetEvents();
    buf[2] = (uint8_t)(val >> 8);
    buf[3] = (uint8_t)(val & 0xFF);

    wiperClearChanged();
    Wire.write(buf, THR_PACKET_SIZE);
    _clearINT();
}

static void _sendIdentityPacket() {
    uint8_t buf[THR_IDENTITY_SIZE];
    buf[0] = THR_MODULE_TYPE_ID;
    buf[1] = THR_FIRMWARE_MAJOR;
    buf[2] = THR_FIRMWARE_MINOR;
    buf[3] = THR_CAP_MOTORIZED;
    Wire.write(buf, THR_IDENTITY_SIZE);
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
            // No NeoPixels — accept and ignore
            break;

        case CMD_BULB_TEST:
            // Flash the panel indicator LED (LED1) briefly
            // LED1 is the board-mounted indicator, not a button LED
            // Use button LEDs as proxy since they are user-visible
            buttonsLEDsOn();
            delay(THR_BULB_TEST_MS);
            if (!_enabled) buttonsLEDsOff();
            break;

        case CMD_SLEEP:
        case CMD_DISABLE:
            _disable();
            break;

        case CMD_WAKE:
        case CMD_ENABLE:
            _enable();
            break;

        case CMD_RESET:
            _disable();
            buttonsClearAll();
            wiperReset();
            _precision       = false;
            _pendingResponse = RESP_NONE;
            _clearINT();
            break;

        case CMD_ACK_FAULT:
            // No fault tracking — acknowledged silently
            break;

        case CMD_SET_THROTTLE:
            // Controller commands a specific throttle position
            if (_cmdLen >= 3 && _enabled && !wiperIsTouched()) {
                uint16_t target16 = ((uint16_t)_cmdBuf[1] << 8) | _cmdBuf[2];
                // Convert from INT16 space back to ADC space
                uint16_t targetADC = (uint16_t)map((long)target16,
                                                    0L, (long)INT16_MAX,
                                                    0L, 1023L);
                bool isLimit = (targetADC <= THR_DEADZONE_HARD) ||
                               (targetADC >= THR_ADC_MAX - THR_DEADZONE_HARD);
                motorSetTarget(targetADC, isLimit);
            }
            break;

        case CMD_SET_PRECISION:
            if (_cmdLen >= 2 && _enabled) {
                bool enterPrecision = (_cmdBuf[1] == 0x01);
                if (enterPrecision && !_precision) {
                    _precision = true;
                    wiperEnterPrecision(wiperGetScaled());
                    // Drive slider to physical center for equal authority
                    motorSetTarget(THR_ADC_CENTER, false);
                } else if (!enterPrecision && _precision) {
                    _precision = false;
                    uint16_t currentOutput = wiperGetScaled();
                    wiperExitPrecision();
                    // Reposition motor to match current output in normal space
                    uint16_t targetADC = (uint16_t)map((long)currentOutput,
                                                        0L, (long)INT16_MAX,
                                                        0L, 1023L);
                    bool isLimit = (targetADC <= THR_DEADZONE_HARD) ||
                                   (targetADC >= THR_ADC_MAX - THR_DEADZONE_HARD);
                    motorSetTarget(targetADC, isLimit);
                }
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
            for (uint8_t i = 0; i < THR_PACKET_SIZE; i++) {
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
    pinMode(THR_PIN_INT, OUTPUT);
    _clearINT();

    Wire.onReceive(_onReceive);
    Wire.onRequest(_onRequest);
}

// ============================================================
//  i2cSyncINT()
//
//  Called every loop. Handles:
//    - Touch while enabled: stop motor, follow wiper
//    - Touch while disabled: resist with motor at 0
//    - Button events: process and set motor targets
//    - Value changes: assert INT for controller read
// ============================================================

void i2cSyncINT() {
    bool touched = wiperIsTouched();

    if (_enabled) {
        if (touched) {
            // Pilot is touching — stop motor, follow wiper
            motorStop();
        } else {
            // Process any pending button events
            uint8_t events = buttonsGetEvents();

            if (events & (1 << THR_BIT_100)) {
                motorSetTarget(THR_ADC_MAX, true);
            }
            if (events & (1 << THR_BIT_00)) {
                motorSetTarget(THR_ADC_MIN, true);
            }
            if (events & (1 << THR_BIT_UP)) {
                uint16_t current = wiperGetRaw();
                uint16_t next = current + THR_STEP_ADC;
                if (next > THR_ADC_MAX || next < current) next = THR_ADC_MAX;
                motorSetTarget(next, false);
            }
            if (events & (1 << THR_BIT_DOWN)) {
                uint16_t current = wiperGetRaw();
                uint16_t next = (THR_STEP_ADC > current)
                               ? THR_ADC_MIN
                               : current - THR_STEP_ADC;
                motorSetTarget(next, false);
            }
        }

        // Update motor with current wiper position
        motorUpdate(wiperGetRaw());

    } else {
        // Disabled — resist any touch by holding motor at 0
        if (touched) {
            motorSetTarget(THR_ADC_MIN, true);
        }
        motorUpdate(wiperGetRaw());
    }

    // Assert INT if controller needs to read
    bool pending = buttonsIsIntPending() || wiperIsValueChanged();
    if (pending) {
        _pendingResponse = RESP_DATA;
        if (!_intAsserted) _assertINT();
    } else {
        if (_intAsserted) _clearINT();
    }
}

// ============================================================
//  Accessors
// ============================================================

bool i2cIsEnabled() {
    return _enabled;
}

bool i2cIsPrecision() {
    return _precision;
}

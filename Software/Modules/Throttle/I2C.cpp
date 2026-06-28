/**
 * @file        I2C.cpp
 * @version     2.0
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

static bool _enabled     = false;
static bool _precision   = false;
static bool _intAsserted = false;

// Lifecycle (KMC_STATUS_*), transaction counter, and fault flag for
// I2C Protocol v2.4 conformance. _enabled tracks the motor/LED active
// state and equals (lifecycle == ACTIVE).
static uint8_t _lifecycle = KMC_STATUS_BOOT_READY;
static uint8_t _txCounter = 0;
static bool    _fault     = false;

// Cached button events snapshot — captured in i2cSyncINT() when
// events are read for motor target logic, consumed in _sendDataPacket()
// when the controller reads. Prevents buttonsGetEvents() being called
// twice (once for motor logic, once for the packet) where the second
// call would always return 0 because the first already cleared the mask.
static uint8_t _pendingEvents = 0;

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
    // Transaction counter increments on every INT assertion (wraps 255→0).
    _txCounter++;
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
//  _buildFlagsByte() — module-specific status flags (payload byte 3)
// ============================================================

static uint8_t _buildFlagsByte() {
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

    // Byte 0-2: universal 3-byte header
    uint8_t status = _lifecycle & KMC_STATUS_LIFECYCLE_MASK;
    if (_fault) status |= KMC_STATUS_FAULT;
    status |= KMC_STATUS_DATA_CHANGED;
    buf[0] = status;
    buf[1] = THR_MODULE_TYPE_ID;
    buf[2] = _txCounter;

    // Byte 3-6: module flags, button events, throttle value (big-endian)
    buf[3] = _buildFlagsByte();
    buf[4] = _pendingEvents;      // events snapshot captured in i2cSyncINT()
    buf[5] = (uint8_t)(val >> 8);
    buf[6] = (uint8_t)(val & 0xFF);

    _pendingEvents = 0;           // consumed — clear after building packet
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

        case CMD_DISABLE:
            // DISABLED — motor driven to 0 and held, LEDs off, inputs
            // suppressed. Also completes BOOT_READY init (clears boot INT).
            _lifecycle = KMC_STATUS_DISABLED;
            _disable();
            _clearINT();
            break;

        case CMD_SLEEP:
            // SLEEPING — freeze state exactly as-is. Motor holds its
            // current position (no seek to 0). INT suppressed.
            _lifecycle = KMC_STATUS_SLEEPING;
            motorStop();
            _clearINT();
            break;

        case CMD_ENABLE:
            // ACTIVE — motor control and LEDs on.
            _lifecycle = KMC_STATUS_ACTIVE;
            _enable();
            break;

        case CMD_WAKE:
            // Resume from SLEEPING to ACTIVE; motor resumes from frozen
            // position. Send a fresh state packet.
            _lifecycle = KMC_STATUS_ACTIVE;
            _enable();
            _assertINT();
            break;

        case CMD_RESET:
            // Reset to defaults and stay ACTIVE. Application state cleared;
            // motor holds current position (enable does not seek).
            buttonsClearAll();
            wiperReset();
            _precision       = false;
            _pendingEvents   = 0;
            _lifecycle       = KMC_STATUS_ACTIVE;
            _enable();
            _pendingResponse = RESP_NONE;
            _clearINT();
            break;

        case CMD_ACK_FAULT:
            _fault = false;
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
    // An identity read (after CMD_GET_IDENTITY) takes priority and does not
    // disturb a pending INT. Every other read returns the current data
    // packet (universal header + flags/events/value payload).
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
    pinMode(THR_PIN_INT, OUTPUT);
    _clearINT();

    Wire.onReceive(_onReceive);
    Wire.onRequest(_onRequest);

    // Power-on lifecycle: BOOT_READY. Assert INT and hold until the
    // controller reads the boot packet and sends CMD_DISABLE (spec §3).
    // The module remains disabled (motor/LEDs off) until CMD_ENABLE.
    _lifecycle       = KMC_STATUS_BOOT_READY;
    _pendingResponse = RESP_DATA;
    _assertINT();
}

// ============================================================
//  i2cSyncINT()
//
//  Called every loop. Responsible for:
//    - Touch while enabled: stop motor immediately
//    - Touch while disabled: re-assert motor target at 0 to resist
//    - Button events: translate to motor targets
//    - INT pin: assert or deassert based on pending state
//
//  motorUpdate() is NOT called here. It is called once per loop()
//  iteration in the main sketch, after i2cSyncINT() returns. This
//  keeps the motor position controller out of this function and
//  ensures it runs exactly once per loop regardless of how many
//  targets are set here.
// ============================================================

void i2cSyncINT() {
    // Frozen while sleeping — hold position and suppress INT.
    if (_lifecycle == KMC_STATUS_SLEEPING) {
        if (_intAsserted) _clearINT();
        return;
    }

    bool touched = wiperIsTouched();

    if (_lifecycle == KMC_STATUS_ACTIVE) {
        if (touched) {
            // Pilot is touching — stop motor and let wiper run free
            motorStop();
        } else {
            // Read and cache button events.
            // OR-accumulate so events aren't lost if i2cSyncINT() runs
            // multiple times before the controller reads the packet.
            uint8_t events = buttonsGetEvents();
            _pendingEvents |= events;

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
    } else {
        // DISABLED or BOOT_READY — re-assert target at 0 on touch to
        // resist pilot input.
        if (touched) {
            motorSetTarget(THR_ADC_MIN, true);
        }
    }

    // Input-driven INT only asserts in ACTIVE. In BOOT_READY the INT is held
    // from i2cBegin() until the boot packet is read; in DISABLED inputs are
    // suppressed.
    if (_lifecycle == KMC_STATUS_ACTIVE) {
        bool pending = buttonsIsIntPending() || wiperIsValueChanged();
        if (pending) {
            _pendingResponse = RESP_DATA;
            if (!_intAsserted) _assertINT();
        } else {
            if (_intAsserted) _clearINT();
        }
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

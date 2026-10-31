/**
 * @file        I2C.cpp
 * @version     2.2.0
 * @date        2026-10-31
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
static bool _bulb        = false;   // bulb test on: LEDs held on until the stop

// Lever hold (CMD_SET_LEVER_HOLD): while the master owns the lever, the last
// CMD_SET_THROTTLE target is kept and the lever is held there against a hand for
// at most THR_PUSH_MAX_MS per touch, then driven back when the hand lets go.
static volatile bool     _hold       = false;
static volatile uint16_t _holdTarget = 0;
static volatile bool     _holdValid  = false;   // a target has arrived since the hold began
static bool     _wasTouched = false;
static uint32_t _touchMs    = 0;

static bool _isLimit(uint16_t adc) {
    return (adc <= THR_DEADZONE_HARD) || (adc >= THR_ADC_MAX - THR_DEADZONE_HARD);
}

// Local throttle-enable state. In the schematic this was a panel switch
// (THRTL_ENA); in the final build that switch is not wired, so the local
// enable is held active. The module is only motor/LED-active when the master
// has commanded ACTIVE *and* the local enable is set.
static bool _localEnable = true;

// Lifecycle (KMC_STATUS_*), transaction counter, and fault flag for
// I2C Protocol v2.10 conformance. _enabled tracks the motor/LED active
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
    // Master-commanded ACTIVE gates on the local enable (held active — the
    // THRTL_ENA switch is not wired in the final build).
    _enabled = _localEnable;
    if (!_enabled) return;
    buttonsLEDsOn();
    // Motor resumes normal operation — was seeking 0 while disabled
    // No target set yet — motor stays where it is
    motorStop();
}

static void _disable() {
    _enabled   = false;
    _precision = false;
    _hold      = false;
    if (!_bulb) buttonsLEDsOff();       // a bulb test keeps them on until its stop
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
            // Spec §5.2: 0x01 (or no payload) starts, 0x00 stops. Persistent, no
            // timeout, not blocking. The five lamps share one enable, so the test
            // turns them all on; the stop puts them back as the lifecycle has them.
            _bulb = !(_cmdLen >= 2 && _cmdBuf[1] == 0x00);
            if (_bulb || _enabled) buttonsLEDsOn();
            else                   buttonsLEDsOff();
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
            _hold            = false;
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
            // Controller commands a specific throttle position. Refused while
            // touched, unless the master holds the lever: then the target is
            // kept, and i2cSyncINT() decides whether to push or wait.
            if (_cmdLen >= 3 && _enabled && (_hold || !wiperIsTouched())) {
                uint16_t target16 = ((uint16_t)_cmdBuf[1] << 8) | _cmdBuf[2];
                // Convert from INT16 space back to ADC space
                uint16_t targetADC = (uint16_t)map((long)target16,
                                                    0L, (long)INT16_MAX,
                                                    0L, 1023L);
                if (_hold) { _holdTarget = targetADC; _holdValid = true; }
                if (!wiperIsTouched()) motorSetTarget(targetADC, _isLimit(targetADC));
            }
            break;

        case CMD_SET_LEVER_HOLD:
            // 0x01: an autopilot owns the throttle and the master drives the
            // lever; hold it against a hand (stall-limited). 0x00: the pilot's.
            if (_cmdLen >= 2) {
                bool on = (_cmdBuf[1] == 0x01) && _enabled;
                if (on && !_hold) _holdValid = false;   // wait for the first target
                _hold = on;
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
//    - Button events: cached for the packet (the master acts on them)
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
    uint32_t now = millis();
    if (touched && !_wasTouched) _touchMs = now;

    // Where the lever belongs when nobody holds it: 0 while DISABLED (or
    // BOOT_READY), the master's target while it holds the lever, otherwise
    // nowhere (the pilot owns it and a touch frees it).
    bool     haveHome = false;
    uint16_t home     = THR_ADC_MIN;
    if (_lifecycle != KMC_STATUS_ACTIVE) { haveHome = true; }
    else if (_hold && _holdValid)        { haveHome = true; home = _holdTarget; }

    if (touched) {
        // Push back for at most THR_PUSH_MAX_MS per touch, then let go.
        if (haveHome && (now - _touchMs) < THR_PUSH_MAX_MS) motorSetTarget(home, _isLimit(home));
        else                                                 motorCancel();
    } else if (_wasTouched && haveHome) {
        motorSetTarget(home, _isLimit(home));             // the hand let go: drive back
    }
    _wasTouched = touched;

    if (_lifecycle == KMC_STATUS_ACTIVE && !touched) {
        // Read and cache button events.
        // OR-accumulate so events aren't lost if i2cSyncINT() runs
        // multiple times before the controller reads the packet.
        uint8_t events = buttonsGetEvents();
        _pendingEvents |= events;
        // The buttons are only reported. The master decides what they do and
        // moves the lever itself (it ignores them in ENGINE SAFE, in FINE and
        // under an autopilot): Module UI Reference §12.2.
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

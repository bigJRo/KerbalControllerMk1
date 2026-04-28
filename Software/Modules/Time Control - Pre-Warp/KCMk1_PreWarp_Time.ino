/**
 * @file        KCMk1_PreWarp_Time.ino
 * @version     2.0.0
 * @date        2026-04-28
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 * @target      ATtiny816 — flash to KC-01-1880 module
 *
 * @brief       Pre-Warp Time module firmware.
 *
 *              I2C Address:    0x2B
 *              Module Type ID: KMC_TYPE_PRE_WARP_TIME (0x0C)
 *              Capability:     KMC_CAP_DISPLAY
 *
 *              All application logic lives here. The library
 *              (Kerbal7SegmentCore v2.0.0) is a hardware interface
 *              layer only — it reports hardware events via inputState
 *              and cmdState. The sketch decides what they mean.
 *
 *              Button behaviour:
 *                BTN01 — 5 min preset  — flashes GOLD 150ms, sets value to 5
 *                BTN02 — 1 hour preset — flashes GOLD 150ms, sets value to 60
 *                BTN03 — 1 day preset  — flashes GOLD 150ms, sets value to 1440
 *                BTN_EN — Resets value to 0
 *
 *              All buttons return to BACKLIT after flash. No persistent
 *              toggle state — all three are the same action type (preset).
 *
 *              Display behaviour:
 *                Always on when ACTIVE. Shows pre-warp duration in minutes.
 *                Range 0–9999. No leading zeros.
 *
 *              Lifecycle:
 *                BOOT_READY  Library asserts INT at startup. Master
 *                             responds with DISABLE.
 *                DISABLED    All dark, value reset to DEFAULT_VALUE,
 *                             all input suppressed.
 *                ACTIVE      Buttons backlit, display on, pilot can interact.
 *                SLEEPING    State frozen exactly. No visual change.
 *                             All input suppressed.
 *
 *              Vessel switch: no action. State persists across vessel
 *              switches — pilot configures as needed.
 *
 *              Data packet payload (bytes 3–7, assembled by sketch):
 *                Byte 3: Button events  (bit0=BTN01 … bit3=BTN_EN)
 *                Byte 4: Change mask    (same layout)
 *                Byte 5: Module state   (always 0x00 — no persistent state)
 *                Byte 6: Value HIGH     (int16 minutes, big-endian)
 *                Byte 7: Value LOW
 *
 * @license     GNU General Public License v3.0
 *
 * @note        Hardware:  KC-01-1880 v2.0 (ATtiny816)
 *              Library:   Kerbal7SegmentCore v2.0.0
 *              IDE settings:
 *                Board:        ATtiny816 (megaTinyCore)
 *                Clock:        20 MHz internal
 *                Programmer:   serialUPDI
 */

#include <KerbalModuleCommon.h>
#include <Kerbal7SegmentCore.h>

// ── Module identity ───────────────────────────────────────────
#define I2C_ADDRESS     0x2B
#define DEFAULT_VALUE   0

// ── Preset values (minutes) ───────────────────────────────────
#define PRESET_5MIN     5
#define PRESET_1HOUR    60
#define PRESET_1DAY     1440

// ── Flash duration for preset buttons ─────────────────────────
#define FLASH_MS        150

// ── State ─────────────────────────────────────────────────────
static int16_t   _value          = DEFAULT_VALUE;
static uint32_t  _flashEnd[3]    = {0, 0, 0};  // per-button flash timer

// ── Helpers ───────────────────────────────────────────────────
static void setLED(uint8_t index, RGBColor c) {
    buttonSetPixel(index, c.r, c.g, c.b);
}

static void renderLEDs() {
    uint32_t now = millis();
    for (uint8_t i = 0; i < 3; i++) {
        setLED(i, (now < _flashEnd[i]) ? KMC_GOLD : KMC_BACKLIT);
    }
    buttonsShow();
}

static void queuePacket(uint8_t events, uint8_t change) {
    uint8_t payload[5] = {
        events,
        change,
        0x00,  // module state — no persistent toggle state on this module
        (uint8_t)(_value >> 8),
        (uint8_t)(_value & 0xFF)
    };
    k7scQueuePacket(payload, 5);
}

// ─────────────────────────────────────────────────────────────
//  setup
// ─────────────────────────────────────────────────────────────

void setup() {
    k7scBegin(I2C_ADDRESS, KMC_TYPE_PRE_WARP_TIME, KMC_CAP_DISPLAY);
    buttonsClearAll();
    displayShutdown();
}

// ─────────────────────────────────────────────────────────────
//  loop
// ─────────────────────────────────────────────────────────────

void loop() {
    k7scUpdate();

    // ── Lifecycle transitions ─────────────────────────────────
    static K7SCLifecycle _lastLC = K7SC_BOOT_READY;
    if (cmdState.lifecycle != _lastLC) {
        switch (cmdState.lifecycle) {
            case K7SC_ACTIVE:
                // ENABLE — restore display and LEDs, send current state
                displaySetValue(_value);
                displayWake();
                for (uint8_t i = 0; i < 3; i++) _flashEnd[i] = 0;
                renderLEDs();
                queuePacket(0, 0);
                break;
            case K7SC_DISABLED:
                // DISABLE — reset state, go dark
                _value = DEFAULT_VALUE;
                for (uint8_t i = 0; i < 3; i++) _flashEnd[i] = 0;
                buttonsClearAll();
                displayShutdown();
                break;
            case K7SC_SLEEPING:
                // SLEEP — library freezes state, nothing to do here
                break;
            default:
                break;
        }
        // WAKE is SLEEPING → ACTIVE — send current state to master
        if (_lastLC == K7SC_SLEEPING && cmdState.lifecycle == K7SC_ACTIVE) {
            queuePacket(0, 0);
        }
        _lastLC = cmdState.lifecycle;
    }

    // ── Bulb test — commandable regardless of lifecycle ───────
    static bool _lastBulb = false;
    if (cmdState.isBulbTest != _lastBulb) {
        if (cmdState.isBulbTest) {
            displayTest();
            setLED(0, KMC_WHITE); setLED(1, KMC_WHITE); setLED(2, KMC_WHITE);
            buttonsShow();
        } else {
            displayTestEnd();
            if (cmdState.lifecycle == K7SC_ACTIVE) {
                displaySetValue(_value);
                renderLEDs();
            } else if (cmdState.lifecycle == K7SC_SLEEPING) {
                // SLEEPING — restore exactly what was frozen
                displaySetValue(_value);
                renderLEDs();
            } else {
                // DISABLED — restore to dark
                buttonsClearAll();
                displayShutdown();
            }
        }
        _lastBulb = cmdState.isBulbTest;
    }

    if (cmdState.lifecycle != K7SC_ACTIVE) {
        inputState.buttonPressed  = 0;
        inputState.buttonReleased = 0;
        inputState.buttonChanged  = 0;
        inputState.encoderDelta   = 0;
        inputState.encoderChanged = false;
        encoderClearDelta();
        return;
    }

    // ── CMD_RESET ─────────────────────────────────────────────
    if (cmdState.isReset) {
        _value = DEFAULT_VALUE;
        for (uint8_t i = 0; i < 3; i++) _flashEnd[i] = 0;
        displaySetValue(_value);
        renderLEDs();
        queuePacket(0, 0);
    }

    // ── CMD_SET_VALUE ─────────────────────────────────────────
    if (cmdState.hasNewValue) {
        _value = cmdState.newValue;
        if (_value < K7SC_VALUE_MIN) _value = K7SC_VALUE_MIN;
        if (_value > K7SC_VALUE_MAX) _value = K7SC_VALUE_MAX;
        displaySetValue(_value);
        queuePacket(0, 0);
    }

    // ── LED flash timer — update on each loop ─────────────────
    // Re-render only when a flash just expired
    static uint32_t _lastFlashState = 0;
    uint32_t now = millis();
    uint32_t flashState = (now < _flashEnd[0]) | ((now < _flashEnd[1]) << 1)
                        | ((now < _flashEnd[2]) << 2);
    if (flashState != _lastFlashState) {
        renderLEDs();
        _lastFlashState = flashState;
    }

    // ── Buttons ───────────────────────────────────────────────
    bool sendPkt  = false;
    uint8_t events = 0, change = 0;

    if (inputState.buttonPressed & (1 << K7SC_BIT_BTN01)) {
        _value = PRESET_5MIN;
        _flashEnd[0] = millis() + FLASH_MS;
        displaySetValue(_value);
        renderLEDs();
        events |= (1 << K7SC_BIT_BTN01);
        change |= (1 << K7SC_BIT_BTN01);
        sendPkt = true;
    }
    if (inputState.buttonPressed & (1 << K7SC_BIT_BTN02)) {
        _value = PRESET_1HOUR;
        _flashEnd[1] = millis() + FLASH_MS;
        displaySetValue(_value);
        renderLEDs();
        events |= (1 << K7SC_BIT_BTN02);
        change |= (1 << K7SC_BIT_BTN02);
        sendPkt = true;
    }
    if (inputState.buttonPressed & (1 << K7SC_BIT_BTN03)) {
        _value = PRESET_1DAY;
        _flashEnd[2] = millis() + FLASH_MS;
        displaySetValue(_value);
        renderLEDs();
        events |= (1 << K7SC_BIT_BTN03);
        change |= (1 << K7SC_BIT_BTN03);
        sendPkt = true;
    }
    if (inputState.buttonPressed & (1 << K7SC_BIT_BTN_EN)) {
        _value = DEFAULT_VALUE;
        displaySetValue(_value);
        events |= (1 << K7SC_BIT_BTN_EN);
        change |= (1 << K7SC_BIT_BTN_EN);
        sendPkt = true;
    }

    // ── Encoder ───────────────────────────────────────────────
    if (inputState.encoderChanged) {
        // Click-count acceleration with inactivity timeout
        static uint16_t _clickCount = 0;
        static int8_t   _lastDir    = 0;
        static uint32_t _lastEncMs  = 0;
        int8_t dir = (inputState.encoderDelta > 0) ? +1 : -1;
        uint32_t now = millis();

        // Reset on direction reversal or inactivity timeout
        if (dir != _lastDir ||
            (K7SC_ENC_ACCEL_TIMEOUT_MS > 0 &&
             now - _lastEncMs > K7SC_ENC_ACCEL_TIMEOUT_MS)) {
            _clickCount = 0;
            _lastDir    = dir;
        }
        _lastEncMs = now;
        if (_clickCount < 65535) _clickCount++;

        int16_t step;
        if      (_clickCount >= K7SC_ENC_TURBO_COUNT)  step = K7SC_STEP_TURBO;
        else if (_clickCount >= K7SC_ENC_FAST_COUNT)   step = K7SC_STEP_FAST;
        else if (_clickCount >= K7SC_ENC_MEDIUM_COUNT) step = K7SC_STEP_MEDIUM;
        else                                            step = K7SC_STEP_SLOW;

        int32_t next = (int32_t)_value + (dir * step);
        if (next < K7SC_VALUE_MIN) next = K7SC_VALUE_MIN;
        if (next > K7SC_VALUE_MAX) next = K7SC_VALUE_MAX;
        _value = (int16_t)next;
        displaySetValue(_value);
        sendPkt = true;
    }

    // ── Clear consumed input ──────────────────────────────────
    inputState.buttonPressed  = 0;
    inputState.buttonReleased = 0;
    inputState.buttonChanged  = 0;
    inputState.encoderDelta   = 0;
    inputState.encoderChanged = false;

    if (sendPkt) queuePacket(events, change);
}

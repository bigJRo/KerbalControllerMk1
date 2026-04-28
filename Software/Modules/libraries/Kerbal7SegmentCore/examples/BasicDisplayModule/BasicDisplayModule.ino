/**
 * @file        BasicDisplayModule.ino
 * @version     2.0.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Minimal Kerbal7SegmentCore v2.0 example.
 *
 *              Demonstrates the library's hardware interface pattern:
 *                - Library reports hardware events via inputState / cmdState
 *                - Sketch owns all application logic
 *
 *              Behaviour:
 *                BTN01: toggle backlit ↔ green, enable/disable display
 *                BTN02: toggle backlit ↔ amber
 *                BTN03: toggle backlit ↔ red
 *                BTN_EN: reset display to DEFAULT_VALUE
 *                Encoder: adjusts displayed value ±1 per click
 *
 * @note        Hardware:  KC-01-1881/1882 v2.0 (ATtiny816)
 *              Library:   Kerbal7SegmentCore v2.0.0
 *              IDE settings:
 *                Board:        ATtiny816 (megaTinyCore)
 *                Clock:        20 MHz internal
 *                Programmer:   serialUPDI
 */

#include <KerbalModuleCommon.h>
#include <Kerbal7SegmentCore.h>

#define I2C_ADDRESS    0x2A
#define DEFAULT_VALUE  200

// ── Button state ──────────────────────────────────────────────
static bool _btn01Active = false;
static bool _btn02Active = false;
static bool _btn03Active = false;
static int16_t _value   = DEFAULT_VALUE;

static void renderLEDs() {
    buttonSetPixel(0, _btn01Active ? KMC_GREEN.r   : KMC_BACKLIT.r,
                      _btn01Active ? KMC_GREEN.g   : KMC_BACKLIT.g,
                      _btn01Active ? KMC_GREEN.b   : KMC_BACKLIT.b);
    buttonSetPixel(1, _btn02Active ? KMC_AMBER.r   : KMC_BACKLIT.r,
                      _btn02Active ? KMC_AMBER.g   : KMC_BACKLIT.g,
                      _btn02Active ? KMC_AMBER.b   : KMC_BACKLIT.b);
    buttonSetPixel(2, _btn03Active ? KMC_RED.r     : KMC_BACKLIT.r,
                      _btn03Active ? KMC_RED.g     : KMC_BACKLIT.g,
                      _btn03Active ? KMC_RED.b     : KMC_BACKLIT.b);
    buttonsShow();
}

void setup() {
    k7scBegin(I2C_ADDRESS, KMC_TYPE_GPWS_INPUT, KMC_CAP_DISPLAY);
    buttonsClearAll();
    displayShutdown();
}

void loop() {
    k7scUpdate();

    // ── Lifecycle ─────────────────────────────────────────────
    static K7SCLifecycle _lastLifecycle = K7SC_BOOT_READY;
    if (cmdState.lifecycle != _lastLifecycle) {
        if (cmdState.lifecycle == K7SC_ACTIVE) {
            renderLEDs();
            if (_btn01Active) { displaySetValue(_value); displayWake(); }
        } else {
            buttonsClearAll();
            displayShutdown();
        }
        _lastLifecycle = cmdState.lifecycle;
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
        _btn01Active = _btn02Active = _btn03Active = false;
        _value = DEFAULT_VALUE;
        renderLEDs();
        displayShutdown();
    }

    // ── CMD_SET_VALUE ─────────────────────────────────────────
    if (cmdState.hasNewValue) {
        _value = cmdState.newValue;
        if (_btn01Active) displaySetValue(_value);
    }

    // ── Buttons ───────────────────────────────────────────────
    bool sendPacket = false;

    if (inputState.buttonPressed & (1 << K7SC_BIT_BTN01)) {
        _btn01Active = !_btn01Active;
        if (_btn01Active) { displaySetValue(_value); displayWake(); }
        else              { displayShutdown(); }
        renderLEDs();
        sendPacket = true;
    }
    if (inputState.buttonPressed & (1 << K7SC_BIT_BTN02)) {
        _btn02Active = !_btn02Active;
        renderLEDs();
        sendPacket = true;
    }
    if (inputState.buttonPressed & (1 << K7SC_BIT_BTN03)) {
        _btn03Active = !_btn03Active;
        renderLEDs();
        sendPacket = true;
    }
    if (inputState.buttonPressed & (1 << K7SC_BIT_BTN_EN)) {
        _value = DEFAULT_VALUE;
        if (_btn01Active) displaySetValue(_value);
        sendPacket = true;
    }

    // ── Encoder ───────────────────────────────────────────────
    if (inputState.encoderChanged) {
        int32_t next = (int32_t)_value + inputState.encoderDelta;
        if (next < K7SC_VALUE_MIN) next = K7SC_VALUE_MIN;
        if (next > K7SC_VALUE_MAX) next = K7SC_VALUE_MAX;
        _value = (int16_t)next;
        if (_btn01Active) displaySetValue(_value);
        sendPacket = true;
    }

    // ── Clear input ───────────────────────────────────────────
    inputState.buttonPressed  = 0;
    inputState.buttonReleased = 0;
    inputState.buttonChanged  = 0;
    inputState.encoderDelta   = 0;
    inputState.encoderChanged = false;

    // ── Queue packet ──────────────────────────────────────────
    if (sendPacket) {
        uint8_t state = (_btn01Active ? (1 << 2) : 0)
                      | (_btn02Active ? (1 << 3) : 0);
        uint8_t payload[5] = {
            inputState.buttonPressed,
            inputState.buttonChanged,
            state,
            (uint8_t)(_value >> 8),
            (uint8_t)(_value & 0xFF)
        };
        k7scQueuePacket(payload, 5);
    }
}

/**
 * @file        KCMk1_GPWS_Input.ino
 * @version     2.0.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 * @target      ATtiny816 — flash to KC-01-1880 module
 *
 * @brief       GPWS Input Panel module firmware.
 *
 *              I2C Address:    0x2A
 *              Module Type ID: KMC_TYPE_GPWS_INPUT (0x0B)
 *              Capability:     KMC_CAP_DISPLAY
 *
 *              All application logic lives here. The library
 *              (Kerbal7SegmentCore v2.0.0) is a hardware interface
 *              layer only — it reports hardware events via inputState
 *              and cmdState. The sketch decides what they mean.
 *
 *              Button behaviour:
 *                BTN01 — GPWS Enable — 3-state cycle
 *                           State 0 (BACKLIT): GPWS off, display blank
 *                           State 1 (GREEN):   Full GPWS active
 *                           State 2 (AMBER):   Proximity tone only
 *                BTN02 — Proximity Alarm — toggle (BACKLIT ↔ GREEN)
 *                           Pressing while BTN01=ACTIVE forces BTN01→PROX
 *                           and sets proximity alarm active.
 *                BTN03 — Rendezvous Radar — toggle (BACKLIT ↔ GREEN)
 *                BTN_EN — Encoder pushbutton — resets threshold to
 *                           DEFAULT_VALUE (200m)
 *
 *              Display behaviour:
 *                On only when GPWS mode is ACTIVE or PROX (state 1 or 2).
 *                Shows current altitude threshold (0–9999m).
 *
 *              INT suppression:
 *                BTN02, BTN03, and encoder events are suppressed (no INT)
 *                when GPWS mode is OFF (state 0). BTN01 always reports
 *                since it is the mode switch.
 *
 *              Lifecycle:
 *                BOOT_READY  Library asserts INT at startup. Master
 *                             responds with DISABLE.
 *                DISABLED    All dark, defaults, all input suppressed.
 *                ACTIVE      Buttons backlit, pilot can interact.
 *                SLEEPING    State frozen exactly. No visual change.
 *                             All input suppressed.
 *
 *              Data packet payload (bytes 3–7, assembled by sketch):
 *                Byte 3: Button events  (bit0=BTN01 … bit3=BTN_EN)
 *                Byte 4: Change mask    (same layout)
 *                Byte 5: Module state   (bits 1:0 = BTN01 cycle state,
 *                                        bit 2 = BTN02 active,
 *                                        bit 3 = BTN03 active)
 *                Byte 6: Threshold HIGH (int16, big-endian)
 *                Byte 7: Threshold LOW
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
#define I2C_ADDRESS     0x2A
#define DEFAULT_VALUE   200

// ── Button indices ────────────────────────────────────────────
#define BTN_GPWS    0   // BTN01 — 3-state cycle
#define BTN_PROX    1   // BTN02 — toggle
#define BTN_RDV     2   // BTN03 — toggle
// BTN_EN is bit 3 in inputState bitmasks

// ── GPWS mode states (BTN01 cycle) ───────────────────────────
#define GPWS_OFF    0
#define GPWS_ACTIVE 1
#define GPWS_PROX   2

// ── Module state ──────────────────────────────────────────────
static uint8_t  _gpwsMode    = GPWS_OFF;
static bool     _proxActive  = false;
static bool     _rdvActive   = false;
static int16_t  _threshold   = DEFAULT_VALUE;
static bool     _displayOn   = false;

// ── LED colours ───────────────────────────────────────────────
static const RGBColor LED_OFF     = {  0,   0,   0};
static const RGBColor LED_BACKLIT = KMC_BACKLIT;
static const RGBColor LED_GREEN   = KMC_GREEN;
static const RGBColor LED_AMBER   = KMC_AMBER;

// ── Packet state for accumulation ─────────────────────────────
static uint8_t _pktEvents  = 0;
static uint8_t _pktChange  = 0;
static bool    _sendPacket = false;

// ── Encoder acceleration state ────────────────────────────────
static int8_t  _encLastDir   = 0;
static uint8_t _encRunCount  = 0;

static uint16_t _stepFromCount(uint8_t count) {
    if      (count >= K7SC_ENC_TURBO_COUNT)  return K7SC_STEP_TURBO;
    else if (count >= K7SC_ENC_FAST_COUNT)   return K7SC_STEP_FAST;
    else if (count >= K7SC_ENC_MEDIUM_COUNT) return K7SC_STEP_MEDIUM;
    else                                      return K7SC_STEP_SLOW;
}

static void applyEncoderDelta(int16_t delta) {
    if (delta == 0) return;

    int8_t dir = (delta > 0) ? +1 : -1;
    uint16_t clicks = (delta > 0) ? (uint16_t)delta : (uint16_t)(-delta);

    if (dir != _encLastDir) {
        _encRunCount = 0;
        _encLastDir  = dir;
    }

    int32_t next = _threshold;
    for (uint16_t i = 0; i < clicks; i++) {
        if (_encRunCount < 255) _encRunCount++;
        uint16_t step = _stepFromCount(_encRunCount);
        next += (int32_t)dir * step;
    }

    if (next < 0)    next = 0;
    if (next > 9999) next = 9999;
    _threshold = (int16_t)next;
}

// ============================================================
//  LED helpers
// ============================================================

static void setLED(uint8_t index, RGBColor c) {
    buttonSetPixel(index, c.r, c.g, c.b);
}

static void renderLEDs() {
    // BTN01 — GPWS mode
    switch (_gpwsMode) {
        case GPWS_OFF:    setLED(BTN_GPWS, LED_BACKLIT); break;
        case GPWS_ACTIVE: setLED(BTN_GPWS, LED_GREEN);   break;
        case GPWS_PROX:   setLED(BTN_GPWS, LED_AMBER);   break;
    }
    // BTN02 — proximity alarm
    setLED(BTN_PROX, _proxActive ? LED_GREEN : LED_BACKLIT);
    // BTN03 — rendezvous radar
    setLED(BTN_RDV, _rdvActive ? LED_GREEN : LED_BACKLIT);
    buttonsShow();
}

static void renderAllOff() {
    setLED(0, LED_OFF);
    setLED(1, LED_OFF);
    setLED(2, LED_OFF);
    buttonsShow();
}

// ============================================================
//  Display helpers
// ============================================================

static void displayOn() {
    if (!_displayOn) {
        displaySetValue(_threshold);
        displayWake();
        _displayOn = true;
    }
}

static void displayOff() {
    if (_displayOn) {
        displayShutdown();
        _displayOn = false;
    }
}

// ============================================================
//  Packet builder
// ============================================================

static void queueUpdate() {
    uint8_t state = (_gpwsMode & 0x03)
                  | (_proxActive ? (1 << 2) : 0)
                  | (_rdvActive  ? (1 << 3) : 0);

    uint8_t payload[5];
    payload[0] = _pktEvents;
    payload[1] = _pktChange;
    payload[2] = state;
    payload[3] = (uint8_t)(_threshold >> 8);
    payload[4] = (uint8_t)(_threshold & 0xFF);

    k7scQueuePacket(payload, 5);

    _pktEvents = 0;
    _pktChange = 0;
    _sendPacket = false;
}

// ============================================================
//  Module reset — called on CMD_RESET or locally
// ============================================================

static void moduleReset() {
    _gpwsMode   = GPWS_OFF;
    _proxActive = false;
    _rdvActive  = false;
    _threshold  = DEFAULT_VALUE;
    _encLastDir  = 0;
    _encRunCount = 0;
    displayOff();
    renderLEDs();   // show backlit — module is ENABLED, GPWS just reset to off
    _pktEvents  = 0;
    _pktChange  = 0;
    _sendPacket = false;
}

// ============================================================
//  Lifecycle handlers
// ============================================================

static void onEnable() {
    // Restore LEDs — display state follows GPWS mode
    renderLEDs();
    if (_gpwsMode != GPWS_OFF) displayOn();
    // Send current state to master
    _sendPacket = true;
}

static void onDisable() {
    _gpwsMode   = GPWS_OFF;
    _proxActive = false;
    _rdvActive  = false;
    _threshold  = DEFAULT_VALUE;
    _encLastDir  = 0;
    _encRunCount = 0;
    _pktEvents  = 0;
    _pktChange  = 0;
    _sendPacket = false;
    displayOff();
    renderAllOff();   // dark — no game context
}

static void onSleep() {
    // Freeze — don't change any state, just suppress INT
    // Visually stays as-is (library doesn't touch hardware on SLEEP)
}

static void onWake() {
    // Resume — send current state to master
    _sendPacket = true;
}

// ============================================================
//  setup
// ============================================================

void setup() {
    k7scBegin(I2C_ADDRESS, KMC_TYPE_GPWS_INPUT, KMC_CAP_DISPLAY);
    // Start dark — master will DISABLE then ENABLE us
    renderAllOff();
    displayOff();
}

// ============================================================
//  loop
// ============================================================

void loop() {
    k7scUpdate();

    // ── Handle lifecycle transitions ─────────────────────────
    // Only act on transitions — track previous lifecycle
    static K7SCLifecycle _lastLifecycle = K7SC_BOOT_READY;

    if (cmdState.lifecycle != _lastLifecycle) {
        switch (cmdState.lifecycle) {
            case K7SC_ACTIVE:   onEnable();  break;
            case K7SC_DISABLED: onDisable(); break;
            case K7SC_SLEEPING: onSleep();   break;
            default: break;
        }
        // WAKE is a transition from SLEEPING back to ACTIVE
        if (_lastLifecycle == K7SC_SLEEPING &&
            cmdState.lifecycle == K7SC_ACTIVE) {
            onWake();
        }
        _lastLifecycle = cmdState.lifecycle;
    }

    // ── Bulb test — commandable regardless of lifecycle ───────
    static bool _lastBulbTest = false;
    if (cmdState.isBulbTest != _lastBulbTest) {
        if (cmdState.isBulbTest) {
            displayTest();
            setLED(0, KMC_WHITE);
            setLED(1, KMC_WHITE);
            setLED(2, KMC_WHITE);
            buttonsShow();
        } else {
            displayTestEnd();
            displaySetValue(_threshold);
            renderLEDs();
            if (_gpwsMode == GPWS_OFF) displayOff();
        }
        _lastBulbTest = cmdState.isBulbTest;
    }

    // ── When not active, suppress everything and return ──────
    if (cmdState.lifecycle != K7SC_ACTIVE) {
        inputState.buttonPressed  = 0;
        inputState.buttonReleased = 0;
        inputState.buttonChanged  = 0;
        inputState.encoderDelta   = 0;
        inputState.encoderChanged = false;
        encoderClearDelta();  // discard ISR-accumulated clicks
        return;
    }

    // ── CMD_RESET ─────────────────────────────────────────────
    if (cmdState.isReset) {
        moduleReset();
        _sendPacket = true;
    }

    // ── CMD_SET_VALUE ─────────────────────────────────────────
    if (cmdState.hasNewValue) {
        _threshold = cmdState.newValue;
        if (_displayOn) displaySetValue(_threshold);
        _sendPacket = true;
    }

    // ── BTN01 — GPWS Enable (cycle) ──────────────────────────
    if (inputState.buttonPressed & (1 << BTN_GPWS)) {
        _pktEvents |= (1 << BTN_GPWS);
        _pktChange |= (1 << BTN_GPWS);

        _gpwsMode = (_gpwsMode + 1) % 3;

        // Display follows GPWS mode
        if (_gpwsMode != GPWS_OFF && !_displayOn) displayOn();
        if (_gpwsMode == GPWS_OFF && _displayOn)  {
            displayOff();
            // Reset BTN02 and BTN03 when GPWS turns off
            _proxActive = false;
            _rdvActive  = false;
        }

        renderLEDs();
        _sendPacket = true;
    }

    // ── BTN02 — Proximity Alarm (toggle) ─────────────────────
    if (inputState.buttonPressed & (1 << BTN_PROX)) {
        if (_gpwsMode == GPWS_ACTIVE) {
            // Force BTN01 to PROX mode and activate proximity alarm
            _gpwsMode   = GPWS_PROX;
            _proxActive = true;
            renderLEDs();
            _pktEvents |= (1 << BTN_PROX);
            _pktChange |= (1 << BTN_PROX);
            _sendPacket = true;
        } else if (_gpwsMode == GPWS_PROX) {
            _proxActive = !_proxActive;
            setLED(BTN_PROX, _proxActive ? LED_GREEN : LED_BACKLIT);
            buttonsShow();
            _pktEvents |= (1 << BTN_PROX);
            _pktChange |= (1 << BTN_PROX);
            _sendPacket = true;
        }
        // GPWS_OFF — ignore entirely, no packet
    }

    // ── BTN03 — Rendezvous Radar (toggle) ────────────────────
    if (inputState.buttonPressed & (1 << BTN_RDV)) {
        if (_gpwsMode != GPWS_OFF) {
            _rdvActive = !_rdvActive;
            setLED(BTN_RDV, _rdvActive ? LED_GREEN : LED_BACKLIT);
            buttonsShow();
            _pktEvents |= (1 << BTN_RDV);
            _pktChange |= (1 << BTN_RDV);
            _sendPacket = true;
        }
        // GPWS_OFF — ignore entirely, no packet
    }

    // ── BTN_EN — Encoder button (reset threshold) ─────────────
    if (inputState.buttonPressed & (1 << K7SC_BIT_BTN_EN)) {
        _pktEvents |= (1 << K7SC_BIT_BTN_EN);
        _pktChange |= (1 << K7SC_BIT_BTN_EN);
        _threshold = DEFAULT_VALUE;
        if (_displayOn) displaySetValue(_threshold);
        _sendPacket = true;
    }

    // ── Encoder rotation ──────────────────────────────────────
    if (inputState.encoderChanged && _gpwsMode != GPWS_OFF) {
        applyEncoderDelta(inputState.encoderDelta);
        if (_displayOn) displaySetValue(_threshold);
        _sendPacket = true;
    }

    // ── Clear consumed input state ────────────────────────────
    inputState.buttonPressed  = 0;
    inputState.buttonReleased = 0;
    inputState.buttonChanged  = 0;
    inputState.encoderDelta   = 0;
    inputState.encoderChanged = false;

    // ── Queue packet if anything changed ─────────────────────
    if (_sendPacket) queueUpdate();
}

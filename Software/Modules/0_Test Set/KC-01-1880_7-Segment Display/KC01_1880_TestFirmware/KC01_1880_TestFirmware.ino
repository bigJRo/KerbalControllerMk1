/**
 * @file        KC01_1880_TestFirmware.ino
 * @version     2.0.0
 * @date        2026-04-28
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 * @target      ATtiny816 — flash to KC-01-1880 module
 *
 * @brief       KC-01-1880 hardware validation firmware.
 *
 *              Exercises all library functions so the KC01_1880_Tester
 *              running on the Xiao RA4M1 can verify every I2C command
 *              and hardware response. Use this firmware when validating
 *              a new board or a Kerbal7SegmentCore library change.
 *
 *              Button configuration:
 *                BTN01 — 3-state cycle: BACKLIT → GREEN → AMBER
 *                BTN02 — toggle:        BACKLIT ↔ BLUE
 *                BTN03 — momentary:     flashes RED on press, returns to BACKLIT
 *                BTN_EN — resets display to 0
 *
 *              Boot sequence:
 *                Digit sweep 0→9 on rightmost digit, then shows 0.
 *
 * @note        Hardware:  KC-01-1880 v2.0 (ATtiny816)
 *              Library:   Kerbal7SegmentCore v2.0.0
 *              Pair with: KC01_1880_Tester on Xiao RA4M1
 *              IDE settings:
 *                Board:        ATtiny816 (megaTinyCore)
 *                Clock:        20 MHz internal
 *                Programmer:   serialUPDI
 */

#include <KerbalModuleCommon.h>
#include <Kerbal7SegmentCore.h>

#define I2C_ADDRESS     0x2A
#define DEFAULT_VALUE   0

// ── Button state ──────────────────────────────────────────────
// BTN01: 3-state cycle (0=BACKLIT, 1=GREEN, 2=AMBER)
// BTN02: toggle (false=BACKLIT, true=BLUE)
// BTN03: momentary flash (BACKLIT normally, RED briefly on press)
static uint8_t _btn01State  = 0;
static bool    _btn02Active = false;
static uint32_t _btn03FlashEnd = 0;
static int16_t _value = DEFAULT_VALUE;

// ── Flash duration for BTN03 ──────────────────────────────────
#define BTN03_FLASH_MS  300

// ── Colour helpers ────────────────────────────────────────────
static void setLED(uint8_t index, RGBColor c) {
    buttonSetPixel(index, c.r, c.g, c.b);
}

static void renderLEDs() {
    // BTN01 — 3-state cycle
    switch (_btn01State) {
        case 0:  setLED(0, KMC_BACKLIT); break;
        case 1:  setLED(0, KMC_GREEN);   break;
        case 2:  setLED(0, KMC_AMBER);   break;
    }
    // BTN02 — toggle
    setLED(1, _btn02Active ? KMC_BLUE : KMC_BACKLIT);
    // BTN03 — flash or backlit (handled in loop)
    if (millis() < _btn03FlashEnd) {
        setLED(2, KMC_RED);
    } else {
        setLED(2, KMC_BACKLIT);
    }
    buttonsShow();
}

// ── Packet builder ────────────────────────────────────────────
static void queueUpdate(uint8_t events, uint8_t change) {
    uint8_t state = (_btn01State & 0x03)
                  | (_btn02Active  ? (1 << 2) : 0);
    uint8_t payload[5] = {
        events,
        change,
        state,
        (uint8_t)(_value >> 8),
        (uint8_t)(_value & 0xFF)
    };
    k7scQueuePacket(payload, 5);
}

// ─────────────────────────────────────────────────────────────
//  setup
// ─────────────────────────────────────────────────────────────

void setup() {
    k7scBegin(I2C_ADDRESS, KMC_TYPE_GPWS_INPUT, KMC_CAP_DISPLAY);

    // Boot display self-test: sweep 0→9 on rightmost digit
    displayWake();
    for (uint8_t d = 0; d <= 9; d++) {
        displaySetValue(d);
        delay(120);
    }
    displaySetValue(DEFAULT_VALUE);

    // All buttons start backlit
    setLED(0, KMC_BACKLIT);
    setLED(1, KMC_BACKLIT);
    setLED(2, KMC_BACKLIT);
    buttonsShow();
}

// ─────────────────────────────────────────────────────────────
//  loop
// ─────────────────────────────────────────────────────────────

void loop() {
    k7scUpdate();

    // ── Lifecycle ─────────────────────────────────────────────
    static K7SCLifecycle _lastLC = K7SC_BOOT_READY;
    if (cmdState.lifecycle != _lastLC) {
        if (cmdState.lifecycle == K7SC_ACTIVE) {
            renderLEDs();
            displaySetValue(_value);
            displayWake();
        } else if (cmdState.lifecycle == K7SC_DISABLED) {
            buttonsClearAll();
            displayShutdown();
            _btn01State  = 0;
            _btn02Active = false;
            _value       = DEFAULT_VALUE;
        }
        _lastLC = cmdState.lifecycle;
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

    // ── Bulb test ─────────────────────────────────────────────
    static bool _lastBulb = false;
    if (cmdState.isBulbTest != _lastBulb) {
        if (cmdState.isBulbTest) {
            displayTest();
            setLED(0, KMC_WHITE); setLED(1, KMC_WHITE); setLED(2, KMC_WHITE);
            buttonsShow();
        } else {
            displayTestEnd();
            displaySetValue(_value);
            renderLEDs();
        }
        _lastBulb = cmdState.isBulbTest;
    }

    // ── CMD_RESET ─────────────────────────────────────────────
    if (cmdState.isReset) {
        _btn01State  = 0;
        _btn02Active = false;
        _value       = DEFAULT_VALUE;
        displaySetValue(_value);
        renderLEDs();
        queueUpdate(0, 0);
    }

    // ── CMD_SET_VALUE ─────────────────────────────────────────
    if (cmdState.hasNewValue) {
        _value = cmdState.newValue;
        displaySetValue(_value);
        queueUpdate(0, 0);
    }

    // ── BTN01 — 3-state cycle ─────────────────────────────────
    bool sendPkt = false;
    uint8_t events = 0, change = 0;

    if (inputState.buttonPressed & (1 << K7SC_BIT_BTN01)) {
        _btn01State = (_btn01State + 1) % 3;
        events |= (1 << K7SC_BIT_BTN01);
        change |= (1 << K7SC_BIT_BTN01);
        renderLEDs();
        sendPkt = true;
    }

    // ── BTN02 — toggle ────────────────────────────────────────
    if (inputState.buttonPressed & (1 << K7SC_BIT_BTN02)) {
        _btn02Active = !_btn02Active;
        events |= (1 << K7SC_BIT_BTN02);
        change |= (1 << K7SC_BIT_BTN02);
        renderLEDs();
        sendPkt = true;
    }

    // ── BTN03 — momentary flash ───────────────────────────────
    if (inputState.buttonPressed & (1 << K7SC_BIT_BTN03)) {
        _btn03FlashEnd = millis() + BTN03_FLASH_MS;
        events |= (1 << K7SC_BIT_BTN03);
        change |= (1 << K7SC_BIT_BTN03);
        renderLEDs();
        sendPkt = true;
    }
    // End of flash — restore backlit
    if (_btn03FlashEnd > 0 && millis() >= _btn03FlashEnd) {
        _btn03FlashEnd = 0;
        renderLEDs();
    }

    // ── BTN_EN — reset display to 0 ───────────────────────────
    if (inputState.buttonPressed & (1 << K7SC_BIT_BTN_EN)) {
        _value = DEFAULT_VALUE;
        displaySetValue(_value);
        events |= (1 << K7SC_BIT_BTN_EN);
        change |= (1 << K7SC_BIT_BTN_EN);
        sendPkt = true;
    }

    // ── Encoder ───────────────────────────────────────────────
    if (inputState.encoderChanged) {
        int32_t next = (int32_t)_value + inputState.encoderDelta;
        if (next < K7SC_VALUE_MIN) next = K7SC_VALUE_MIN;
        if (next > K7SC_VALUE_MAX) next = K7SC_VALUE_MAX;
        _value = (int16_t)next;
        displaySetValue(_value);
        sendPkt = true;
    }

    // ── Clear input ───────────────────────────────────────────
    inputState.buttonPressed  = 0;
    inputState.buttonReleased = 0;
    inputState.buttonChanged  = 0;
    inputState.encoderDelta   = 0;
    inputState.encoderChanged = false;

    if (sendPkt) queueUpdate(events, change);
}

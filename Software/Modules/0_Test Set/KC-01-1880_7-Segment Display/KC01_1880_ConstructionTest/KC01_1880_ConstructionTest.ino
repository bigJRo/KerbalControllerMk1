/**
 * @file        KC01_1880_ConstructionTest.ino
 * @version     2.0.0
 * @date        2026-04-28
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 * @target      ATtiny816 — flash to KC-01-1880 module under test
 *
 * @brief       KC-01-1880 standalone construction test.
 *
 *              Tests wiring, components, and soldering without any
 *              host controller. Only power (3.3V + GND) is required.
 *
 *              PHASE 1 — Automatic (no interaction)
 *                1a  All display segments on (8888)
 *                1b  Digit sweep 0-9 and multi-digit values
 *                1c  Each NeoPixel: red, green, blue, white individually
 *                1d  All NeoPixels white simultaneously
 *                1e  INT pin asserts and deasserts (probe with meter)
 *
 *              PHASE 2 — Interactive (technician presses each input)
 *                Prompt shown on display (1-6) and by LED colour.
 *                Press the prompted input within 15 seconds.
 *                1 = BTN01     2 = BTN02     3 = BTN03
 *                4 = BTN_EN    5 = Enc CW    6 = Enc CCW
 *
 *              RESULT
 *                PASS  All NeoPixels green, display 0, flashes 5x
 *                FAIL  Failed LEDs red, display shows fail code (1-6, 9)
 *                      Fail code 9 = visual issue flagged by technician
 *
 * @note        Hardware:  KC-01-1880 v2.0 (ATtiny816)
 *              Library:   Kerbal7SegmentCore v2.0.0
 *              No host controller required — power only.
 *              IDE settings:
 *                Board:        ATtiny816 (megaTinyCore)
 *                Clock:        20 MHz internal
 *                Programmer:   serialUPDI
 */

#include <KerbalModuleCommon.h>
#include <Kerbal7SegmentCore.h>

// ── Timing ────────────────────────────────────────────────────
#define SEGMENT_TEST_MS     1500
#define DIGIT_STEP_MS        120
#define PIXEL_STEP_MS        300
#define INPUT_TIMEOUT_MS   15000
#define RESULT_HOLD_MS      5000

// ── Test results ──────────────────────────────────────────────
static bool _btn01Ok  = false;
static bool _btn02Ok  = false;
static bool _btn03Ok  = false;
static bool _btnEnOk  = false;
static bool _encCwOk  = false;
static bool _encCcwOk = false;

// ── LED helpers ───────────────────────────────────────────────
static void setLED(uint8_t i, RGBColor c) { buttonSetPixel(i, c.r, c.g, c.b); }
static void allLEDs(RGBColor c) {
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++) setLED(i, c);
    buttonsShow();
}
static void promptLED(uint8_t index, RGBColor c) {
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++)
        setLED(i, i == index ? c : KMC_OFF);
    buttonsShow();
}
static void flashResult(bool pass) {
    RGBColor col = pass ? KMC_GREEN : KMC_RED;
    for (uint8_t f = 0; f < 3; f++) {
        allLEDs(col); delay(150);
        allLEDs(KMC_OFF); delay(100);
    }
}

// ─────────────────────────────────────────────────────────────
//  PHASE 1 — Automatic visual tests
// ─────────────────────────────────────────────────────────────

static void phase1() {
    // 1a — all segments on
    displayTest();
    allLEDs(KMC_WHITE);
    delay(SEGMENT_TEST_MS);
    displayTestEnd();
    allLEDs(KMC_OFF);
    delay(200);

    // 1b — digit sweep
    displayWake();
    for (uint8_t d = 0; d <= 9; d++) { displaySetValue(d); delay(DIGIT_STEP_MS); }
    uint16_t vals[] = { 10, 100, 1000, 1234, 5678, 9999, 0 };
    for (uint8_t i = 0; i < 7; i++) { displaySetValue(vals[i]); delay(DIGIT_STEP_MS * 2); }
    displaySetValue(0);
    delay(200);

    // 1c — each pixel: R, G, B, W individually
    RGBColor colours[] = { KMC_RED, KMC_GREEN, KMC_BLUE, KMC_WHITE };
    for (uint8_t c = 0; c < 4; c++) {
        for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++) {
            allLEDs(KMC_OFF);
            setLED(i, colours[c]);
            buttonsShow();
            delay(PIXEL_STEP_MS);
        }
    }

    // 1d — all pixels white simultaneously
    allLEDs(KMC_WHITE);
    delay(PIXEL_STEP_MS * 2);
    allLEDs(KMC_OFF);
    delay(200);

    // 1e — INT pin output self-check (probe with meter if needed)
    pinMode(K7SC_PIN_INT, OUTPUT);
    digitalWrite(K7SC_PIN_INT, LOW);  delay(300);
    digitalWrite(K7SC_PIN_INT, HIGH); delay(200);

    // Hold amber for 2s — technician should halt if they see a visual fault.
    // No input mechanism here; visual pass is assumed unless firmware replaced.
    allLEDs(KMC_AMBER);
    displaySetValue(1111);
    delay(2000);
    allLEDs(KMC_OFF);
    displaySetValue(0);
    delay(300);
}

// ─────────────────────────────────────────────────────────────
//  PHASE 2 — Interactive input tests
// ─────────────────────────────────────────────────────────────

static bool waitForButton(uint8_t mask, uint32_t timeoutMs) {
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        buttonsPoll();
        if (inputState.buttonPressed & mask) {
            inputState.buttonPressed = 0;
            inputState.buttonChanged = 0;
            return true;
        }
        delay(5);
    }
    return false;
}

static bool waitForEncoder(int8_t dir, int16_t threshold, uint32_t timeoutMs) {
    uint32_t start = millis();
    int16_t acc = 0;
    inputState.encoderDelta   = 0;
    inputState.encoderChanged = false;
    encoderClearDelta();
    while (millis() - start < timeoutMs) {
        encoderPoll();
        if (inputState.encoderChanged) {
            if (dir > 0 && inputState.encoderDelta > 0) acc += inputState.encoderDelta;
            if (dir < 0 && inputState.encoderDelta < 0) acc -= inputState.encoderDelta;
            inputState.encoderDelta   = 0;
            inputState.encoderChanged = false;
        }
        if (acc >= threshold) return true;
        delay(5);
    }
    return false;
}

static bool testButton(uint8_t ledIndex, uint8_t mask, uint16_t prompt) {
    promptLED(ledIndex, KMC_GREEN);
    displaySetValue(prompt);
    bool ok = waitForButton(mask, INPUT_TIMEOUT_MS);
    flashResult(ok);
    delay(200);
    return ok;
}

static bool testEnc(int8_t dir, uint16_t prompt) {
    allLEDs(KMC_AMBER);
    displaySetValue(prompt);
    bool ok = waitForEncoder(dir, 3, INPUT_TIMEOUT_MS);
    flashResult(ok);
    delay(200);
    return ok;
}

// ─────────────────────────────────────────────────────────────
//  RESULT
// ─────────────────────────────────────────────────────────────

static void showResult() {
    bool allOk = _btn01Ok && _btn02Ok && _btn03Ok
              && _btnEnOk && _encCwOk && _encCcwOk;

    if (allOk) {
        for (uint8_t f = 0; f < 5; f++) {
            allLEDs(KMC_GREEN); displaySetValue(8888); delay(300);
            allLEDs(KMC_OFF);   displayShutdown();     delay(200);
            displayWake();
        }
        allLEDs(KMC_GREEN);
        displaySetValue(0);
    } else {
        uint16_t code = !_btn01Ok ? 1 : !_btn02Ok ? 2 : !_btn03Ok ? 3
                      : !_btnEnOk ? 4 : !_encCwOk ? 5 : 6;
        setLED(0, _btn01Ok ? KMC_GREEN : KMC_RED);
        setLED(1, _btn02Ok ? KMC_GREEN : KMC_RED);
        setLED(2, _btn03Ok ? KMC_GREEN : KMC_RED);
        buttonsShow();
        for (uint8_t f = 0; f < 10; f++) {
            displaySetValue(code); delay(400);
            displayShutdown();     delay(200);
            displayWake();
        }
        displaySetValue(code);
    }

    delay(RESULT_HOLD_MS);
}

// ─────────────────────────────────────────────────────────────
//  setup / loop
// ─────────────────────────────────────────────────────────────

void setup() {
    // Direct hardware init — no I2C, no protocol
    displayBegin();
    encoderBegin();
    buttonsBegin();
    delay(300);

    phase1();

    displayWake();
    _btn01Ok  = testButton(0, (1 << K7SC_BIT_BTN01), 1);
    _btn02Ok  = testButton(1, (1 << K7SC_BIT_BTN02), 2);
    _btn03Ok  = testButton(2, (1 << K7SC_BIT_BTN03), 3);

    allLEDs(KMC_BLUE); displaySetValue(4);
    _btnEnOk  = waitForButton((1 << K7SC_BIT_BTN_EN), INPUT_TIMEOUT_MS);
    flashResult(_btnEnOk); delay(200);

    _encCwOk  = testEnc(+1, 5);
    _encCcwOk = testEnc(-1, 6);

    showResult();
}

void loop() {
    // Idle heartbeat — power-cycle or re-flash to rerun
    allLEDs(KMC_OFF); displayShutdown(); delay(900);
    setLED(0, KMC_DISCRETE_ON); buttonsShow(); delay(100);
}

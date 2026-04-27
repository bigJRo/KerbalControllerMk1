/*
 * KC-01-1880  Construction Test Firmware
 * Board:      ATtiny816  (megaTinyCore, 20 MHz internal)
 *
 * Standalone test — no I2C master required.
 * Tests every component on the board in sequence then enters
 * interactive mode. Use this immediately after assembly to
 * verify wiring, soldering, and components.
 *
 * ── Boot sequence (automatic) ────────────────────────────────
 *
 *   Phase 1 — Display test
 *     All 4 digits count 0→9 in sequence (segment driver check)
 *     All segments on for 1 second (bulb test)
 *
 *   Phase 2 — NeoPixel test
 *     Each pixel individually: RED → GREEN → BLUE → OFF
 *     Tests each channel of each pixel independently
 *
 *   Phase 3 — Interactive mode (display shows encoder value)
 *
 * ── Interactive mode ─────────────────────────────────────────
 *
 *   Encoder CW/CCW  : increments/decrements display value
 *   BTN_EN          : resets display to 0
 *   BTN01           : pixel 0 flashes white while held
 *   BTN02           : pixel 1 flashes white while held
 *   BTN03           : pixel 2 flashes white while held
 *
 * All 3 NeoPixels cycle R→G→B once per second in the background
 * during interactive mode so the assembler can verify all 3
 * channels of all 3 pixels under normal operation.
 *
 * Libraries required:
 *   - Kerbal7SegmentCore  (v1.1.0)
 *   - KerbalModuleCommon
 *   - tinyNeoPixel        (bundled with megaTinyCore)
 */

#include <KerbalModuleCommon.h>
#include <Kerbal7SegmentCore.h>

// Dummy I2C address — no master present but Wire.begin() still needed
#define I2C_ADDRESS    0x2A
#define MODULE_TYPE_ID KMC_TYPE_GPWS_INPUT

// All buttons momentary — we handle LEDs manually in this firmware
const ButtonConfig btnConfigs[K7SC_NEO_COUNT] = {
    { BTN_MODE_MOMENTARY, {K7SC_OFF, K7SC_OFF, K7SC_OFF}, 0, 0 },
    { BTN_MODE_MOMENTARY, {K7SC_OFF, K7SC_OFF, K7SC_OFF}, 0, 0 },
    { BTN_MODE_MOMENTARY, {K7SC_OFF, K7SC_OFF, K7SC_OFF}, 0, 0 },
};

// ── Button direct-read (bypasses library state machine) ──────
// We need raw button state for the "held" detection in interactive mode.
// BTN01/02/03 are active HIGH on PC1/PC2/PA1 (see schematic).
static bool btn01Held() { return digitalRead(K7SC_PIN_BTN01); }
static bool btn02Held() { return digitalRead(K7SC_PIN_BTN02); }
static bool btn03Held() { return digitalRead(K7SC_PIN_BTN03); }

// ── NeoPixel colour helpers ───────────────────────────────────
static void setAllPixels(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++)
        buttonsSetPixelColor(i, r, g, b);
    buttonsShow();
}

static void setPixel(uint8_t px, uint8_t r, uint8_t g, uint8_t b)
{
    buttonsSetPixelColor(px, r, g, b);
    buttonsShow();
}

// ── Phase 1: Display test ────────────────────────────────────
static void testDisplay()
{
    // Sweep 0-9 across all 4 digits simultaneously
    for (uint8_t d = 0; d <= 9; d++) {
        uint16_t val = d * 1111;  // 0000, 1111 ... 9999
        displaySetValue(val);
        delay(150);
    }
    displaySetValue(0);

    // Digit position verification — each digit shows a unique value
    // so the assembler can confirm digit 0 (units) is rightmost,
    // digit 3 (thousands) is leftmost, and none are crossed.
    //   1234 → thousands=1, hundreds=2, tens=3, units=4
    //   5678 → thousands=5, hundreds=6, tens=7, units=8
    //   1357 → odd digits only — easy to spot a swap
    //   2468 → even digits only
    uint16_t patterns[] = { 1234, 5678, 1357, 2468 };
    for (uint8_t p = 0; p < 4; p++) {
        displaySetValue(patterns[p]);
        delay(600);
    }
    displaySetValue(0);

    // All segments on (8888) at max intensity for 1 second
    displaySetIntensity(15);
    displaySetValue(8888);
    delay(1000);

    // Return to default intensity and zero
    displaySetIntensity(7);
    displaySetValue(0);
}

// ── Phase 2: NeoPixel test ───────────────────────────────────
static void testNeoPixels()
{
    // Test each pixel independently through R, G, B
    // Pixel index shown on display (1, 2, 3)
    for (uint8_t px = 0; px < K7SC_NEO_COUNT; px++) {
        displaySetValue(px + 1);

        setPixel(px, 255,   0,   0); delay(400);  // RED
        setPixel(px,   0, 255,   0); delay(400);  // GREEN
        setPixel(px,   0,   0, 255); delay(400);  // BLUE
        setPixel(px,   0,   0,   0);              // OFF
    }

    // All pixels white briefly to confirm chain is intact
    setAllPixels(255, 255, 255);
    delay(500);
    setAllPixels(0, 0, 0);

    displaySetValue(0);
}

// ─────────────────────────────────────────────────────────────
//  setup
// ─────────────────────────────────────────────────────────────

void setup()
{
    k7scBegin(I2C_ADDRESS, MODULE_TYPE_ID, KMC_CAP_DISPLAY, btnConfigs, 0);
    buttonsSetBrightness(80);

    // Run boot test sequence
    testDisplay();
    delay(200);
    testNeoPixels();
    delay(200);

    // Enter interactive mode — display 0
    displaySetValue(0);
}

// ─────────────────────────────────────────────────────────────
//  loop — interactive mode
// ─────────────────────────────────────────────────────────────

void loop()
{
    static uint32_t lastNeo   = 0;
    static uint8_t  neoPhase  = 0;   // 0=R 1=G 2=B

    // Colour table for background cycle
    static const uint8_t ct[3][3] = {
        {255,   0,   0},  // RED
        {  0, 255,   0},  // GREEN
        {  0,   0, 255},  // BLUE
    };

    k7scUpdate();

    // ── BTN_EN: reset display to 0 ──
    if (buttonsGetEncoderPress()) {
        encoderSetValue(0);
    }

    // ── BTN01/02/03: light corresponding pixel white while held ──
    // Override the background colour while held so the assembler
    // can verify each button drives the correct NeoPixel.
    bool b1 = btn01Held();
    bool b2 = btn02Held();
    bool b3 = btn03Held();

    if (b1 || b2 || b3) {
        // Show white on held button's pixel, background cycle on others
        uint8_t cr = ct[neoPhase][0];
        uint8_t cg = ct[neoPhase][1];
        uint8_t cb = ct[neoPhase][2];

        buttonsSetPixelColor(0, b1 ? 255 : cr, b1 ? 255 : cg, b1 ? 255 : cb);
        buttonsSetPixelColor(1, b2 ? 255 : cr, b2 ? 255 : cg, b2 ? 255 : cb);
        buttonsSetPixelColor(2, b3 ? 255 : cr, b3 ? 255 : cg, b3 ? 255 : cb);
        buttonsShow();
    } else {
        // ── Background NeoPixel colour cycle, once per second ──
        uint32_t now = millis();
        if (now - lastNeo >= 1000UL) {
            lastNeo  = now;
            neoPhase = (neoPhase + 1) % 3;
            for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++)
                buttonsSetPixelColor(i, ct[neoPhase][0], ct[neoPhase][1], ct[neoPhase][2]);
            buttonsShow();
        }
    }
}

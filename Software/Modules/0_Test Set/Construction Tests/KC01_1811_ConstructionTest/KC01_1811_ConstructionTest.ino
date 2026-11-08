/**
 * @file        KC01_1811_ConstructionTest.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — construction test firmware
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 * @target      ATtiny816 on the Wide Button Module (KC-01-1811/1812)
 *
 * @brief       Stand-alone construction test. Flash it to a freshly built
 *              board, apply power (12 V on the slot connector, or the
 *              Module Tester) and work through the inputs. No master, no I2C,
 *              no tester sketch needed. Flash the module's real firmware
 *              afterwards.
 *
 *              Modules on this board: Function Control (Switch Group 1), Vehicle Control (Switch Group 2).
 *
 *              1. LED test (about 10 s, automatic): each of the 12 button
 *                 LEDs lights RED, GREEN, BLUE in turn, then all WHITE.
 *                 A dark step is a dead LED or a broken data chain (every
 *                 LED after it stays dark); swapped colours are a colour-
 *                 order fault.
 *
 *              2. Input test (until power-off):
 *                   dim RED      button not pressed yet
 *                   WHITE        button held now
 *                   dim GREEN    button seen
 *                   BLUE         terminal input closed (shown on a button LED,
 *                                see the table below)
 *                   RED / WHITE  flashing: input was already closed at
 *                                power-on (stuck button, solder bridge, switch
 *                                left on); normal once it opens
 *                   MAGENTA      flashing on LEDs 9-12: unconnected input
 *                                B12-B15 reads closed (board fault)
 *                 When every button and every terminal input has been seen,
 *                 all LEDs flash GREEN three times.
 *
 *              Terminal inputs (close each one with a wire from the
 *              terminal's +5 V pin to the input pin):
 *                U16 pin 6 (B16) -> LED 1     U17 pin 4 (B20) -> LED 5
 *                U16 pin 5 (B17) -> LED 2     U17 pin 3 (B21) -> LED 6
 *                U16 pin 4 (B18) -> LED 3     U17 pin 2 (B22) -> LED 7
 *                U16 pin 3 (B19) -> LED 4     U17 pin 1 (B23) -> LED 8
 *              (+5 V: U16 pins 1-2, U17 pins 5-6. LED n = the n-th LED of the
 *              chain, i.e. button input B(n-1).) With the panel wired, use
 *              the switches instead: Switch Group 1 / 2 in input order.
 *
 *              The INT pin is pulled low while any input is closed (probe it
 *              on slot pin 13 if needed).
 *
 * @note        Uses the input mapping and debounce of KerbalButtonCore
 *              (KBCShiftReg), so a button that lights the wrong LED here is
 *              wired to the wrong input on the board.
 *
 * @note        IDE settings: Board ATtiny816 (megaTinyCore), Clock 10 MHz
 *              internal, tinyNeoPixel Port A. Libraries: KerbalButtonCore,
 *              KerbalModuleCommon, ShiftIn.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#define KBC_INPUT_COUNT     24
#define KBC_SHIFTREG_COUNT  3
#include <KerbalModuleCommon.h>
#include <tinyNeoPixel_Static.h>
#include <KBC_ShiftReg.h>
#include <KBC_ShiftReg.ipp>      // KBCShiftReg implementation (compiled in this sketch)

// ------------------------------------------------------------
//  Board
// ------------------------------------------------------------
#define BUTTONS        12
// Terminal inputs B16-B23 (U16, U17) have no LED; they show BLUE on button
// LEDs 1-8. B12-B15 are not connected and must never read closed.
static const int8_t TERMINAL_FOR_LED[BUTTONS] = { 16, 17, 18, 19, 20, 21, 22, 23, -1, -1, -1, -1 };
#define ALL_INPUTS   0xFFFFFFUL    // B0-B23
#define REQUIRED     0xFF0FFFUL    // buttons + terminals (B12-B15 not connected)
#define PHANTOM      0x00F000UL    // B12-B15

// ------------------------------------------------------------
//  Timing
// ------------------------------------------------------------
#define LED_STEP_MS     250
#define WHITE_MS        1000
#define FLASH_MS        125      // stuck / phantom flash half-period
#define RENDER_MS       20

static uint8_t      _pixBuf[BUTTONS * 3];
static tinyNeoPixel _pixels(BUTTONS, KBC_PIN_NEO, KBC_NEO_COLOR_ORDER, _pixBuf);
static KBCShiftReg  _inputs;

static uint32_t _seen   = 0;     // inputs seen closed during the input test
static uint32_t _stuck  = 0;     // inputs closed when the input test started
static bool     _done   = false; // all inputs seen (celebrated once)
static uint32_t _lastPoll = 0, _lastRender = 0;

static const RGBColor C_NOT_SEEN = { 40, 0, 0 };
static const RGBColor C_SEEN     = { 0, 60, 0 };
static const RGBColor C_HELD     = { 255, 255, 255 };
static const RGBColor C_TERMINAL = { 0, 0, 255 };
static const RGBColor C_PHANTOM  = { 255, 0, 255 };

static void setPixel(uint8_t i, RGBColor c) { _pixels.setPixelColor(i, c.r, c.g, c.b); }
static void fillPixels(RGBColor c) {
    for (uint8_t i = 0; i < BUTTONS; i++) setPixel(i, c);
    _pixels.show();
}

// ------------------------------------------------------------
//  1. LED test
// ------------------------------------------------------------
static void ledTest() {
    static const RGBColor STEP[3] = { { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 } };
    for (uint8_t i = 0; i < BUTTONS; i++) {
        for (uint8_t s = 0; s < 3; s++) {
            fillPixels(KMC_OFF);
            setPixel(i, STEP[s]);
            _pixels.show();
            delay(LED_STEP_MS);
        }
    }
    fillPixels(KMC_WHITE);
    delay(WHITE_MS);
    fillPixels(KMC_OFF);
}

// ------------------------------------------------------------
//  2. Input test
// ------------------------------------------------------------
static void celebrate() {
    for (uint8_t f = 0; f < 3; f++) {
        fillPixels(KMC_GREEN); delay(200);
        fillPixels(KMC_OFF);   delay(150);
    }
}

static void render(uint32_t live) {
    bool flash = (millis() / FLASH_MS) & 1;
    for (uint8_t i = 0; i < BUTTONS; i++) {
        uint32_t b = 1UL << i;
        RGBColor c = (_seen & b) ? C_SEEN : C_NOT_SEEN;
        if (live & b) c = C_HELD;
        int8_t t = TERMINAL_FOR_LED[i];
        if (t >= 0 && (live & (1UL << t))) c = C_TERMINAL;
        if (i >= 8 && (live & (1UL << (i + 4)) & PHANTOM)) c = flash ? C_PHANTOM : KMC_OFF;
        if (_stuck & b) c = flash ? KMC_RED : KMC_WHITE;
        if (t >= 0 && (_stuck & (1UL << t))) c = flash ? KMC_RED : KMC_WHITE;
        setPixel(i, c);
    }
    _pixels.show();
}

void setup() {
    pinMode(KBC_PIN_INT, OUTPUT);
    digitalWrite(KBC_PIN_INT, HIGH);
    pinMode(KBC_PIN_NEO, OUTPUT);
    _inputs.begin();
    ledTest();
    // Let the debounce settle, then note anything already closed.
    uint32_t t0 = millis();
    while (millis() - t0 < 100) { _inputs.poll(); delay(KBC_POLL_INTERVAL_MS); }
    _stuck = _inputs.getLiveState() & ALL_INPUTS;
}

void loop() {
    uint32_t now = millis();
    if (now - _lastPoll >= KBC_POLL_INTERVAL_MS) {
        _lastPoll = now;
        _inputs.poll();
    }
    uint32_t live = _inputs.getLiveState();
    _stuck &= live;                               // released: no longer stuck
    _seen |= live & ALL_INPUTS & ~_stuck;
    digitalWrite(KBC_PIN_INT, (live & ALL_INPUTS) ? LOW : HIGH);
    if (!_done && (_seen & REQUIRED) == REQUIRED) {
        _done = true;
        celebrate();
    }
    if (now - _lastRender >= RENDER_MS) {
        _lastRender = now;
        render(live);
    }
}

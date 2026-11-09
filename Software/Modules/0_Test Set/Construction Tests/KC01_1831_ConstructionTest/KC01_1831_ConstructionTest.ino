/**
 * @file        KC01_1831_ConstructionTest.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — construction test firmware
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 * @target      ATtiny816 on the Joystick Module (KC-01-1831/1832)
 *
 * @brief       Stand-alone construction test. Flash it to a freshly built
 *              board with the stick wired to U6, apply power (12 V on the
 *              slot connector, or the Module Tester) and work the stick and
 *              buttons. No master, no I2C, no tester sketch needed. Flash the
 *              module's real firmware (Joystick Rotation or Translation)
 *              afterwards.
 *
 *              Keep hands off the stick at power-on: the centre of each axis
 *              is sampled then.
 *
 *              1. LED test (automatic): LED 1 (BTN01) RED, GREEN, BLUE, then
 *                 LED 2 (BTN02) the same, then both WHITE. Then, if an axis
 *                 rests far from mid-scale (pot not centred, or a wiring
 *                 fault), both LEDs flash AMBER three times.
 *
 *              2. Input test (until power-off):
 *                   AXIS1 (left/right)   LED 1: GREEN one way, RED the other,
 *                                        brighter with more deflection,
 *                                        WHITE at the end of travel
 *                   AXIS2 (fwd/back)     LED 2: the same
 *                   AXIS3 (twist)        BLUE added to LED 1 one way and to
 *                                        LED 2 the other; CYAN at the end
 *                   BTN01 / BTN02 held   its LED WHITE
 *                   Stick button held    both LEDs MAGENTA
 *                 Both LEDs dark with the stick released: centre and
 *                 deadzone are fine. When both ends of all three axes and
 *                 all three buttons have been seen, both LEDs flash GREEN
 *                 three times.
 *
 *              The INT pin is pulled low while any button is held.
 *
 * @note        Raw readings, deadzone (+-32 counts) and end-of-travel limits
 *              (8 / 1015) match KerbalJoystickCore.
 *
 * @note        IDE settings: Board ATtiny816 (megaTinyCore), Clock 10 MHz
 *              internal, tinyNeoPixel Port C. Libraries: KerbalJoystickCore
 *              (KJC_Config.h only), KerbalModuleCommon.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include <KerbalModuleCommon.h>
#include <tinyNeoPixel_Static.h>
#include <KJC_Config.h>

#define DEADZONE      32
#define END_LOW       8
#define END_HIGH      1015
#define CENTRE_WARN   120        // centre further than this from 512: warn
#define DEBOUNCE_MS   20
#define STEP_MS       300

static uint8_t      _pixBuf[KJC_NEO_COUNT * 3];
static tinyNeoPixel _pixels(KJC_NEO_COUNT, KJC_PIN_NEOPIX, KJC_NEO_COLOR_ORDER, _pixBuf);

static const uint8_t AXIS_PIN[3] = { KJC_PIN_AXIS1, KJC_PIN_AXIS2, KJC_PIN_AXIS3 };
static const uint8_t BTN_PIN[3]  = { KJC_PIN_BTN01, KJC_PIN_BTN02, KJC_PIN_BTN_JOY };
static int16_t  _centre[3];
static uint8_t  _btnState = 0, _btnCand = 0;
static uint32_t _btnSince = 0;
static uint16_t _seen = 0;       // bits 0-5: AXIS1-, AXIS1+, AXIS2-, AXIS2+, AXIS3-, AXIS3+; 6-8: buttons
static bool     _done = false;

static void show2(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1) {
    _pixels.setPixelColor(0, r0, g0, b0);
    _pixels.setPixelColor(1, r1, g1, b1);
    _pixels.show();
}

static void flashBoth(uint8_t r, uint8_t g, uint8_t b, uint8_t times) {
    for (uint8_t f = 0; f < times; f++) {
        show2(r, g, b, r, g, b); delay(200);
        show2(0, 0, 0, 0, 0, 0); delay(150);
    }
}

/** Deflection 0..255 on the side of the centre the reading is on; 0 inside the deadzone. */
static uint8_t deflection(int16_t raw, int16_t centre, bool& positive) {
    int16_t d = raw - centre;
    positive = d > 0;
    if (d < 0) d = -d;
    if (d <= DEADZONE) return 0;
    int16_t range = positive ? (1023 - centre) : centre;
    if (range < 1) range = 1;
    long v = (long)(d - DEADZONE) * 255 / range;
    return v > 255 ? 255 : (uint8_t)v;
}

void setup() {
    pinMode(KJC_PIN_INT, OUTPUT);
    digitalWrite(KJC_PIN_INT, HIGH);
    pinMode(KJC_PIN_NEOPIX, OUTPUT);
    for (uint8_t i = 0; i < 3; i++) pinMode(BTN_PIN[i], INPUT);

    // Centre: average 16 readings per axis while the stick is released.
    for (uint8_t a = 0; a < 3; a++) {
        long sum = 0;
        for (uint8_t i = 0; i < 16; i++) { sum += analogRead(AXIS_PIN[a]); delay(2); }
        _centre[a] = (int16_t)(sum / 16);
    }

    // LED test.
    show2(255, 0, 0, 0, 0, 0); delay(STEP_MS);
    show2(0, 255, 0, 0, 0, 0); delay(STEP_MS);
    show2(0, 0, 255, 0, 0, 0); delay(STEP_MS);
    show2(0, 0, 0, 255, 0, 0); delay(STEP_MS);
    show2(0, 0, 0, 0, 255, 0); delay(STEP_MS);
    show2(0, 0, 0, 0, 0, 255); delay(STEP_MS);
    show2(255, 255, 255, 255, 255, 255); delay(2 * STEP_MS);
    show2(0, 0, 0, 0, 0, 0); delay(STEP_MS);

    bool off = false;
    for (uint8_t a = 0; a < 3; a++)
        if (_centre[a] < 512 - CENTRE_WARN || _centre[a] > 512 + CENTRE_WARN) off = true;
    if (off) flashBoth(255, 80, 0, 3);
}

void loop() {
    // Buttons (active high), debounced together.
    uint8_t raw = 0;
    for (uint8_t i = 0; i < 3; i++) if (digitalRead(BTN_PIN[i])) raw |= 1 << i;
    if (raw != _btnCand) { _btnCand = raw; _btnSince = millis(); }
    else if (millis() - _btnSince >= DEBOUNCE_MS) _btnState = _btnCand;
    _seen |= (uint16_t)_btnState << 6;
    digitalWrite(KJC_PIN_INT, _btnState ? LOW : HIGH);

    // Axes.
    uint8_t mag[3];
    bool pos[3], end[3];
    for (uint8_t a = 0; a < 3; a++) {
        int16_t r = analogRead(AXIS_PIN[a]);
        mag[a] = deflection(r, _centre[a], pos[a]);
        end[a] = (r <= END_LOW || r >= END_HIGH);
        if (end[a]) _seen |= 1 << (a * 2 + (r >= END_HIGH ? 1 : 0));
    }

    uint8_t c[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } };
    for (uint8_t a = 0; a < 2; a++) {             // AXIS1 -> LED 1, AXIS2 -> LED 2
        if (end[a]) { c[a][0] = c[a][1] = c[a][2] = 255; continue; }
        if (pos[a]) c[a][1] = mag[a]; else c[a][0] = mag[a];
    }
    if (mag[2]) {                                 // twist: blue on one LED per direction
        uint8_t l = pos[2] ? 1 : 0;
        if (end[2]) { c[l][0] = 0; c[l][1] = 255; c[l][2] = 255; }
        else c[l][2] = mag[2];
    }
    if (_btnState & 0x01) { c[0][0] = c[0][1] = c[0][2] = 255; }
    if (_btnState & 0x02) { c[1][0] = c[1][1] = c[1][2] = 255; }
    if (_btnState & 0x04) { c[0][0] = 255; c[0][1] = 0; c[0][2] = 255; c[1][0] = 255; c[1][1] = 0; c[1][2] = 255; }
    show2(c[0][0], c[0][1], c[0][2], c[1][0], c[1][1], c[1][2]);

    if (!_done && _seen == 0x1FF) {
        _done = true;
        flashBoth(0, 255, 0, 3);
    }
    delay(10);
}

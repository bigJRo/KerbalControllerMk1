/**
 * @file        KC01_1871_ConstructionTest.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — construction test firmware
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 * @target      ATtiny816 on the Throttle Module (KC-01-1871/1872)
 *
 * @brief       Stand-alone construction test. Flash it to a freshly built
 *              board with the motorised fader and the panel lamps wired,
 *              apply power (12 V on the slot connector, or the Module
 *              Tester; the motor runs from the board's 9 V supply) and watch
 *              the lamps and the lever. No master, no I2C, no tester sketch
 *              needed. Flash the Throttle firmware afterwards.
 *
 *              The panel lamps (one shared LED_ENA output) and the lever are
 *              the only indicators. Keep hands off the lever until step 3.
 *
 *              1. Lamp test: the lamps blink 3 times.
 *
 *              2. Motor test (automatic): the lever drives to 0 %, then
 *                 100 %, then 50 %; the lamps blink once at each arrival.
 *                 A fault stops the motor and repeats a blink code (N blinks,
 *                 pause) until any throttle button is pressed:
 *                   1  did not reach 0 %
 *                   2  did not reach 100 %
 *                   3  did not reach 50 %
 *                   4  lever moved the wrong way (motor or wiper reversed)
 *                   5  lever did not move (no 9 V, motor, driver or wiper
 *                      fault)
 *
 *              3. Input test (until power-off): lamps on.
 *                   Touch the fader knob      lamps off while touched
 *                   100% / 0%                 lever drives to the top / bottom
 *                   UP / DOWN                 lever steps 10 % up / down
 *                 Touching the lever stops the motor. When all four buttons
 *                 and the touch sensor have been seen, the lamps blink
 *                 quickly 5 times.
 *
 *              The INT pin is pulled low while a button or the touch
 *              sensor is active.
 *
 * @note        Pins and motor control (deadzones 8 / 20 counts, slow zone
 *              50 counts, PWM 200 / 80) follow the Throttle firmware 2.3
 *              (Throttle/Config.h).
 *
 * @note        IDE settings: Board ATtiny816 (megaTinyCore), Clock 10 MHz
 *              internal. Libraries: none.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */

// ------------------------------------------------------------
//  Pins (Throttle/Config.h, board KC-01-1871 V2.0)
// ------------------------------------------------------------
#define PIN_MTR_FWD     PIN_PA7
#define PIN_MTR_REV     PIN_PA6
#define PIN_MTR_SPEED   PIN_PC0
#define PIN_BTN_100     PIN_PB4
#define PIN_BTN_UP      PIN_PB3
#define PIN_BTN_DOWN    PIN_PC1
#define PIN_BTN_00      PIN_PC2
#define PIN_LAMPS       PIN_PC3
#define PIN_TOUCH       PIN_PA3
#define PIN_WIPER       PIN_PA2
#define PIN_INT         PIN_PA5

// ------------------------------------------------------------
//  Motor control (as the Throttle firmware)
// ------------------------------------------------------------
#define DEADZONE_LIMIT  8
#define DEADZONE_MID    20
#define SLOW_ZONE       50
#define SPEED_FULL      200
#define SPEED_SLOW      80
#define SEEK_TIMEOUT_MS 3000
#define STALL_CHECK_MS  500        // the lever must have moved by then
#define STEP_COUNTS     102        // 10 % of travel

#define DEBOUNCE_MS     20

static const uint8_t BTN_PIN[4] = { PIN_BTN_100, PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_00 };
static uint8_t  _btnState = 0, _btnCand = 0;
static uint32_t _btnSince = 0;
static uint8_t  _seen = 0;            // bits 0-3 buttons, bit 4 touch
static bool     _done = false;

// Interactive seek
static bool     _seeking = false;
static int16_t  _target = 0;
static uint32_t _seekStart = 0;

static void lamps(bool on) { digitalWrite(PIN_LAMPS, on ? HIGH : LOW); }
static int16_t wiper() { return analogRead(PIN_WIPER); }
static bool touched() { return digitalRead(PIN_TOUCH) == HIGH; }

static void motorStop() {
    digitalWrite(PIN_MTR_FWD, LOW);
    digitalWrite(PIN_MTR_REV, LOW);
    analogWrite(PIN_MTR_SPEED, 0);
}

static void motorDrive(bool forward, uint8_t speed) {
    digitalWrite(PIN_MTR_FWD, forward ? HIGH : LOW);
    digitalWrite(PIN_MTR_REV, forward ? LOW : HIGH);
    analogWrite(PIN_MTR_SPEED, speed);
}

static int16_t deadzoneFor(int16_t target) {
    return (target <= DEADZONE_LIMIT || target >= 1023 - DEADZONE_LIMIT) ? DEADZONE_LIMIT : DEADZONE_MID;
}

/** One control step towards target. Returns true when there. */
static bool motorStep(int16_t target) {
    int16_t err = target - wiper();
    int16_t a = err < 0 ? -err : err;
    if (a <= deadzoneFor(target)) { motorStop(); return true; }
    motorDrive(err > 0, a <= SLOW_ZONE ? SPEED_SLOW : SPEED_FULL);
    return false;
}

/** Blocking seek for the motor test. Returns 0 on arrival or a fault code. */
static uint8_t seekTest(int16_t target, uint8_t failCode) {
    int16_t start = wiper();
    int16_t startErr = target - start;
    if (startErr < 0) startErr = -startErr;
    uint32_t t0 = millis();
    while (!motorStep(target)) {
        uint32_t t = millis() - t0;
        int16_t now = wiper();
        int16_t err = target - now;
        if (err < 0) err = -err;
        if (err > startErr + 30) { motorStop(); return 4; }                         // wrong way
        int16_t moved = now - start;
        if (moved < 0) moved = -moved;
        if (t >= STALL_CHECK_MS && moved < 10) { motorStop(); return 5; }          // no movement
        if (t >= SEEK_TIMEOUT_MS) { motorStop(); return failCode; }
        delay(1);
    }
    return 0;
}

static void blink(uint8_t times, uint16_t onMs, uint16_t offMs) {
    for (uint8_t i = 0; i < times; i++) {
        lamps(true);  delay(onMs);
        lamps(false); delay(offMs);
    }
}

static bool anyButton() {
    for (uint8_t i = 0; i < 4; i++) if (digitalRead(BTN_PIN[i])) return true;
    return false;
}

/** Repeat a fault code until a button is pressed (then wait for release). */
static void showFault(uint8_t code) {
    while (true) {
        for (uint8_t i = 0; i < code; i++) {
            lamps(true);  if (anyButton()) goto out; delay(250);
            lamps(false); if (anyButton()) goto out; delay(250);
        }
        for (uint8_t i = 0; i < 20; i++) { if (anyButton()) goto out; delay(50); }
    }
out:
    lamps(false);
    while (anyButton()) delay(10);
    delay(50);
}

void setup() {
    pinMode(PIN_MTR_FWD, OUTPUT);
    pinMode(PIN_MTR_REV, OUTPUT);
    pinMode(PIN_MTR_SPEED, OUTPUT);
    motorStop();
    pinMode(PIN_LAMPS, OUTPUT);
    pinMode(PIN_INT, OUTPUT);
    digitalWrite(PIN_INT, HIGH);
    pinMode(PIN_TOUCH, INPUT);
    for (uint8_t i = 0; i < 4; i++) pinMode(BTN_PIN[i], INPUT);

    // 1. Lamp test
    lamps(false); delay(300);
    blink(3, 300, 300);
    delay(500);

    // 2. Motor test
    static const int16_t  TARGET[3] = { 0, 1023, 512 };
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t f = seekTest(TARGET[i], i + 1);
        if (f) { showFault(f); break; }
        blink(1, 150, 0);
        delay(400);
    }
    lamps(true);
}

void loop() {
    // Buttons (active high), debounced together; act on press.
    uint8_t raw = 0;
    for (uint8_t i = 0; i < 4; i++) if (digitalRead(BTN_PIN[i])) raw |= 1 << i;
    if (raw != _btnCand) { _btnCand = raw; _btnSince = millis(); }
    uint8_t pressed = 0;
    if (millis() - _btnSince >= DEBOUNCE_MS && _btnState != _btnCand) {
        pressed = _btnCand & ~_btnState;
        _btnState = _btnCand;
    }
    bool touch = touched();
    _seen |= _btnState | (touch ? 0x10 : 0);
    digitalWrite(PIN_INT, (_btnState || touch) ? LOW : HIGH);

    if (pressed) {
        int16_t w = wiper();
        if (pressed & 0x01) _target = 1023;
        else if (pressed & 0x08) _target = 0;
        else if (pressed & 0x02) _target = (w + STEP_COUNTS > 1023) ? 1023 : w + STEP_COUNTS;
        else if (pressed & 0x04) _target = (w - STEP_COUNTS < 0) ? 0 : w - STEP_COUNTS;
        _seeking = true;
        _seekStart = millis();
    }

    // A touch stops the motor and drops the target, as in the module firmware.
    if (touch && _seeking) { _seeking = false; motorStop(); }
    if (_seeking) {
        if (motorStep(_target) || millis() - _seekStart > SEEK_TIMEOUT_MS) { _seeking = false; motorStop(); }
    }

    if (!_done && _seen == 0x1F && !_btnState && !touch && !_seeking) {
        _done = true;
        lamps(false); delay(200);
        blink(5, 80, 80);
    }
    lamps(!touch);
    delay(2);
}

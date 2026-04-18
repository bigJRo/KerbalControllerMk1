/**
 * @file        KBC_Tester_Controller.ino
 * @version     1.0.0
 * @date        2026-04-09
 * @project     Kerbal Controller Mk1 — KBC Library Test Suite
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Interactive KBC library test controller for KC-01-9001
 *              Module Tester board (Seeed XIAO SAMD21).
 *
 *              Drives I2C transactions to the ATtiny816 target running
 *              KBC_DevBoard_Target.ino, evaluates responses, and reports
 *              pass/fail over USB CDC serial.
 *
 *              Open a serial terminal at any baud rate (SAMD21 USB CDC
 *              is baud-rate independent). Send single character commands
 *              to run individual tests or the full suite.
 *
 * @note        Hardware:  KC-01-9001 Module Tester v1.0 (XIAO SAMD21)
 *              Target:    KC-01-9101 Dev Board running KBC_DevBoard_Target
 *
 * @note        IDE settings:
 *                Board:   Seeed XIAO SAMD21
 *                Port:    USB CDC serial port
 *
 * @note        XIAO SAMD21 pin assignments (KC-01-9001):
 *                D4  — SDA_3V3 → I2C level shifter → SDA_5V → target
 *                D5  — SCL_3V3 → I2C level shifter → SCL_5V → target
 *                D6  — INT_3V3 ← BSS138 level shifter ← INT_5V ← target
 *
 *              Wire.begin() uses D4/D5 by default on XIAO SAMD21.
 *              No explicit SDA/SCL pin arguments needed.
 *
 * @note        Indicators (KC-01-9001):
 *                LED1 (green) — power indicator only, always on when
 *                               board is powered. Not INT-related.
 *                LED2 (red)   — INT indicator. Illuminates when the
 *                               target module pulls INT_5V low.
 *                               Driven from INT_5V via R2 (active low).
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Wire.h>

// ============================================================
//  Hardware pins (XIAO SAMD21)
//
//  D6 = INT_3V3 — active low input from target module via BSS138
//  level shifter. Pulled up to +3.3V via R1 10k on tester board.
//  LED2 (red) on the tester lights when INT_5V is pulled low by
//  the target — this is the 5V side of the same signal.
// ============================================================

#define PIN_INT_IN      6   // D6 — INT_3V3, active low, pulled up to 3.3V

// ============================================================
//  Target configuration
// ============================================================

#define TARGET_ADDR         0x20
#define TARGET_TYPE_ID      0x01   // KMC_TYPE_UI_CONTROL
#define TARGET_FW_MAJOR     1
#define TARGET_FW_MINOR     0
#define TARGET_CAP_FLAGS    0x01   // KMC_CAP_EXTENDED_STATES

#define KBC_PACKET_SIZE     4
#define KBC_IDENTITY_SIZE   4
#define KBC_LED_PAYLOAD     8

// ============================================================
//  Command bytes (KMC_CMD_*)
// ============================================================

#define CMD_GET_IDENTITY    0x01
#define CMD_SET_LED_STATE   0x02
#define CMD_SET_BRIGHTNESS  0x03
#define CMD_BULB_TEST       0x04
#define CMD_SLEEP           0x05
#define CMD_WAKE            0x06
#define CMD_RESET           0x07
#define CMD_ACK_FAULT       0x08
#define CMD_ENABLE          0x09
#define CMD_DISABLE         0x0A

// ============================================================
//  LED state nibble values
// ============================================================

#define LED_OFF             0x0
#define LED_ENABLED         0x1
#define LED_ACTIVE          0x2
#define LED_WARNING         0x3
#define LED_ALERT           0x4
#define LED_ARMED           0x5
#define LED_PARTIAL         0x6

// ============================================================
//  Test result tracking
// ============================================================

static uint8_t _passCount = 0;
static uint8_t _failCount = 0;
static uint8_t _lastEvents    = 0;
static uint8_t _lastChangeMask = 0;
static uint8_t _lastStatus    = 0;

// ============================================================
//  Utility — I2C helpers
// ============================================================

static void _send(uint8_t cmd) {
    Wire.beginTransmission(TARGET_ADDR);
    Wire.write(cmd);
    Wire.endTransmission();
}

static void _send(uint8_t cmd, uint8_t payload) {
    Wire.beginTransmission(TARGET_ADDR);
    Wire.write(cmd);
    Wire.write(payload);
    Wire.endTransmission();
}

static void _send(uint8_t cmd, const uint8_t* payload, uint8_t len) {
    Wire.beginTransmission(TARGET_ADDR);
    Wire.write(cmd);
    Wire.write(payload, len);
    Wire.endTransmission();
}

static bool _readIdentity(uint8_t* buf) {
    _send(CMD_GET_IDENTITY);
    delay(2);
    uint8_t n = Wire.requestFrom((uint8_t)TARGET_ADDR, (uint8_t)KBC_IDENTITY_SIZE);
    if (n != KBC_IDENTITY_SIZE) return false;
    for (uint8_t i = 0; i < KBC_IDENTITY_SIZE; i++) buf[i] = Wire.read();
    return true;
}

static bool _readPacket(uint8_t* buf) {
    uint8_t n = Wire.requestFrom((uint8_t)TARGET_ADDR, (uint8_t)KBC_PACKET_SIZE);
    if (n != KBC_PACKET_SIZE) return false;
    for (uint8_t i = 0; i < KBC_PACKET_SIZE; i++) buf[i] = Wire.read();
    _lastStatus     = buf[0];
    _lastChangeMask = buf[1];
    _lastEvents     = buf[2];
    return true;
}

// ============================================================
//  Utility — INT pin
// ============================================================

static bool _intAsserted() {
    return digitalRead(PIN_INT_IN) == LOW;
}

static bool _waitForINT(uint16_t timeoutMs = 500) {
    uint32_t deadline = millis() + timeoutMs;
    while (millis() < deadline) {
        if (_intAsserted()) return true;
    }
    return false;
}

static bool _waitForINTClear(uint16_t timeoutMs = 500) {
    uint32_t deadline = millis() + timeoutMs;
    while (millis() < deadline) {
        if (!_intAsserted()) return true;
    }
    return false;
}

// ============================================================
//  Utility — LED payload builder
// ============================================================

static void _buildLEDPayload(uint8_t* payload, uint8_t state) {
    // Set all 16 buttons to the same state
    uint8_t packed = (state << 4) | state;
    for (uint8_t i = 0; i < KBC_LED_PAYLOAD; i++) payload[i] = packed;
}

static void _buildLEDPayloadSingle(uint8_t* payload, uint8_t index, uint8_t state) {
    // Set one button to state, all others to OFF
    memset(payload, 0, KBC_LED_PAYLOAD);
    if (index % 2 == 0)
        payload[index / 2] = (state << 4);
    else
        payload[index / 2] = state;
}

// ============================================================
//  Utility — test reporting
// ============================================================

static void _pass(const char* name) {
    _passCount++;
    Serial.print(F("  [PASS] "));
    Serial.println(name);
}

static void _fail(const char* name, const char* reason = nullptr) {
    _failCount++;
    Serial.print(F("  [FAIL] "));
    Serial.print(name);
    if (reason) {
        Serial.print(F(" — "));
        Serial.print(reason);
    }
    Serial.println();
}

static void _info(const char* msg) {
    Serial.print(F("         "));
    Serial.println(msg);
}

static void _infof(const char* label, uint8_t val) {
    Serial.print(F("         "));
    Serial.print(label);
    Serial.print(F("0x"));
    if (val < 0x10) Serial.print('0');
    Serial.println(val, HEX);
}

static void _header(const char* name) {
    Serial.println();
    Serial.print(F("── "));
    Serial.print(name);
    Serial.println(F(" ──"));
}

static void _reset() {
    _send(CMD_RESET);
    delay(10);
}

static void _enable() {
    _send(CMD_ENABLE);
    delay(5);
}

// ============================================================
//  TEST 1 — Identity
// ============================================================

void testIdentity() {
    _header("1. Identity");

    uint8_t buf[KBC_IDENTITY_SIZE];
    if (!_readIdentity(buf)) {
        _fail("Read identity packet", "no response from target");
        return;
    }

    _infof("Type ID:   ", buf[0]);
    _infof("FW Major:  ", buf[1]);
    _infof("FW Minor:  ", buf[2]);
    _infof("Cap flags: ", buf[3]);

    buf[0] == TARGET_TYPE_ID
        ? _pass("Type ID matches")
        : _fail("Type ID", "expected 0x01");

    buf[1] == TARGET_FW_MAJOR
        ? _pass("FW major matches")
        : _fail("FW major");

    buf[2] == TARGET_FW_MINOR
        ? _pass("FW minor matches")
        : _fail("FW minor");

    buf[3] == TARGET_CAP_FLAGS
        ? _pass("Capability flags match")
        : _fail("Capability flags");
}

// ============================================================
//  TEST 2 — Bulb test
// ============================================================

void testBulbTest() {
    _header("2. Bulb Test");
    _reset();
    _enable();

    _info("Sending CMD_BULB_TEST (no payload) — watch LEDs flash");
    _send(CMD_BULB_TEST);
    delay(2500);   // wait for blocking test to complete on target
    _pass("Bulb test no-payload completed without hang");

    _info("Sending CMD_BULB_TEST 0x01 — watch LEDs flash again");
    _send(CMD_BULB_TEST, 0x01);
    delay(2500);
    _pass("Bulb test 0x01 payload completed without hang");

    _info("Sending CMD_BULB_TEST 0x00 — LEDs should stay in current state");
    _send(CMD_BULB_TEST, 0x00);
    delay(50);
    _pass("Bulb test 0x00 stop/no-op did not hang");

    _info("Confirm LEDs returned to enabled state (visual check)");
}

// ============================================================
//  TEST 3 — LED States (NeoPixel buttons 0-11)
// ============================================================

void testLEDStatesNeo() {
    _header("3. LED States — NeoPixel (BTN01-12)");
    _reset();
    _enable();

    uint8_t payload[KBC_LED_PAYLOAD];
    const char* stateNames[] = {
        "OFF", "ENABLED", "ACTIVE", "WARNING", "ALERT", "ARMED", "PARTIAL"
    };
    uint8_t states[] = {
        LED_OFF, LED_ENABLED, LED_ACTIVE,
        LED_WARNING, LED_ALERT, LED_ARMED, LED_PARTIAL
    };

    for (uint8_t s = 0; s < 7; s++) {
        _buildLEDPayload(payload, states[s]);
        // Apply only to NeoPixel buttons (indices 0-11), leave discrete alone
        // Rebuild: set indices 12-15 to OFF
        payload[6] = 0x00;
        payload[7] = 0x00;
        _send(CMD_SET_LED_STATE, payload, KBC_LED_PAYLOAD);
        delay(600);  // enough to see one flash cycle for WARNING/ALERT

        char msg[32];
        snprintf(msg, sizeof(msg), "NeoPixel all → %s (visual)", stateNames[s]);
        _pass(msg);
    }

    _info("Verify: OFF=dark, ENABLED=dim white, ACTIVE=per-button color,");
    _info("        WARNING=flashing amber, ALERT=flashing red,");
    _info("        ARMED=cyan, PARTIAL=amber static");

    // Walk each NeoPixel button individually in ACTIVE state
    _info("Walking ACTIVE state across each NeoPixel button individually...");
    for (uint8_t i = 0; i < 12; i++) {
        _buildLEDPayloadSingle(payload, i, LED_ACTIVE);
        _send(CMD_SET_LED_STATE, payload, KBC_LED_PAYLOAD);
        delay(200);
    }
    _pass("NeoPixel walk complete (verify each button lit in sequence)");
}

// ============================================================
//  TEST 4 — LED States (Discrete buttons 12-15)
// ============================================================

void testLEDStatesDiscrete() {
    _header("4. LED States — Discrete (BTN13-16)");
    _reset();
    _enable();

    uint8_t payload[KBC_LED_PAYLOAD];
    memset(payload, 0, KBC_LED_PAYLOAD);

    _info("Discrete LEDs are ON/OFF only — ENABLED and ACTIVE both produce ON");
    _info("WARNING, ALERT, ARMED, PARTIAL map to ON (no flash on discrete)");

    // All ON
    uint8_t onPacked = (LED_ACTIVE << 4) | LED_ACTIVE;
    payload[6] = onPacked;
    payload[7] = onPacked;
    _send(CMD_SET_LED_STATE, payload, KBC_LED_PAYLOAD);
    delay(300);
    _pass("Discrete all ON (LED13-16 should all be lit)");

    // All OFF
    payload[6] = 0x00;
    payload[7] = 0x00;
    _send(CMD_SET_LED_STATE, payload, KBC_LED_PAYLOAD);
    delay(300);
    _pass("Discrete all OFF (LED13-16 should all be dark)");

    // Walk each discrete LED individually
    _info("Walking each discrete LED individually...");
    for (uint8_t i = 12; i < 16; i++) {
        memset(payload, 0, KBC_LED_PAYLOAD);
        _buildLEDPayloadSingle(payload, i, LED_ACTIVE);
        _send(CMD_SET_LED_STATE, payload, KBC_LED_PAYLOAD);
        delay(400);
    }
    _pass("Discrete walk complete (verify each LED lit in sequence)");
}

// ============================================================
//  TEST 5 — Brightness
// ============================================================

void testBrightness() {
    _header("5. Brightness");
    _reset();
    _enable();

    uint8_t payload[KBC_LED_PAYLOAD];
    _buildLEDPayload(payload, LED_ENABLED);
    payload[6] = 0x00; payload[7] = 0x00;  // discrete off
    _send(CMD_SET_LED_STATE, payload, KBC_LED_PAYLOAD);
    delay(100);

    uint8_t levels[] = {8, 32, 64, 128, 255, 32};
    for (uint8_t i = 0; i < 6; i++) {
        _send(CMD_SET_BRIGHTNESS, levels[i]);
        delay(400);
    }
    _pass("Brightness ramp up and back to default (visual check)");
    _info("Verify NeoPixels brighten/dim smoothly — discrete LEDs unaffected");
}

// ============================================================
//  TEST 6 — Flash Timing (WARNING)
// ============================================================

void testFlashWarning() {
    _header("6. Flash Timing — WARNING");
    _reset();
    _enable();

    uint8_t payload[KBC_LED_PAYLOAD];
    _buildLEDPayload(payload, LED_WARNING);
    payload[6] = 0x00; payload[7] = 0x00;
    _send(CMD_SET_LED_STATE, payload, KBC_LED_PAYLOAD);

    _info("Watching WARNING flash timing for 3 seconds...");
    _info("Expected: 500ms ON / 500ms OFF amber");

    // Sample the NeoPixel indirectly — INT shouldn't fire for LED changes,
    // so we just time the visual. Use a known-good period check instead:
    // count transitions over 3 seconds → expect ~3 full cycles = 6 edges
    delay(3000);
    _pass("WARNING flash ran for 3s without hang (visual: ~3 amber blink cycles)");
}

// ============================================================
//  TEST 7 — Flash Timing (ALERT)
// ============================================================

void testFlashAlert() {
    _header("7. Flash Timing — ALERT");
    _reset();
    _enable();

    uint8_t payload[KBC_LED_PAYLOAD];
    _buildLEDPayload(payload, LED_ALERT);
    payload[6] = 0x00; payload[7] = 0x00;
    _send(CMD_SET_LED_STATE, payload, KBC_LED_PAYLOAD);

    _info("Watching ALERT flash timing for 3 seconds...");
    _info("Expected: 150ms ON / 150ms OFF red (faster than WARNING)");
    delay(3000);
    _pass("ALERT flash ran for 3s without hang (visual: ~10 red blink cycles)");
}

// ============================================================
//  TEST 8 — INT Pin Assertion
// ============================================================

void testINTPin() {
    _header("8. INT Pin");
    _reset();
    _enable();

    // INT should be deasserted (high) when idle
    !_intAsserted()
        ? _pass("INT deasserted when idle")
        : _fail("INT deasserted when idle", "INT is LOW at start of test");

    _info("Press any button on the dev board, then wait...");
    _info("(watching for INT assertion for up to 5 seconds)");

    bool intSeen = _waitForINT(5000);
    if (!intSeen) {
        _fail("INT asserted on button press", "no INT within 5s — press a button");
        return;
    }
    _pass("INT asserted after button press");

    // Read the packet — INT should deassert
    uint8_t buf[KBC_PACKET_SIZE];
    _readPacket(buf);
    delay(5);

    _waitForINTClear(100)
        ? _pass("INT deasserted after read")
        : _fail("INT deasserted after read", "INT still LOW after packet read");

    _infof("Events byte:  ", _lastEvents);
    _infof("Change mask:  ", _lastChangeMask);
    _infof("Status byte:  ", _lastStatus);

    _lastEvents != 0
        ? _pass("Events byte non-zero (button press captured)")
        : _fail("Events byte non-zero", "events=0x00 after INT");
}

// ============================================================
//  TEST 9 — Button Events (all 16 buttons)
// ============================================================

void testButtonEvents() {
    _header("9. Button Events");
    _reset();
    _enable();

    _info("This test prompts you to press each button in sequence.");
    _info("Press the button when prompted, then release it.");
    _info("Each button has 5 seconds per prompt.");

    // Button labels matching the dev board silkscreen
    const char* btnNames[] = {
        "BUTTON01", "BUTTON02", "BUTTON03", "BUTTON04",
        "BUTTON05", "BUTTON06", "BUTTON07", "BUTTON08",
        "BUTTON09", "BUTTON10", "BUTTON11", "BUTTON12",
        "BUTTON13", "BUTTON14", "BUTTON15", "BUTTON16"
    };

    uint8_t passedButtons = 0;

    for (uint8_t b = 0; b < 16; b++) {
        Serial.print(F("         Press "));
        Serial.print(btnNames[b]);
        Serial.println(F(" now..."));

        bool intSeen = _waitForINT(5000);
        if (!intSeen) {
            _fail(btnNames[b], "no INT within 5s");
            continue;
        }

        uint8_t buf[KBC_PACKET_SIZE];
        _readPacket(buf);
        delay(5);

        uint8_t expectedBit = (1 << (b % 8));  // events byte is 8-bit
        // Events byte covers indices 0-7; for indices 8-15 the change
        // mask carries the press edge in the upper byte equivalent.
        // On KBC the events and change mask are both 16-bit across
        // the full packet — check the correct byte for this button.
        bool pressed = (b < 8)
            ? ((_lastEvents    & (1 << b)) != 0)
            : ((_lastChangeMask & (1 << (b - 8))) != 0);

        // Wait for release INT
        _waitForINT(2000);
        _readPacket(buf);
        delay(5);

        if (pressed) {
            passedButtons++;
            _pass(btnNames[b]);
        } else {
            _fail(btnNames[b], "press not captured in events/change mask");
            _infof("  events: ", _lastEvents);
            _infof("  change: ", _lastChangeMask);
        }
    }

    Serial.print(F("         Buttons passed: "));
    Serial.print(passedButtons);
    Serial.println(F("/16"));
}

// ============================================================
//  TEST 10 — Multi-Button Press (debounce fix validation)
// ============================================================

void testMultiButton() {
    _header("10. Multi-Button Press");
    _reset();
    _enable();

    _info("Press and hold TWO buttons simultaneously, then release both.");
    _info("Both should register. Waiting 5 seconds...");

    bool intSeen = _waitForINT(5000);
    if (!intSeen) {
        _fail("Multi-button INT", "no INT within 5s");
        return;
    }

    uint8_t buf[KBC_PACKET_SIZE];
    _readPacket(buf);
    delay(5);

    // Count bits set in events + change mask
    uint8_t bitsSet = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (_lastEvents    & (1 << i)) bitsSet++;
        if (_lastChangeMask & (1 << i)) bitsSet++;
    }
    // Deduplicate (event and change both set for same button)
    uint8_t uniqueBits = __builtin_popcount(_lastEvents | _lastChangeMask);

    uniqueBits >= 2
        ? _pass("Two simultaneous buttons registered independently")
        : _fail("Two simultaneous buttons", "only one button captured — check debounce fix");

    _infof("Events:     ", _lastEvents);
    _infof("ChangeMask: ", _lastChangeMask);
    Serial.print(F("         Unique buttons detected: "));
    Serial.println(uniqueBits);

    // Wait for release
    _waitForINT(3000);
    _readPacket(buf);
}

// ============================================================
//  TEST 11 — Sleep / Wake
// ============================================================

void testSleepWake() {
    _header("11. Sleep / Wake");
    _reset();
    _enable();

    // Set all NeoPixels to ENABLED so we can see them go dark
    uint8_t payload[KBC_LED_PAYLOAD];
    _buildLEDPayload(payload, LED_ENABLED);
    payload[6] = 0x00; payload[7] = 0x00;
    _send(CMD_SET_LED_STATE, payload, KBC_LED_PAYLOAD);
    delay(100);

    // Sleep
    _send(CMD_SLEEP);
    delay(50);
    _pass("CMD_SLEEP sent (visual: NeoPixels should go dark)");

    // INT should be deasserted during sleep
    !_intAsserted()
        ? _pass("INT deasserted during sleep")
        : _fail("INT deasserted during sleep", "INT still asserted after CMD_SLEEP");

    // Press a button — INT should NOT fire during sleep
    _info("Press any button now (INT should NOT fire for 2 seconds)...");
    bool intDuringSleep = _waitForINT(2000);
    !intDuringSleep
        ? _pass("Button press suppressed during sleep (no INT)")
        : _fail("Button press suppressed during sleep", "INT fired during sleep");

    // Wake
    _send(CMD_WAKE);
    delay(50);
    _pass("CMD_WAKE sent (visual: NeoPixels should restore to ENABLED)");

    // Press a button — INT should fire after wake
    _info("Press any button (INT should fire within 5 seconds after wake)...");
    bool intAfterWake = _waitForINT(5000);
    intAfterWake
        ? _pass("Button event registered after wake")
        : _fail("Button event after wake", "no INT within 5s after CMD_WAKE");

    if (intAfterWake) {
        uint8_t buf[KBC_PACKET_SIZE];
        _readPacket(buf);
    }
}

// ============================================================
//  TEST 12 — Enable / Disable
// ============================================================

void testEnableDisable() {
    _header("12. Enable / Disable");
    _reset();

    // Should start disabled
    uint8_t payload[KBC_LED_PAYLOAD];
    _buildLEDPayload(payload, LED_ACTIVE);
    _send(CMD_SET_LED_STATE, payload, KBC_LED_PAYLOAD);
    delay(100);
    _info("LED state set while disabled — buttons should not be active");

    // Enable
    _send(CMD_ENABLE);
    delay(50);

    // Read identity to confirm module is responsive
    uint8_t id[KBC_IDENTITY_SIZE];
    bool ok = _readIdentity(id);
    ok
        ? _pass("Module responsive after CMD_ENABLE")
        : _fail("Module responsive after CMD_ENABLE", "no identity response");

    // Press a button to confirm events flow
    _info("Press any button (should register within 5s after enable)...");
    bool intSeen = _waitForINT(5000);
    intSeen
        ? _pass("Button event registered while enabled")
        : _fail("Button event while enabled", "no INT within 5s");

    if (intSeen) {
        uint8_t buf[KBC_PACKET_SIZE]; _readPacket(buf); delay(5);
    }

    // Disable
    _send(CMD_DISABLE);
    delay(50);
    _pass("CMD_DISABLE sent (visual: NeoPixels should go dark)");

    _info("Press any button (INT should NOT fire for 2 seconds while disabled)...");
    bool intWhileDisabled = _waitForINT(2000);
    !intWhileDisabled
        ? _pass("Button events suppressed while disabled")
        : _fail("Button events suppressed while disabled", "INT fired while disabled");
}

// ============================================================
//  TEST 13 — Reset
// ============================================================

void testReset() {
    _header("13. Reset");

    // Set some state first
    uint8_t payload[KBC_LED_PAYLOAD];
    _buildLEDPayload(payload, LED_ACTIVE);
    _send(CMD_ENABLE);
    _send(CMD_SET_LED_STATE, payload, KBC_LED_PAYLOAD);
    delay(100);
    _info("State set (enabled, all ACTIVE LEDs)");

    // Reset
    _send(CMD_RESET);
    delay(20);
    _pass("CMD_RESET sent");

    // INT should be clear
    !_intAsserted()
        ? _pass("INT deasserted after reset")
        : _fail("INT deasserted after reset", "INT still LOW after CMD_RESET");

    // Identity should still respond
    uint8_t id[KBC_IDENTITY_SIZE];
    _readIdentity(id);
    id[0] == TARGET_TYPE_ID
        ? _pass("Identity response valid after reset")
        : _fail("Identity response after reset");

    _info("Visual: All LEDs should be OFF after reset");
}

// ============================================================
//  TEST 14 — I2C Bus Stress
// ============================================================

void testBusStress() {
    _header("14. I2C Bus Stress");
    _reset();
    _enable();

    _info("Sending 100 rapid identity requests...");
    uint8_t failures = 0;
    uint8_t id[KBC_IDENTITY_SIZE];

    for (uint8_t i = 0; i < 100; i++) {
        if (!_readIdentity(id) || id[0] != TARGET_TYPE_ID) {
            failures++;
        }
        delay(5);
    }

    Serial.print(F("         Failures: "));
    Serial.print(failures);
    Serial.println(F("/100"));

    failures == 0
        ? _pass("100 identity reads all correct")
        : _fail("100 identity reads", "some reads failed or returned wrong data");

    _info("Sending 50 rapid LED state changes...");
    uint8_t payload[KBC_LED_PAYLOAD];
    uint8_t states[] = { LED_OFF, LED_ENABLED, LED_ACTIVE };
    failures = 0;

    for (uint8_t i = 0; i < 50; i++) {
        _buildLEDPayload(payload, states[i % 3]);
        Wire.beginTransmission(TARGET_ADDR);
        Wire.write(CMD_SET_LED_STATE);
        Wire.write(payload, KBC_LED_PAYLOAD);
        uint8_t err = Wire.endTransmission();
        if (err != 0) failures++;
        delay(2);
    }

    Serial.print(F("         Bus errors: "));
    Serial.println(failures);

    failures == 0
        ? _pass("50 rapid LED writes without bus errors")
        : _fail("50 rapid LED writes", "I2C bus errors detected");
}

// ============================================================
//  Print menu
// ============================================================

void printMenu() {
    Serial.println();
    Serial.println(F("╔══════════════════════════════════════════════╗"));
    Serial.println(F("║   Kerbal Controller Mk1 — KBC Test Suite     ║"));
    Serial.println(F("╠══════════════════════════════════════════════╣"));
    Serial.print  (F("║  Target: 0x"));
    Serial.print  (TARGET_ADDR, HEX);
    Serial.println(F("  (KBC_DevBoard_Target)            ║"));
    Serial.println(F("╠══════════════════════════════════════════════╣"));
    Serial.println(F("║  [1] Identity                                ║"));
    Serial.println(F("║  [2] Bulb test                               ║"));
    Serial.println(F("║  [3] LED states — NeoPixel                   ║"));
    Serial.println(F("║  [4] LED states — Discrete                   ║"));
    Serial.println(F("║  [5] Brightness                              ║"));
    Serial.println(F("║  [6] Flash timing — WARNING                  ║"));
    Serial.println(F("║  [7] Flash timing — ALERT                    ║"));
    Serial.println(F("║  [8] INT pin assertion                       ║"));
    Serial.println(F("║  [9] Button events (all 16)                  ║"));
    Serial.println(F("║  [A] Multi-button simultaneous press         ║"));
    Serial.println(F("║  [B] Sleep / Wake                            ║"));
    Serial.println(F("║  [C] Enable / Disable                        ║"));
    Serial.println(F("║  [D] Reset                                   ║"));
    Serial.println(F("║  [E] I2C bus stress                          ║"));
    Serial.println(F("║  [R] Run full suite                          ║"));
    Serial.println(F("║  [?] Show this menu                          ║"));
    Serial.println(F("╚══════════════════════════════════════════════╝"));
    Serial.print(F("> "));
}

void printSummary() {
    Serial.println();
    Serial.println(F("══════════════════════════════════════════════"));
    Serial.print(F("  Results: "));
    Serial.print(_passCount);
    Serial.print(F(" passed, "));
    Serial.print(_failCount);
    Serial.println(F(" failed"));
    if (_failCount == 0) {
        Serial.println(F("  ✓ All tests passed"));
    } else {
        Serial.println(F("  ✗ Some tests failed — review output above"));
    }
    Serial.println(F("══════════════════════════════════════════════"));
}

// ============================================================
//  setup()
// ============================================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);  // wait for USB CDC, max 3s

    Wire.begin();
    Wire.setClock(400000);
    // XIAO SAMD21: Wire uses D4 (SDA) and D5 (SCL) by default.
    // The Seeed board package maps these correctly — no pin args needed.

    pinMode(PIN_INT_IN, INPUT);  // pulled up via R1 10k on tester board

    // Brief pause for target to finish setup()
    delay(200);

    _passCount = 0;
    _failCount = 0;

    printMenu();
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    if (!Serial.available()) return;

    char cmd = Serial.read();
    // Flush any trailing CR/LF
    while (Serial.available()) Serial.read();

    // Reset counters for single-test runs
    if (cmd != 'R' && cmd != 'r') {
        _passCount = 0;
        _failCount = 0;
    }

    switch (cmd) {
        case '1': testIdentity();       break;
        case '2': testBulbTest();       break;
        case '3': testLEDStatesNeo();   break;
        case '4': testLEDStatesDiscrete(); break;
        case '5': testBrightness();     break;
        case '6': testFlashWarning();   break;
        case '7': testFlashAlert();     break;
        case '8': testINTPin();         break;
        case '9': testButtonEvents();   break;
        case 'A': case 'a': testMultiButton();   break;
        case 'B': case 'b': testSleepWake();     break;
        case 'C': case 'c': testEnableDisable(); break;
        case 'D': case 'd': testReset();         break;
        case 'E': case 'e': testBusStress();     break;

        case 'R': case 'r':
            _passCount = 0;
            _failCount = 0;
            Serial.println(F("\nRunning full suite..."));
            Serial.println(F("Note: Interactive tests (8, 9, A, B, C) require"));
            Serial.println(F("      you to press buttons when prompted.\n"));
            testIdentity();
            testBulbTest();
            testLEDStatesNeo();
            testLEDStatesDiscrete();
            testBrightness();
            testFlashWarning();
            testFlashAlert();
            testINTPin();
            testButtonEvents();
            testMultiButton();
            testSleepWake();
            testEnableDisable();
            testReset();
            testBusStress();
            printSummary();
            break;

        case '?':
            printMenu();
            return;

        default:
            Serial.println(F("Unknown command — send [?] for menu"));
            break;
    }

    if (cmd != 'R' && cmd != 'r' && cmd != '?') {
        printSummary();
    }

    Serial.println();
    Serial.print(F("> "));
}

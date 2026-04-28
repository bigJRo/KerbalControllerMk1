/**
 * @file        KC01_1880_Tester.ino
 * @version     2.0.0
 * @date        2026-04-28
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 * @target      Xiao RA4M1
 *
 * @brief       KC-01-1880 hardware validation tester.
 *
 *              Acts as I2C controller to fully exercise the KC-01-1880
 *              module running KC01_1880_TestFirmware.ino. Tests the
 *              Kerbal7SegmentCore library functions and hardware in
 *              isolation — not GPWS application behaviour.
 *
 *              Use this tester when validating a new board or a
 *              library change. For GPWS application testing use
 *              KCMk1_GPWS_Tester instead.
 *
 *              Tests:
 *                T00  I2C bus scan
 *                T01  GET_IDENTITY
 *                T02  Data packet read       [manual: turn encoder]
 *                T03  CMD_SET_VALUE
 *                T04  Display intensity sweep [visual]
 *                T05  CMD_BULB_TEST           [visual]
 *                T06  CMD_SLEEP / CMD_WAKE    [visual]
 *                T07  CMD_RESET
 *                T08  BTN01 CYCLE             [manual: press 3x]
 *                T09  BTN02 TOGGLE            [manual: press 2x]
 *                T10  BTN03 FLASH             [manual: press 1x]
 *                T11  BTN_EN encoder button   [manual: press 1x]
 *                T12  CMD_DISABLE / CMD_ENABLE [visual]
 *                T13  CMD_ACK_FAULT
 *                T14  INT pin behaviour
 *
 * @note        Pair with: KC01_1880_TestFirmware on the module
 *              Wiring:
 *                Xiao D4 → SDA (P1 pin 13)
 *                Xiao D5 → SCL (P1 pin 14)
 *                Xiao D6 → INT (P1 pin 15) [10kΩ pull-up to 3.3V]
 *                3.3V    → VCC (P1 pin 4)
 *                GND     → GND (P1 pin 3)
 */

#include <Wire.h>
#include <KerbalModuleCommon.h>

// ── I2C ──────────────────────────────────────────────────────
#define MODULE_ADDR     0x2A
#define PACKET_SIZE     8       // 3-byte header + 5-byte payload
#define IDENTITY_SIZE   4

// ── INT pin ───────────────────────────────────────────────────
#define PIN_INT         6       // Xiao D6, active LOW

// ── Timing ───────────────────────────────────────────────────
#define CMD_DELAY_MS        100
#define IDENTITY_DELAY_MS   50
#define SET_VALUE_WAIT_MS   300
#define BULB_WAIT_MS        2200

// ── Default value (test firmware resets to 0) ─────────────────
#define DEFAULT_VALUE   0

// ─────────────────────────────────────────────────────────────
//  Low-level helpers
// ─────────────────────────────────────────────────────────────

static void sendCmd(uint8_t cmd) {
    Wire.beginTransmission(MODULE_ADDR);
    Wire.write(cmd);
    uint8_t err = Wire.endTransmission();
    if (err) {
        Serial.print(F("  [WARN] sendCmd 0x")); Serial.print(cmd, HEX);
        Serial.print(F(" TX error: ")); Serial.println(err);
    }
}

static void sendCmd1(uint8_t cmd, uint8_t b0) {
    Wire.beginTransmission(MODULE_ADDR);
    Wire.write(cmd); Wire.write(b0);
    uint8_t err = Wire.endTransmission();
    if (err) { Serial.print(F("  [WARN] sendCmd1 TX error: ")); Serial.println(err); }
}

static void sendCmd2(uint8_t cmd, uint8_t b0, uint8_t b1) {
    Wire.beginTransmission(MODULE_ADDR);
    Wire.write(cmd); Wire.write(b0); Wire.write(b1);
    uint8_t err = Wire.endTransmission();
    if (err) { Serial.print(F("  [WARN] sendCmd2 TX error: ")); Serial.println(err); }
}

static uint8_t readBytes(uint8_t* buf, uint8_t n) {
    uint8_t got = Wire.requestFrom((uint8_t)MODULE_ADDR, n);
    if (got != n) {
        Serial.print(F("  [WARN] requestFrom: asked ")); Serial.print(n);
        Serial.print(F(" got ")); Serial.println(got);
    }
    uint8_t i = 0;
    while (Wire.available() && i < n) buf[i++] = Wire.read();
    return got;
}

static bool intAsserted()              { return digitalRead(PIN_INT) == LOW; }

static bool waitForINT(uint32_t ms) {
    uint32_t start = millis();
    while (!intAsserted()) {
        if (millis() - start >= ms) return false;
        delay(5);
    }
    return true;
}

static void drainINT() {
    uint32_t start = millis();
    while (intAsserted() && millis() - start < 500) {
        uint8_t buf[PACKET_SIZE];
        readBytes(buf, PACKET_SIZE);
        delay(20);
    }
}

static void waitForUser(const char* prompt) {
    Serial.print(F("  [>>>]  ")); Serial.print(prompt);
    Serial.println(F(" — press Enter when ready"));
    while (!Serial.available()) {}
    while (Serial.available()) Serial.read();
}

static void printSeparator() { Serial.println(F("─────────────────────────────────────────")); }
static void printPass(const char* msg) { Serial.print(F("  [PASS] ")); Serial.println(msg); }
static void printFail(const char* msg) { Serial.print(F("  [FAIL] ")); Serial.println(msg); }
static void printInfo(const char* msg) { Serial.print(F("  [INFO] ")); Serial.println(msg); }

static bool askYN(const char* question) {
    Serial.print(F("  [Y/N]  ")); Serial.print(question); Serial.print(F(" (Y/N): "));
    while (true) {
        while (!Serial.available()) {}
        char c = Serial.read();
        while (Serial.available()) Serial.read();
        if (c == 'Y' || c == 'y') { Serial.println(F("Y")); return true;  }
        if (c == 'N' || c == 'n') { Serial.println(F("N")); return false; }
    }
}

// ─────────────────────────────────────────────────────────────
//  Packet parsing
// ─────────────────────────────────────────────────────────────

// Universal header helpers
static uint8_t  pktLifecycle(const uint8_t* b) { return b[0] & KMC_STATUS_LIFECYCLE_MASK; }
static bool     pktFault(const uint8_t* b)     { return b[0] & KMC_STATUS_FAULT; }
static uint8_t  pktTypeId(const uint8_t* b)    { return b[1]; }
static uint8_t  pktCounter(const uint8_t* b)   { return b[2]; }

// Payload helpers (test firmware: same layout as GPWS)
static uint8_t  pktEvents(const uint8_t* b)    { return b[3]; }
static uint8_t  pktChange(const uint8_t* b)    { return b[4]; }
static uint8_t  pktState(const uint8_t* b)     { return b[5]; }
static int16_t  pktValue(const uint8_t* b)     { return (int16_t)(((uint16_t)b[6] << 8) | b[7]); }

static void printPacket(const uint8_t* b) {
    Serial.println(F("  ── Header ──────────────────────────────"));
    Serial.print(F("  Lifecycle  : ")); Serial.println(pktLifecycle(b));
    Serial.print(F("  Type ID    : 0x")); Serial.println(pktTypeId(b), HEX);
    Serial.print(F("  Counter    : ")); Serial.println(pktCounter(b));
    Serial.println(F("  ── Payload ─────────────────────────────"));
    Serial.print(F("  Events     : 0b")); Serial.println(pktEvents(b), BIN);
    Serial.print(F("  Change     : 0b")); Serial.println(pktChange(b), BIN);
    Serial.print(F("  State      : 0b")); Serial.println(pktState(b), BIN);
    Serial.print(F("  Value      : ")); Serial.println(pktValue(b));
}

// ─────────────────────────────────────────────────────────────
//  Hardware tests T00–T14
// ─────────────────────────────────────────────────────────────

void testBusScan() {
    printSeparator();
    Serial.println(F("T00: I2C Bus Scan"));
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print(F("  0x"));
            if (addr < 16) Serial.print(F("0"));
            Serial.print(addr, HEX);
            if (addr == MODULE_ADDR) Serial.print(F(" <-- MODULE"));
            Serial.println();
            found++;
        }
    }
    Wire.beginTransmission(MODULE_ADDR);
    Wire.endTransmission() == 0
        ? printPass("Module at 0x2A")
        : printFail("Module NOT at 0x2A");
    if (found == 0) printFail("No I2C devices found — check wiring");
}

void testIdentity() {
    printSeparator();
    Serial.println(F("T01: GET_IDENTITY"));
    sendCmd(KMC_CMD_GET_IDENTITY);
    delay(IDENTITY_DELAY_MS);
    uint8_t buf[IDENTITY_SIZE] = {0};
    uint8_t got = readBytes(buf, IDENTITY_SIZE);
    if (got != IDENTITY_SIZE) { printFail("Wrong byte count"); return; }
    Serial.print(F("  Type ID  : 0x")); Serial.println(buf[0], HEX);
    Serial.print(F("  FW       : ")); Serial.print(buf[1]); Serial.print('.'); Serial.println(buf[2]);
    Serial.print(F("  Caps     : 0b")); Serial.println(buf[3], BIN);
    buf[0] == KMC_TYPE_GPWS_INPUT  ? printPass("Type ID correct")    : printFail("Type ID wrong");
    buf[3] & KMC_CAP_DISPLAY       ? printPass("CAP_DISPLAY set")    : printFail("CAP_DISPLAY missing");
}

void testDataPacket() {
    printSeparator();
    Serial.println(F("T02: Data Packet Read  [manual: turn encoder]"));
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    drainINT();
    waitForUser("Turn encoder a few clicks");
    drainINT();
    printInfo("Waiting for INT (5s)...");
    if (!waitForINT(5000)) { printFail("No INT within 5s"); return; }
    uint8_t buf[PACKET_SIZE] = {0};
    readBytes(buf, PACKET_SIZE);
    printPacket(buf);
    printPass("Packet received");
    !intAsserted() ? printPass("INT cleared after read") : printFail("INT still asserted");
}

void testSetValue() {
    printSeparator();
    Serial.println(F("T03: CMD_SET_VALUE"));
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    drainINT();
    uint16_t vals[] = { 1234, 9999, 0, 42 };
    bool allOk = true;
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t v = vals[i];
        sendCmd2(KMC_CMD_SET_VALUE, (uint8_t)(v >> 8), (uint8_t)(v & 0xFF));
        if (waitForINT(SET_VALUE_WAIT_MS)) {
            uint8_t buf[PACKET_SIZE] = {0};
            readBytes(buf, PACKET_SIZE);
            int16_t got = pktValue(buf);
            bool ok = (got == (int16_t)v);
            Serial.print(F("  SET ")); Serial.print(v);
            Serial.print(F(" → ")); Serial.print(got);
            Serial.println(ok ? F(" [PASS]") : F(" [FAIL]"));
            if (!ok) allOk = false;
        } else {
            Serial.print(F("  SET ")); Serial.print(v); Serial.println(F(" → No INT [FAIL]"));
            allOk = false;
        }
    }
    allOk ? printPass("All SET_VALUE values confirmed") : printFail("Some values mismatched");
}

void testIntensity() {
    printSeparator();
    Serial.println(F("T04: Display intensity sweep  [visual]"));
    printInfo("Display shows 8888. Steps min→max then restores.");
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    sendCmd2(KMC_CMD_SET_VALUE, 0x22, 0xB8);  // 8888
    delay(CMD_DELAY_MS);
    drainINT();
    const uint8_t levels[] = { 0x00, 0x33, 0x77, 0xBB, 0xFF };
    const char*   labels[] = { "0 (min)", "3", "7 (mid)", "11", "15 (max)" };
    bool allOk = true;
    for (uint8_t i = 0; i < 5; i++) {
        sendCmd1(KMC_CMD_SET_BRIGHTNESS, levels[i]);
        delay(CMD_DELAY_MS);
        Serial.print(F("  Level ")); Serial.println(labels[i]);
        if (!askYN("Brightness change visible on display?")) allOk = false;
    }
    sendCmd1(KMC_CMD_SET_BRIGHTNESS, 0x77);
    delay(CMD_DELAY_MS);
    drainINT();
    allOk ? printPass("All intensity levels confirmed") : printFail("Issues reported");
}

void testBulbTest() {
    printSeparator();
    Serial.println(F("T05: CMD_BULB_TEST  [visual]"));
    printInfo("All segments on, all NeoPixels white for 2s...");
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    drainINT();
    sendCmd1(KMC_CMD_BULB_TEST, 0x01);  // start
    delay(BULB_WAIT_MS);
    sendCmd1(KMC_CMD_BULB_TEST, 0x00);  // stop
    delay(CMD_DELAY_MS);
    drainINT();
    askYN("Did all 7-segment segments light up?") ? printPass("Display OK") : printFail("Display issue");
    askYN("Did all 3 NeoPixels light white?")     ? printPass("NeoPixel OK") : printFail("NeoPixel issue");
}

void testSleepWake() {
    printSeparator();
    Serial.println(F("T06: CMD_SLEEP / CMD_WAKE  [visual]"));
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    sendCmd2(KMC_CMD_SET_VALUE, 0x01, 0x00);  // 256 — something visible
    delay(CMD_DELAY_MS);
    drainINT();
    printInfo("Sending SLEEP — state should freeze exactly as-is");
    sendCmd(KMC_CMD_SLEEP);
    delay(1000);
    askYN("Display and LEDs frozen (unchanged)?") ? printPass("SLEEP visual OK") : printFail("SLEEP issue");
    bool intDuring = intAsserted();
    !intDuring ? printPass("INT suppressed during SLEEP") : printFail("INT fired during SLEEP");
    printInfo("Sending WAKE — should restore to exactly pre-sleep state");
    sendCmd(KMC_CMD_WAKE);
    delay(CMD_DELAY_MS);
    askYN("Display and LEDs restored exactly?") ? printPass("WAKE visual OK") : printFail("WAKE issue");
}

void testReset() {
    printSeparator();
    Serial.println(F("T07: CMD_RESET"));
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    sendCmd2(KMC_CMD_SET_VALUE, 0x01, 0xF4);  // 500
    delay(CMD_DELAY_MS);
    drainINT();
    printInfo("Value set to 500 — sending RESET");
    sendCmd(KMC_CMD_RESET);
    if (waitForINT(SET_VALUE_WAIT_MS)) {
        uint8_t buf[PACKET_SIZE] = {0};
        readBytes(buf, PACKET_SIZE);
        int16_t val = pktValue(buf);
        Serial.print(F("  Value after reset: ")); Serial.println(val);
        val == DEFAULT_VALUE ? printPass("Reset to DEFAULT_VALUE (0)") : printFail("Wrong reset value");
        pktState(buf) == 0   ? printPass("Button states cleared")      : printFail("Button states not cleared");
    } else {
        printFail("No INT after RESET");
    }
    askYN("NeoPixels back to backlit state?") ? printPass("RESET visual OK") : printFail("Visual issue");
}

void testButtonCycle() {
    printSeparator();
    Serial.println(F("T08: BTN01 — 3-state CYCLE  [manual: press 3x]"));
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    drainINT();
    printInfo("Press BTN01 three times: BACKLIT → GREEN → AMBER → BACKLIT");
    waitForUser("Ready to press BTN01");
    drainINT();
    for (uint8_t press = 1; press <= 3; press++) {
        Serial.print(F("  Press ")); Serial.print(press); Serial.print(F(" of 3..."));
        if (!waitForINT(10000)) { Serial.println(F(" TIMEOUT [FAIL]")); return; }
        uint8_t buf[PACKET_SIZE] = {0};
        readBytes(buf, PACKET_SIZE);
        drainINT();
        uint8_t btn01State = pktState(buf) & 0x03;
        bool evtOk = pktEvents(buf) & 0x01;
        Serial.print(F(" btn01State=")); Serial.print(btn01State);
        Serial.println(evtOk ? F(" [PASS]") : F(" [FAIL — BTN01 bit not set]"));
    }
    askYN("LED cycled BACKLIT→GREEN→AMBER→BACKLIT?") ? printPass("CYCLE visual OK") : printFail("Visual issue");
}

void testButtonToggle() {
    printSeparator();
    Serial.println(F("T09: BTN02 — TOGGLE  [manual: press 2x]"));
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    drainINT();
    printInfo("Press BTN02 twice: BACKLIT → BLUE → BACKLIT");
    waitForUser("Ready to press BTN02");
    drainINT();
    for (uint8_t press = 1; press <= 2; press++) {
        Serial.print(F("  Press ")); Serial.print(press); Serial.print(F(" of 2..."));
        if (!waitForINT(10000)) { Serial.println(F(" TIMEOUT [FAIL]")); return; }
        uint8_t buf[PACKET_SIZE] = {0};
        readBytes(buf, PACKET_SIZE);
        drainINT();
        bool btn02Active = (pktState(buf) >> 2) & 0x01;
        bool evtOk = (pktEvents(buf) >> 1) & 0x01;
        Serial.print(F(" btn02Active=")); Serial.print(btn02Active);
        Serial.println(evtOk ? F(" [PASS]") : F(" [FAIL — BTN02 bit not set]"));
    }
    askYN("LED toggled BACKLIT→BLUE→BACKLIT?") ? printPass("TOGGLE visual OK") : printFail("Visual issue");
}

void testButtonFlash() {
    printSeparator();
    Serial.println(F("T10: BTN03 — FLASH  [manual: press 1x]"));
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    drainINT();
    printInfo("Press BTN03 once: LED flashes RED briefly then returns to BACKLIT");
    waitForUser("Ready to press BTN03");
    drainINT();
    Serial.print(F("  Waiting for press..."));
    if (!waitForINT(10000)) { Serial.println(F(" TIMEOUT [FAIL]")); return; }
    uint8_t buf[PACKET_SIZE] = {0};
    readBytes(buf, PACKET_SIZE);
    bool evtOk = (pktEvents(buf) >> 2) & 0x01;
    Serial.println(evtOk ? F(" [PASS]") : F(" [FAIL — BTN03 bit not set]"));
    askYN("LED flashed RED then returned to BACKLIT?") ? printPass("FLASH visual OK") : printFail("Visual issue");
}

void testEncoderPress() {
    printSeparator();
    Serial.println(F("T11: BTN_EN — Encoder button  [manual: press 1x]"));
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    sendCmd2(KMC_CMD_SET_VALUE, 0x01, 0x00);  // 256
    delay(CMD_DELAY_MS);
    drainINT();
    printInfo("Display set to 256 — press encoder button to reset to 0");
    waitForUser("Ready to press BTN_EN");
    drainINT();
    if (!waitForINT(10000)) { printFail("Timeout"); return; }
    uint8_t buf[PACKET_SIZE] = {0};
    readBytes(buf, PACKET_SIZE);
    bool evtOk = (pktEvents(buf) >> 3) & 0x01;
    int16_t val = pktValue(buf);
    evtOk        ? printPass("BTN_EN event received") : printFail("BTN_EN bit not set");
    val == 0     ? printPass("Value reset to 0")      : printFail("Value not 0 after press");
}

void testDisableEnable() {
    printSeparator();
    Serial.println(F("T12: CMD_DISABLE / CMD_ENABLE  [visual]"));
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    drainINT();
    printInfo("Sending DISABLE — all dark, defaults, INT suppressed");
    sendCmd(KMC_CMD_DISABLE);
    delay(1000);
    askYN("Display and LEDs all off?")  ? printPass("DISABLE visual OK") : printFail("DISABLE issue");
    !intAsserted()                      ? printPass("INT suppressed")     : printFail("INT fired during DISABLE");
    printInfo("Sending ENABLE — buttons backlit, display shows 0");
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    drainINT();
    askYN("Buttons backlit, display shows 0?") ? printPass("ENABLE visual OK") : printFail("ENABLE issue");
}

void testAckFault() {
    printSeparator();
    Serial.println(F("T13: CMD_ACK_FAULT"));
    sendCmd(KMC_CMD_ACK_FAULT);
    delay(CMD_DELAY_MS);
    sendCmd(KMC_CMD_GET_IDENTITY);
    delay(IDENTITY_DELAY_MS);
    uint8_t buf[IDENTITY_SIZE] = {0};
    (readBytes(buf, IDENTITY_SIZE) == IDENTITY_SIZE && buf[0] == KMC_TYPE_GPWS_INPUT)
        ? printPass("Module alive after ACK_FAULT") : printFail("Module not responding");
}

void testINTPin() {
    printSeparator();
    Serial.println(F("T14: INT pin behaviour"));
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    sendCmd(KMC_CMD_RESET);
    delay(200);
    if (intAsserted()) { uint8_t buf[PACKET_SIZE]; readBytes(buf, PACKET_SIZE); delay(50); }
    !intAsserted() ? printPass("INT idle HIGH") : printFail("INT stuck LOW");
    sendCmd2(KMC_CMD_SET_VALUE, 0x00, 0x05);
    bool asserted = waitForINT(SET_VALUE_WAIT_MS);
    asserted ? printPass("INT asserts on value change") : printFail("No INT on value change");
    if (asserted) {
        uint8_t buf[PACKET_SIZE]; readBytes(buf, PACKET_SIZE); delay(50);
        !intAsserted() ? printPass("INT clears after read") : printFail("INT not cleared");
    }
}

// ─────────────────────────────────────────────────────────────
//  Run all automated tests
// ─────────────────────────────────────────────────────────────

void runAllAuto() {
    testBusScan();       drainINT();
    testIdentity();      drainINT();
    testSetValue();      drainINT();
    testIntensity();     drainINT();
    testSleepWake();     drainINT();
    testReset();         drainINT();
    testDisableEnable(); drainINT();
    testAckFault();      drainINT();
    testINTPin();        drainINT();
    printSeparator();
    Serial.println(F("Auto tests complete."));
    Serial.println(F("Run T02, T08-T11 manually (require physical interaction)."));
    sendCmd(KMC_CMD_RESET);
    delay(CMD_DELAY_MS);
    drainINT();
}

// ─────────────────────────────────────────────────────────────
//  Menu
// ─────────────────────────────────────────────────────────────

void printMenu() {
    printSeparator();
    Serial.println(F("KC-01-1880 Hardware Tester v2.0.0 — Xiao RA4M1"));
    Serial.println(F("Pair with: KC01_1880_TestFirmware on module"));
    printSeparator();
    Serial.println(F("  A  Run all automated tests"));
    Serial.println(F("  S  T00: I2C bus scan"));
    Serial.println(F("  1  T01: GET_IDENTITY"));
    Serial.println(F("  2  T02: Data packet read   [manual: turn encoder]"));
    Serial.println(F("  3  T03: CMD_SET_VALUE"));
    Serial.println(F("  4  T04: Display intensity  [visual]"));
    Serial.println(F("  5  T05: CMD_BULB_TEST      [visual]"));
    Serial.println(F("  6  T06: CMD_SLEEP / WAKE   [visual]"));
    Serial.println(F("  7  T07: CMD_RESET"));
    Serial.println(F("  8  T08: BTN01 CYCLE        [manual: press 3x]"));
    Serial.println(F("  9  T09: BTN02 TOGGLE       [manual: press 2x]"));
    Serial.println(F("  0  T10: BTN03 FLASH        [manual: press 1x]"));
    Serial.println(F("  B  T11: BTN_EN encoder btn [manual: press 1x]"));
    Serial.println(F("  C  T12: CMD_DISABLE/ENABLE [visual]"));
    Serial.println(F("  D  T13: CMD_ACK_FAULT"));
    Serial.println(F("  E  T14: INT pin behaviour"));
    printSeparator();
    Serial.println(F("Send letter/number:"));
}

// ─────────────────────────────────────────────────────────────
//  setup / loop
// ─────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    uint32_t t = millis();
    while (!Serial && millis() - t < 3000) delay(10);
    Wire.begin();
    Wire.setClock(100000);
    pinMode(PIN_INT, INPUT_PULLUP);
    // Handle BOOT_READY if module just powered on
    if (waitForINT(1000)) {
        uint8_t buf[PACKET_SIZE] = {0};
        readBytes(buf, PACKET_SIZE);
        if ((buf[0] & KMC_STATUS_LIFECYCLE_MASK) == KMC_STATUS_BOOT_READY) {
            Serial.println(F("[BOOT] BOOT_READY received — sending DISABLE"));
            sendCmd(KMC_CMD_DISABLE);
            delay(CMD_DELAY_MS);
        }
    }
    printMenu();
}

void loop() {
    if (!Serial.available()) return;
    char c = Serial.read();
    while (Serial.available()) Serial.read();
    switch (c) {
        case 'A': case 'a': runAllAuto();        break;
        case 'S': case 's': testBusScan();       break;
        case '1':           testIdentity();      break;
        case '2':           testDataPacket();    break;
        case '3':           testSetValue();      break;
        case '4':           testIntensity();     break;
        case '5':           testBulbTest();      break;
        case '6':           testSleepWake();     break;
        case '7':           testReset();         break;
        case '8':           testButtonCycle();   break;
        case '9':           testButtonToggle();  break;
        case '0':           testButtonFlash();   break;
        case 'B': case 'b': testEncoderPress();  break;
        case 'C': case 'c': testDisableEnable(); break;
        case 'D': case 'd': testAckFault();      break;
        case 'E': case 'e': testINTPin();        break;
        default: Serial.print(F("Unknown: ")); Serial.println(c); break;
    }
    Serial.println();
    printMenu();
}

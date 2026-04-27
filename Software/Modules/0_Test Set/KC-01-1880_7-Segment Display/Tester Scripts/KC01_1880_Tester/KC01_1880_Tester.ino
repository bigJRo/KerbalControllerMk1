/*
 * KC-01-1880  7-Segment Display Module — I2C Test Suite
 * Board:      Seeed XIAO RA4M1
 *
 * Acts as I2C controller to fully exercise the KC-01-1880 module
 * running KC01_1882_TestFirmware.ino.
 *
 * Wiring (Xiao RA4M1 pin → KC-01-1882 P1 connector):
 *   D4 (SDA) → SDA  (P1 pin 13)
 *   D5 (SCL) → SCL  (P1 pin 14)
 *   D6       → INT  (P1 pin 15)  [active LOW — add 10k pull-up to 3.3V]
 *   3.3V     → VCC  (P1 pin 4)
 *   GND      → GND  (P1 pin 3)
 *
 * Usage:
 *   Open Serial Monitor at 115200 baud.
 *   The menu reprints after each test.
 *   Send the letter shown to run that test.
 *
 * Libraries required (Xiao side):
 *   - KerbalModuleCommon
 *   - Wire (built-in)
 */

#include <Wire.h>
#include <KerbalModuleCommon.h>

// ── I2C ──────────────────────────────────────────────────────
#define MODULE_ADDR     0x2A
#define PACKET_SIZE     6        // K7SC data packet: 6 bytes
#define IDENTITY_SIZE   KMC_IDENTITY_SIZE   // 4 bytes

// ── INT pin ──────────────────────────────────────────────────
#define PIN_INT         6        // Xiao D6, active LOW from module

// ── Timing ───────────────────────────────────────────────────
#define CMD_DELAY_MS        100      // settle time after most commands
#define IDENTITY_DELAY_MS   20       // extra gap between write and read for GET_IDENTITY
#define SET_VALUE_WAIT_MS   300      // wait for INT after SET_VALUE / RESET
#define BULB_WAIT_MS        2200     // buttonsBulbTest blocks for 2000ms on module

// ─────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────

static void sendCmd(uint8_t cmd)
{
    Wire.beginTransmission(MODULE_ADDR);
    Wire.write(cmd);
    uint8_t err = Wire.endTransmission();
    if (err) {
        Serial.print(F("  [WARN] sendCmd 0x")); Serial.print(cmd, HEX);
        Serial.print(F(" TX error: ")); Serial.println(err);
    }
}

static void sendCmd1(uint8_t cmd, uint8_t b0)
{
    Wire.beginTransmission(MODULE_ADDR);
    Wire.write(cmd);
    Wire.write(b0);
    uint8_t err = Wire.endTransmission();
    if (err) {
        Serial.print(F("  [WARN] sendCmd1 0x")); Serial.print(cmd, HEX);
        Serial.print(F(" TX error: ")); Serial.println(err);
    }
}

static void sendCmd2(uint8_t cmd, uint8_t b0, uint8_t b1)
{
    Wire.beginTransmission(MODULE_ADDR);
    Wire.write(cmd);
    Wire.write(b0);
    Wire.write(b1);
    uint8_t err = Wire.endTransmission();
    if (err) {
        Serial.print(F("  [WARN] sendCmd2 0x")); Serial.print(cmd, HEX);
        Serial.print(F(" TX error: ")); Serial.println(err);
    }
}

// Read N bytes from module. Returns actual count received.
static uint8_t readBytes(uint8_t* buf, uint8_t n)
{
    uint8_t got = Wire.requestFrom((uint8_t)MODULE_ADDR, n);
    if (got != n) {
        Serial.print(F("  [WARN] requestFrom: asked ")); Serial.print(n);
        Serial.print(F(" got ")); Serial.println(got);
    }
    uint8_t i = 0;
    while (Wire.available() && i < n) buf[i++] = Wire.read();
    return got;
}

static bool intAsserted()
{
    return digitalRead(PIN_INT) == LOW;
}

// Wait up to timeoutMs for INT to assert. Returns true if it did.
static bool waitForINT(uint32_t timeoutMs)
{
    uint32_t start = millis();
    while (!intAsserted()) {
        if (millis() - start >= timeoutMs) return false;
        delay(5);
    }
    return true;
}

// Drain ALL pending INTs before waiting for user input.
static void drainINT()
{
    uint32_t start = millis();
    while (intAsserted() && millis() - start < 500) {
        uint8_t buf[PACKET_SIZE];
        readBytes(buf, PACKET_SIZE);
        delay(20);
    }
}

// Block until user presses Enter (sends any character) to proceed.
static void waitForUser(const char* prompt)
{
    Serial.print(F("  [>>>]  "));
    Serial.print(prompt);
    Serial.println(F(" — press Enter when ready"));
    while (!Serial.available()) {}
    while (Serial.available()) Serial.read();  // flush
}

static void printSeparator()
{
    Serial.println(F("─────────────────────────────────────────"));
}

static void printPass(const char* msg)
{
    Serial.print(F("  [PASS] "));
    Serial.println(msg);
}

static void printFail(const char* msg)
{
    Serial.print(F("  [FAIL] "));
    Serial.println(msg);
}

static void printInfo(const char* msg)
{
    Serial.print(F("  [INFO] "));
    Serial.println(msg);
}

// Block until user sends Y or N. Returns true for Y.
static bool askYN(const char* question)
{
    Serial.print(F("  [Y/N]  "));
    Serial.print(question);
    Serial.print(F(" (Y/N): "));
    while (true) {
        while (!Serial.available()) {}
        char c = Serial.read();
        while (Serial.available()) Serial.read();  // flush
        if (c == 'Y' || c == 'y') { Serial.println(F("Y")); return true;  }
        if (c == 'N' || c == 'n') { Serial.println(F("N")); return false; }
    }
}

// ─────────────────────────────────────────────────────────────
//  Individual tests
// ─────────────────────────────────────────────────────────────

// ── T00: I2C Bus Scan ─────────────────────────────────────────
void testBusScan()
{
    printSeparator();
    Serial.println(F("T00: I2C Bus Scan"));
    Serial.println(F("  Scanning 0x01-0x7F..."));

    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            Serial.print(F("  Found device at 0x"));
            if (addr < 16) Serial.print(F("0"));
            Serial.print(addr, HEX);
            if (addr == MODULE_ADDR) Serial.print(F(" <-- MODULE"));
            Serial.println();
            found++;
        }
    }

    if (found == 0) {
        printFail("No I2C devices found — check wiring and pull-ups");
    } else {
        bool moduleFound = false;
        // Re-check module specifically
        Wire.beginTransmission(MODULE_ADDR);
        moduleFound = (Wire.endTransmission() == 0);
        moduleFound
            ? printPass("Module found at 0x2A")
            : printFail("Module NOT found at 0x2A — check address and wiring");
    }
}
void testIdentity()
{
    printSeparator();
    Serial.println(F("T01: GET_IDENTITY"));

    sendCmd(KMC_CMD_GET_IDENTITY);
    delay(IDENTITY_DELAY_MS);  // allow module ISR to set _pendingResponse

    uint8_t buf[IDENTITY_SIZE];
    uint8_t got = readBytes(buf, IDENTITY_SIZE);

    if (got != IDENTITY_SIZE) {
        printFail("Did not receive 4 bytes");
        Serial.print(F("  Got: ")); Serial.println(got);
        return;
    }

    Serial.print(F("  Type ID  : 0x"));  Serial.print(buf[0], HEX);
    Serial.print(buf[0] == KMC_TYPE_GPWS_INPUT ? F(" (GPWS_INPUT OK)") : F(" (UNEXPECTED)"));
    Serial.println();
    Serial.print(F("  FW Major : "));    Serial.println(buf[1]);
    Serial.print(F("  FW Minor : "));    Serial.println(buf[2]);
    Serial.print(F("  Cap Flags: 0b"));  Serial.println(buf[3], BIN);

    bool typeOk = (buf[0] == KMC_TYPE_GPWS_INPUT);
    bool capOk  = (buf[3] & KMC_CAP_DISPLAY);

    typeOk ? printPass("Type ID correct") : printFail("Type ID wrong");
    capOk  ? printPass("CAP_DISPLAY flag set") : printFail("CAP_DISPLAY flag missing");
}

// ── T02: Data packet read (INT-driven) ───────────────────────
void testDataPacket()
{
    printSeparator();
    Serial.println(F("T02: Data Packet Read (encoder turn required)"));
    sendCmd(KMC_CMD_RESET);
    delay(CMD_DELAY_MS);
    drainINT();
    waitForUser("Turn the encoder a few clicks then wait");
    drainINT();
    printInfo("Waiting for INT (5s timeout)...");

    bool got = waitForINT(5000);
    if (!got) {
        printFail("INT did not assert within 5 seconds");
        return;
    }
    printPass("INT asserted (LOW)");

    uint8_t buf[PACKET_SIZE] = {0};
    uint8_t n = readBytes(buf, PACKET_SIZE);

    if (n != PACKET_SIZE) {
        printFail("Wrong packet length");
        return;
    }

    uint16_t dispVal = ((uint16_t)buf[4] << 8) | buf[5];

    Serial.print(F("  Events     : 0b")); Serial.println(buf[0], BIN);
    Serial.print(F("  ChangeMask : 0b")); Serial.println(buf[1], BIN);
    Serial.print(F("  StateByte  : 0b")); Serial.println(buf[2], BIN);
    Serial.print(F("  Reserved   : "));   Serial.println(buf[3]);
    Serial.print(F("  Display    : "));   Serial.println(dispVal);

    printPass("Packet received and parsed");

    if (!intAsserted()) {
        printPass("INT deasserted after read");
    } else {
        printFail("INT still asserted after read");
    }
}

// ── T03: SET_VALUE ────────────────────────────────────────────
void testSetValue()
{
    printSeparator();
    Serial.println(F("T03: CMD_SET_VALUE"));

    uint16_t testVals[] = { 1234, 9999, 0, 42 };
    bool allOk = true;

    for (uint8_t i = 0; i < 4; i++) {
        uint16_t v = testVals[i];
        sendCmd2(KMC_CMD_SET_VALUE, (uint8_t)(v >> 8), (uint8_t)(v & 0xFF));
        delay(CMD_DELAY_MS);

        // Read packet to confirm display value echoed back
        // We need to trigger a read — encoder value change sets INT
        // Wait briefly then read directly (INT may already be set from
        // the encoder value change caused by SET_VALUE)
        if (waitForINT(SET_VALUE_WAIT_MS)) {
            uint8_t buf[PACKET_SIZE] = {0};
            readBytes(buf, PACKET_SIZE);
            uint16_t got = ((uint16_t)buf[4] << 8) | buf[5];
            if (got == v) {
                Serial.print(F("  SET ")); Serial.print(v);
                Serial.print(F(" → READ ")); Serial.print(got);
                Serial.println(F(" [PASS]"));
            } else {
                Serial.print(F("  SET ")); Serial.print(v);
                Serial.print(F(" → READ ")); Serial.print(got);
                Serial.println(F(" [FAIL]"));
                allOk = false;
            }
        } else {
            Serial.print(F("  SET ")); Serial.print(v);
            Serial.println(F(" → No INT response [FAIL]"));
            allOk = false;
        }
    }

    allOk ? printPass("All SET_VALUE values confirmed") : printFail("Some values mismatched");
}

// ── T04: Display intensity ────────────────────────────────────
void testIntensity()
{
    printSeparator();
    Serial.println(F("T04: Brightness — display + NeoPixel 5-level sweep"));
    printInfo("Display shows 8888. NeoPixels lit. Both step dimmest→brightest.");

    // SET_BRIGHTNESS byte: top nibble → display intensity (0-15)
    //                      full byte  → NeoPixel brightness (0-255)
    // We use values where top nibble and full byte both represent the level:
    //   0x00=0, 0x33=3/51, 0x77=7/119, 0xBB=11/187, 0xFF=15/255
    const uint8_t brightBytes[5] = { 0x00, 0x33, 0x77, 0xBB, 0xFF };
    const char*   labels[5]      = { "0 (dimmest)", "3", "7 (mid)", "11", "15 (brightest)" };

    // Set display to 8888 for maximum segment coverage
    sendCmd2(KMC_CMD_SET_VALUE, 0x22, 0xB8);
    delay(CMD_DELAY_MS);
    drainINT();

    bool allOk = true;
    for (uint8_t i = 0; i < 5; i++) {
        sendCmd1(KMC_CMD_SET_BRIGHTNESS, brightBytes[i]);
        delay(CMD_DELAY_MS);
        Serial.print(F("  Level ")); Serial.println(labels[i]);
        if (!askYN("Is the brightness change visible on both display and NeoPixels?"))
            allOk = false;
    }

    // Restore default brightness
    sendCmd1(KMC_CMD_SET_BRIGHTNESS, 0x77);
    delay(CMD_DELAY_MS);
    drainINT();

    allOk ? printPass("All 5 brightness levels confirmed on display and NeoPixels")
          : printFail("One or more brightness levels had issues");
}

// ── T05: BULB_TEST ────────────────────────────────────────────
void testBulbTest()
{
    printSeparator();
    Serial.println(F("T05: CMD_BULB_TEST"));
    printInfo("All NeoPixels → white, all display segments on for 2 seconds...");

    sendCmd(KMC_CMD_BULB_TEST);
    delay(2500);  // wait for module to complete its 2s bulb test and restore

    askYN("Did all 7-segment segments light up during the test?")
        ? printPass("Display bulb test OK") : printFail("Display bulb test issue reported");
    askYN("Did all 3 NeoPixels light white during the test?")
        ? printPass("NeoPixel bulb test OK") : printFail("NeoPixel bulb test issue reported");
}

// ── T06: SLEEP / WAKE ─────────────────────────────────────────
void testSleepWake()
{
    printSeparator();
    Serial.println(F("T06: CMD_SLEEP / CMD_WAKE"));

    printInfo("Sending SLEEP — display and NeoPixels should go dark");
    sendCmd(KMC_CMD_SLEEP);
    delay(1000);
    askYN("Is the display off and NeoPixels off?")
        ? printPass("SLEEP visual OK") : printFail("SLEEP visual issue reported");

    printInfo("Sending WAKE — display and NeoPixels should restore");
    sendCmd(KMC_CMD_WAKE);
    delay(CMD_DELAY_MS);
    askYN("Is the display restored and NeoPixels lit?")
        ? printPass("WAKE visual OK") : printFail("WAKE visual issue reported");

    // Verify INT does not assert during sleep
    sendCmd(KMC_CMD_SLEEP);
    delay(CMD_DELAY_MS);
    bool intDuringSleep = intAsserted();
    sendCmd(KMC_CMD_WAKE);
    delay(CMD_DELAY_MS);

    intDuringSleep ? printFail("INT asserted during sleep") : printPass("INT suppressed during sleep");
    printPass("Sleep/Wake sequence complete");
}

// ── T07: RESET ────────────────────────────────────────────────
void testReset()
{
    printSeparator();
    Serial.println(F("T07: CMD_RESET"));

    // First set a non-zero state: toggle BTN02, set value to 500
    sendCmd2(KMC_CMD_SET_VALUE, 0x01, 0xF4);  // 500
    delay(CMD_DELAY_MS);

    printInfo("Display set to 500. Sending RESET...");
    sendCmd(KMC_CMD_RESET);
    delay(CMD_DELAY_MS);

    // Read packet to check display is 0
    // RESET triggers _valueChanged via encoderSetValue(0)
    if (waitForINT(SET_VALUE_WAIT_MS)) {
        uint8_t buf[PACKET_SIZE] = {0};
        readBytes(buf, PACKET_SIZE);
        uint16_t val = ((uint16_t)buf[4] << 8) | buf[5];
        val == 0 ? printPass("Display reset to 0") : printFail("Display not reset to 0");
    } else {
        printFail("No INT after RESET");
    }

    askYN("Are NeoPixels back to initial ENABLED state?")
        ? printPass("RESET visual OK") : printFail("RESET visual issue reported");
}

// ── T08: Button events — BTN01 CYCLE ─────────────────────────
void testButtonCycle()
{
    printSeparator();
    Serial.println(F("T08: BTN01 — CYCLE mode (manual press required)"));
    sendCmd(KMC_CMD_RESET);  // clear any stale snapshot from previous test
    delay(CMD_DELAY_MS);
    drainINT();
    printInfo("Press BTN01 three times. Watch LED: dim→green→amber→dim");
    waitForUser("Ready to start pressing BTN01");
    drainINT();
    printInfo("Waiting for 3 button events (10s timeout each)...");

    for (uint8_t press = 1; press <= 3; press++) {
        Serial.print(F("  Waiting for press ")); Serial.print(press); Serial.print(F(" of 3..."));
        bool got = waitForINT(10000);
        if (!got) { Serial.println(F(" TIMEOUT [FAIL]")); return; }

        uint8_t buf[PACKET_SIZE] = {0};
        readBytes(buf, PACKET_SIZE);
        drainINT();  // clear any residual INT before waiting for next press

        uint8_t events     = buf[0];
        uint8_t state      = buf[2];
        uint8_t btn01state = state & 0x03;

        Serial.print(F(" raw=0x")); Serial.print(buf[0], HEX);
        Serial.print(F(",0x")); Serial.print(buf[1], HEX);
        Serial.print(F(",0x")); Serial.print(buf[2], HEX);
        Serial.print(F(" events=0b")); Serial.print(events, BIN);
        Serial.print(F(" btn01state="));  Serial.print(btn01state);

        bool evtOk = (events & 0x01);
        evtOk ? Serial.println(F(" [PASS]")) : Serial.println(F(" [FAIL — BTN01 bit not set]"));
    }
    askYN("Did the LED cycle dim→green→amber→dim correctly?")
        ? printPass("BTN01 CYCLE visual OK") : printFail("BTN01 CYCLE visual issue reported");
}

// ── T09: Button events — BTN02 TOGGLE ────────────────────────
void testButtonToggle()
{
    printSeparator();
    Serial.println(F("T09: BTN02 — TOGGLE mode (manual press required)"));
    sendCmd(KMC_CMD_RESET);
    delay(CMD_DELAY_MS);
    drainINT();
    printInfo("Press BTN02 twice. Watch LED: dim→blue→dim");
    waitForUser("Ready to start pressing BTN02");
    drainINT();
    printInfo("Waiting for 2 button events (10s timeout each)...");

    for (uint8_t press = 1; press <= 2; press++) {
        Serial.print(F("  Waiting for press ")); Serial.print(press); Serial.print(F(" of 2..."));
        bool got = waitForINT(10000);
        if (!got) { Serial.println(F(" TIMEOUT [FAIL]")); return; }

        uint8_t buf[PACKET_SIZE] = {0};
        readBytes(buf, PACKET_SIZE);
        drainINT();  // clear residual INT before waiting for next press

        bool btn02active = (buf[2] >> 2) & 0x01;  // bit 2 of state byte
        Serial.print(F(" BTN02 active=")); Serial.print(btn02active);
        Serial.println((buf[0] & 0x02) ? F(" [PASS]") : F(" [FAIL — BTN02 bit not set]"));
    }
    askYN("Did the LED toggle dim→blue→dim correctly?")
        ? printPass("BTN02 TOGGLE visual OK") : printFail("BTN02 TOGGLE visual issue reported");
}

// ── T10: Button events — BTN03 FLASH ─────────────────────────
void testButtonFlash()
{
    printSeparator();
    Serial.println(F("T10: BTN03 — FLASH mode (manual press required)"));
    sendCmd(KMC_CMD_RESET);
    delay(CMD_DELAY_MS);
    drainINT();
    printInfo("Press BTN03. Watch LED: flashes red briefly then returns to dim");
    waitForUser("Ready to press BTN03");
    drainINT();
    printInfo("Waiting for button event (10s timeout)...");

    bool got = waitForINT(10000);
    if (!got) { printFail("Timeout — no INT"); return; }

    uint8_t buf[PACKET_SIZE] = {0};
    readBytes(buf, PACKET_SIZE);

    // Debug: print all raw bytes
    Serial.print(F("  Raw bytes: "));
    for (uint8_t i = 0; i < PACKET_SIZE; i++) {
        Serial.print(F("0x")); Serial.print(buf[i], HEX); Serial.print(F(" "));
    }
    Serial.println();

    bool btn03evt = (buf[0] >> 2) & 0x01;  // bit 2 of events
    btn03evt ? printPass("BTN03 event received") : printFail("BTN03 bit not set in events");
    askYN("Did the LED flash red then return to dim white?")
        ? printPass("BTN03 FLASH visual OK") : printFail("BTN03 FLASH visual issue reported");
}

// ── T11: ENCODER_PRESS (BTN_EN) ──────────────────────────────
void testEncoderPress()
{
    printSeparator();
    Serial.println(F("T11: BTN_EN — Encoder Button (manual press required)"));

    // First set a non-zero value so reset is visible on display
    sendCmd2(KMC_CMD_SET_VALUE, 0x01, 0x00);  // 256
    delay(CMD_DELAY_MS);
    drainINT();  // discard the INT from SET_VALUE
    printInfo("Display set to 256.");
    waitForUser("Press encoder button to reset display to 0");
    printInfo("Waiting for INT (10s timeout)...");

    bool got = waitForINT(10000);
    if (!got) { printFail("Timeout — no INT"); return; }

    uint8_t buf[PACKET_SIZE] = {0};
    readBytes(buf, PACKET_SIZE);

    uint16_t val = ((uint16_t)buf[4] << 8) | buf[5];
    bool btnEnEvt = (buf[0] >> 3) & 0x01;  // bit 3 of events

    btnEnEvt ? printPass("BTN_EN event in packet (bit 3)") : printFail("BTN_EN bit not set");
    (val == 0) ? printPass("Display reset to 0 by encoder press") : printFail("Display not 0 after encoder press");
}

// ── T12: DISABLE / ENABLE ────────────────────────────────────
void testDisableEnable()
{
    printSeparator();
    Serial.println(F("T12: CMD_DISABLE / CMD_ENABLE"));

    printInfo("Sending DISABLE...");
    sendCmd(KMC_CMD_DISABLE);
    delay(1000);
    askYN("Is the display off and NeoPixels off?")
        ? printPass("DISABLE visual OK") : printFail("DISABLE visual issue reported");

    printInfo("Sending ENABLE...");
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    askYN("Is the display restored and NeoPixels lit?")
        ? printPass("ENABLE visual OK") : printFail("ENABLE visual issue reported");

    printPass("DISABLE/ENABLE sequence complete");
}

// ── T13: ACK_FAULT ───────────────────────────────────────────
void testAckFault()
{
    printSeparator();
    Serial.println(F("T13: CMD_ACK_FAULT"));

    // Module has no fault tracking — should accept silently
    sendCmd(KMC_CMD_ACK_FAULT);
    delay(CMD_DELAY_MS);

    // Verify module still alive via identity
    sendCmd(KMC_CMD_GET_IDENTITY);
    delay(IDENTITY_DELAY_MS);
    uint8_t buf[IDENTITY_SIZE];
    uint8_t got = readBytes(buf, IDENTITY_SIZE);

    (got == IDENTITY_SIZE && buf[0] == KMC_TYPE_GPWS_INPUT)
        ? printPass("Module still alive after ACK_FAULT")
        : printFail("Module not responding after ACK_FAULT");
}

// ── T14: INT pin behaviour ────────────────────────────────────
void testINTPin()
{
    printSeparator();
    Serial.println(F("T14: INT Pin — Idle state check"));

    // After reset, no events pending — INT should be high
    sendCmd(KMC_CMD_RESET);
    delay(200);

    // Drain any pending read
    if (intAsserted()) {
        uint8_t buf[PACKET_SIZE];
        readBytes(buf, PACKET_SIZE);
        delay(50);
    }

    bool idleHigh = !intAsserted();
    idleHigh ? printPass("INT idle HIGH (deasserted)") : printFail("INT stuck LOW at idle");

    // Set a value to trigger encoder change → INT should assert
    sendCmd2(KMC_CMD_SET_VALUE, 0x00, 0x05);
    bool asserted = waitForINT(SET_VALUE_WAIT_MS);
    asserted ? printPass("INT asserts on value change") : printFail("INT did not assert on value change");

    if (asserted) {
        uint8_t buf[PACKET_SIZE];
        readBytes(buf, PACKET_SIZE);
        delay(50);
        bool cleared = !intAsserted();
        cleared ? printPass("INT clears after packet read") : printFail("INT not cleared after read");
    }
}

// ── T15: Run all automated tests ─────────────────────────────
void runAllAuto()
{
    testBusScan();
    drainINT();
    testIdentity();
    drainINT();
    testSetValue();
    drainINT();
    testIntensity();
    drainINT();
    testSleepWake();
    drainINT();
    testReset();
    drainINT();
    testDisableEnable();
    drainINT();
    testAckFault();
    drainINT();
    testINTPin();

    printSeparator();
    Serial.println(F("Auto tests complete."));
    Serial.println(F("Run T02, T08-T11 manually (require physical interaction)."));

    // Leave module in a clean state
    sendCmd(KMC_CMD_RESET);
    delay(CMD_DELAY_MS);
    drainINT();
}

// ─────────────────────────────────────────────────────────────
//  Menu
// ─────────────────────────────────────────────────────────────

void printMenu()
{
    printSeparator();
    Serial.println(F("KC-01-1880 Test Suite — Xiao RA4M1"));
    printSeparator();
    Serial.println(F("  A  Run all automated tests"));
    Serial.println(F("  S  T00: I2C Bus Scan"));
    Serial.println(F("  1  T01: GET_IDENTITY"));
    Serial.println(F("  2  T02: Data packet read  [manual: turn encoder]"));
    Serial.println(F("  3  T03: SET_VALUE"));
    Serial.println(F("  4  T04: Display intensity [visual]"));
    Serial.println(F("  5  T05: BULB_TEST         [visual]"));
    Serial.println(F("  6  T06: SLEEP / WAKE      [visual]"));
    Serial.println(F("  7  T07: RESET"));
    Serial.println(F("  8  T08: BTN01 CYCLE       [manual: press 3x]"));
    Serial.println(F("  9  T09: BTN02 TOGGLE      [manual: press 2x]"));
    Serial.println(F("  0  T10: BTN03 FLASH       [manual: press 1x]"));
    Serial.println(F("  B  T11: Encoder button    [manual: press BTN_EN]"));
    Serial.println(F("  C  T12: DISABLE / ENABLE  [visual]"));
    Serial.println(F("  D  T13: ACK_FAULT"));
    Serial.println(F("  E  T14: INT pin behaviour"));
    printSeparator();
    Serial.println(F("Send letter/number to run test:"));
}

// ─────────────────────────────────────────────────────────────
//  setup / loop
// ─────────────────────────────────────────────────────────────

void setup()
{
    Serial.begin(115200);
    while (!Serial) delay(10);

    Wire.begin();
    Wire.setClock(100000);  // 100 kHz — more reliable with ATtiny816 TWI

    pinMode(PIN_INT, INPUT_PULLUP);  // INT is active LOW open-drain

    delay(500);  // let module finish boot self-test
    printMenu();
}

void loop()
{
    if (!Serial.available()) return;

    char c = Serial.read();
    // Flush any trailing newline
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
        default:
            Serial.print(F("Unknown command: ")); Serial.println(c);
            break;
    }

    Serial.println();
    printMenu();
}

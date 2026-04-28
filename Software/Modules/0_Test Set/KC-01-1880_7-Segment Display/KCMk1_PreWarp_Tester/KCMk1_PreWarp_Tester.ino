/**
 * @file        KCMk1_PreWarp_Controller.ino
 * @version     2.0.0
 * @date        2026-04-28
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 * @target      Xiao RA4M1
 *
 * @brief       Pre-Warp Time module controller / tester.
 *
 *              Acts as I2C master for the KC-01-1880 module running
 *              KCMk1_PreWarp_Time.ino. Handles the full lifecycle and
 *              prints module state to serial on every INT.
 *
 *              In production this logic is folded into the main
 *              KSP master controller. Here it runs standalone on
 *              a Xiao RA4M1 for development and validation.
 *
 * @note        Wiring (Xiao RA4M1 → KC-01-1882 P1 connector):
 *                D4 (SDA) → SDA (P1 pin 13)
 *                D5 (SCL) → SCL (P1 pin 14)
 *                D6       → INT (P1 pin 15) [10kΩ pull-up to 3.3V]
 *                3.3V     → VCC (P1 pin 4)
 *                GND      → GND (P1 pin 3)
 *
 *              Serial commands (115200 baud):
 *                Lifecycle:
 *                  E  Flight scene load  (ENABLE)
 *                  D  Flight scene exit  (DISABLE)
 *                  P  Game paused        (SLEEP)
 *                  U  Game resumed       (WAKE)
 *                  V  Vessel switch      (no action)
 *                  L  Simpit serial loss (DISABLE)
 *                  B  Master reboot      (DISABLE + re-init)
 *                  N  Full lifecycle demo (automated)
 *                Module commands:
 *                  S  Set value directly
 *                  I  Query identity
 *                  T  Bulb test
 *                  R  Reset module
 *                  ?  This menu
 */

#include <Wire.h>
#include <KerbalModuleCommon.h>

// ── Module ────────────────────────────────────────────────────
#define MODULE_ADDR     0x2B
#define PACKET_SIZE     8
#define IDENTITY_SIZE   4
#define PIN_INT         6

// ── Timing ───────────────────────────────────────────────────
#define CMD_DELAY_MS      50
#define DRAIN_TIMEOUT_MS  300

// ── Default value ─────────────────────────────────────────────
#define DEFAULT_VALUE   0

// ── Module state — mirrors current reported state ─────────────
struct PreWarpState {
    uint8_t lifecycle;
    uint8_t counter;
    int16_t value;      // pre-warp duration in minutes
};

static PreWarpState _prewarp = { KMC_STATUS_DISABLED, 0, DEFAULT_VALUE };
static bool _stateChanged = false;

// ── I2C helpers ───────────────────────────────────────────────

static void sendCmd(uint8_t cmd) {
    Wire.beginTransmission(MODULE_ADDR);
    Wire.write(cmd);
    Wire.endTransmission();
}

static void sendCmd1(uint8_t cmd, uint8_t b0) {
    Wire.beginTransmission(MODULE_ADDR);
    Wire.write(cmd);
    Wire.write(b0);
    Wire.endTransmission();
}

static void sendCmd2(uint8_t cmd, uint8_t hi, uint8_t lo) {
    Wire.beginTransmission(MODULE_ADDR);
    Wire.write(cmd);
    Wire.write(hi);
    Wire.write(lo);
    Wire.endTransmission();
}

static uint8_t readPacket(uint8_t* buf, uint8_t n) {
    uint8_t got = Wire.requestFrom((uint8_t)MODULE_ADDR, n);
    uint8_t i = 0;
    while (Wire.available() && i < n) buf[i++] = Wire.read();
    return got;
}

static bool intAsserted()          { return digitalRead(PIN_INT) == LOW; }

static bool waitForINT(uint32_t ms) {
    uint32_t t = millis();
    while (!intAsserted()) { if (millis() - t >= ms) return false; delay(5); }
    return true;
}

static void drainINT() {
    uint32_t t = millis();
    while (intAsserted() && millis() - t < DRAIN_TIMEOUT_MS) {
        uint8_t buf[PACKET_SIZE];
        readPacket(buf, PACKET_SIZE);
        delay(20);
    }
}

// ── Packet parsing ────────────────────────────────────────────

static const char* lifecycleStr(uint8_t lc) {
    switch (lc) {
        case KMC_STATUS_ACTIVE:     return "ACTIVE";
        case KMC_STATUS_SLEEPING:   return "SLEEPING";
        case KMC_STATUS_DISABLED:   return "DISABLED";
        case KMC_STATUS_BOOT_READY: return "BOOT_READY";
        default:                    return "?";
    }
}

static void printState() {
    Serial.println(F("─── Pre-Warp State ───────────────────────────"));
    Serial.print(F("  Lifecycle  : ")); Serial.println(lifecycleStr(_prewarp.lifecycle));
    Serial.print(F("  Counter    : ")); Serial.println(_prewarp.counter);
    Serial.print(F("  Value      : ")); Serial.print(_prewarp.value); Serial.println(F(" min"));
    Serial.println(F("──────────────────────────────────────────────"));
}

// Parse incoming packet, update _prewarp state.
// Returns true if BOOT_READY detected.
static bool parsePacket(const uint8_t* buf) {
    uint8_t lifecycle = buf[0] & KMC_STATUS_LIFECYCLE_MASK;
    bool    fault     = buf[0] & KMC_STATUS_FAULT;
    bool    dataChgd  = buf[0] & KMC_STATUS_DATA_CHANGED;

    PreWarpState prev = _prewarp;
    _prewarp.lifecycle = lifecycle;
    _prewarp.counter   = buf[2];
    _prewarp.value     = (int16_t)(((uint16_t)buf[6] << 8) | buf[7]);

    _stateChanged = dataChgd || (_prewarp.value != prev.value);

    if (fault) {
        Serial.println(F("[FAULT] Module hardware fault — sending ACK_FAULT"));
        sendCmd(KMC_CMD_ACK_FAULT);
    }

    return (lifecycle == KMC_STATUS_BOOT_READY);
}

static void readAndHandle() {
    uint8_t buf[PACKET_SIZE] = {0};
    readPacket(buf, PACKET_SIZE);
    bool bootReady = parsePacket(buf);

    if (bootReady) {
        Serial.println(F("[BOOT] Module BOOT_READY — sending DISABLE"));
        sendCmd(KMC_CMD_DISABLE);
        delay(CMD_DELAY_MS);
        return;
    }

    if (_stateChanged) {
        Serial.println(F("[INT] Pre-Warp state update:"));
        printState();
        // Production: forward value to Simpit here
    }
}

// ── Identity query ────────────────────────────────────────────

static void queryIdentity() {
    sendCmd(KMC_CMD_GET_IDENTITY);
    delay(50);
    uint8_t buf[IDENTITY_SIZE] = {0};
    readPacket(buf, IDENTITY_SIZE);
    Serial.println(F("\n─── Module Identity ──────────────────────────"));
    Serial.print(F("  Type ID    : 0x")); Serial.println(buf[0], HEX);
    Serial.print(F("  FW version : ")); Serial.print(buf[1]); Serial.print('.'); Serial.println(buf[2]);
    Serial.print(F("  Cap flags  : 0b")); Serial.println(buf[3], BIN);
    Serial.println(F("──────────────────────────────────────────────"));
}

// ── Lifecycle actions ─────────────────────────────────────────

static void doEnable() {
    Serial.println(F("\n[ENABLE] Flight scene active — enabling module"));
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    if (waitForINT(500)) readAndHandle();
    Serial.println(F("[ENABLE] Module active — buttons backlit, display on"));
}

static void doDisable() {
    Serial.println(F("\n[DISABLE] No valid game context — disabling module"));
    sendCmd(KMC_CMD_DISABLE);
    delay(CMD_DELAY_MS);
    Serial.println(F("[DISABLE] Module dark — waiting for flight scene"));
}

static void doSleep() {
    Serial.println(F("\n[SLEEP] Game paused — freezing module state"));
    sendCmd(KMC_CMD_SLEEP);
    delay(CMD_DELAY_MS);
    Serial.println(F("[SLEEP] Module frozen — INT suppressed, state preserved"));
}

static void doWake() {
    Serial.println(F("\n[WAKE] Game resumed — restoring module state"));
    sendCmd(KMC_CMD_WAKE);
    delay(CMD_DELAY_MS);
    if (waitForINT(500)) readAndHandle();
    Serial.println(F("[WAKE] Module resumed from exactly where it was"));
}

static void doVesselSwitch() {
    Serial.println(F("\n[VESSEL SWITCH] Vessel switched"));
    Serial.println(F("[VESSEL SWITCH] No action for Pre-Warp module — pilot configures as needed"));
}

static void doSerialLoss() {
    Serial.println(F("\n[SERIAL LOSS] Simpit connection lost — disabling module"));
    sendCmd(KMC_CMD_DISABLE);
    delay(CMD_DELAY_MS);
    Serial.println(F("[SERIAL LOSS] Module dark — will ENABLE on reconnect"));
}

static void doMasterReboot() {
    Serial.println(F("\n[REBOOT] Master rebooting — sending DISABLE"));
    sendCmd(KMC_CMD_DISABLE);
    delay(CMD_DELAY_MS);
    Serial.println(F("[REBOOT] All modules disabled — re-initialising..."));
    delay(500);
    queryIdentity();
    if (intAsserted()) {
        uint8_t buf[PACKET_SIZE] = {0};
        readPacket(buf, PACKET_SIZE);
        if (parsePacket(buf)) {
            Serial.println(F("[REBOOT] Module in BOOT_READY — sending DISABLE"));
            sendCmd(KMC_CMD_DISABLE);
            delay(CMD_DELAY_MS);
        }
    }
    Serial.println(F("[REBOOT] Re-init complete — send E when in flight scene"));
}

static void doBulbTest() {
    Serial.println(F("\n[BULB TEST] Starting — all LEDs and display on"));
    sendCmd1(KMC_CMD_BULB_TEST, 0x01);
    delay(2000);
    sendCmd1(KMC_CMD_BULB_TEST, 0x00);
    delay(CMD_DELAY_MS);
    drainINT();
    Serial.println(F("[BULB TEST] Complete"));
}

static void doSetValue() {
    Serial.print(F("\nEnter value (0-9999): "));
    uint32_t start = millis();
    String input = "";
    while (millis() - start < 10000) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '\r' || c == '\n') break;
            if (c >= '0' && c <= '9') input += c;
        }
    }
    if (input.length() == 0) { Serial.println(F("(cancelled)")); return; }
    int16_t val = (int16_t)input.toInt();
    if (val < 0)    val = 0;
    if (val > 9999) val = 9999;
    Serial.println(val);
    sendCmd2(KMC_CMD_SET_VALUE, (uint8_t)(val >> 8), (uint8_t)(val & 0xFF));
    delay(CMD_DELAY_MS);
    if (waitForINT(500)) readAndHandle();
}

// ── Full lifecycle demo ───────────────────────────────────────

static void doFullDemo() {
    Serial.println(F("\n══ Full Lifecycle Demo ══════════════════════"));
    Serial.println(F("  Boot → No Scene → Flight → Preset Use →"));
    Serial.println(F("  Vessel Switch → Pause → Resume → Exit"));
    Serial.println(F("  Watch the module at each step."));
    Serial.println();

    // Step 1: Boot
    Serial.println(F("── Step 1: Boot (module waiting for master)"));
    sendCmd(KMC_CMD_RESET);
    delay(CMD_DELAY_MS);
    if (waitForINT(500)) readAndHandle();
    sendCmd(KMC_CMD_DISABLE);
    delay(1000);
    Serial.println(F("   Module dark — boot complete, no flight scene yet"));
    Serial.println();

    // Step 2: No flight scene
    Serial.println(F("── Step 2: No flight scene (DISABLED)"));
    Serial.println(F("   Module dark, defaults, INT suppressed"));
    delay(2000);
    Serial.println();

    // Step 3: Flight scene loads
    Serial.println(F("── Step 3: Flight scene loaded (ENABLE)"));
    doEnable();
    delay(2000);
    Serial.println();

    // Step 4: Simulate pilot pressing 1 hour preset
    Serial.println(F("── Step 4: Pilot sets 1 hour preset (60 min via SET_VALUE)"));
    sendCmd2(KMC_CMD_SET_VALUE, 0x00, 0x3C);  // 60
    delay(CMD_DELAY_MS);
    if (waitForINT(500)) readAndHandle();
    Serial.println(F("   Value: 60 min"));
    delay(2000);
    Serial.println();

    // Step 5: Vessel switch
    Serial.println(F("── Step 5: Vessel switch"));
    doVesselSwitch();
    delay(2000);
    Serial.println();

    // Step 6: Pause
    Serial.println(F("── Step 6: Game paused (SLEEP)"));
    doSleep();
    delay(2000);
    Serial.println();

    // Step 7: Resume
    Serial.println(F("── Step 7: Game resumed (WAKE)"));
    doWake();
    delay(2000);
    Serial.println();

    // Step 8: Exit
    Serial.println(F("── Step 8: Flight scene exits (DISABLE)"));
    doDisable();
    delay(1000);
    Serial.println();

    Serial.println(F("══ Demo complete ════════════════════════════"));
    Serial.println(F("   Send E to re-enable when ready"));
}

// ── Menu ──────────────────────────────────────────────────────

static void printMenu() {
    Serial.println(F("\n══════════════════════════════════════════════"));
    Serial.println(F("  KCMk1 Pre-Warp Controller — Xiao RA4M1"));
    Serial.println(F("  (INT events print automatically)"));
    Serial.println(F("──────────────────────────────────────────────"));
    Serial.println(F("  Lifecycle:"));
    Serial.println(F("    E  Flight scene load  (ENABLE)"));
    Serial.println(F("    D  Flight scene exit  (DISABLE)"));
    Serial.println(F("    P  Game paused        (SLEEP)"));
    Serial.println(F("    U  Game resumed       (WAKE)"));
    Serial.println(F("    V  Vessel switch      (no action)"));
    Serial.println(F("    L  Simpit serial loss (DISABLE)"));
    Serial.println(F("    B  Master reboot      (DISABLE + re-init)"));
    Serial.println(F("    N  Full lifecycle demo (automated)"));
    Serial.println(F("──────────────────────────────────────────────"));
    Serial.println(F("  Module commands:"));
    Serial.println(F("    S  Set value (minutes)"));
    Serial.println(F("    I  Query identity"));
    Serial.println(F("    T  Bulb test"));
    Serial.println(F("    R  Reset module"));
    Serial.println(F("──────────────────────────────────────────────"));
    Serial.println(F("  ?  This menu"));
    Serial.println(F("══════════════════════════════════════════════"));
}

// ── setup / loop ──────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    uint32_t t = millis();
    while (!Serial && millis() - t < 3000) delay(10);
    delay(100);

    Wire.begin();
    Wire.setClock(100000);
    pinMode(PIN_INT, INPUT_PULLUP);

    Serial.println(F("\nKCMk1 Pre-Warp Controller — starting up"));

    queryIdentity();

    if (waitForINT(1000)) {
        uint8_t buf[PACKET_SIZE] = {0};
        readPacket(buf, PACKET_SIZE);
        bool bootReady = parsePacket(buf);
        if (bootReady) {
            Serial.println(F("[BOOT] Module BOOT_READY — sending DISABLE"));
        } else {
            Serial.println(F("[INIT] Module already running — sending DISABLE"));
        }
    } else {
        Serial.println(F("[INIT] No startup INT — sending DISABLE"));
    }

    sendCmd(KMC_CMD_DISABLE);
    delay(CMD_DELAY_MS);
    drainINT();

    Serial.println(F("[INIT] Module disabled — send E when in flight scene"));
    printMenu();
}

void loop() {
    if (intAsserted()) readAndHandle();

    if (!Serial.available()) return;
    char c = Serial.read();
    while (Serial.available()) Serial.read();

    switch (c) {
        case 'E': case 'e': doEnable();        break;
        case 'D': case 'd': doDisable();       break;
        case 'P': case 'p': doSleep();         break;
        case 'U': case 'u': doWake();          break;
        case 'V': case 'v': doVesselSwitch();  break;
        case 'L': case 'l': doSerialLoss();    break;
        case 'B': case 'b': doMasterReboot();  break;
        case 'N': case 'n': doFullDemo();      break;
        case 'S': case 's': doSetValue();      break;
        case 'I': case 'i': queryIdentity();   break;
        case 'T': case 't': doBulbTest();      break;
        case 'R': case 'r':
            Serial.println(F("\n[RESET] Resetting module to defaults"));
            sendCmd(KMC_CMD_RESET);
            delay(CMD_DELAY_MS);
            if (waitForINT(500)) readAndHandle();
            break;
        case '?': printMenu(); break;
        default:
            Serial.print(F("Unknown: ")); Serial.println(c);
            break;
    }
}

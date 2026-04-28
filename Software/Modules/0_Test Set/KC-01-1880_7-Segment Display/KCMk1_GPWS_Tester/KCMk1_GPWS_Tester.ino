/*
 * KCMk1_GPWS_Controller.ino
 * Board:  Seeed Xiao RA4M1
 * Target: KC-01-1880 running KCMk1_GPWS_Input.ino
 *
 * Master controller simulation for the GPWS Input Panel module.
 * Handles the full lifecycle of module interaction — boot handshake,
 * flight scene transitions, vessel switches, pause/resume, and
 * serial loss. Prints GPWS state to serial on every INT.
 *
 * Wiring (Xiao RA4M1 → KC-01-1882 P1 connector):
 *   D4 (SDA) → SDA  (P1 pin 13)
 *   D5 (SCL) → SCL  (P1 pin 14)
 *   D6       → INT  (P1 pin 15)  [active LOW — 10k pull-up to 3.3V]
 *   3.3V     → VCC  (P1 pin 4)
 *   GND      → GND  (P1 pin 3)
 *
 * Serial commands (115200 baud):
 *
 *   Lifecycle:
 *     E  — Flight scene load    (ENABLE)
 *     D  — Flight scene exit    (DISABLE)
 *     P  — Game paused          (SLEEP)
 *     U  — Game resumed         (WAKE)
 *     V  — Vessel switch        (RESET + restore cached threshold)
 *     L  — Serial/Simpit loss   (DISABLE)
 *     B  — Master reboot        (DISABLE + re-init)
 *     N  — Full lifecycle demo  (automated sequence)
 *
 *   Module commands:
 *     S  — Set display value
 *     I  — Query identity
 *     T  — Bulb test
 *     R  — Reset module
 *
 *   Utilities:
 *     ?  — This menu
 *
 * Libraries required:
 *   KerbalModuleCommon
 *   Wire (built-in)
 */

#include <Wire.h>
#include <KerbalModuleCommon.h>

// ── Module ───────────────────────────────────────────────────
#define GPWS_ADDR        0x2A
#define PACKET_SIZE      8
#define IDENTITY_SIZE    4
#define PIN_INT          6

// ── Timing ───────────────────────────────────────────────────
#define CMD_DELAY_MS     50
#define DRAIN_TIMEOUT_MS 300

// ── GPWS defaults ────────────────────────────────────────────
#define GPWS_DEFAULT_ALT 200


// ============================================================
//  GPWS state — mirrors the module's current reported state
// ============================================================

struct GPWSState {
    uint8_t  gpwsMode;       // 0=off, 1=full GPWS, 2=proximity only
    bool     proximityAlarm; // BTN02
    bool     rdvRadar;       // BTN03
    int16_t  altThreshold;   // metres (signed per protocol)
    uint8_t  lifecycle;      // last seen lifecycle nibble
    uint8_t  counter;        // last seen transaction counter
};

static GPWSState _gpws = { 0, false, false, GPWS_DEFAULT_ALT,
                            KMC_STATUS_DISABLED, 0 };
static bool _stateChanged = false;

// ============================================================
//  I2C helpers
// ============================================================

static void sendCmd(uint8_t cmd) {
    Wire.beginTransmission(GPWS_ADDR);
    Wire.write(cmd);
    Wire.endTransmission();
}

static void sendCmd1(uint8_t cmd, uint8_t b0) {
    Wire.beginTransmission(GPWS_ADDR);
    Wire.write(cmd);
    Wire.write(b0);
    Wire.endTransmission();
}

static void sendCmd2(uint8_t cmd, uint8_t hi, uint8_t lo) {
    Wire.beginTransmission(GPWS_ADDR);
    Wire.write(cmd);
    Wire.write(hi);
    Wire.write(lo);
    Wire.endTransmission();
}

static uint8_t readPacket(uint8_t* buf, uint8_t n) {
    uint8_t got = Wire.requestFrom((uint8_t)GPWS_ADDR, n);
    uint8_t i = 0;
    while (Wire.available() && i < n) buf[i++] = Wire.read();
    return got;
}

static bool intAsserted() { return digitalRead(PIN_INT) == LOW; }

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

// ============================================================
//  Packet parsing and display
// ============================================================

static const char* lifecycleStr(uint8_t lc) {
    switch (lc) {
        case KMC_STATUS_ACTIVE:     return "ACTIVE";
        case KMC_STATUS_SLEEPING:   return "SLEEPING";
        case KMC_STATUS_DISABLED:   return "DISABLED";
        case KMC_STATUS_BOOT_READY: return "BOOT_READY";
        default:                    return "?";
    }
}

static const char* gpwsModeStr(uint8_t mode) {
    switch (mode) {
        case 0:  return "OFF";
        case 1:  return "ACTIVE (full GPWS)";
        case 2:  return "PROXIMITY TONE ONLY";
        default: return "?";
    }
}

static void printState() {
    Serial.println(F("─── GPWS State ───────────────────────────────"));
    Serial.print(F("  Lifecycle  : ")); Serial.println(lifecycleStr(_gpws.lifecycle));
    Serial.print(F("  Counter    : ")); Serial.println(_gpws.counter);
    Serial.print(F("  GPWS mode  : ")); Serial.println(gpwsModeStr(_gpws.gpwsMode));
    Serial.print(F("  Prox alarm : ")); Serial.println(_gpws.proximityAlarm ? F("ENABLED") : F("off"));
    Serial.print(F("  RDV radar  : ")); Serial.println(_gpws.rdvRadar       ? F("ENABLED") : F("off"));
    Serial.print(F("  Threshold  : ")); Serial.print(_gpws.altThreshold); Serial.println(F(" m"));
    Serial.println(F("──────────────────────────────────────────────"));
}

// Parse packet, update _gpws, return true if lifecycle event needs handling
static bool parsePacket(const uint8_t* buf) {
    uint8_t lifecycle = buf[0] & KMC_STATUS_LIFECYCLE_MASK;
    bool    fault     = buf[0] & KMC_STATUS_FAULT;
    bool    dataChgd  = buf[0] & KMC_STATUS_DATA_CHANGED;
    uint8_t state     = buf[5];

    GPWSState prev = _gpws;
    _gpws.lifecycle     = lifecycle;
    _gpws.counter       = buf[2];
    _gpws.gpwsMode      = state & 0x03;
    _gpws.proximityAlarm = (state >> 2) & 0x01;
    _gpws.rdvRadar      = (state >> 3) & 0x01;
    _gpws.altThreshold  = (int16_t)(((uint16_t)buf[6] << 8) | buf[7]);

    _stateChanged = dataChgd || (
        _gpws.gpwsMode       != prev.gpwsMode      ||
        _gpws.proximityAlarm != prev.proximityAlarm ||
        _gpws.rdvRadar       != prev.rdvRadar       ||
        _gpws.altThreshold   != prev.altThreshold
    );

    if (fault) {
        Serial.println(F("[FAULT] Module hardware fault — sending ACK_FAULT"));
        sendCmd(KMC_CMD_ACK_FAULT);
    }

    return (lifecycle == KMC_STATUS_BOOT_READY);
}

// Read one packet, parse it, handle BOOT_READY automatically
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
        Serial.println(F("[INT] GPWS state update:"));
        printState();
        // Cache threshold whenever pilot changes it
        // Production: send to Simpit here
    }
}

// ============================================================
//  Identity query
// ============================================================

static void queryIdentity() {
    sendCmd(KMC_CMD_GET_IDENTITY);
    delay(50);  // ATtiny816 needs time to process and queue response
    uint8_t buf[IDENTITY_SIZE] = {0};
    readPacket(buf, IDENTITY_SIZE);
    Serial.println(F("\n─── Module Identity ──────────────────────────"));
    Serial.print(F("  Type ID    : 0x")); Serial.println(buf[0], HEX);
    Serial.print(F("  FW version : ")); Serial.print(buf[1]); Serial.print(F(".")); Serial.println(buf[2]);
    Serial.print(F("  Cap flags  : 0b")); Serial.println(buf[3], BIN);
    Serial.println(F("──────────────────────────────────────────────"));
}

// ============================================================
//  Set value interactively
// ============================================================

static void doSetValue() {
    Serial.print(F("\nEnter threshold value (0-9999): "));
    while (!Serial.available()) {}
    uint16_t val = (uint16_t)Serial.parseInt();
    while (Serial.available()) Serial.read();
    if (val > 9999) val = 9999;
    sendCmd2(KMC_CMD_SET_VALUE, (uint8_t)(val >> 8), (uint8_t)(val & 0xFF));
    delay(CMD_DELAY_MS);
    // Read and parse the module's confirmation packet
    if (waitForINT(500)) {
        readAndHandle();
    } else {
        Serial.print(F("  Threshold set to ")); Serial.print(val);
        Serial.println(F(" m (no confirmation from module)"));
    }
}

// ============================================================
//  Lifecycle command handlers
// ============================================================

static void doEnable() {
    Serial.println(F("\n[ENABLE] Flight scene active — enabling module"));
    sendCmd(KMC_CMD_ENABLE);
    delay(CMD_DELAY_MS);
    // Module sends current state on ENABLE — read and parse it
    if (waitForINT(500)) readAndHandle();
    Serial.println(F("[ENABLE] Module active — buttons backlit, awaiting pilot"));
}

static void doDisable() {
    Serial.println(F("\n[DISABLE] No valid game context — disabling module"));
    sendCmd(KMC_CMD_DISABLE);
    delay(CMD_DELAY_MS);
    Serial.println(F("[DISABLE] Module dark — waiting for flight scene"));
}

static void doSleep() {
    Serial.println(F("\n[SLEEP] Game paused / alt-tab — freezing module state"));
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
    Serial.println(F("[VESSEL SWITCH] No action for GPWS module — pilot configures as needed"));
}

static void doSerialLoss() {
    Serial.println(F("\n[SERIAL LOSS] Simpit connection lost — disabling module"));
    sendCmd(KMC_CMD_DISABLE);
    delay(CMD_DELAY_MS);
    Serial.println(F("[SERIAL LOSS] Module dark — master will ENABLE on reconnect"));
}

static void doMasterReboot() {
    Serial.println(F("\n[REBOOT] Master controller rebooting — sending DISABLE"));
    sendCmd(KMC_CMD_DISABLE);
    delay(CMD_DELAY_MS);
    Serial.println(F("[REBOOT] All modules disabled — re-initialising..."));
    delay(500);

    // Re-init: scan bus, check identity, handle any BOOT_READY
    queryIdentity();
    if (intAsserted()) {
        uint8_t buf[PACKET_SIZE] = {0};
        readPacket(buf, PACKET_SIZE);
        bool bootReady = parsePacket(buf);
        if (bootReady) {
            Serial.println(F("[REBOOT] Module in BOOT_READY — sending DISABLE"));
            sendCmd(KMC_CMD_DISABLE);
            delay(CMD_DELAY_MS);
        }
    }
    Serial.println(F("[REBOOT] Re-init complete — send E to ENABLE when in flight scene"));
}

static void doBulbTest() {
    Serial.println(F("\n[BULB TEST] Starting — all LEDs and display on"));
    sendCmd1(KMC_CMD_BULB_TEST, 0x01);   // start
    delay(2000);
    sendCmd1(KMC_CMD_BULB_TEST, 0x00);   // stop
    delay(CMD_DELAY_MS);
    drainINT();
    Serial.println(F("[BULB TEST] Complete"));
}

// ============================================================
//  Full lifecycle demo sequence
// ============================================================

static void doFullDemo() {
    Serial.println(F("\n══ Full Lifecycle Demo ══════════════════════"));
    Serial.println(F("  Walks through: Boot → No Scene → Flight →"));
    Serial.println(F("  Vessel Switch → Pause → Resume → Exit"));
    Serial.println(F("  Watch the module at each step."));
    Serial.println();

    // Step 1: Boot state (simulate with RESET)
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

    // Step 4: Set a threshold to simulate pilot interaction
    Serial.println(F("── Step 4: Pilot sets threshold to 350m"));
    sendCmd2(KMC_CMD_SET_VALUE, 0x01, 0x5E);  // 350
    delay(CMD_DELAY_MS);
    if (waitForINT(500)) readAndHandle();
    else Serial.println(F("   (no INT — GPWS off, threshold stored internally)"));
    Serial.println(F("   Threshold cached: 350m"));
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

    // Step 8: Flight scene exits
    Serial.println(F("── Step 8: Flight scene exits (DISABLE)"));
    doDisable();
    delay(1000);
    Serial.println();

    Serial.println(F("══ Demo complete ════════════════════════════"));
    Serial.println(F("   Send E to re-enable when ready"));
}

// ============================================================
//  Menu
// ============================================================

static void printMenu() {
    Serial.println(F("\n══════════════════════════════════════════════"));
    Serial.println(F("  KCMk1 GPWS Controller — Xiao RA4M1"));
    Serial.println(F("  (INT events print automatically)"));
    Serial.println(F("──────────────────────────────────────────────"));
    Serial.println(F("  Lifecycle:"));
    Serial.println(F("    E  Flight scene load    (ENABLE)"));
    Serial.println(F("    D  Flight scene exit    (DISABLE)"));
    Serial.println(F("    P  Game paused          (SLEEP)"));
    Serial.println(F("    U  Game resumed         (WAKE)"));
    Serial.println(F("    V  Vessel switch        (module-specific)"));
    Serial.println(F("    L  Simpit serial loss   (DISABLE)"));
    Serial.println(F("    B  Master reboot        (DISABLE + re-init)"));
    Serial.println(F("    N  Full lifecycle demo  (automated)"));
    Serial.println(F("──────────────────────────────────────────────"));
    Serial.println(F("  Module commands:"));
    Serial.println(F("    S  Set threshold value"));
    Serial.println(F("    I  Query identity"));
    Serial.println(F("    T  Bulb test"));
    Serial.println(F("    R  Reset module"));
    Serial.println(F("──────────────────────────────────────────────"));
    Serial.println(F("  ?  This menu"));
    Serial.println(F("══════════════════════════════════════════════"));
}

// ============================================================
//  setup / loop
// ============================================================

void setup() {
    Serial.begin(115200);
    uint32_t t = millis();
    while (!Serial && millis() - t < 3000) delay(10);
    delay(100);

    Wire.begin();
    Wire.setClock(100000);
    pinMode(PIN_INT, INPUT_PULLUP);

    Serial.println(F("\nKCMk1 GPWS Controller — starting up"));

    // Query identity to confirm module is present
    queryIdentity();

    // Handle BOOT_READY if module just powered on.
    // If we missed the INT (module booted before serial connected),
    // send DISABLE anyway — it's always the correct initial state.
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
    // Auto-handle any INT from module
    if (intAsserted()) {
        readAndHandle();
    }

    if (!Serial.available()) return;

    char c = Serial.read();
    while (Serial.available()) Serial.read();

    switch (c) {
        // Lifecycle
        case 'E': case 'e': doEnable();        break;
        case 'D': case 'd': doDisable();       break;
        case 'P': case 'p': doSleep();         break;
        case 'U': case 'u': doWake();          break;
        case 'V': case 'v': doVesselSwitch();  break;
        case 'L': case 'l': doSerialLoss();    break;
        case 'B': case 'b': doMasterReboot();  break;
        case 'N': case 'n': doFullDemo();      break;
        // Module commands
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

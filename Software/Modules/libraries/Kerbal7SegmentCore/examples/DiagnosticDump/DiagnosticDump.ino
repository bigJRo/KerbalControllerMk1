/**
 * @file        DiagnosticDump.ino
 * @version     1.1.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Kerbal7SegmentCore serial diagnostic — no I2C master required.
 *
 *              Outputs to Serial at 115200 baud:
 *                - Display value changes from encoder
 *                - Button press events (BTN01/02/03/BTN_EN)
 *                - Module state byte on any change
 *                - Full state dump every DUMP_INTERVAL_MS
 *
 *              Runs display and NeoPixel bulb test at startup.
 *              Useful for verifying hardware after assembly.
 *
 * @note        Hardware: KC-01-1881/1882 v2.0 (ATtiny816)
 *              IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             20 MHz internal
 *                tinyNeoPixel Port: Port C
 *                Programmer:        serialUPDI
 */

#include <KerbalModuleCommon.h>
#include <Kerbal7SegmentCore.h>

#define I2C_ADDRESS       0x2A
#define SERIAL_BAUD       115200
#define DUMP_INTERVAL_MS  2000
#define DEFAULT_VALUE     0

const ButtonConfig btnConfigs[K7SC_NEO_COUNT] = {
    { BTN_MODE_CYCLE,  { K7SC_ENABLED_COLOR, toGRBW(KMC_GREEN), toGRBW(KMC_AMBER) }, 3, 0 },
    { BTN_MODE_TOGGLE, { K7SC_ENABLED_COLOR, toGRBW(KMC_BLUE),  K7SC_OFF          }, 2, 0 },
    { BTN_MODE_FLASH,  { toGRBW(KMC_RED),   K7SC_OFF,           K7SC_OFF          }, 0, 300 },
};

static uint16_t _lastValue    = DEFAULT_VALUE;
static uint8_t  _lastState    = 0;
static uint32_t _lastDumpTime = 0;

static void dumpState() {
    uint16_t val   = displayGetValue();
    uint8_t  state = buttonsGetStateByte();
    Serial.println(F("\n=== State Dump ==="));
    Serial.print(F("Display value  : ")); Serial.println(val);
    Serial.print(F("BTN01 cycle    : ")); Serial.println(state & K7SC_STATE_BTN01_MASK);
    Serial.print(F("BTN02 active   : ")); Serial.println((state >> K7SC_STATE_BTN02_BIT) & 0x01);
    Serial.print(F("BTN03 active   : ")); Serial.println((state >> K7SC_STATE_BTN03_BIT) & 0x01);
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(500);

    Serial.println(F("\nKerbal7SegmentCore Diagnostic"));
    Serial.println(F("=============================="));
    Serial.print(F("Firmware: "));
    Serial.print(K7SC_FIRMWARE_MAJOR); Serial.print(F("."));
    Serial.println(K7SC_FIRMWARE_MINOR);

    k7scBegin(I2C_ADDRESS, KMC_TYPE_GPWS_INPUT,
              KMC_CAP_DISPLAY, btnConfigs, DEFAULT_VALUE);

    Serial.println(F("Running bulb test..."));
    buttonsBulbTest();
    displayTest();
    delay(2000);
    buttonsBulbTestEnd();
    displayTestEnd();
    Serial.println(F("Bulb test complete."));
    Serial.println(F("\nTurn encoder and press buttons."));

    _lastDumpTime = millis();
}

void loop() {
    k7scUpdate();

    // Report encoder value changes
    uint16_t val = displayGetValue();
    if (val != _lastValue) {
        Serial.print(F("Value: ")); Serial.println(val);
        _lastValue = val;
    }

    // Report encoder button
    if (buttonsGetEncoderPress()) {
        Serial.println(F("BTN_EN — reset to default"));
        encoderSetValue(DEFAULT_VALUE);
    }

    // Report state byte changes
    uint8_t state = buttonsGetStateByte();
    if (state != _lastState) {
        Serial.print(F("State: 0x")); Serial.println(state, HEX);
        _lastState = state;
    }

    // Periodic full dump
    if (millis() - _lastDumpTime >= DUMP_INTERVAL_MS) {
        _lastDumpTime = millis();
        dumpState();
    }
}

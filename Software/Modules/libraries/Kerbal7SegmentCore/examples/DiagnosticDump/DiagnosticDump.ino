/**
 * @file        DiagnosticDump.ino
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       DiagnosticDump — Kerbal7SegmentCore serial debug example.
 *
 *              Standalone diagnostic sketch. Does NOT require an I2C
 *              master to be connected.
 *
 *              Outputs to Serial at 115200 baud:
 *                - Display value changes from encoder
 *                - Button press/release events
 *                - Encoder button presses
 *                - Module state byte on any change
 *                - Full state dump every DUMP_INTERVAL_MS
 *
 *              Also runs display and LED bulb test at startup.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware: KC-01-1881/1882 v2.0 (ATtiny816)
 *              IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             10 MHz or higher
 *                tinyNeoPixel Port: Port C
 */

#include <Wire.h>
#include <Kerbal7SegmentCore.h>

#define I2C_ADDRESS       0x2A
#define MODULE_TYPE_ID    0x0B
#define SERIAL_BAUD       115200
#define DUMP_INTERVAL_MS  2000
#define DEFAULT_VALUE     200

const ButtonConfig btnConfigs[K7SC_NEO_COUNT] = {
    { BTN_MODE_CYCLE,  {K7SC_OFF, K7SC_GREEN, K7SC_AMBER}, 3, 0 },
    { BTN_MODE_TOGGLE, {K7SC_OFF, K7SC_GREEN, K7SC_OFF},   2, 0 },
    { BTN_MODE_TOGGLE, {K7SC_OFF, K7SC_GREEN, K7SC_OFF},   2, 0 },
};

static uint16_t _lastValue    = DEFAULT_VALUE;
static uint8_t  _lastState    = 0;
static uint32_t _lastDumpTime = 0;

static void dumpState() {
    uint16_t val   = displayGetValue();
    uint8_t  state = buttonsGetStateByte();

    Serial.println(F("\n=== State Dump ==="));
    Serial.print(F("Display value: "));
    Serial.println(val);
    Serial.print(F("BTN01 cycle state: "));
    Serial.println(state & K7SC_STATE_BTN01_MASK);
    Serial.print(F("BTN02 active: "));
    Serial.println((state >> K7SC_STATE_BTN02_BIT) & 0x01);
    Serial.print(F("BTN03 active: "));
    Serial.println((state >> K7SC_STATE_BTN03_BIT) & 0x01);
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(500);

    Serial.println(F("\nKerbal7SegmentCore Diagnostic"));
    Serial.println(F("================================"));
    Serial.print(F("Firmware: "));
    Serial.print(K7SC_FIRMWARE_MAJOR);
    Serial.print(F("."));
    Serial.println(K7SC_FIRMWARE_MINOR);

    Wire.begin(I2C_ADDRESS);
    k7scBegin(MODULE_TYPE_ID, K7SC_CAP_DISPLAY, btnConfigs, DEFAULT_VALUE);

    Serial.println(F("Running bulb test..."));
    buttonsBulbTest(1000);
    displayTest(1000);
    Serial.println(F("Bulb test complete."));
    Serial.println(F("\nEntering main loop. Turn encoder and press buttons."));

    _lastValue    = DEFAULT_VALUE;
    _lastDumpTime = millis();
}

void loop() {
    k7scUpdate();

    // Report encoder value changes
    uint16_t currentVal = displayGetValue();
    if (currentVal != _lastValue) {
        Serial.print(F("Value: "));
        Serial.println(currentVal);
        _lastValue = currentVal;
    }

    // Report encoder button
    if (buttonsGetEncoderPress()) {
        Serial.println(F("BTN_EN pressed — reset to default"));
        encoderSetValue(DEFAULT_VALUE);
    }

    // Report state byte changes
    uint8_t currentState = buttonsGetStateByte();
    if (currentState != _lastState) {
        Serial.print(F("State byte: 0x"));
        Serial.println(currentState, HEX);
        _lastState = currentState;
    }

    // Periodic full dump
    if (millis() - _lastDumpTime >= DUMP_INTERVAL_MS) {
        _lastDumpTime = millis();
        dumpState();
    }
}

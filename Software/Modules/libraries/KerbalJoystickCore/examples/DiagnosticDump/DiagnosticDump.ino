/**
 * @file        DiagnosticDump.ino
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       DiagnosticDump — KerbalJoystickCore serial debug example.
 *
 *              Standalone diagnostic sketch for hardware verification.
 *              Does NOT require an I2C master to be connected.
 *
 *              Outputs to Serial at 115200 baud:
 *                - Calibrated center values for each axis at startup
 *                - Raw ADC values and scaled INT16 values per axis
 *                - Button press/release events
 *                - Full state dump every DUMP_INTERVAL_MS
 *
 *              Do not touch the joystick during the first ~80ms
 *              after power-on — calibration runs at startup.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware: KC-01-1831/1832 v1.0 (ATtiny816)
 *              IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             10 MHz or higher
 *                tinyNeoPixel Port: Port C
 */

#include <Wire.h>
#include <KerbalJoystickCore.h>

#define I2C_ADDRESS         0x28
#define MODULE_TYPE_ID      0x09
#define SERIAL_BAUD         115200
#define DUMP_INTERVAL_MS    1000

const RGBColor activeColors[KJC_NEO_COUNT] = {
    KJC_GREEN,
    KJC_AMBER,
};

static uint8_t  _lastBtnState  = 0;
static uint32_t _lastDumpTime  = 0;

// ============================================================
//  dumpState()
// ============================================================

static void dumpState() {
    Serial.println(F("\n=== State Dump ==="));

    // Calibrated centers
    Serial.println(F("Calibrated centers:"));
    for (uint8_t i = 0; i < KJC_AXIS_COUNT; i++) {
        Serial.print(F("  AXIS"));
        Serial.print(i + 1);
        Serial.print(F(": center="));
        Serial.println(adcGetCenter(i));
    }

    // Current axis values
    Serial.println(F("Current axis values (INT16):"));
    const char* axisNames[KJC_AXIS_COUNT] = {"AXIS1", "AXIS2", "AXIS3"};
    for (uint8_t i = 0; i < KJC_AXIS_COUNT; i++) {
        Serial.print(F("  "));
        Serial.print(axisNames[i]);
        Serial.print(F(": "));
        Serial.println(adcGetScaled(i));
    }

    // Button states
    uint8_t state = buttonsGetState();
    Serial.println(F("Button states:"));
    Serial.print(F("  BTN_JOY: "));
    Serial.println((state >> KJC_BIT_BTN_JOY) & 0x01 ? F("PRESSED") : F("released"));
    Serial.print(F("  BTN01:   "));
    Serial.println((state >> KJC_BIT_BTN01)   & 0x01 ? F("PRESSED") : F("released"));
    Serial.print(F("  BTN02:   "));
    Serial.println((state >> KJC_BIT_BTN02)   & 0x01 ? F("PRESSED") : F("released"));
}

// ============================================================
//  setup()
// ============================================================

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(500);

    Serial.println(F("\nKerbalJoystickCore Diagnostic"));
    Serial.println(F("================================"));
    Serial.print(F("Firmware: "));
    Serial.print(KJC_FIRMWARE_MAJOR);
    Serial.print(F("."));
    Serial.println(KJC_FIRMWARE_MINOR);
    Serial.print(F("I2C Address: 0x"));
    Serial.println(I2C_ADDRESS, HEX);
    Serial.println(F("Calibrating — keep joystick at rest..."));

    Wire.begin(I2C_ADDRESS);
    kjcBegin(MODULE_TYPE_ID, KJC_CAP_JOYSTICK, activeColors);

    Serial.println(F("Calibration complete."));
    Serial.print(F("Centers: AXIS1="));
    Serial.print(adcGetCenter(KJC_AXIS1));
    Serial.print(F(" AXIS2="));
    Serial.print(adcGetCenter(KJC_AXIS2));
    Serial.print(F(" AXIS3="));
    Serial.println(adcGetCenter(KJC_AXIS3));
    Serial.println(F("\nEntering main loop."));

    _lastDumpTime = millis();
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    kjcUpdate();

    // Report button changes
    uint8_t currentState = buttonsGetState();
    if (currentState != _lastBtnState) {
        uint8_t changed = currentState ^ _lastBtnState;
        const char* btnNames[KJC_BUTTON_COUNT] = {"BTN_JOY", "BTN01", "BTN02"};
        for (uint8_t i = 0; i < KJC_BUTTON_COUNT; i++) {
            if ((changed >> i) & 0x01) {
                bool pressed = (currentState >> i) & 0x01;
                Serial.print(pressed ? F("PRESS   ") : F("release "));
                Serial.println(btnNames[i]);
            }
        }
        _lastBtnState = currentState;
    }

    // Periodic full state dump
    if (millis() - _lastDumpTime >= DUMP_INTERVAL_MS) {
        _lastDumpTime = millis();
        dumpState();
    }
}

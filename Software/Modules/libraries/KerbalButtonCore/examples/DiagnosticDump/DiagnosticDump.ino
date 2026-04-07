/**
 * @file        DiagnosticDump.ino
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       DiagnosticDump — KerbalButtonCore serial debug example.
 *
 *              Standalone diagnostic sketch for hardware verification
 *              and debugging. Does NOT require an I2C master to be
 *              connected — operates independently.
 *
 *              Outputs to Serial (UPDI/debug UART) at 115200 baud:
 *                - Button state changes as they occur (debounced)
 *                - Full 16-button state bitmask on any change
 *                - Current LED state of each button
 *                - Running press/release event log
 *
 *              Also cycles through all LED states on all buttons
 *              at startup to verify LED hardware visually.
 *
 *              IMPORTANT: This sketch uses the Serial library for
 *              debug output. On the ATtiny816 with megaTinyCore,
 *              Serial maps to the UART on PB2/PB3 or the UPDI pin
 *              depending on fuse settings. Confirm your debug
 *              interface before flashing.
 *
 *              This sketch sets I2C address 0x20 but the I2C
 *              callbacks are registered — connect a logic analyzer
 *              to verify I2C responses if needed.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Requires: KerbalButtonCore library
 *              Hardware: KC-01-1822 v1.1 (ATtiny816)
 *              IDE settings:
 *                Board: ATtiny816 (megaTinyCore)
 *                Clock: 10 MHz or higher
 *                tinyNeoPixel Port: Port A
 */

#include <Wire.h>
#include <KerbalButtonCore.h>

// ============================================================
//  Configuration
// ============================================================

#define I2C_ADDRESS         0x20
#define MODULE_TYPE_ID      KBC_TYPE_UI_CONTROL
#define SERIAL_BAUD         115200

/** @brief Interval between full state dumps in milliseconds. */
#define DUMP_INTERVAL_MS    2000

/** @brief Duration of each LED state during visual LED test. */
#define LED_TEST_STEP_MS    300

// ============================================================
//  Active colors — all green for diagnostic purposes
// ============================================================

const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_GREEN, KBC_GREEN, KBC_GREEN, KBC_GREEN,
    KBC_GREEN, KBC_GREEN, KBC_GREEN, KBC_GREEN,
    KBC_GREEN, KBC_GREEN, KBC_GREEN, KBC_GREEN,
    KBC_GREEN, KBC_GREEN, KBC_GREEN, KBC_GREEN,
};

// ============================================================
//  Library instantiation
// ============================================================

KerbalButtonCore kbc(MODULE_TYPE_ID, KBC_CAP_EXTENDED_STATES, activeColors);

// ============================================================
//  State tracking for change detection
// ============================================================

static uint16_t lastButtonState = 0;
static uint32_t lastDumpTime   = 0;

// ============================================================
//  Helper — LED state name string
// ============================================================

static const char* ledStateName(uint8_t state) {
    switch (state) {
        case KBC_LED_OFF:            return "OFF";
        case KBC_LED_ENABLED:        return "ENABLED";
        case KBC_LED_ACTIVE:         return "ACTIVE";
        case KBC_LED_WARNING:        return "WARNING";
        case KBC_LED_ALERT:          return "ALERT";
        case KBC_LED_ARMED:          return "ARMED";
        case KBC_LED_PARTIAL_DEPLOY: return "PARTIAL_DEPLOY";
        default:                     return "UNKNOWN";
    }
}

// ============================================================
//  LED visual test — cycles through all states on all buttons
// ============================================================

static void runLEDTest() {
    Serial.println(F("--- LED Visual Test ---"));

    const uint8_t testStates[] = {
        KBC_LED_ENABLED,
        KBC_LED_ACTIVE,
        KBC_LED_WARNING,
        KBC_LED_ALERT,
        KBC_LED_ARMED,
        KBC_LED_PARTIAL_DEPLOY,
        KBC_LED_OFF
    };

    for (uint8_t s = 0; s < sizeof(testStates); s++) {
        uint8_t state = testStates[s];
        Serial.print(F("  State: "));
        Serial.println(ledStateName(state));

        for (uint8_t i = 0; i < KBC_BUTTON_COUNT; i++) {
            kbc.ledControl().setButtonState(i, state);
        }
        kbc.ledControl().render();

        // Service flash timing during the test step
        uint32_t stepStart = millis();
        while (millis() - stepStart < LED_TEST_STEP_MS) {
            kbc.ledControl().update();
        }
    }

    // Restore ENABLED state
    for (uint8_t i = 0; i < KBC_BUTTON_COUNT; i++) {
        kbc.ledControl().setButtonState(i, KBC_LED_ENABLED);
    }
    kbc.ledControl().render();

    Serial.println(F("--- LED Test Complete ---"));
}

// ============================================================
//  Dump full button and LED state to Serial
// ============================================================

static void dumpState() {
    uint16_t state = kbc.shiftReg().getLiveState();

    Serial.println(F("\n=== State Dump ==="));

    // Button states
    Serial.print(F("Buttons (raw bitmask): 0x"));
    if (state < 0x1000) Serial.print(F("0"));
    if (state < 0x0100) Serial.print(F("0"));
    if (state < 0x0010) Serial.print(F("0"));
    Serial.println(state, HEX);

    Serial.println(F("Button states:"));
    for (uint8_t i = 0; i < KBC_BUTTON_COUNT; i++) {
        Serial.print(F("  B"));
        if (i < 10) Serial.print(F("0"));
        Serial.print(i);
        Serial.print(F(" [KBC idx "));
        Serial.print(i);
        Serial.print(F("]: "));
        Serial.print((state >> i) & 0x01 ? F("PRESSED ") : F("released"));
        Serial.print(F("  LED: "));
        Serial.println(ledStateName(kbc.ledControl().getButtonState(i)));
    }
}

// ============================================================
//  setup()
// ============================================================

void setup() {
    Serial.begin(SERIAL_BAUD);

    // Brief delay for serial monitor to connect
    delay(500);

    Serial.println(F("\nKerbalButtonCore Diagnostic"));
    Serial.println(F("============================"));
    Serial.print(F("Firmware: "));
    Serial.print(KBC_FIRMWARE_MAJOR);
    Serial.print(F("."));
    Serial.println(KBC_FIRMWARE_MINOR);
    Serial.print(F("I2C Address: 0x"));
    Serial.println(I2C_ADDRESS, HEX);
    Serial.print(F("Button count: "));
    Serial.println(KBC_BUTTON_COUNT);
    Serial.println();

    Wire.begin(I2C_ADDRESS);
    kbc.begin();

    // Run visual LED test before entering main loop
    runLEDTest();

    lastDumpTime = millis();
    Serial.println(F("\nEntering main loop. Press buttons to see events."));
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    kbc.update();

    // Detect and report button state changes
    uint16_t currentState = kbc.shiftReg().getLiveState();
    if (currentState != lastButtonState) {
        uint16_t changed = currentState ^ lastButtonState;

        for (uint8_t i = 0; i < KBC_BUTTON_COUNT; i++) {
            if ((changed >> i) & 0x01) {
                bool pressed = (currentState >> i) & 0x01;
                Serial.print(pressed ? F("PRESS   ") : F("release "));
                Serial.print(F("B"));
                if (i < 10) Serial.print(F("0"));
                Serial.print(i);
                Serial.print(F("  state=0x"));
                if (currentState < 0x1000) Serial.print(F("0"));
                if (currentState < 0x0100) Serial.print(F("0"));
                if (currentState < 0x0010) Serial.print(F("0"));
                Serial.println(currentState, HEX);
            }
        }

        lastButtonState = currentState;
    }

    // Periodic full state dump
    if (millis() - lastDumpTime >= DUMP_INTERVAL_MS) {
        lastDumpTime = millis();
        dumpState();
    }
}

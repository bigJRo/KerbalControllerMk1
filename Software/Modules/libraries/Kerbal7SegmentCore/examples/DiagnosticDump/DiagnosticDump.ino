/**
 * @file        DiagnosticDump.ino
 * @version     2.0.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Kerbal7SegmentCore v2.0 hardware diagnostic.
 *
 *              Prints all inputState and cmdState changes to Serial.
 *              Useful for verifying hardware, I2C, and library behaviour
 *              before writing module-specific application logic.
 *
 *              Connect a USB-UART adapter to the ATtiny816 UPDI/TX pin
 *              and open a serial monitor at 115200 baud.
 *
 * @note        Hardware:  KC-01-1881/1882 v2.0 (ATtiny816)
 *              Library:   Kerbal7SegmentCore v2.0.0
 *              IDE settings:
 *                Board:        ATtiny816 (megaTinyCore)
 *                Clock:        20 MHz internal
 *                Programmer:   serialUPDI
 */

#include <KerbalModuleCommon.h>
#include <Kerbal7SegmentCore.h>

#define I2C_ADDRESS  0x2A

void setup() {
    Serial.begin(115200);
    Serial.println(F("K7SC DiagnosticDump v2.0.0"));
    k7scBegin(I2C_ADDRESS, KMC_TYPE_GPWS_INPUT, KMC_CAP_DISPLAY);
    buttonsClearAll();
    displaySetValue(8888);
    displayWake();
    Serial.println(F("Ready — waiting for events"));
}

void loop() {
    k7scUpdate();

    // ── cmdState changes ──────────────────────────────────────
    static K7SCLifecycle _lastLC = K7SC_BOOT_READY;
    if (cmdState.lifecycle != _lastLC) {
        Serial.print(F("[CMD] lifecycle → "));
        switch (cmdState.lifecycle) {
            case K7SC_ACTIVE:     Serial.println(F("ACTIVE"));     break;
            case K7SC_DISABLED:   Serial.println(F("DISABLED"));   break;
            case K7SC_SLEEPING:   Serial.println(F("SLEEPING"));   break;
            case K7SC_BOOT_READY: Serial.println(F("BOOT_READY")); break;
        }
        _lastLC = cmdState.lifecycle;
    }
    if (cmdState.isBulbTest) {
        static bool _lastBulb = false;
        if (cmdState.isBulbTest != _lastBulb) {
            Serial.println(F("[CMD] BULB_TEST start"));
            _lastBulb = true;
        }
    }
    if (cmdState.isReset)    Serial.println(F("[CMD] RESET"));
    if (cmdState.hasNewValue) {
        Serial.print(F("[CMD] SET_VALUE → "));
        Serial.println(cmdState.newValue);
    }
    if (cmdState.hasLEDState) {
        Serial.print(F("[CMD] SET_LED_STATE → 0x"));
        Serial.println(cmdState.ledState, HEX);
    }

    // ── inputState changes ────────────────────────────────────
    if (inputState.buttonPressed) {
        Serial.print(F("[HW] buttonPressed  = 0b"));
        Serial.println(inputState.buttonPressed, BIN);
    }
    if (inputState.buttonReleased) {
        Serial.print(F("[HW] buttonReleased = 0b"));
        Serial.println(inputState.buttonReleased, BIN);
    }
    if (inputState.encoderChanged) {
        Serial.print(F("[HW] encoderDelta   = "));
        Serial.println(inputState.encoderDelta);
    }

    // ── Clear consumed input ──────────────────────────────────
    inputState.buttonPressed  = 0;
    inputState.buttonReleased = 0;
    inputState.buttonChanged  = 0;
    inputState.encoderDelta   = 0;
    inputState.encoderChanged = false;
}

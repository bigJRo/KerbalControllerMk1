/*
 * KC-01-1880  NeoPixel Colour Validation — Xiao RA4M1 Tester
 *
 * Works alongside KC01_1882_ColorValidation.ino on the module.
 * Drives each colour via SET_BRIGHTNESS (to ensure consistent
 * brightness) and SET_VALUE (to show index on display), then
 * asks for visual confirmation.
 *
 * Actually the colour is driven by the module firmware itself
 * via the encoder button — this tester just logs the results
 * by asking Y/N for each colour as you advance through them.
 *
 * Wiring:
 *   D4 (SDA) → SDA  (P1 pin 13)
 *   D5 (SCL) → SCL  (P1 pin 14)
 *   D6       → INT  (P1 pin 15)
 *   3.3V     → VCC  (P1 pin 4)
 *   GND      → GND  (P1 pin 3)
 *
 * Usage:
 *   1. Flash KC01_1882_ColorValidation to the module
 *   2. Flash this sketch to the Xiao
 *   3. Open Serial Monitor at 115200 baud
 *   4. Press Enter to start — each Enter advances to the next colour
 *   5. Enter Y if the colour looks correct, N if it needs adjustment
 */

#include <Wire.h>
#include <KerbalModuleCommon.h>

#define MODULE_ADDR   0x2A
#define PIN_INT       6
#define PACKET_SIZE   6

// Mirror of the colour table in the module firmware
struct ColorEntry {
    const char*  name;
    uint8_t      r, g, b;
};

static const ColorEntry COLORS[] = {
    { "OFF",            0,   0,   0 },
    { "GREEN",          0, 255,   0 },
    { "RED",          255,   0,   0 },
    { "AMBER",        255,  80,   0 },
    { "WHITE_COOL",   255, 180, 255 },
    { "WHITE_NEUTRAL",255, 160, 180 },
    { "WHITE_WARM",   255, 140, 100 },
    { "WHITE_SOFT",   120,  70,  20 },
    { "ORANGE",       255,  60,   0 },
    { "YELLOW",       234, 179,   8 },
    { "GOLD",         255, 160,   0 },
    { "CHARTREUSE",   163, 230,  53 },
    { "LIME",         132, 204,  22 },
    { "MINT",         100, 255, 160 },
    { "BLUE",           0,   0, 255 },
    { "SKY",           14, 165, 233 },
    { "TEAL",          20, 184, 166 },
    { "CYAN",           6, 182, 212 },
    { "PURPLE",       120,   0, 220 },
    { "INDIGO",        60,   0, 200 },
    { "VIOLET",       160,   0, 255 },
    { "CORAL",        255, 100,  54 },
    { "ROSE",         255,  80, 120 },
    { "PINK",         236,  72, 153 },
    { "MAGENTA",      255,   0, 255 },
};

static const uint8_t NUM_COLORS = sizeof(COLORS) / sizeof(COLORS[0]);

// ── Helpers ───────────────────────────────────────────────────

static void printSep() {
    Serial.println(F("─────────────────────────────────────────"));
}

// Y/N prompt — returns true for Y
static bool askYN(const char* q) {
    Serial.print(F("  [Y/N] ")); Serial.print(q); Serial.print(F(": "));
    while (true) {
        while (!Serial.available()) {}
        char c = Serial.read();
        while (Serial.available()) Serial.read();
        if (c == 'Y' || c == 'y') { Serial.println(F("Y")); return true;  }
        if (c == 'N' || c == 'n') { Serial.println(F("N")); return false; }
    }
}

// Wait for INT to assert (module advanced colour)
static bool waitINT(uint32_t ms) {
    uint32_t start = millis();
    while (digitalRead(PIN_INT) != LOW) {
        if (millis() - start >= ms) return false;
        delay(5);
    }
    return true;
}

// Drain pending INT
static void drainINT() {
    uint32_t start = millis();
    while (digitalRead(PIN_INT) == LOW && millis() - start < 300) {
        Wire.requestFrom((uint8_t)MODULE_ADDR, (uint8_t)PACKET_SIZE);
        while (Wire.available()) Wire.read();
        delay(20);
    }
}

// ── Main test ─────────────────────────────────────────────────

void runColorTest()
{
    printSep();
    Serial.println(F("NeoPixel Colour Validation"));
    Serial.println(F("Press BTN_EN on module to advance to next colour."));
    Serial.println(F("Enter Y if correct, N if it needs adjustment."));
    printSep();

    uint8_t passed = 0;
    uint8_t failed = 0;

    for (uint8_t i = 0; i < NUM_COLORS; i++) {
        Serial.print(F("\nColour ")); Serial.print(i);
        Serial.print(F(": ")); Serial.println(COLORS[i].name);
        Serial.print(F("  Expected RGB: ("));
        Serial.print(COLORS[i].r); Serial.print(F(", "));
        Serial.print(COLORS[i].g); Serial.print(F(", "));
        Serial.print(COLORS[i].b); Serial.println(F(")"));

        if (i == 0) {
            Serial.println(F("  [Module showing colour 0 on boot]"));
        } else {
            // Wait for BTN_EN press on the module — INT asserts when encoder
            // button is pressed and the module advances to the next colour
            Serial.println(F("  [Waiting for BTN_EN press on module...]"));
            bool got = waitINT(30000);  // 30s timeout
            if (!got) {
                Serial.println(F("  [TIMEOUT — skipping remaining colours]"));
                break;
            }
            drainINT();
        }

        bool ok = askYN("Does the colour look correct?");
        if (ok) {
            Serial.print(F("  [PASS] ")); Serial.println(COLORS[i].name);
            passed++;
        } else {
            Serial.print(F("  [FAIL] ")); Serial.println(COLORS[i].name);
            failed++;
            // Ask for description of what was seen
            Serial.println(F("  Describe what you see (type and press Enter):"));
            while (!Serial.available()) {}
            Serial.print(F("  Observed: "));
            while (Serial.available()) {
                char c = Serial.read();
                if (c == '\n' || c == '\r') break;
                Serial.print(c);
            }
            Serial.println();
        }
    }

    printSep();
    Serial.print(F("Results: "));
    Serial.print(passed); Serial.print(F(" passed, "));
    Serial.print(failed); Serial.println(F(" failed"));
    printSep();
}

void setup()
{
    Serial.begin(115200);
    while (!Serial) delay(10);

    Wire.begin();
    Wire.setClock(100000);
    pinMode(PIN_INT, INPUT_PULLUP);

    delay(500);

    printSep();
    Serial.println(F("KC-01-1880 NeoPixel Colour Validation"));
    Serial.println(F("Press Enter to begin, then BTN_EN on module to advance colours."));
    printSep();
}

void loop()
{
    if (!Serial.available()) return;
    while (Serial.available()) Serial.read();
    runColorTest();
    Serial.println(F("\nPress Enter to run again..."));
}

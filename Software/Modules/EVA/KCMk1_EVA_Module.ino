/**
 * @file        KCMk1_EVA_Module.ino
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       EVA Module sketch for the Kerbal Controller Mk1.
 *
 *              Provides EVA (Extra-Vehicular Activity) control functions
 *              for Kerbal Space Program including lights, jetpack,
 *              boarding, construction, jumping, and grabbing.
 *
 *              This module is NOT a KerbalButtonCore (KBC) module.
 *              It shares the same I2C wire protocol as KBC modules
 *              but is implemented as a standalone sketch with tab-based
 *              organization rather than using the KerbalButtonCore library.
 *
 *              Hardware differences from KBC base module (KC-01-1822):
 *                - 6 NeoPixel RGB buttons (WS2811) instead of 12
 *                - 6 direct GPIO button inputs (no shift registers)
 *                - 2 rotary encoder headers (ENC1, ENC2 — future use)
 *                - NeoPixel on PC0 (Port C) instead of PA4
 *
 *              I2C Address: 0x26
 *              Module Type: EVA_MODULE_TYPE_ID (0x07)
 *              Extended States: No
 *
 *              Panel Layout (2 rows x 3 columns):
 *
 *                        Col 1 (left)       Col 2 (center)    Col 3 (right)
 *                Row 1:  B0                 B2                B4
 *                        EVA Lights         Board Craft       Jump / Let Go
 *                        MINT               GREEN             CHARTREUSE
 *
 *                Row 2:  B1                 B3                B5
 *                        Jetpack Enable     EVA Construction  Grab
 *                        LIME               TEAL              SEAFOAM
 *
 *              Encoder headers (future use — not implemented):
 *                ENC1 — H1 header (PA3=A, PA2=B, PA1=SW)
 *                ENC2 — H2 header (PC3=A, PC2=B, PC1=SW)
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware:  KC-01-1852 v1.0 (ATtiny816)
 *              IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             10 MHz or higher
 *                tinyNeoPixel Port: Port C
 *              Dependencies:
 *                tinyNeoPixel_Static (megaTinyCore)
 *                Wire (megaTinyCore)
 */

#include <Wire.h>
#include <tinyNeoPixel_Static.h>
#include "Config.h"

// ============================================================
//  Global subsystem state
//  All subsystem state is declared in their respective tabs.
//  This file coordinates initialisation and the main loop.
// ============================================================

void setup() {
    // 1. I2C — must be first so callbacks are live
    Wire.begin(EVA_I2C_ADDRESS);
    i2cBegin();

    // 2. Buttons — direct GPIO
    buttonsBegin();

    // 3. Encoders — configure pins (future use)
    encodersBegin();

    // 4. LEDs — NeoPixel chain
    ledsBegin();

    // 5. Set all buttons to ENABLED state and render
    for (uint8_t i = 0; i < EVA_BUTTON_COUNT; i++) {
        ledSetState(i, LED_ENABLED);
    }
    ledsRender();
}

void loop() {
    uint32_t now = millis();

    // 1. Poll buttons on interval
    static uint32_t lastPoll = 0;
    if ((now - lastPoll) >= EVA_POLL_INTERVAL_MS) {
        lastPoll = now;
        buttonsPoll();
    }

    // 2. Render pending LED changes from I2C commands
    if (i2cIsRenderPending()) {
        ledsRender();
        i2cClearRenderPending();
    }

    // 3. Sync INT pin with button state
    i2cSyncINT();
}

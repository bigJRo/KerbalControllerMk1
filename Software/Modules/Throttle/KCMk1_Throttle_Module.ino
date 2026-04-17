/**
 * @file        KCMk1_Throttle_Module.ino
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Throttle Module for the Kerbal Controller Mk1.
 *
 *              Provides motorized throttle control for Kerbal Space
 *              Program. A motorized 10k slide potentiometer provides
 *              position feedback and physical control. A capacitive
 *              touch sensor detects pilot contact. Four buttons set
 *              throttle to 100%, 0%, or step up/down.
 *
 *              I2C Address:    0x2C
 *              Module Type ID: 0x0D
 *
 *              Tab structure:
 *                Config.h      — pins, constants, thresholds
 *                Motor.h/.cpp  — L293D H-bridge control, position seek
 *                Wiper.h/.cpp  — ADC position reading, scaling
 *                Buttons.h/.cpp— button input, LED output
 *                I2C.h/.cpp    — protocol handler, packet build
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware:  KC-01-1861/1862 Throttle Module v1.1
 *              Power:     +5V VCC from main board, +12V for motor bus
 *              IDE settings:
 *                Board:   ATtiny816 (megaTinyCore)
 *                Clock:   10 MHz or higher
 */

#include <Wire.h>
#include "Config.h"
#include "Motor.h"
#include "Wiper.h"
#include "Buttons.h"
#include "I2C.h"

// ============================================================
//  setup()
// ============================================================

void setup() {
    // I2C first — callbacks live before anything else
    Wire.begin(THR_I2C_ADDRESS);
    i2cBegin();

    // Motor — configure PWM and direction pins, hold at 0
    motorBegin();

    // Wiper — ADC init and initial read
    wiperBegin();

    // Buttons and LEDs
    buttonsBegin();

    // Module starts disabled — motor holds at 0, LEDs off
    // Controller must send CMD_ENABLE to activate
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    uint32_t now = millis();

    // Poll buttons and wiper on interval
    static uint32_t lastPoll = 0;
    if ((now - lastPoll) >= THR_POLL_INTERVAL_MS) {
        lastPoll = now;
        buttonsPoll();
        wiperPoll();
    }

    // Sync INT, process touch and button events, set motor targets.
    // motorUpdate() is called separately below — i2cSyncINT() only
    // sets targets; it does not drive the motor itself.
    i2cSyncINT();

    // Run motor position controller once per loop.
    // motorUpdate() is a fast no-op when not seeking a target.
    motorUpdate(wiperGetRaw());
}

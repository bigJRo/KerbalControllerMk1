/**
 * @file        KCMk1_Dual_Encoder.ino
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Dual Rotary Encoder Module for the Kerbal Controller Mk1.
 *
 *              Provides two independent quadrature rotary encoders with
 *              pushbutton switches. Encoder functions are defined by the
 *              main controller — this module reports deltas and button
 *              events without semantic interpretation.
 *
 *              I2C Address:    0x2D
 *              Module Type ID: 0x0E
 *              Capability:     0x04 (encoder data in packet)
 *
 *              Hardware: KC-01-1871/1872 Dual Encoder Module v1.0
 *              Both encoders: PEC11R-4220F-S0024 with hardware RC
 *              debounce (10nF caps on A/B channels).
 *
 *              Tab structure:
 *                Config.h         — pins, constants, commands
 *                Encoders.h/.cpp  — quadrature decode, delta accumulation
 *                Buttons.h/.cpp   — pushbutton debounce and event tracking
 *                I2C.h/.cpp       — protocol handler, packet build, INT
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware: KC-01-1871/1872 v1.0 (ATtiny816)
 *              IDE settings:
 *                Board:   ATtiny816 (megaTinyCore)
 *                Clock:   10 MHz or higher
 */

#include <Wire.h>
#include "Config.h"
#include "Encoders.h"
#include "Buttons.h"
#include "I2C.h"

void setup() {
    Wire.begin(DEC_I2C_ADDRESS);
    i2cBegin();
    encodersBegin();
    buttonsBegin();
}

void loop() {
    static uint32_t lastPoll = 0;
    uint32_t now = millis();

    if ((now - lastPoll) >= DEC_POLL_INTERVAL_MS) {
        lastPoll = now;
        encodersPoll();
        buttonsPoll();
    }

    i2cSyncINT();
}

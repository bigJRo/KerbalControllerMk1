/**
 * @file        KCMk1_Switch_Panel.ino
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Switch Panel Input Module for the Kerbal Controller Mk1.
 *
 *              Provides 10 toggle switch inputs to the system controller
 *              via I2C. Switch functions are TBD — defined by the main
 *              controller. Supports both latching and momentary toggle
 *              switches — the dual-buffer strategy guarantees every
 *              state change edge is reported regardless of switch type.
 *
 *              I2C Address:    0x2E
 *              Module Type ID: 0x0F
 *              Capability:     0x00
 *              Packet format:  Standard 4-byte (state HI/LO + change HI/LO)
 *                              Bits 0-9 = SW1-SW10, bits 10-15 always 0
 *
 *              Hardware: KC-01 Switch Panel Input Module v1.0
 *
 *              Tab structure:
 *                Config.h       — pins, constants, commands
 *                Switches.h/.cpp— GPIO reading, debounce, dual-buffer
 *                I2C.h/.cpp     — protocol handler, packet build, INT
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware: ATtiny816-MNR
 *              IDE settings:
 *                Board:   ATtiny816 (megaTinyCore)
 *                Clock:   10 MHz or higher
 */

#include <Wire.h>
#include "Config.h"
#include "Switches.h"
#include "I2C.h"

void setup() {
    Wire.begin(SWP_I2C_ADDRESS);
    i2cBegin();
    switchesBegin();
}

void loop() {
    static uint32_t lastPoll = 0;
    uint32_t now = millis();

    if ((now - lastPoll) >= SWP_POLL_INTERVAL_MS) {
        lastPoll = now;
        switchesPoll();
    }

    i2cSyncINT();
}

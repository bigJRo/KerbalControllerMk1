/**
 * @file        KCMk1_Switch_Panel.ino
 * @version     1.1
 * @date        2026-04-09
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Switch Panel Input Module for the Kerbal Controller Mk1.
 *
 *              Provides 10 toggle switch inputs via I2C. SW1 and SW2
 *              are mechanically coupled to a single 3-position latching
 *              MODE switch. All other switches are independent.
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
 *                Config.h       — pins, constants, bit assignments, notes
 *                Switches.h/.cpp— GPIO reading, debounce, dual-buffer
 *                I2C.h/.cpp     — protocol handler, packet build, INT
 *
 *              Panel layout (5 col x 2 row, left to right):
 *
 *                +----------+----------+----------+----------+----------+
 *                | MODE     | MSTR     | SCE      | AUDIO    | ENGINE   |
 *                | CTRL(up) | OPER(up) | NORM(up) | OFF(up)  | SAFE(up) |
 *                | SW1+SW2  | SW3      | SW5      | SW7      | SW9      |
 *                | DEMO(dn) | RESET(dn)| AUX(dn)  | ON(dn)   | ARM(dn)  |
 *                +----------+----------+----------+----------+----------+
 *                | (lower)  | DISPLAY  | LTG      | INPUT    | THRTL    |
 *                |          | OPER(up) | OPER(up) | NORM(up) | STD(up)  |
 *                |          | SW4      | SW6      | SW8      | SW10     |
 *                |          | RESET(dn)| TEST(dn) | FINE(dn) | FINE(dn) |
 *                +----------+----------+----------+----------+----------+
 *
 *              MODE switch (SW1+SW2) — 3-position latching:
 *                Up     (CTRL) : SW1=1, SW2=0 → Live Control
 *                Center (DBG)  : SW1=0, SW2=0 → Debug  [power-on default]
 *                Down   (DEMO) : SW1=0, SW2=1 → Demo / Simulated
 *
 *              Use SWP_GET_MODE(stateWord) and SWP_MODE_* constants
 *              to decode mode in controller firmware (see Config.h).
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

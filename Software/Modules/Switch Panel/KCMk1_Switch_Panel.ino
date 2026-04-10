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
 *              Provides 10 toggle switch inputs to the system controller
 *              via I2C. Supports both latching and momentary toggle
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
 *                Config.h       — pins, constants, bit assignments, notes
 *                Switches.h/.cpp— GPIO reading, debounce, dual-buffer
 *                I2C.h/.cpp     — protocol handler, packet build, INT
 *
 *              Switch function assignments:
 *
 *                SW1  (Bit 0) — Live Control Mode
 *                               Toggle: enable/suspend live control link
 *
 *                SW2  (Bit 1) — Demo Mode
 *                               Toggle: enter/exit simulated data mode
 *                               → INDICATOR B8 (DEMO) ACTIVE while on
 *
 *                SW3  (Bit 2) — Master Reset
 *                               Rising edge only: CMD_RESET all modules
 *
 *                SW4  (Bit 3) — Display Reset
 *                               Rising edge only: CMD_SET_VALUE 0 to all
 *                               display modules (0x2A, 0x2B)
 *
 *                SW5  (Bit 4) — SCE to Auxiliary
 *                               Toggle: activate/deactivate SCE aux power
 *                               → INDICATOR B12 (SCE AUX) ACTIVE while on
 *
 *                SW6  (Bit 5) — Bulb Test
 *                               Rising edge: CMD_BULB_TEST (start)
 *                               Falling edge: CMD_BULB_TEST 0x00 (stop)
 *
 *                SW7  (Bit 6) — Audio On
 *                               Toggle: enable/disable audio system
 *                               → INDICATOR B9 (AUDIO) ACTIVE while on
 *
 *                SW8  (Bit 7) — Precision Input
 *                               Toggle: enable/disable precision input mode
 *                               → INDICATOR B6 (PREC INPUT) ACTIVE while on
 *
 *                SW9  (Bit 8) — Engine Arm
 *                               Toggle: arm/disarm engine ignition system
 *
 *                SW10 (Bit 9) — Throttle Fine Control
 *                               Rising edge: CMD_SET_PRECISION 0x01 → Throttle
 *                               Falling edge: CMD_SET_PRECISION 0x00 → Throttle
 *                               → INDICATOR B3 (THRTL PREC) ACTIVE while on
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

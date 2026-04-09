/**
 * @file        KCMk1_Indicator_Module.ino
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Indicator Module for the Kerbal Controller Mk1.
 *
 *              Pure NeoPixel output module. 16 SK6812mini-012 RGB
 *              LEDs display system status indicators driven entirely
 *              by the main controller via CMD_SET_LED_STATE. Each
 *              indicator maps a KBC LED state nibble value to a
 *              per-pixel active color defined in this sketch.
 *
 *              I2C Address:    0x2F
 *              Module Type ID: 0x10
 *              Capability:     0x00
 *
 *              Two rotary encoder headers (H1, H2) are present on
 *              the PCB for future expansion but are not currently
 *              installed. Pins are defined in Config.h for reference.
 *
 *              Pixel index map:
 *                 0: COMM ACTIVE      8: LNDG GEAR LOCK
 *                 1: USB ACTIVE       9: CHUTE ARM
 *                 2: THRTL ENA       10: CTRL
 *                 3: AUTO THRTL      11: DEBUG
 *                 4: PREC INPUT      12: DEMO
 *                 5: AUDIO           13: SWITCH ERROR
 *                 6: LIGHT ENA       14: RCS
 *                 7: BRAKE LOCK      15: SAS
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware: KC-01 Indicator Module v1.0 (ATtiny816)
 *              IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             10 MHz or higher
 *                tinyNeoPixel Port: Port A  ← PA5, not Port C
 */

#include <Wire.h>
#include "Config.h"
#include "LEDs.h"
#include "I2C.h"

void setup() {
    Wire.begin(IND_I2C_ADDRESS);
    i2cBegin();
    ledsBegin();
}

void loop() {
    ledsUpdate();
    i2cSyncINT();
}

/**
 * @file        KCMk1_Indicator_Module.ino
 * @version     2.0.0
 * @date        2026-04-09
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Indicator Module for the Kerbal Controller Mk1.
 *
 *              Pure NeoPixel output module. 18 SK6812mini-012 RGB
 *              LEDs display system status indicators driven entirely
 *              by the main controller via CMD_SET_LED_STATE. Each
 *              indicator maps a KBC LED state nibble value to a
 *              per-pixel active color defined in LEDs.h.
 *
 *              I2C Address:    0x2F
 *              Module Type ID: 0x10
 *              Capability:     0x00
 *
 *              CMD_SET_LED_STATE payload: 9 bytes (18 nibbles).
 *              This differs from the standard 8-byte KMC payload —
 *              the controller must send 9-byte payloads to this address.
 *
 *              Two rotary encoder headers (H1, H2) are present on
 *              the PCB for future expansion but are not currently
 *              installed. Pins are defined in Config.h for reference.
 *
 *              Panel layout (6 columns × 3 rows, column-major):
 *
 *                Col:   1           2           3           4           5           6
 *                Row 1: B0          B3          B6          B9          B12         B15
 *                       THRTL ENA   THRTL PREC  PREC INPUT  AUDIO       SCE AUX     AUTO PILOT
 *                Row 2: B1          B4          B7          B10         B13         B16
 *                       LIGHT ENA   BRAKE LOCK  LNDG GEAR   CHUTE ARM   RCS         SAS
 *                Row 3: B2          B5          B8          B11         B14         B17
 *                       CTRL        DEBUG       DEMO        COMM ACTIVE SWITCH ERR  ABORT
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware: KC-01 Indicator Module v2.0 (ATtiny816)
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

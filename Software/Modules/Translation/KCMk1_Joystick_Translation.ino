/**
 * @file        KCMk1_Joystick_Translation.ino
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Joystick Translation module for the Kerbal Controller Mk1.
 *
 *              Provides three-axis translational input (surge, sway, heave)
 *              for Kerbal Space Program vessel control. Reports axis
 *              data as signed INT16 values and button state to the
 *              system controller via I2C.
 *
 *              I2C Address:    0x29
 *              Module Type ID: 0x0A (Joystick Translation)
 *              Capability:     0x08 (KJC_CAP_JOYSTICK)
 *
 *              Axes:
 *                AXIS1 (PB4) — X axis
 *                AXIS2 (PB5) — Y axis
 *                AXIS3 (PA7) — Z axis
 *
 *              Buttons:
 *                BTN01 (PC0) — Cycle Camera Mode — MAGENTA — NeoPixel
 *                BTN02 (PB3) — Camera Reset      — GREEN   — NeoPixel
 *                BTN_JOY(PA6)— Enable Camera Mode — no LED — GPIO
 *
 *              Do not touch the joystick during the first ~80ms
 *              after power-on. Startup calibration reads the center
 *              position on all three axes at boot.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware:  KC-01-1831/1832 v1.0 (ATtiny816)
 *              Library:   KerbalJoystickCore v1.0.0
 *              IDE settings:
 *                Board:             ATtiny816 (megaTinyCore)
 *                Clock:             10 MHz or higher
 *                tinyNeoPixel Port: Port C
 */

#include <Wire.h>
#include <KerbalJoystickCore.h>

// ============================================================
//  Module identity
// ============================================================

/** @brief I2C address for the Joystick Translation module. */
#define I2C_ADDRESS     0x29

/** @brief Module type identifier. */
#define MODULE_TYPE_ID  0x0A

// ============================================================
//  NeoPixel button active colors
//
//  Index 0 = BTN01, Index 1 = BTN02
//  BTN_JOY has no LED — not represented here.
// ============================================================

const RGBColor activeColors[KJC_NEO_COUNT] = {
    KJC_MAGENTA,  // BTN01 — Cycle Camera Mode
    KJC_GREEN,    // BTN02 — Camera Reset
};

// ============================================================
//  setup()
// ============================================================

void setup() {
    // Wire.begin() must be called before kjcBegin()
    Wire.begin(I2C_ADDRESS);

    // Initialise all subsystems.
    // Startup calibration runs here (~80ms).
    // Keep joystick at rest during power-on.
    kjcBegin(MODULE_TYPE_ID, KJC_CAP_JOYSTICK, activeColors);
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    // kjcUpdate() handles all library tasks each iteration:
    //   - Button polling and debounce (immediate INT on change)
    //   - ADC polling with deadzone and change threshold filtering
    //   - Render pending LED changes from I2C commands
    //   - INT pin sync (Option E hybrid strategy)
    kjcUpdate();
}

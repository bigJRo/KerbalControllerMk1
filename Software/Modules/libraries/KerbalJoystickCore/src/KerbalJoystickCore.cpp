/**
 * @file        KerbalJoystickCore.cpp
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Top-level implementation of KerbalJoystickCore.
 *              Coordinates all subsystems via kjcBegin() and kjcUpdate().
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include "KerbalJoystickCore.h"

// ============================================================
//  Poll interval tracking
// ============================================================

static uint32_t _lastPollTime = 0;

// ============================================================
//  kjcBegin()
// ============================================================

void kjcBegin(uint8_t typeId, uint8_t capFlags,
              const RGBColor* activeColors,
              uint8_t brightness) {

    // 1. I2C — register callbacks first so we are responsive immediately
    kjcI2CBegin(typeId, capFlags);

    // 2. ADC — calibrate center position on all axes
    //    Note: adds ~80ms delay for 16-sample calibration
    adcBegin();

    // 3. Buttons and NeoPixels
    buttonsBegin(activeColors, brightness);

    _lastPollTime = millis();
}

// ============================================================
//  kjcUpdate()
// ============================================================

void kjcUpdate() {
    if (kjcI2CIsSleeping()) {
        // Reduced poll rate during sleep
        uint32_t now = millis();
        if ((now - _lastPollTime) >= KJC_SLEEP_POLL_MS) {
            _lastPollTime = now;
            buttonsPoll();
        }
        kjcI2CSyncINT();
        return;
    }

    uint32_t now = millis();
    if ((now - _lastPollTime) >= KJC_POLL_INTERVAL_MS) {
        _lastPollTime = now;

        // Poll buttons — always immediate INT on change
        buttonsPoll();

        // Poll ADC — filtered by deadzone and change threshold
        adcPoll();
    }

    // Render pending LED changes from I2C commands
    if (kjcI2CIsRenderPending()) {
        buttonsRender();
        kjcI2CClearRenderPending();
    }

    // Sync INT pin — Option E hybrid strategy
    kjcI2CSyncINT();
}

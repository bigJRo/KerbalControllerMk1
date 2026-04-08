/**
 * @file        Kerbal7SegmentCore.cpp
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Top-level Kerbal7SegmentCore implementation.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Arduino.h>
#include "Kerbal7SegmentCore.h"

static uint32_t _lastPollTime = 0;

// ============================================================
//  k7scBegin()
// ============================================================

void k7scBegin(uint8_t typeId, uint8_t capFlags,
               const ButtonConfig* btnConfigs,
               uint16_t initialValue) {

    // 1. I2C — register callbacks first
    k7scI2CBegin(typeId, capFlags);

    // 2. Display — initialise MAX7219
    displayBegin();

    // 3. Encoder — set initial value and read A/B state
    encoderBegin(initialValue);

    // 4. Buttons and NeoPixels
    buttonsBegin(btnConfigs);

    _lastPollTime = millis();
}

// ============================================================
//  k7scUpdate()
// ============================================================

void k7scUpdate() {
    if (k7scI2CIsSleeping()) {
        uint32_t now = millis();
        if ((now - _lastPollTime) >= K7SC_SLEEP_POLL_MS) {
            _lastPollTime = now;
            buttonsPoll();
        }
        k7scI2CSyncINT();
        return;
    }

    uint32_t now = millis();
    if ((now - _lastPollTime) >= K7SC_POLL_INTERVAL_MS) {
        _lastPollTime = now;
        buttonsPoll();
        encoderPoll();
    }

    k7scI2CSyncINT();
}

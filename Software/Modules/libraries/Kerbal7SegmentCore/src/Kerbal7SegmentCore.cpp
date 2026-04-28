/**
 * @file        Kerbal7SegmentCore.cpp
 * @version     2.0.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @license     GNU General Public License v3.0
 */

#include <Arduino.h>
#include "Kerbal7SegmentCore.h"

static uint32_t _lastPollTime = 0;

void k7scBegin(uint8_t i2cAddress, uint8_t typeId, uint8_t capFlags) {
    displayBegin();     // MAX7219 init — starts in shutdown (dark)
    encoderBegin();     // encoder ISR — no display coupling
    buttonsBegin();     // GPIO + NeoPixel init — all pixels off
    k7scI2CBegin(i2cAddress, typeId, capFlags);  // I2C + BOOT_READY INT
    _lastPollTime = millis();
}

void k7scUpdate() {
    uint32_t now = millis();
    if ((now - _lastPollTime) >= K7SC_POLL_INTERVAL_MS) {
        _lastPollTime = now;
        // Only poll hardware when active — prevents spurious INT when
        // disabled or sleeping. Sketch drains inputState in its early return.
        if (cmdState.lifecycle == K7SC_ACTIVE) {
            buttonsPoll();
            encoderPoll();
        }
    }
    k7scI2CPoll();
}

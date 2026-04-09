/**
 * @file        KerbalButtonCore.cpp
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Implementation of the KerbalButtonCore top-level library
 *              class for the KerbalButtonCore library.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the KerbalButtonCore (KBC) library.
 *              Hardware: KC-01-1822 v1.1
 *              Protocol: KBC_Protocol_Spec.md v1.1
 */

#include "KerbalButtonCore.h"

// ============================================================
//  Constructor
// ============================================================

KerbalButtonCore::KerbalButtonCore(uint8_t         moduleTypeId,
                                   uint8_t         capFlags,
                                   const RGBColor* activeColors)
    : _shiftReg()
    , _ledControl()
    , _i2c(_shiftReg, _ledControl, moduleTypeId, capFlags)
    , _activeColors(activeColors)
    , _lastPollTime(0)
    , _lastSleepPollTime(0)
{
}

// ============================================================
//  begin()
// ============================================================

void KerbalButtonCore::begin(uint8_t brightness) {
    // 1. Initialise shift registers first — no dependencies
    _shiftReg.begin();

    // 2. Initialise LED control — depends on active color array
    //    and brightness setting
    _ledControl.begin(_activeColors, brightness);

    // 3. Set all buttons to ENABLED state as initial condition
    for (uint8_t i = 0; i < KBC_BUTTON_COUNT; i++) {
        _ledControl.setButtonState(i, KBC_LED_ENABLED);
    }
    _ledControl.render();

    // 4. Initialise I2C last — Wire.begin(address) must have
    //    been called in the sketch before this point.
    //    Registers Wire callbacks and configures INT pin.
    _i2c.begin();

    // Seed poll timestamps so first update() fires immediately
    uint32_t now = millis();
    _lastPollTime      = now - KBC_POLL_INTERVAL_MS;
    _lastSleepPollTime = now - KBC_SLEEP_POLL_INTERVAL_MS;
}

// ============================================================
//  update()
// ============================================================

void KerbalButtonCore::update() {
    uint32_t now = millis();

    // --------------------------------------------------------
    //  1. Button polling — gated by poll interval timer.
    //     Uses sleep-mode interval when sleeping.
    // --------------------------------------------------------

    bool sleeping = _i2c.isSleeping();

    uint32_t pollInterval = sleeping
        ? KBC_SLEEP_POLL_INTERVAL_MS
        : KBC_POLL_INTERVAL_MS;

    uint32_t& lastPoll = sleeping
        ? _lastSleepPollTime
        : _lastPollTime;

    if ((now - lastPoll) >= pollInterval) {
        lastPoll = now;
        _shiftReg.poll();
    }

    // --------------------------------------------------------
    //  2. LED flash timing update.
    //     update() calls render() internally when flash toggles.
    //     Skip while sleeping — LEDs are off.
    // --------------------------------------------------------

    if (!sleeping) {
        _ledControl.update();
    }

    // --------------------------------------------------------
    //  3. Render pending LED changes from I2C commands.
    //     The I2C ISR sets renderPending rather than calling
    //     render() directly to keep expensive NeoPixel writes
    //     out of interrupt context.
    // --------------------------------------------------------

    if (_i2c.isRenderPending()) {
        _ledControl.render();
        _i2c.clearRenderPending();
    }

    // --------------------------------------------------------
    //  4. INT pin synchronisation.
    //     Asserts or deasserts based on ShiftReg pending state.
    //     Safe to call every loop — only toggles on change.
    // --------------------------------------------------------

    _i2c.syncINT();
}

// ============================================================
//  Subsystem accessors
// ============================================================

KBCShiftReg& KerbalButtonCore::shiftReg() {
    return _shiftReg;
}

KBCLEDControl& KerbalButtonCore::ledControl() {
    return _ledControl;
}

KBCi2C& KerbalButtonCore::i2c() {
    return _i2c;
}

// ============================================================
//  reportFault()
// ============================================================

void KerbalButtonCore::reportFault() {
    _i2c.setFault();
}

/**
 * @file        KBC_ShiftReg.cpp
 * @version     2.0.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Implementation of the KBCShiftReg shift register
 *              abstraction for the KerbalButtonCore library.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the KerbalButtonCore (KBC) library.
 *              Requires: ShiftIn library (InfectedBytes/ArduinoShiftIn)
 *              Hardware: KC-01-1822 v1.1
 *              Protocol: I2C_Protocol_Specification.md v2.4
 */

#include "KBC_ShiftReg.h"
#include <avr/pgmspace.h>

// ============================================================
//  ShiftIn bit → KBC button index remap table
//
//  The ShiftIn<2> library reads 16 bits into a uint16_t with the
//  first bit shifted out placed in the MSB (bit 15). The hardware
//  chain order (U15 feeds U14's SER, U14's QH feeds DATA_IN) and
//  the 74HC165's H-first shift order produce the following mapping
//  from ShiftIn bit position to PCB button label to KBC index:
//
//  ShiftIn bit  PCB button  KBC index
//  ───────────  ──────────  ─────────
//       15       BUTTON09       8
//       14       BUTTON10       9
//       13       BUTTON11      10
//       12       BUTTON12      11
//       11       BUTTON13      12
//       10       BUTTON14      13
//        9       BUTTON15      14
//        8       BUTTON16      15
//        7       BUTTON01       0
//        6       BUTTON02       1
//        5       BUTTON03       2
//        4       BUTTON04       3
//        3       BUTTON05       4
//        2       BUTTON06       5
//        1       BUTTON07       6
//        0       BUTTON08       7
//
//  KBC_SR_BUTTON_MAP[i] = KBC index for ShiftIn bit i.
//  Stored in PROGMEM to preserve SRAM on the ATtiny816.
//
//  24-input switch-group modules (KBC_INPUT_COUNT == 24) add a third
//  register U16 furthest from the MCU (U16 → U15 → U14 → DATA_IN), so
//  its inputs occupy ShiftIn bits 16-23 and map to KBC indices 16-23.
//  The U16 sub-map mirrors the H-first per-byte order of U14/U15
//  (ShiftIn bit 16+i → KBC index 23-i). This ordering is a hardware
//  assumption to confirm against the KC-01-1812 schematic for the
//  Switch Group 1/2 wiring, exactly as the U14/U15 map above is.
// ============================================================

static const uint8_t KBC_SR_BUTTON_MAP[KBC_INPUT_COUNT] PROGMEM = {
    7,   // ShiftIn bit 0  → BUTTON08 → KBC index 7
    6,   // ShiftIn bit 1  → BUTTON07 → KBC index 6
    5,   // ShiftIn bit 2  → BUTTON06 → KBC index 5
    4,   // ShiftIn bit 3  → BUTTON05 → KBC index 4
    3,   // ShiftIn bit 4  → BUTTON04 → KBC index 3
    2,   // ShiftIn bit 5  → BUTTON03 → KBC index 2
    1,   // ShiftIn bit 6  → BUTTON02 → KBC index 1
    0,   // ShiftIn bit 7  → BUTTON01 → KBC index 0
    15,  // ShiftIn bit 8  → BUTTON16 → KBC index 15
    14,  // ShiftIn bit 9  → BUTTON15 → KBC index 14
    13,  // ShiftIn bit 10 → BUTTON14 → KBC index 13
    12,  // ShiftIn bit 11 → BUTTON13 → KBC index 12
    11,  // ShiftIn bit 12 → BUTTON12 → KBC index 11
    10,  // ShiftIn bit 13 → BUTTON11 → KBC index 10
    9,   // ShiftIn bit 14 → BUTTON10 → KBC index 9
    8,   // ShiftIn bit 15 → BUTTON09 → KBC index 8
#if KBC_INPUT_COUNT == 24
    23,  // ShiftIn bit 16 → SW group input → KBC index 23 (B23)
    22,  // ShiftIn bit 17 → SW group input → KBC index 22 (B22)
    21,  // ShiftIn bit 18 → SW group input → KBC index 21 (B21)
    20,  // ShiftIn bit 19 → SW group input → KBC index 20 (B20)
    19,  // ShiftIn bit 20 → SW group input → KBC index 19 (B19)
    18,  // ShiftIn bit 21 → SW group input → KBC index 18 (B18)
    17,  // ShiftIn bit 22 → SW group input → KBC index 17 (B17)
    16   // ShiftIn bit 23 → SW group input → KBC index 16 (B16)
#endif
};

// ============================================================
//  Constructor
// ============================================================

KBCShiftReg::KBCShiftReg()
    : _liveState(0)
    , _latchedState(0)
    , _changeMask(0)
    , _intPending(false)
{
    memset(_debounceCount,     0, sizeof(_debounceCount));
    memset(_debounceCandidate, 0, sizeof(_debounceCandidate));
}

// ============================================================
//  begin()
// ============================================================

void KBCShiftReg::begin() {
    // Initialise ShiftIn library with hardware pins.
    // Argument order: ploadPin, clockEnablePin, dataPin, clockPin
    _shift.begin(
        KBC_PIN_SR_LOAD,
        KBC_PIN_SR_CLK_EN,
        KBC_PIN_SR_DATA,
        KBC_PIN_SR_CLK
    );

    // Use a slightly longer pulse width than the default 5us for
    // reliable operation across the full operating temperature range.
    _shift.setPulseWidth(KBC_SR_LOAD_PULSE_US);

    // Clear all state
    _liveState    = 0;
    _latchedState = 0;
    _changeMask   = 0;
    _intPending   = false;
    memset(_debounceCount,     0, sizeof(_debounceCount));
    memset(_debounceCandidate, 0, sizeof(_debounceCandidate));
}

// ============================================================
//  poll()
// ============================================================

bool KBCShiftReg::poll() {
    uint32_t raw = _readRaw();
    bool anyChanged = false;

    // Per-button debounce with independent candidate tracking.
    // Each button tracks its own candidate bit independently, so
    // a transition on one button does not reset the debounce
    // progress of any other button. Multi-button presses are
    // handled correctly regardless of their relative timing.
    for (uint8_t i = 0; i < KBC_INPUT_COUNT; i++) {
        bool rawBit  = (raw >> i) & 0x01;
        bool liveBit = (_liveState >> i) & 0x01;

        if (rawBit != _debounceCandidate[i]) {
            // Raw changed from candidate — update candidate and
            // reset this button's counter. Other buttons unaffected.
            _debounceCandidate[i] = rawBit;
            _debounceCount[i]     = 0;
        } else if (rawBit != liveBit) {
            // Raw stable at candidate, but candidate differs from live
            // state — this button is in a confirmed transition.
            _debounceCount[i]++;

            if (_debounceCount[i] >= KBC_DEBOUNCE_COUNT) {
                // Debounce threshold reached — commit state change
                if (rawBit) {
                    _liveState |=  (uint32_t)(1UL << i);
                } else {
                    _liveState &= ~(uint32_t)(1UL << i);
                }

                // Accumulate into change mask (OR so rapid
                // press/release between reads is not lost)
                _changeMask |= (uint32_t)(1UL << i);

                // Reset this button's counter
                _debounceCount[i] = 0;

                // Signal INT pending
                _intPending = true;
                anyChanged  = true;
            }
        } else {
            // Raw matches live state — no transition, keep counter clear
            _debounceCount[i] = 0;
        }
    }

    return anyChanged;
}

// ============================================================
//  isIntPending()
// ============================================================

bool KBCShiftReg::isIntPending() const {
    return _intPending;
}

// ============================================================
//  buildPayload()
//
//  Serializes the latched button state into the button-event payload.
//  Layout (KBC_INPUT_BYTES bytes per plane):
//    [events 0-7][events 8-15]([events 16-23])
//    [change 0-7][change 8-15]([change 16-23])
//  Event bytes carry current pressed state (1 = pressed); the
//  controller ANDs events with the change mask to identify which
//  inputs changed and what they changed to.
// ============================================================

void KBCShiftReg::buildPayload(uint8_t* buf) {
    // Step 1: Snapshot live state into latch
    _latchedState = _liveState;

    // Step 2: Emit events plane then change plane, byte per 8 inputs
    for (uint8_t i = 0; i < KBC_INPUT_BYTES; i++) {
        buf[i]                  = (uint8_t)((_latchedState >> (8 * i)) & 0xFF);
        buf[KBC_INPUT_BYTES + i] = (uint8_t)((_changeMask   >> (8 * i)) & 0xFF);
    }

    // Step 3: Clear change mask and INT flag
    _changeMask = 0;
    _intPending = false;

    // Step 4: Re-evaluate — if live state has already diverged
    // from the latch (button changed during this transaction),
    // immediately re-assert INT so the controller gets another read.
    if (_liveState != _latchedState) {
        _changeMask = _liveState ^ _latchedState;
        _intPending = true;
    }
}

// ============================================================
//  getButtonState()
// ============================================================

bool KBCShiftReg::getButtonState(uint8_t index) const {
    if (index >= KBC_INPUT_COUNT) return false;
    return (_liveState >> index) & 0x01;
}

// ============================================================
//  getLiveState()
// ============================================================

uint32_t KBCShiftReg::getLiveState() const {
    return _liveState;
}

// ============================================================
//  _readRaw() — internal
// ============================================================

uint32_t KBCShiftReg::_readRaw() {
    return _remap((uint32_t)_shift.read());
}

// ============================================================
//  _remap() — internal
//
//  Translates a raw ShiftIn<2> uint16_t reading into a uint16_t
//  where bit N corresponds to KBC button index N.
//
//  Iterates through all 16 ShiftIn bit positions, looks up the
//  corresponding KBC index from the PROGMEM remap table, and
//  builds the remapped result one bit at a time.
// ============================================================

uint32_t KBCShiftReg::_remap(uint32_t raw) {
    uint32_t result = 0;

    for (uint8_t i = 0; i < KBC_INPUT_COUNT; i++) {
        // Read raw bit i
        if ((raw >> i) & 0x01) {
            // Look up its KBC index from PROGMEM table
            uint8_t kbcIndex = pgm_read_byte(&KBC_SR_BUTTON_MAP[i]);
            result |= (uint32_t)(1UL << kbcIndex);
        }
    }

    return result;
}

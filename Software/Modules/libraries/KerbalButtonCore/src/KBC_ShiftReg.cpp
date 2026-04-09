/**
 * @file        KBC_ShiftReg.cpp
 * @version     1.0
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
 *              Protocol: KBC_Protocol_Spec.md v1.1
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
// ============================================================

static const uint8_t KBC_SR_BUTTON_MAP[KBC_BUTTON_COUNT] PROGMEM = {
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
    8    // ShiftIn bit 15 → BUTTON09 → KBC index 8
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
    uint16_t raw = _readRaw();
    bool anyChanged = false;

    // Per-button debounce with independent candidate tracking.
    // Each button tracks its own candidate bit independently, so
    // a transition on one button does not reset the debounce
    // progress of any other button. Multi-button presses are
    // handled correctly regardless of their relative timing.
    for (uint8_t i = 0; i < KBC_BUTTON_COUNT; i++) {
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
                    _liveState |=  (uint16_t)(1 << i);
                } else {
                    _liveState &= ~(uint16_t)(1 << i);
                }

                // Accumulate into change mask (OR so rapid
                // press/release between reads is not lost)
                _changeMask |= (uint16_t)(1 << i);

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
//  getButtonPacket()
// ============================================================

void KBCShiftReg::getButtonPacket(KBCButtonPacket& pkt) {
    // Step 1: Snapshot live state into latch
    _latchedState = _liveState;

    // Step 2: Build packet from latch and accumulated change mask
    pkt.currentState = _latchedState;
    pkt.changeMask   = _changeMask;

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
    if (index >= KBC_BUTTON_COUNT) return false;
    return (_liveState >> index) & 0x01;
}

// ============================================================
//  getLiveState()
// ============================================================

uint16_t KBCShiftReg::getLiveState() const {
    return _liveState;
}

// ============================================================
//  _readRaw() — internal
// ============================================================

uint16_t KBCShiftReg::_readRaw() {
    return _remap(_shift.read());
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

uint16_t KBCShiftReg::_remap(uint16_t raw) {
    uint16_t result = 0;

    for (uint8_t i = 0; i < KBC_BUTTON_COUNT; i++) {
        // Read raw bit i
        if ((raw >> i) & 0x01) {
            // Look up its KBC index from PROGMEM table
            uint8_t kbcIndex = pgm_read_byte(&KBC_SR_BUTTON_MAP[i]);
            result |= (uint16_t)(1 << kbcIndex);
        }
    }

    return result;
}

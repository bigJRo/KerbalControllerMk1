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
 *              Hardware: KC-01-1801/1802 and KC-01-1811/1812
 *              Protocol: I2C_Protocol_Specification.md v2.9
 */

#include "KBC_ShiftReg.h"
#include <avr/pgmspace.h>

// ============================================================
//  ShiftIn bit → KBC button index remap table
//
//  ShiftIn::read() places the FIRST bit clocked out in the MSB
//  (result |= value << ((dataWidth-1) - i)). The register NEAREST the MCU
//  (BUTTON00-07, its QH wired directly to DATA_IN) is clocked out first,
//  and each 74HC165 presents its H input first. So for a 16-bit read the
//  near register fills the HIGH byte (H→bit15 .. A→bit8) and the far
//  register (BUTTON08-15) fills the LOW byte (H→bit7 .. A→bit0). Physical
//  BUTTONnn maps directly to KBC index nn.
//
//  ShiftIn bit  PCB button  KBC index
//  ───────────  ──────────  ─────────
//       15       BUTTON07       7
//       14       BUTTON06       6
//       13       BUTTON05       5
//       12       BUTTON04       4
//       11       BUTTON03       3
//       10       BUTTON02       2
//        9       BUTTON01       1
//        8       BUTTON00       0
//        7       BUTTON15      15
//        6       BUTTON14      14
//        5       BUTTON13      13
//        4       BUTTON12      12
//        3       BUTTON11      11
//        2       BUTTON10      10
//        1       BUTTON09       9
//        0       BUTTON08       8
//
//  KBC_SR_BUTTON_MAP[i] = KBC index for ShiftIn bit i.
//  Stored in PROGMEM to preserve SRAM on the ATtiny816.
//
//  24-input switch-group modules (KBC_INPUT_COUNT == 24) add a third
//  register (BUTTON16-23, A=BUTTON16 .. H=BUTTON23) furthest from the MCU.
//  With dataWidth = 24 the byte positions shift: the near register
//  (BUTTON00-07) moves to bits 16-23, the middle (BUTTON08-15) to bits
//  8-15, and the third register to bits 0-7 — so the 16- and 24-input
//  tables are DISTINCT, not one appended to the other. Verified against
//  the KC-01-1811 (Wide Button) schematic.
// ============================================================

static const uint8_t KBC_SR_BUTTON_MAP[KBC_INPUT_COUNT] PROGMEM = {
#if KBC_INPUT_COUNT == 24
    // dataWidth = 24: 3rd register (BUTTON16-23) in bits 0-7,
    // middle (BUTTON08-15) in bits 8-15, near (BUTTON00-07) in bits 16-23.
    16,  // ShiftIn bit 0  → BUTTON16 → KBC index 16
    17,  // ShiftIn bit 1  → BUTTON17 → KBC index 17
    18,  // ShiftIn bit 2  → BUTTON18 → KBC index 18
    19,  // ShiftIn bit 3  → BUTTON19 → KBC index 19
    20,  // ShiftIn bit 4  → BUTTON20 → KBC index 20
    21,  // ShiftIn bit 5  → BUTTON21 → KBC index 21
    22,  // ShiftIn bit 6  → BUTTON22 → KBC index 22
    23,  // ShiftIn bit 7  → BUTTON23 → KBC index 23
    8,   // ShiftIn bit 8  → BUTTON08 → KBC index 8
    9,   // ShiftIn bit 9  → BUTTON09 → KBC index 9
    10,  // ShiftIn bit 10 → BUTTON10 → KBC index 10
    11,  // ShiftIn bit 11 → BUTTON11 → KBC index 11
    12,  // ShiftIn bit 12 → BUTTON12 → KBC index 12
    13,  // ShiftIn bit 13 → BUTTON13 → KBC index 13
    14,  // ShiftIn bit 14 → BUTTON14 → KBC index 14
    15,  // ShiftIn bit 15 → BUTTON15 → KBC index 15
    0,   // ShiftIn bit 16 → BUTTON00 → KBC index 0
    1,   // ShiftIn bit 17 → BUTTON01 → KBC index 1
    2,   // ShiftIn bit 18 → BUTTON02 → KBC index 2
    3,   // ShiftIn bit 19 → BUTTON03 → KBC index 3
    4,   // ShiftIn bit 20 → BUTTON04 → KBC index 4
    5,   // ShiftIn bit 21 → BUTTON05 → KBC index 5
    6,   // ShiftIn bit 22 → BUTTON06 → KBC index 6
    7    // ShiftIn bit 23 → BUTTON07 → KBC index 7
#else
    // dataWidth = 16: far register (BUTTON08-15) in bits 0-7,
    // near (BUTTON00-07) in bits 8-15.
    8,   // ShiftIn bit 0  → BUTTON08 → KBC index 8
    9,   // ShiftIn bit 1  → BUTTON09 → KBC index 9
    10,  // ShiftIn bit 2  → BUTTON10 → KBC index 10
    11,  // ShiftIn bit 3  → BUTTON11 → KBC index 11
    12,  // ShiftIn bit 4  → BUTTON12 → KBC index 12
    13,  // ShiftIn bit 5  → BUTTON13 → KBC index 13
    14,  // ShiftIn bit 6  → BUTTON14 → KBC index 14
    15,  // ShiftIn bit 7  → BUTTON15 → KBC index 15
    0,   // ShiftIn bit 8  → BUTTON00 → KBC index 0
    1,   // ShiftIn bit 9  → BUTTON01 → KBC index 1
    2,   // ShiftIn bit 10 → BUTTON02 → KBC index 2
    3,   // ShiftIn bit 11 → BUTTON03 → KBC index 3
    4,   // ShiftIn bit 12 → BUTTON04 → KBC index 4
    5,   // ShiftIn bit 13 → BUTTON05 → KBC index 5
    6,   // ShiftIn bit 14 → BUTTON06 → KBC index 6
    7    // ShiftIn bit 15 → BUTTON07 → KBC index 7
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

    // Apply the KBC load pulse width (KBC_SR_LOAD_PULSE_US = 5us, equal
    // to the ShiftIn default) explicitly for reliable operation across
    // the full operating temperature range.
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
//  Translates a raw ShiftIn<KBC_SHIFTREG_COUNT> uint32_t reading into a
//  uint32_t where bit N corresponds to KBC button index N.
//
//  Iterates through all KBC_INPUT_COUNT ShiftIn bit positions (16 or 24),
//  looks up the corresponding KBC index from the PROGMEM remap table, and
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

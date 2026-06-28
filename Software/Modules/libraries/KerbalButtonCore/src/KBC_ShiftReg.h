/**
 * @file        KBC_ShiftReg.h
 * @version     2.0.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Shift register abstraction for the KerbalButtonCore library.
 *              Manages reading KBC_INPUT_COUNT button inputs from
 *              KBC_SHIFTREG_COUNT daisy-chained SN74HC165PWR shift
 *              registers using the ShiftIn library
 *              (InfectedBytes/ArduinoShiftIn): two registers (U14, U15)
 *              for the default 16 inputs, or three (U14, U15, U16) for
 *              the 24-input switch-group modules.
 *
 *              Provides dual-buffer latching to guarantee every button
 *              press and release edge is captured and reported to the
 *              I2C controller, even if buttons change between reads.
 *
 *              All button state is expressed using canonical KBC indices
 *              (0 .. KBC_INPUT_COUNT-1). The ShiftIn bit-to-index
 *              remapping is handled internally and never exposed to callers.
 *
 *              Button index conventions:
 *                KBC index 0-11  : NeoPixel RGB buttons (BUTTON01-12 on PCB)
 *                KBC index 12-15 : Discrete LED buttons (BUTTON13-16 on PCB)
 *                KBC index 16-23 : Switch-group discrete inputs (U16), no LED
 *                                  (24-input modules only)
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the KerbalButtonCore (KBC) library.
 *              Requires: ShiftIn library (InfectedBytes/ArduinoShiftIn)
 *              Hardware: KC-01-1822 v1.1
 *              Protocol: I2C_Protocol_Specification.md v2.4
 */

#ifndef KBC_SHIFTREG_H
#define KBC_SHIFTREG_H

#include <Arduino.h>
#include <ShiftIn.h>
#include "KBC_Config.h"
#include "KBC_Protocol.h"

// ============================================================
//  KBCShiftReg class
// ============================================================

/**
 * @class  KBCShiftReg
 * @brief  Reads KBC_INPUT_COUNT button inputs from KBC_SHIFTREG_COUNT
 *         daisy-chained 74HC165 shift registers and manages dual-buffer
 *         state latching.
 *
 *         Call begin() once in setup().
 *         Call poll() on a regular interval (KBC_POLL_INTERVAL_MS).
 *         Use buildPayload() to serialize the latched state into the
 *         button-event payload for transmission to the I2C controller.
 *
 * @note   The dual-buffer design guarantees that every press and
 *         release edge generates at least one INT assertion, even
 *         if the controller is slow to respond. See buildPayload()
 *         for full latch/clear behavior.
 */
class KBCShiftReg {
public:

    // --------------------------------------------------------
    //  Construction
    // --------------------------------------------------------

    KBCShiftReg();

    // --------------------------------------------------------
    //  Initialisation
    // --------------------------------------------------------

    /**
     * @brief  Initialise shift register pins and clear all state.
     *         Must be called once in setup() before any other method.
     */
    void begin();

    // --------------------------------------------------------
    //  Polling
    // --------------------------------------------------------

    /**
     * @brief  Poll all KBC_INPUT_COUNT button inputs with debounce.
     *
     *         Must be called at regular intervals defined by
     *         KBC_POLL_INTERVAL_MS. Each call reads the raw shift
     *         register state and applies the debounce counter.
     *         A state change is registered only after
     *         KBC_DEBOUNCE_COUNT consecutive reads agree.
     *
     *         When a debounced state change is detected:
     *           - The live buffer is updated
     *           - The change is accumulated into the pending change mask
     *           - intPending is set true
     *
     * @return true if any debounced button state changed this poll cycle.
     */
    bool poll();

    // --------------------------------------------------------
    //  INT line management
    // --------------------------------------------------------

    /**
     * @brief  Returns true if there is unread button state data
     *         pending for the I2C controller (INT line should be low).
     */
    bool isIntPending() const;

    // --------------------------------------------------------
    //  Button state access
    // --------------------------------------------------------

    /**
     * @brief  Serialize the latched button state into the button-event
     *         payload for transmission to the I2C controller.
     *
     *         Writes KBC_BUTTON_PAYLOAD_SIZE bytes: KBC_INPUT_BYTES
     *         event bytes (current pressed state, byte 0 = inputs 0-7,
     *         byte 1 = inputs 8-15, byte 2 = inputs 16-23) followed by
     *         KBC_INPUT_BYTES change-mask bytes in the same order.
     *
     *         On call:
     *           1. Snapshots the live buffer into the latch buffer.
     *           2. Writes events + change planes into buf.
     *           3. Clears the pending change mask.
     *           4. Clears intPending.
     *           5. Re-evaluates intPending — if the live buffer has
     *              changed again since the snapshot, intPending is
     *              immediately re-asserted.
     *
     * @param  buf  Destination array, at least KBC_BUTTON_PAYLOAD_SIZE bytes.
     */
    void buildPayload(uint8_t* buf);

    /**
     * @brief  Returns the current debounced state of button at KBC index.
     * @param  index  KBC button index (0 .. KBC_INPUT_COUNT-1).
     * @return true if button is currently pressed.
     */
    bool getButtonState(uint8_t index) const;

    /**
     * @brief  Returns the full live button state bitmask.
     *         Bit N corresponds to KBC button index N.
     *         1 = pressed, 0 = released.
     */
    uint32_t getLiveState() const;

private:

    // --------------------------------------------------------
    //  ShiftIn instance (2 registers for 16 inputs, 3 for 24)
    // --------------------------------------------------------

    ShiftIn<KBC_SHIFTREG_COUNT> _shift;

    // --------------------------------------------------------
    //  Dual-buffer state (up to 24 inputs — uint32_t)
    // --------------------------------------------------------

    /** @brief Live button state. Updated on each debounced change. */
    uint32_t _liveState;

    /** @brief Latched state snapshot. Sent to controller on read. */
    uint32_t _latchedState;

    /**
     * @brief  Accumulated change mask. Tracks all buttons that have
     *         changed since the last buildPayload() call.
     *         Bit N = 1 means button N changed at least once.
     *         OR-accumulated so rapid press/release is not lost.
     */
    uint32_t _changeMask;

    /** @brief True when there is unread state pending for the controller. */
    bool _intPending;

    // --------------------------------------------------------
    //  Debounce state
    // --------------------------------------------------------

    /**
     * @brief  Per-button debounce counter array.
     *         Incremented each poll cycle that the raw bit matches
     *         the candidate but differs from live state.
     *         Reset when the raw bit changes or the threshold is reached.
     */
    uint8_t _debounceCount[KBC_INPUT_COUNT];

    /**
     * @brief  Per-button candidate state array.
     *         Each entry holds the last seen raw bit for that button.
     *         Tracked independently per button so a transition on one
     *         button does not reset debounce progress on any other.
     */
    uint8_t _debounceCandidate[KBC_INPUT_COUNT];

    // --------------------------------------------------------
    //  Internal helpers
    // --------------------------------------------------------

    /**
     * @brief  Read raw shift-register state from ShiftIn and remap to
     *         canonical KBC button indices.
     * @return Bitmask in KBC index order (bit 0 = KBC index 0).
     */
    uint32_t _readRaw();

    /**
     * @brief  Remap a raw ShiftIn value to KBC index order.
     *         Applies the hardware-defined bit reordering from
     *         KBC_SR_BUTTON_MAP lookup table.
     * @param  raw  Raw value from ShiftIn<KBC_SHIFTREG_COUNT>::read()
     * @return Remapped bitmask in KBC index order.
     */
    uint32_t _remap(uint32_t raw);
};

#endif // KBC_SHIFTREG_H

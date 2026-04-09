/**
 * @file        KBC_ShiftReg.h
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Shift register abstraction for the KerbalButtonCore library.
 *              Manages reading 16 button inputs from two daisy-chained
 *              SN74HC165PWR shift registers (U14, U15) using the
 *              ShiftIn<2> library (InfectedBytes/ArduinoShiftIn).
 *
 *              Provides dual-buffer latching to guarantee every button
 *              press and release edge is captured and reported to the
 *              I2C controller, even if buttons change between reads.
 *
 *              All button state is expressed using canonical KBC indices
 *              (0-15). The ShiftIn bit-to-index remapping is handled
 *              internally and never exposed to callers.
 *
 *              Button index conventions:
 *                KBC index 0-11  : NeoPixel RGB buttons (BUTTON01-12 on PCB)
 *                KBC index 12-15 : Discrete LED buttons (BUTTON13-16 on PCB)
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the KerbalButtonCore (KBC) library.
 *              Requires: ShiftIn library (InfectedBytes/ArduinoShiftIn)
 *              Hardware: KC-01-1822 v1.1
 *              Protocol: KBC_Protocol_Spec.md v1.1
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
 * @brief  Reads 16 button inputs from two daisy-chained 74HC165
 *         shift registers and manages dual-buffer state latching.
 *
 *         Call begin() once in setup().
 *         Call poll() on a regular interval (KBC_POLL_INTERVAL_MS).
 *         Use getButtonPacket() to retrieve the latched state for
 *         transmission to the I2C controller.
 *
 * @note   The dual-buffer design guarantees that every press and
 *         release edge generates at least one INT assertion, even
 *         if the controller is slow to respond. See getButtonPacket()
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
     * @brief  Poll all 16 button inputs with debounce.
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
     * @brief  Build and return the 4-byte button state packet for
     *         transmission to the I2C controller.
     *
     *         On call:
     *           1. Snapshots the live buffer into the latch buffer.
     *           2. Copies the pending change mask into the packet.
     *           3. Clears the pending change mask.
     *           4. Clears intPending.
     *           5. Re-evaluates intPending — if the live buffer has
     *              changed again since the snapshot, intPending is
     *              immediately re-asserted.
     *
     *         The returned packet is valid for immediate I2C transmission.
     *
     * @param  pkt  Reference to a KBCButtonPacket to populate.
     */
    void getButtonPacket(KBCButtonPacket& pkt);

    /**
     * @brief  Returns the current debounced state of button at KBC index.
     * @param  index  KBC button index (0-15).
     * @return true if button is currently pressed.
     */
    bool getButtonState(uint8_t index) const;

    /**
     * @brief  Returns the full 16-bit live button state bitmask.
     *         Bit N corresponds to KBC button index N.
     *         1 = pressed, 0 = released.
     */
    uint16_t getLiveState() const;

private:

    // --------------------------------------------------------
    //  ShiftIn instance
    // --------------------------------------------------------

    ShiftIn<2> _shift;

    // --------------------------------------------------------
    //  Dual-buffer state
    // --------------------------------------------------------

    /** @brief Live button state. Updated on each debounced change. */
    uint16_t _liveState;

    /** @brief Latched state snapshot. Sent to controller on read. */
    uint16_t _latchedState;

    /**
     * @brief  Accumulated change mask. Tracks all buttons that have
     *         changed since the last getButtonPacket() call.
     *         Bit N = 1 means button N changed at least once.
     *         OR-accumulated so rapid press/release is not lost.
     */
    uint16_t _changeMask;

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
    uint8_t _debounceCount[KBC_BUTTON_COUNT];

    /**
     * @brief  Per-button candidate state array.
     *         Each entry holds the last seen raw bit for that button.
     *         Tracked independently per button so a transition on one
     *         button does not reset debounce progress on any other.
     */
    uint8_t _debounceCandidate[KBC_BUTTON_COUNT];

    // --------------------------------------------------------
    //  Internal helpers
    // --------------------------------------------------------

    /**
     * @brief  Read raw 16-bit state from ShiftIn and remap to
     *         canonical KBC button indices.
     * @return 16-bit bitmask in KBC index order (bit 0 = KBC index 0).
     */
    uint16_t _readRaw();

    /**
     * @brief  Remap a raw ShiftIn uint16_t value to KBC index order.
     *         Applies the hardware-defined bit reordering from
     *         KBC_SR_BUTTON_MAP lookup table.
     * @param  raw  Raw value from ShiftIn<2>::read()
     * @return Remapped 16-bit bitmask in KBC index order.
     */
    uint16_t _remap(uint16_t raw);
};

#endif // KBC_SHIFTREG_H

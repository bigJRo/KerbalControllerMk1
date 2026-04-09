/**
 * @file        KBC_I2C.h
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C target handler for the KerbalButtonCore library.
 *              Manages all I2C communication between the KBC button
 *              module (target) and the system controller.
 *
 *              Implements the full KBC command set defined in
 *              KBC_Protocol_Spec.md v1.1:
 *                CMD_GET_IDENTITY    — respond with module identity packet
 *                CMD_SET_LED_STATE   — apply 8-byte nibble-packed LED update
 *                CMD_SET_BRIGHTNESS  — set ENABLED state brightness
 *                CMD_BULB_TEST       — trigger all-white LED test
 *                CMD_SLEEP           — enter low power mode
 *                CMD_WAKE            — resume normal operation
 *                CMD_RESET           — clear all state and LEDs
 *                CMD_ACK_FAULT       — clear module fault flag
 *
 *              Wire library callbacks (onReceive, onRequest) are
 *              routed through static trampoline functions to a single
 *              global instance pointer. Only one KBCi2C instance
 *              should exist per sketch.
 *
 *              INT pin (KBC_PIN_INT) is asserted low when button state
 *              changes are pending and cleared when the controller
 *              completes a read transaction.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the KerbalButtonCore (KBC) library.
 *              Requires: Wire library (megaTinyCore)
 *              Hardware: KC-01-1822 v1.1
 *              Protocol: KBC_Protocol_Spec.md v1.1
 */

#ifndef KBC_I2C_H
#define KBC_I2C_H

#include <Arduino.h>
#include <Wire.h>
#include "KBC_Config.h"
#include "KBC_Protocol.h"
#include "KBC_ShiftReg.h"
#include "KBC_LEDControl.h"

// ============================================================
//  KBCi2C class
// ============================================================

/**
 * @class  KBCi2C
 * @brief  I2C target handler for a KBC button module.
 *
 *         Owns the Wire library interaction and INT pin management.
 *         Holds references to KBCShiftReg and KBCLEDControl which
 *         it dispatches commands to.
 *
 *         Call begin() once in setup() after Wire.begin().
 *         Call syncINT() regularly from the main loop to keep the
 *         INT pin in sync with pending button state.
 *         Call handleSleep() from the main loop to apply sleep
 *         mode polling rate changes.
 *
 * @note   Only one instance of this class should exist per sketch.
 *         The Wire callback trampolines use a global pointer to
 *         route calls to this instance.
 */
class KBCi2C {
public:

    // --------------------------------------------------------
    //  Construction
    // --------------------------------------------------------

    /**
     * @brief  Construct a KBCi2C handler.
     *
     * @param  shiftReg      Reference to the KBCShiftReg instance.
     * @param  ledControl    Reference to the KBCLEDControl instance.
     * @param  moduleTypeId  Module Type ID (KBC_TYPE_* from KBC_Protocol.h).
     * @param  capFlags      Capability flags bitmask (KBC_CAP_*).
     */
    KBCi2C(KBCShiftReg&   shiftReg,
           KBCLEDControl& ledControl,
           uint8_t        moduleTypeId,
           uint8_t        capFlags = 0);

    // --------------------------------------------------------
    //  Initialisation
    // --------------------------------------------------------

    /**
     * @brief  Initialise the I2C target interface and INT pin.
     *
     *         Must be called once in setup() AFTER Wire.begin(address)
     *         has been called in the module sketch.
     *
     *         Registers Wire onReceive and onRequest callbacks,
     *         configures the INT pin as output and deasserts it,
     *         and clears all pending state.
     */
    void begin();

    // --------------------------------------------------------
    //  Main loop services
    // --------------------------------------------------------

    /**
     * @brief  Synchronise the INT pin with pending button state.
     *
     *         Must be called regularly from the main loop.
     *         Asserts INT low if KBCShiftReg has pending state.
     *         Deasserts INT high if no state is pending.
     *         Safe to call at high frequency — only toggles the
     *         pin when the state has actually changed.
     */
    void syncINT();

    /**
     * @brief  Returns true if the module is currently in sleep mode.
     */
    bool isSleeping() const;

    /**
     * @brief  Returns true if a fault condition is active.
     */
    bool hasFault() const;

    /**
     * @brief  Set the fault flag. Call from application code when
     *         a hardware or software fault is detected.
     *         The fault is reported to the controller via the
     *         capability flags byte in the identity response.
     */
    void setFault();

    /**
     * @brief  Returns true if a pending command requires a render.
     *         Call render on KBCLEDControl if this returns true,
     *         then call clearRenderPending().
     */
    bool isRenderPending() const;

    /**
     * @brief  Clear the render pending flag after rendering.
     */
    void clearRenderPending();

private:

    // --------------------------------------------------------
    //  References to subsystems
    // --------------------------------------------------------

    KBCShiftReg&   _shiftReg;
    KBCLEDControl& _ledControl;

    // --------------------------------------------------------
    //  Module identity
    // --------------------------------------------------------

    uint8_t _moduleTypeId;
    uint8_t _capFlags;

    // --------------------------------------------------------
    //  INT pin state tracking
    // --------------------------------------------------------

    /** @brief Current driven state of INT pin (true = asserted low). */
    bool _intAsserted;

    // --------------------------------------------------------
    //  Command receive buffer
    //
    //  Holds the most recently received command byte and payload.
    //  Max payload is 8 bytes (CMD_SET_LED_STATE).
    //  Sized to command byte + maximum payload.
    // --------------------------------------------------------

    static const uint8_t _CMD_BUF_SIZE = 1 + KBC_LED_PAYLOAD_SIZE;
    uint8_t _cmdBuf[_CMD_BUF_SIZE];
    uint8_t _cmdLen;

    // --------------------------------------------------------
    //  Module state flags
    // --------------------------------------------------------

    bool _sleeping;
    bool _fault;
    bool _renderPending;

    // --------------------------------------------------------
    //  INT pin helpers
    // --------------------------------------------------------

    void _assertINT();
    void _clearINT();

    // --------------------------------------------------------
    //  Command dispatch
    // --------------------------------------------------------

    /**
     * @brief  Dispatch a received command from the Wire onReceive buffer.
     *         Called from the static trampoline after the ISR completes.
     *         Parses _cmdBuf and calls the appropriate handler.
     */
    void _dispatch();

    void _handleGetIdentity();
    void _handleSetLEDState();
    void _handleSetBrightness();
    void _handleBulbTest();
    void _handleSleep();
    void _handleWake();
    void _handleReset();
    void _handleAckFault();

    /**
     * @brief  Build and send the button state packet in response to
     *         a controller read request (Wire onRequest callback).
     *         Clears INT on completion and re-asserts if needed.
     */
    void _sendButtonPacket();

    /**
     * @brief  Build and send the identity response packet.
     *         Called from _handleGetIdentity() via the onRequest path.
     */
    void _sendIdentityPacket();

    // --------------------------------------------------------
    //  Response queuing
    //
    //  The Wire onRequest callback must call Wire.write() to
    //  queue the response. Since the response type depends on
    //  what command was last received, we track the pending
    //  response type here.
    // --------------------------------------------------------

    enum ResponseType : uint8_t {
        RESPONSE_NONE     = 0,
        RESPONSE_BUTTONS  = 1,
        RESPONSE_IDENTITY = 2
    };

    volatile ResponseType _pendingResponse;

    // --------------------------------------------------------
    //  Static trampoline functions for Wire callbacks
    // --------------------------------------------------------

    static void _onReceiveTrampoline(int numBytes);
    static void _onRequestTrampoline();

    // --------------------------------------------------------
    //  Global instance pointer for trampolines
    // --------------------------------------------------------

    static KBCi2C* _instance;
};

#endif // KBC_I2C_H

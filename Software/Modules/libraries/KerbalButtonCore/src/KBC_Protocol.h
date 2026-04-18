/**
 * @file        KBC_Protocol.h
 * @version     1.1.0
 * @date        2026-04-09
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C protocol constants, command bytes, LED state nibble
 *              values, packet sizes, and data structures for the
 *              KerbalButtonCore library.
 *
 *              This file defines the complete communication contract
 *              between KBC target modules and the system controller.
 *              It is intentionally free of any transport dependencies
 *              (no Wire library includes) so it can be included by
 *              both target module firmware and any future controller-side
 *              library without coupling to a specific I2C implementation.
 *
 *              Full protocol specification: KBC_Protocol_Spec.md v1.1
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the KerbalButtonCore (KBC) library.
 *              Terminology follows the 2021 NXP I2C specification:
 *                Controller = main microcontroller (initiates transactions)
 *                Target     = KBC button module (responds to transactions)
 */

#ifndef KBC_PROTOCOL_H
#define KBC_PROTOCOL_H

#include <stdint.h>
#include <KerbalModuleCommon.h>

// ============================================================
//  I2C addressing
// ============================================================

/** @brief First target address in the KBC address range. */
#define KBC_I2C_ADDR_BASE           0x20

/** @brief Last target address in the KBC address range. */
#define KBC_I2C_ADDR_MAX            0x2E

/** @brief Total number of addressable KBC slots on the bus. */
#define KBC_I2C_MAX_DEVICES         15

// ============================================================
//  Command bytes — aliases for KMC_CMD_* from KerbalModuleCommon
//
//  KMC_CMD_* are the canonical names. KBC_CMD_* retained for
//  backward compatibility with existing library code.
//  Note: KBC only exposes 0x01-0x08; full command set including
//  0x09-0x0D is in KerbalModuleCommon.
// ============================================================

#define KBC_CMD_GET_IDENTITY        KMC_CMD_GET_IDENTITY
#define KBC_CMD_SET_LED_STATE       KMC_CMD_SET_LED_STATE
#define KBC_CMD_SET_BRIGHTNESS      KMC_CMD_SET_BRIGHTNESS
#define KBC_CMD_BULB_TEST           KMC_CMD_BULB_TEST
#define KBC_CMD_SLEEP               KMC_CMD_SLEEP
#define KBC_CMD_WAKE                KMC_CMD_WAKE
#define KBC_CMD_RESET               KMC_CMD_RESET
#define KBC_CMD_ACK_FAULT           KMC_CMD_ACK_FAULT

// ============================================================
//  LED state nibble values — aliases for KMC_LED_*
// ============================================================

// LED state nibble values — aliases for KMC_LED_* from KerbalModuleCommon.
// KMC_LED_* are the canonical names; KBC_LED_* retained for compatibility.
#define KBC_LED_OFF             KMC_LED_OFF
#define KBC_LED_ENABLED         KMC_LED_ENABLED
#define KBC_LED_ACTIVE          KMC_LED_ACTIVE
#define KBC_LED_WARNING         KMC_LED_WARNING
#define KBC_LED_ALERT           KMC_LED_ALERT
#define KBC_LED_ARMED           KMC_LED_ARMED
#define KBC_LED_PARTIAL_DEPLOY  KMC_LED_PARTIAL_DEPLOY

// ============================================================
//  Packet sizes
// ============================================================

/** @brief Button state response packet size in bytes (target → controller). */
#define KBC_BUTTON_PACKET_SIZE      4

/**
 * @brief LED state command payload size in bytes (controller → target).
 *        Does not include the command byte itself.
 *        16 buttons × 4 bits = 64 bits = 8 bytes.
 */
#define KBC_LED_PAYLOAD_SIZE        KMC_LED_PAYLOAD_SIZE

/** @brief Identity response packet size in bytes. Alias for KMC_IDENTITY_SIZE. */
#define KBC_IDENTITY_PACKET_SIZE    KMC_IDENTITY_SIZE

/** @brief Total bytes on wire for a CMD_SET_LED_STATE transaction. */
#define KBC_LED_WIRE_SIZE           (1 + KBC_LED_PAYLOAD_SIZE)

// ============================================================
//  Button count
// ============================================================

/** @brief Number of button inputs per module. */
#define KBC_BUTTON_COUNT            16

/** @brief Number of RGB LED buttons per module (NeoPixel chain). */
#define KBC_RGB_BUTTON_COUNT        12

/** @brief Number of discrete LED buttons per module. */
#define KBC_DISCRETE_BUTTON_COUNT   4

// ============================================================
//  Capability flags — aliases for KMC_CAP_* from KerbalModuleCommon
// ============================================================

/** @brief Module supports extended LED states (0x3-0x6). */
#define KBC_CAP_EXTENDED_STATES     KMC_CAP_EXTENDED_STATES

/** @brief Module has an active fault condition. Clear with CMD_ACK_FAULT. */
#define KBC_CAP_FAULT               KMC_CAP_FAULT

// ============================================================
//  Module Type ID registry — aliases for KMC_TYPE_* from KerbalModuleCommon
//
//  Full registry (all module types) is in KerbalModuleCommon.h.
//  KBC_TYPE_* aliases provided here for the six standard KBC modules.
// ============================================================

#define KBC_TYPE_RESERVED           KMC_TYPE_RESERVED
#define KBC_TYPE_UI_CONTROL         KMC_TYPE_UI_CONTROL
#define KBC_TYPE_FUNCTION_CONTROL   KMC_TYPE_FUNCTION_CONTROL
#define KBC_TYPE_ACTION_CONTROL     KMC_TYPE_ACTION_CONTROL
#define KBC_TYPE_STABILITY_CONTROL  KMC_TYPE_STABILITY_CONTROL
#define KBC_TYPE_VEHICLE_CONTROL    KMC_TYPE_VEHICLE_CONTROL
#define KBC_TYPE_TIME_CONTROL       KMC_TYPE_TIME_CONTROL
#define KBC_TYPE_UNKNOWN            KMC_TYPE_UNKNOWN

// ============================================================
//  Packet structs
//
//  Convenience structs for building and parsing packets.
//  These are not transmitted directly — always serialize to/from
//  raw byte arrays for I2C transactions.
// ============================================================

/**
 * @struct KBCButtonPacket
 * @brief  Button state response packet (target → controller, 4 bytes).
 *
 *         currentState and changeMask are 16-bit bitmasks.
 *         Bit 0 = Button 0, Bit 15 = Button 15.
 *         In currentState: 1 = pressed, 0 = released.
 *         In changeMask:   1 = changed since last read.
 *
 *         Controller logic:
 *           changedAndPressed  = currentState & changeMask
 *           changedAndReleased = ~currentState & changeMask
 */
struct KBCButtonPacket {
    uint16_t currentState;  ///< Current button state bitmask
    uint16_t changeMask;    ///< Changed buttons bitmask since last read
};

/**
 * @struct KBCIdentityPacket
 * @brief  Identity response packet (target → controller, 4 bytes).
 */
struct KBCIdentityPacket {
    uint8_t typeId;          ///< Module Type ID (KBC_TYPE_*)
    uint8_t firmwareMajor;   ///< Firmware version major
    uint8_t firmwareMinor;   ///< Firmware version minor
    uint8_t capabilityFlags; ///< Capability flags bitmask (KBC_CAP_*)
};

// ============================================================
//  LED state payload pack / unpack helpers
//
//  Canonical implementations are kmcLedPackGet() / kmcLedPackSet()
//  in KerbalModuleCommon. The KBC_* names below are inline wrappers
//  retained for backward compatibility with existing library code.
// ============================================================

/**
 * @brief  Get the LED state nibble for button N from a packed payload.
 *         Wrapper for kmcLedPackGet() — see KerbalModuleCommon.h.
 */
inline uint8_t KBC_ledPackGet(const uint8_t* payload, uint8_t button) {
    return kmcLedPackGet(payload, button);
}

/**
 * @brief  Set the LED state nibble for button N in a packed payload.
 *         Wrapper for kmcLedPackSet() — see KerbalModuleCommon.h.
 */
inline void KBC_ledPackSet(uint8_t* payload, uint8_t button, uint8_t state) {
    kmcLedPackSet(payload, button, state);
}

/**
 * @brief  Serialize a KBCButtonPacket into a 4-byte array for transmission.
 * @param  pkt  Source packet
 * @param  buf  Destination byte array (must be at least KBC_BUTTON_PACKET_SIZE bytes)
 */
inline void KBC_serializeButtonPacket(const KBCButtonPacket* pkt, uint8_t* buf) {
    buf[0] = (pkt->currentState >> 8) & 0xFF;
    buf[1] = (pkt->currentState)      & 0xFF;
    buf[2] = (pkt->changeMask   >> 8) & 0xFF;
    buf[3] = (pkt->changeMask)        & 0xFF;
}

/**
 * @brief  Deserialize a 4-byte array into a KBCButtonPacket.
 * @param  buf  Source byte array (must be at least KBC_BUTTON_PACKET_SIZE bytes)
 * @param  pkt  Destination packet
 */
inline void KBC_deserializeButtonPacket(const uint8_t* buf, KBCButtonPacket* pkt) {
    pkt->currentState = ((uint16_t)buf[0] << 8) | buf[1];
    pkt->changeMask   = ((uint16_t)buf[2] << 8) | buf[3];
}

/**
 * @brief  Serialize a KBCIdentityPacket into a 4-byte array for transmission.
 * @param  pkt  Source packet
 * @param  buf  Destination byte array (must be at least KBC_IDENTITY_PACKET_SIZE bytes)
 */
inline void KBC_serializeIdentityPacket(const KBCIdentityPacket* pkt, uint8_t* buf) {
    buf[0] = pkt->typeId;
    buf[1] = pkt->firmwareMajor;
    buf[2] = pkt->firmwareMinor;
    buf[3] = pkt->capabilityFlags;
}

#endif // KBC_PROTOCOL_H

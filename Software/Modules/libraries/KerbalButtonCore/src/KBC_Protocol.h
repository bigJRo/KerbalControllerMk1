/**
 * @file        KBC_Protocol.h
 * @version     2.0.0
 * @date        2026-06-28
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
 *              v2.0 brings the library to full conformance with the
 *              system I2C Protocol Specification v2.4: every data packet
 *              now carries the universal 3-byte header (status byte,
 *              module type ID, transaction counter) ahead of the
 *              button-event payload, and the full lifecycle command set
 *              (CMD_ENABLE/CMD_DISABLE) is supported. Switch-group
 *              modules read 24 inputs (KBC_INPUT_COUNT == 24) and emit a
 *              6-byte payload (9-byte total packet).
 *
 *              Full protocol specification: I2C_Protocol_Specification.md v2.5
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
//  KBC implements the full shared command set 0x01-0x0A; the
//  display-only CMD_SET_VALUE (0x0D) does not apply to button modules.
// ============================================================

#define KBC_CMD_GET_IDENTITY        KMC_CMD_GET_IDENTITY
#define KBC_CMD_SET_LED_STATE       KMC_CMD_SET_LED_STATE
#define KBC_CMD_SET_BRIGHTNESS      KMC_CMD_SET_BRIGHTNESS
#define KBC_CMD_BULB_TEST           KMC_CMD_BULB_TEST
#define KBC_CMD_SLEEP               KMC_CMD_SLEEP
#define KBC_CMD_WAKE                KMC_CMD_WAKE
#define KBC_CMD_RESET               KMC_CMD_RESET
#define KBC_CMD_ACK_FAULT           KMC_CMD_ACK_FAULT
#define KBC_CMD_ENABLE              KMC_CMD_ENABLE
#define KBC_CMD_DISABLE             KMC_CMD_DISABLE

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
#define KBC_LED_CUT             KMC_LED_CUT
#define KBC_LED_ACTIVE_ALT      KMC_LED_ACTIVE_ALT

// ============================================================
//  Packet sizes
//
//  Every data packet leads with the universal 3-byte header
//  (status byte, module type ID, transaction counter) followed by
//  the button-event payload. The payload carries two bitmask planes
//  — button events (current state) and the change mask — each one
//  byte per group of 8 inputs:
//
//    16-input module: 2 event bytes + 2 change bytes = 4-byte payload,
//                     7-byte total packet.
//    24-input module: 3 event bytes + 3 change bytes = 6-byte payload,
//                     9-byte total packet (switch-group variant).
// ============================================================

/** @brief Universal data-packet header size. Alias for KMC_HEADER_SIZE. */
#define KBC_HEADER_SIZE             KMC_HEADER_SIZE

/** @brief Number of input bytes per bitmask plane (2 for 16 inputs, 3 for 24). */
#define KBC_INPUT_BYTES             ((KBC_INPUT_COUNT + 7) / 8)

/**
 * @brief Button-event payload size in bytes (excludes the 3-byte header).
 *        Two bitmask planes (events + change), one byte per 8 inputs.
 */
#define KBC_BUTTON_PAYLOAD_SIZE     (2 * KBC_INPUT_BYTES)

/** @brief Total button state packet size in bytes (header + payload). */
#define KBC_BUTTON_PACKET_SIZE      (KBC_HEADER_SIZE + KBC_BUTTON_PAYLOAD_SIZE)

/**
 * @brief LED state command payload size in bytes (controller → target).
 *        Does not include the command byte itself.
 *        16 LED positions × 4 bits = 64 bits = 8 bytes.
 */
#define KBC_LED_PAYLOAD_SIZE        KMC_LED_PAYLOAD_SIZE

/** @brief Identity response packet size in bytes. Alias for KMC_IDENTITY_SIZE. */
#define KBC_IDENTITY_PACKET_SIZE    KMC_IDENTITY_SIZE

/** @brief Total bytes on wire for a CMD_SET_LED_STATE transaction. */
#define KBC_LED_WIRE_SIZE           (1 + KBC_LED_PAYLOAD_SIZE)

// ============================================================
//  Button / input counts
//
//  KBC_BUTTON_COUNT is the number of LED/colour positions (12 RGB +
//  4 discrete) and sizes the per-button active-colour array in every
//  sketch — it is always 16. KBC_INPUT_COUNT (KBC_Config.h) is the
//  number of shift-register inputs read and reported (16 or 24); on
//  24-input modules, indices 16-23 are discrete switch inputs with no
//  LED hardware.
// ============================================================

/** @brief Number of LED/colour positions per module (sizes activeColors[]). */
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
//  KBC_TYPE_* aliases provided here for the seven standard KBC modules.
// ============================================================

#define KBC_TYPE_RESERVED           KMC_TYPE_RESERVED
#define KBC_TYPE_UI_CONTROL         KMC_TYPE_UI_CONTROL
#define KBC_TYPE_FUNCTION_CONTROL   KMC_TYPE_FUNCTION_CONTROL
#define KBC_TYPE_ACTION_CONTROL     KMC_TYPE_ACTION_CONTROL
#define KBC_TYPE_STABILITY_CONTROL  KMC_TYPE_STABILITY_CONTROL
#define KBC_TYPE_VEHICLE_CONTROL    KMC_TYPE_VEHICLE_CONTROL
#define KBC_TYPE_TIME_CONTROL       KMC_TYPE_TIME_CONTROL
#define KBC_TYPE_AUX_CTRL           KMC_TYPE_AUX_CTRL
#define KBC_TYPE_UNKNOWN            KMC_TYPE_UNKNOWN

// ============================================================
//  Packet structs
//
//  Convenience structs for building and parsing packets.
//  These are not transmitted directly — always serialize to/from
//  raw byte arrays for I2C transactions.
// ============================================================

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

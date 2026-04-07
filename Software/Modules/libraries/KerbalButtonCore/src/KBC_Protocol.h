/**
 * @file        KBC_Protocol.h
 * @version     1.0
 * @date        2026-04-07
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
//  Command bytes (controller → target)
//
//  All I2C write transactions begin with one of these command
//  bytes as the first byte after the address. Payload bytes,
//  if any, follow immediately.
// ============================================================

/** @brief Query module identity. No payload. Target responds with 4-byte identity packet. */
#define KBC_CMD_GET_IDENTITY        0x01

/**
 * @brief Set full LED state for all 16 buttons.
 *        Payload: 8 bytes, nibble-packed (2 buttons per byte, high nibble first).
 *        See KBC_LED_STATE_* constants for nibble values.
 */
#define KBC_CMD_SET_LED_STATE       0x02

/**
 * @brief Set ENABLED state backlight brightness.
 *        Payload: 1 byte (0-255). Applied via RGB scaling at render time.
 *        Does not affect ACTIVE or extended state brightness.
 */
#define KBC_CMD_SET_BRIGHTNESS      0x03

/**
 * @brief Trigger bulb test sequence.
 *        No payload. Behavior defined per module sketch.
 *        Intended to verify all LEDs are functional.
 */
#define KBC_CMD_BULB_TEST           0x04

/**
 * @brief Enter low power sleep mode.
 *        No payload. LEDs off, polling may reduce.
 *        Module remains responsive on I2C.
 */
#define KBC_CMD_SLEEP               0x05

/**
 * @brief Resume normal operation from sleep.
 *        No payload. Restores previous LED state and full polling rate.
 */
#define KBC_CMD_WAKE                0x06

/**
 * @brief Reset module to default state.
 *        No payload. All LEDs → OFF, latched button state cleared,
 *        INT line deasserted. Used for controller startup and recovery.
 */
#define KBC_CMD_RESET               0x07

/**
 * @brief Acknowledge and clear module fault flag.
 *        No payload. Call after handling a fault condition reported
 *        via capability flags in the identity response.
 */
#define KBC_CMD_ACK_FAULT           0x08

// ============================================================
//  LED state nibble values
//
//  Each button is assigned one nibble (4 bits) in the
//  CMD_SET_LED_STATE payload. Two buttons per byte, high nibble first.
//
//  Core states (all modules):
//    0x0  OFF             — unlit
//    0x1  ENABLED         — dim white backlight (scaled by brightness setting)
//    0x2  ACTIVE          — full brightness, module-defined color
//
//  Extended states (capability-flagged modules only):
//    0x3  WARNING         — flashing amber, 500ms on / 500ms off
//    0x4  ALERT           — flashing red,   150ms on / 150ms off
//    0x5  ARMED           — full brightness static cyan
//    0x6  PARTIAL_DEPLOY  — full brightness static amber
//
//  All LED animation and display behavior for extended states is
//  handled entirely by the target module. The controller only
//  transmits the state nibble value.
//
//  Modules that do not implement extended states treat values
//  above 0x2 as OFF.
//
//  0x7-0xF are reserved for future use.
// ============================================================

/** @brief LED state: unlit. */
#define KBC_LED_OFF                 0x0

/** @brief LED state: dim white backlight. Brightness set via CMD_SET_BRIGHTNESS. */
#define KBC_LED_ENABLED             0x1

/** @brief LED state: full brightness, module-defined active color. */
#define KBC_LED_ACTIVE              0x2

/** @brief LED state: flashing amber. 500ms on / 500ms off. Extended modules only. */
#define KBC_LED_WARNING             0x3

/** @brief LED state: flashing red. 150ms on / 150ms off. Extended modules only. */
#define KBC_LED_ALERT               0x4

/** @brief LED state: full brightness static cyan. Extended modules only. */
#define KBC_LED_ARMED               0x5

/** @brief LED state: full brightness static amber. Extended modules only. */
#define KBC_LED_PARTIAL_DEPLOY      0x6

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
#define KBC_LED_PAYLOAD_SIZE        8

/** @brief Identity response packet size in bytes (target → controller). */
#define KBC_IDENTITY_PACKET_SIZE    4

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
//  Capability flags (identity response byte 3)
//
//  Bitmask reported by the target in the identity response.
//  The controller reads these at startup to determine module
//  capabilities without hardcoding per-address knowledge.
// ============================================================

/** @brief Module supports extended LED states (0x3-0x6). */
#define KBC_CAP_EXTENDED_STATES     (1 << 0)

/** @brief Module has an active fault condition. Clear with CMD_ACK_FAULT. */
#define KBC_CAP_FAULT               (1 << 1)

// Bits 2-7 reserved for future use.

// ============================================================
//  Module Type ID registry
//
//  The Type ID is independent of I2C address. It identifies
//  what a module IS, not where it is on the bus. The controller
//  maps each bus address to an expected Type ID at compile time
//  and validates this during startup enumeration.
//
//  Add new module Type IDs here as module sketches are defined.
// ============================================================

/** @brief Reserved. Must not be used as a valid Type ID. */
#define KBC_TYPE_RESERVED           0x00

/** @brief UI Control module. */
#define KBC_TYPE_UI_CONTROL         0x01

/** @brief Function Control module. */
#define KBC_TYPE_FUNCTION_CONTROL   0x02

/** @brief Action Control module. */
#define KBC_TYPE_ACTION_CONTROL     0x03

/** @brief Stability Control module. */
#define KBC_TYPE_STABILITY_CONTROL  0x04

/** @brief Vehicle Control module. */
#define KBC_TYPE_VEHICLE_CONTROL    0x05

/** @brief Time Control module. */
#define KBC_TYPE_TIME_CONTROL       0x06

/** @brief Unknown or uninitialized. Returned if module has not set its Type ID. */
#define KBC_TYPE_UNKNOWN            0xFF

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
//  Pack and unpack the 8-byte nibble-packed LED state payload.
//  Two buttons per byte, high nibble = even button index,
//  low nibble = odd button index.
//
//  Usage (pack):
//    uint8_t payload[KBC_LED_PAYLOAD_SIZE] = {0};
//    KBC_ledPackSet(payload, 3, KBC_LED_ACTIVE);
//
//  Usage (unpack):
//    uint8_t state = KBC_ledPackGet(payload, 3);
// ============================================================

/**
 * @brief  Get the LED state nibble for button N from a packed payload.
 * @param  payload  Pointer to 8-byte payload array
 * @param  button   Button index (0-15)
 * @return LED state nibble value (0x0-0xF)
 */
inline uint8_t KBC_ledPackGet(const uint8_t* payload, uint8_t button) {
    return (button % 2 == 0)
        ? (payload[button / 2] >> 4) & 0x0F   // high nibble
        : (payload[button / 2]) & 0x0F;        // low nibble
}

/**
 * @brief  Set the LED state nibble for button N in a packed payload.
 * @param  payload  Pointer to 8-byte payload array
 * @param  button   Button index (0-15)
 * @param  state    LED state nibble value (KBC_LED_*)
 */
inline void KBC_ledPackSet(uint8_t* payload, uint8_t button, uint8_t state) {
    if (button % 2 == 0) {
        payload[button / 2] = (payload[button / 2] & 0x0F) | ((state & 0x0F) << 4);
    } else {
        payload[button / 2] = (payload[button / 2] & 0xF0) | (state & 0x0F);
    }
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

/***************************************************************************************
   module_packets.h — Controller-side I2C packet framing helpers
   Kerbal Controller Mk1 — Main Controller

   Single source of truth for resolving, per module Type ID, the total number of
   bytes the controller must read from a target on each INT-triggered read, and
   for parsing the universal 3-byte packet header that every conformant module
   (I2C Protocol Specification v2.6) places ahead of its payload.

   Every data packet is:
       Byte 0: Status byte       (lifecycle bits 1:0, fault bit 2, data-changed bit 3)
       Byte 1: Module Type ID
       Byte 2: Transaction counter (increments per INT assertion, wraps 255->0)
       Byte 3+: module-specific payload (see spec §9)

   Usage pattern in a read handler:
       uint8_t n = moduleTotalPacketSize(typeId);
       Wire.requestFrom(addr, n);
       uint8_t pkt[12];
       for (uint8_t i = 0; i < n && Wire.available(); i++) pkt[i] = Wire.read();
       uint8_t status   = pkt[KMC_PKT_STATUS_OFFSET];
       uint8_t lifecycle = moduleLifecycle(status);
       // payload begins at pkt[KMC_PKT_PAYLOAD_OFFSET]

   Dependency: KerbalModuleCommon.h (the same shared header the target modules
   use) must be on the controller's Arduino include path. It provides the
   KMC_TYPE_*, KMC_HEADER_SIZE, and KMC_STATUS_* definitions referenced below.

   NOTE — controller conformance is NOT complete. This header supplies the
   correct packet framing, but the rest of Controller_Main still reflects the
   pre-v5.x design and must be reconciled separately:
     * I2C address map (Controller_Main.ino) does not match the v5.x registry
       (e.g. UI=0x20, Function=0x21, Action=0x22, Stability=0x23, Vehicle=0x24,
        Time=0x25, EVA=0x26, Joy Rot=0x28, Joy Trans=0x29, GPWS=0x2A,
        Pre-Warp=0x2B, Throttle=0x2C, Dual Encoder=0x2D, Switch Panel=0x2E).
     * Only three read handlers exist; the remaining modules need handlers.
     * Payload bit layouts in module_variables.h predate the v5.x module UI.

   Licensed under the GNU General Public License v3.0 (GPL-3.0).
****************************************************************************************/
#pragma once
#include <stdint.h>
#include <KerbalModuleCommon.h>   // KMC_TYPE_*, KMC_HEADER_SIZE, KMC_STATUS_*

// ============================================================
//  Universal header byte offsets (within a received packet)
// ============================================================

#define KMC_PKT_STATUS_OFFSET     0
#define KMC_PKT_TYPEID_OFFSET     1
#define KMC_PKT_TXCOUNTER_OFFSET  2
#define KMC_PKT_PAYLOAD_OFFSET    KMC_HEADER_SIZE   // first payload byte (3)

// ============================================================
//  Total packet size (header + payload) by module Type ID
//
//  Payload sizes per I2C Protocol Specification v2.6 §8/§9:
//    standard button (16-input)      4-byte payload -> 7 total
//    switch-group button (24-input)  6-byte payload -> 9 total  (0x21, 0x24)
//    joystick                        9-byte payload -> 12 total (0x09, 0x0A)
//    display                         5-byte payload -> 8 total  (0x0B, 0x0C)
//    throttle / dual encoder         4-byte payload -> 7 total
// ============================================================

static inline uint8_t moduleTotalPacketSize(uint8_t typeId) {
    switch (typeId) {
        case KMC_TYPE_FUNCTION_CONTROL:   // 24-input switch group (Switch Group 1)
        case KMC_TYPE_VEHICLE_CONTROL:    // 24-input switch group (Switch Group 2)
            return KMC_HEADER_SIZE + 6;   // 9

        case KMC_TYPE_JOYSTICK_ROTATION:
        case KMC_TYPE_JOYSTICK_TRANS:
            return KMC_HEADER_SIZE + 9;   // 12

        case KMC_TYPE_GPWS_INPUT:
        case KMC_TYPE_PRE_WARP_TIME:
            return KMC_HEADER_SIZE + 5;   // 8 (display modules)

        case KMC_TYPE_UI_CONTROL:
        case KMC_TYPE_ACTION_CONTROL:
        case KMC_TYPE_STABILITY_CONTROL:
        case KMC_TYPE_TIME_CONTROL:
        case KMC_TYPE_EVA_MODULE:
        case KMC_TYPE_THROTTLE:
        case KMC_TYPE_DUAL_ENCODER:
        case KMC_TYPE_SWITCH_PANEL:
            return KMC_HEADER_SIZE + 4;   // 7

        default:
            return KMC_HEADER_SIZE + 4;   // safe default (7)
    }
}

// ============================================================
//  Number of input/event bytes per bitmask plane for a module
//  (events plane and change plane each use this many bytes):
//    2 for 16-input modules, 3 for 24-input switch-group modules.
// ============================================================

static inline uint8_t moduleInputBytes(uint8_t typeId) {
    return (typeId == KMC_TYPE_FUNCTION_CONTROL ||
            typeId == KMC_TYPE_VEHICLE_CONTROL) ? 3 : 2;
}

// ============================================================
//  Status-byte field accessors
// ============================================================

/** @brief Lifecycle state (KMC_STATUS_ACTIVE/SLEEPING/DISABLED/BOOT_READY). */
static inline uint8_t moduleLifecycle(uint8_t status) {
    return status & KMC_STATUS_LIFECYCLE_MASK;
}

/** @brief True if the module is reporting a hardware fault. */
static inline bool moduleHasFault(uint8_t status) {
    return (status & KMC_STATUS_FAULT) != 0;
}

/** @brief True if the data-changed flag is set (always set on a queued packet). */
static inline bool moduleDataChanged(uint8_t status) {
    return (status & KMC_STATUS_DATA_CHANGED) != 0;
}

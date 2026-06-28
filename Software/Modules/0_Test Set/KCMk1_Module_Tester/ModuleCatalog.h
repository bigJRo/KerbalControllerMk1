/**
 * @file        ModuleCatalog.h
 * @version     2.0.0
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Per-module metadata for the tester: name, expected address,
 *              rendering kind, and input labels for the current module set.
 *              Type IDs, packet sizes, and LED/command constants come from
 *              the shared KerbalModuleCommon library (single source of truth).
 *
 *              RA4M1 is an ARM target with memory-mapped flash, so const
 *              tables are addressed directly — no PROGMEM.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */

#pragma once
#include <Arduino.h>
#include <KerbalModuleCommon.h>   // KMC_TYPE_*, KMC_*_PACKET_SIZE, etc.

// ============================================================
//  Module rendering kind — drives the dashboard layout and the
//  packet parsing the tester applies.
// ============================================================
enum ModuleKind : uint8_t {
    MK_BUTTON12,      // 12 NeoPixel buttons, 7-byte packet
    MK_BUTTON24,      // 12 buttons + 8 switch-group inputs, 9-byte packet
    MK_JOYSTICK,      // 3 axes + up to 3 buttons, 12-byte packet
    MK_DISPLAY,       // encoder + 3 buttons + value, 8-byte packet
    MK_THROTTLE,      // 4 buttons + value + flags, 7-byte packet
    MK_DUAL_ENCODER   // 2 encoders + 2 buttons, 7-byte packet
};

// ============================================================
//  Module catalog entry
// ============================================================
struct ModuleInfo {
    uint8_t      typeId;       // KMC_TYPE_* (matches identity byte 0)
    const char*  name;         // human-readable module name
    uint8_t      addr;         // expected I2C address (0x20-0x2E)
    ModuleKind   kind;         // dashboard/parse kind
    uint8_t      inputCount;   // number of entries in labels[]
    const char* const* labels; // input labels, length == inputCount
};

// ============================================================
//  Lookup by Type ID (identity byte 0). Returns nullptr if unknown.
// ============================================================
const ModuleInfo* catalogByType(uint8_t typeId);

// ============================================================
//  Total I2C packet size (header + payload) for a rendering kind,
//  expressed via the shared KMC_*_PACKET_SIZE constants.
// ============================================================
uint8_t kindPacketSize(ModuleKind kind);

/**
 * @file        TesterHW.h
 * @version     2.0.0
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Hardware I/O layer: shared-bus I2C controller operations for
 *              talking to a module under test, plus the INA228 power monitor.
 *              All protocol constants come from KerbalModuleCommon.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */

#pragma once
#include <Arduino.h>
#include <KerbalModuleCommon.h>
#include "ModuleCatalog.h"

// ============================================================
//  Normalized module input state (parsed from a data packet)
//  Fields are populated according to the module kind; unused
//  fields are left zero.
// ============================================================
struct ModuleState {
    // Universal header
    uint8_t  lifecycle;     // KMC_STATUS_* lifecycle bits
    bool     fault;         // status fault bit
    bool     dataChanged;   // status data-changed bit
    uint8_t  txCounter;     // transaction counter

    // Button / switch planes (bit N = input N). Up to 24 inputs.
    uint32_t events;        // rising-edge / current-state plane
    uint32_t change;        // change mask plane

    // Analog (joystick)
    int16_t  axis[3];

    // Encoders (dual encoder)
    int8_t   enc[2];

    // Scalar value + flags (throttle / display)
    uint16_t value;
    uint8_t  flags;
};

// ============================================================
//  Identity (CMD_GET_IDENTITY response, 4 bytes)
// ============================================================
struct ModuleIdentity {
    uint8_t typeId;
    uint8_t fwMajor;
    uint8_t fwMinor;
    uint8_t caps;
    bool    valid;
};

// ============================================================
//  Power reading from the INA228
// ============================================================
struct PowerReading {
    float volts;
    float amps;
    float watts;
    bool  ok;
};

// ------------------------------------------------------------
//  Bus / module control
// ------------------------------------------------------------

/** @brief Init Wire (400 kHz), INT/RST GPIO, and the INA228. */
void hwBegin();

/** @brief Pulse the module reset line (active low) for a hard reset. */
void hwPulseModuleReset();

/** @brief True while the module INT line is asserted (low = data pending). */
bool hwModuleIntAsserted();

/**
 * @brief  Scan 0x20-0x2E for an ACKing module (skips on-board 0x38/0x40).
 * @param  outAddrs  buffer to receive found addresses
 * @param  maxAddrs  capacity of outAddrs
 * @return number of modules found
 */
uint8_t hwScanModules(uint8_t* outAddrs, uint8_t maxAddrs);

/** @brief Query CMD_GET_IDENTITY from a module. */
ModuleIdentity hwIdentify(uint8_t addr);

/** @brief Send a command byte with optional payload. Returns Wire status (0 = ok). */
uint8_t hwSendCommand(uint8_t addr, uint8_t cmd, const uint8_t* payload, uint8_t len);

/**
 * @brief  Read a data packet of `n` bytes from the module.
 * @return number of bytes actually read.
 */
uint8_t hwReadPacket(uint8_t addr, uint8_t* buf, uint8_t n);

/**
 * @brief  Parse a raw data packet into a normalized ModuleState according
 *         to the module kind. Reads the universal 3-byte header plus the
 *         kind-specific payload.
 */
void hwParsePacket(const ModuleInfo* info, const uint8_t* pkt, uint8_t n,
                   ModuleState& out);

// ------------------------------------------------------------
//  Power monitor
// ------------------------------------------------------------

/** @brief Read bus voltage / current / power from the INA228. */
PowerReading hwReadPower();

/**
 * @file        KTC_Bus.h
 * @version     1.1.1
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       I2C access to the module under test (identity, commands,
 *              INT-gated packet reads), module RST/INT lines, and the INA228
 *              supply monitor. Protocol constants come from KerbalModuleCommon.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#pragma once
#include <Arduino.h>
#include <KerbalModuleCommon.h>

/** @brief A CMD_GET_IDENTITY reply (I2C spec §6, 8 bytes): fw* is the
 *         library version (the sketch version for a standalone sketch), sk*
 *         the module sketch's own version. fwPatch is KMC_FW_PATCH_UNKNOWN
 *         when the module sent the 4-byte form (no patch byte, or 0xFF from
 *         the idle bus); skMajor is KMC_SKETCH_VERSION_UNKNOWN when it sent
 *         no sketch version (the 5- or 4-byte form, or 0xFF). */
struct KtcIdentity {
    uint8_t typeId;
    uint8_t fwMajor;
    uint8_t fwMinor;
    uint8_t caps;
    uint8_t fwPatch;
    uint8_t skMajor;
    uint8_t skMinor;
    uint8_t skPatch;
    bool    valid;
};

/** @brief An identity with nothing read: versions unknown, not valid. */
#define KTC_IDENTITY_NONE { 0, 0, 0, 0, KMC_FW_PATCH_UNKNOWN, \
                            KMC_SKETCH_VERSION_UNKNOWN, KMC_SKETCH_VERSION_UNKNOWN, \
                            KMC_SKETCH_VERSION_UNKNOWN, false }

struct KtcPower {
    float volts;    // module supply voltage
    float amps;     // module supply current
    float watts;
    bool  ok;
};

/** @brief Start Wire at 400 kHz, set up INT/RST and the INA228. */
void ktcBusBegin();

/** @brief Hold the module RST line low for lowMs, then release. */
void ktcResetPulse(uint16_t lowMs = 5);

/** @brief True while the module holds INT low (data pending). */
bool ktcIntAsserted();

/** @brief True if addr ACKs two zero-length probes in a row. */
bool ktcProbe(uint8_t addr);

/** @brief Scan 0x20-0x2E (skipping the on-board 0x38/0x40). Returns count. */
uint8_t ktcScan(uint8_t* out, uint8_t maxAddrs);

/** @brief CMD_GET_IDENTITY, one retry. Asks for 8 bytes, accepts the 5- and 4-byte forms. */
KtcIdentity ktcIdentify(uint8_t addr);

/** @brief "X.Y.Z" into buf, or "X.Y" when patch is KMC_FW_PATCH_UNKNOWN. */
void ktcFmtVersion(char* buf, uint8_t n, uint8_t major, uint8_t minor, uint8_t patch);

/** @brief Both versions of an identity, as the master shows them:
 *         "2.1.0 / lib 2.5.0" (sketch, then library); one "X.Y.Z" when the
 *         two are equal (a standalone sketch); "- / lib 2.5.0" when the
 *         sketch version is unknown. compact: "2.1.0/2.5.0", "-/2.5.0". */
void ktcFmtVersions(char* buf, uint8_t n, const KtcIdentity& id, bool compact = false);

/** @brief Send a command with an optional payload. Returns the Wire status (0 = ACK). */
uint8_t ktcCommand(uint8_t addr, uint8_t cmd, const uint8_t* payload = nullptr, uint8_t len = 0);

/** @brief Send a command with a single payload byte. */
uint8_t ktcCommand1(uint8_t addr, uint8_t cmd, uint8_t b);

/** @brief Read n bytes. Returns the number actually read. */
uint8_t ktcRead(uint8_t addr, uint8_t* buf, uint8_t n);

/** @brief Fill an 8-byte CMD_SET_LED_STATE payload with one state for all 16 positions. */
void ktcLedFill(uint8_t* payload, uint8_t state);

/** @brief Read the INA228 (two attempts). ok=false if both fail. */
KtcPower ktcReadPower();

/**
 * @file        KTC_Runner.h
 * @version     1.0.0
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Step runner. A tester sketch describes its module in a
 *              KtcModuleDef (identity, address, packet size, and an ordered
 *              list of test steps) and calls ktcBegin() / ktcLoop().
 *
 *              App flow:
 *                SPLASH -> WAIT (probe the module's address, check identity)
 *                -> MENU (tap a step to run it alone, RUN ALL, or LIVE)
 *                -> RUN (steps in order) -> SUMMARY
 *              Results also go to the USB serial port (115200) as a report.
 *
 *              A step is a function called every loop() until it returns a
 *              verdict. It keeps its own state in the context (phase, timer,
 *              scratch v[] and m[]). The runner owns the screen frame, the
 *              operator buttons SKIP / RETRY / ABORT, INA228 polling, and the
 *              INT-gated packet reads (c.fresh is true on a call with a new,
 *              validated packet in c.pkt).
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#pragma once
#include <Arduino.h>
#include <KerbalModuleCommon.h>
#include "KTC_Bus.h"
#include "KTC_UI.h"

enum KtcVerdict : uint8_t { KTC_RUNNING = 0, KTC_PASS, KTC_FAIL, KTC_SKIP };

struct KtcCtx;
typedef KtcVerdict (*KtcStepFn)(KtcCtx& c);

/** @brief One test step: menu/summary name, function, optional argument. */
struct KtcStep {
    const char* name;
    KtcStepFn   fn;
    const void* arg;
};

#define KTC_MAX_STEPS  12

/** @brief Everything the runner needs to know about the module under test. */
struct KtcModuleDef {
    const char*    name;        // e.g. "Action Control"
    const char*    board;       // board / drawing reference shown on the splash, or nullptr
    uint8_t        typeId;      // KMC_TYPE_*
    uint8_t        addr;        // I2C address
    uint8_t        pktSize;     // KMC_*_PACKET_SIZE
    uint8_t        fwMajor;     // firmware the steps were written against
    uint8_t        fwMinor;
    uint8_t        caps;        // expected KMC_CAP_* flags
    const KtcStep* steps;
    uint8_t        stepCount;   // at most KTC_MAX_STEPS
    /** Optional: put the module back in a known state after ABORT or a
     *  finished run (bulb test off, LEDs ENABLED, ...). The runner always
     *  stops a bulb test first. */
    void (*cleanup)(KtcCtx& c);
    /** Optional: decode c.pkt into live-area rows 5..13 on the LIVE screen. */
    void (*liveDecode)(KtcCtx& c);
};

/** @brief Runner and step context. Fields marked step-private are reset at each step start. */
struct KtcCtx {
    const KtcModuleDef* mod;
    const void* arg;          // the current step's argument
    uint8_t  addr;
    uint32_t now;             // millis() for this call

    // Step-private state
    bool     entry;           // first call of the step (also after RETRY)
    bool     phaseEntry;      // first call in the current phase
    uint8_t  phase;
    uint32_t t0;              // millis() when the phase was entered
    int32_t  v[8];
    uint32_t m[4];

    uint16_t btn;             // operator button pressed this call (KTC_BTN_*), 0 if none

    // Latest packet (INT-gated, validated: full length and type ID match)
    bool     fresh;
    uint8_t  pkt[16];
    uint8_t  pktLen;
    uint8_t  lifecycle;       // KMC_STATUS_* lifecycle bits of pkt[0]
    bool     fault;
    bool     dataChanged;
    uint32_t pktCount;        // validated packets since power-up
    uint32_t badReads;        // reads rejected (short / wrong type)
    bool     autoRead;        // runner does INT-gated reads (default true)

    KtcPower power;           // latest good INA228 reading
    char     note[40];        // result detail shown on screen and in the report
};

// ------------------------------------------------------------
//  Sketch entry points
// ------------------------------------------------------------
void ktcBegin(const KtcModuleDef& def);
void ktcLoop();

// ------------------------------------------------------------
//  Step helpers
// ------------------------------------------------------------
/** @brief Enter phase p: resets the phase timer; c.phaseEntry is true on the next call. */
void ktcPhase(KtcCtx& c, uint8_t p);
/** @brief Milliseconds since the current phase was entered. */
uint32_t ktcPhaseMs(const KtcCtx& c);
/** @brief Set the result note and return KTC_PASS. */
KtcVerdict ktcPass(KtcCtx& c, const char* fmt = nullptr, ...);
/** @brief Set the result note and return KTC_FAIL. */
KtcVerdict ktcFail(KtcCtx& c, const char* fmt, ...);
/** @brief Note text without ending the step (shown in the report if it ends). */
void ktcNote(KtcCtx& c, const char* fmt, ...);

#define KTC_SHOW_NOSKIP  0x8000   // ktcShow(): leave SKIP off the button row
/** @brief Instruction text + step buttons (SKIP and ABORT are added). Clears the live area. */
void ktcShow(KtcCtx& c, const char* instr, uint16_t buttons);

/** @brief Command to the module under test. Returns the Wire status. */
uint8_t ktcSend(KtcCtx& c, uint8_t cmd, const uint8_t* payload = nullptr, uint8_t len = 0);
uint8_t ktcSend1(KtcCtx& c, uint8_t cmd, uint8_t b);
/** @brief CMD_SET_LED_STATE with one state on all 16 positions. */
uint8_t ktcLedsAll(KtcCtx& c, uint8_t state);
/** @brief CMD_SET_LED_STATE from 16 per-position states. */
uint8_t ktcLeds(KtcCtx& c, const uint8_t* states16);

/** @brief Read a packet now, without waiting for INT. True if it validated (also sets c.fresh). */
bool ktcReadNow(KtcCtx& c);
/** @brief Read and discard queued packets until INT releases (max ~100 ms). */
void ktcDrain(KtcCtx& c);

/** @brief Little-endian bit plane from the payload (byte offset after the 3-byte header). */
uint32_t ktcBits(const KtcCtx& c, uint8_t payloadOff, uint8_t nBytes);
/** @brief Payload byte. */
uint8_t ktcByte(const KtcCtx& c, uint8_t payloadOff);
/** @brief Big-endian signed / unsigned 16-bit payload field. */
int16_t  ktcS16(const KtcCtx& c, uint8_t payloadOff);
uint16_t ktcU16(const KtcCtx& c, uint8_t payloadOff);

/** @brief Short text for a lifecycle value. */
const char* ktcLifecycleName(uint8_t lifecycle);
/** @brief Module name for a type ID (for "wrong module" messages). */
const char* ktcTypeName(uint8_t typeId);

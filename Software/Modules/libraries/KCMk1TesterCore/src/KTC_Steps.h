/**
 * @file        KTC_Steps.h
 * @version     1.0.0
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Reusable test steps. A sketch lists them in its step table
 *              with a module-specific argument (labels, bit numbers, colours,
 *              expected counts). Module-only behaviour (throttle motor,
 *              joystick axes, display digits, ...) is written in the sketch.
 *
 *              Input reporting differs between the module families, so steps
 *              that watch inputs take a decoder (KtcDecodeFn) that turns the
 *              latest packet into a level plane and/or a rising-edge plane:
 *                ktcDecodeKbc16 / ktcDecodeKbc24  level + change (button modules)
 *                ktcDecodeKjc                     level + change (joysticks)
 *                ktcDecodeRiseB0                  rising edges in payload byte 0
 *                ktcDecodeRiseB1                  rising edges in payload byte 1 (throttle)
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#pragma once
#include "KTC_Runner.h"

// ============================================================
//  Input decoding
// ============================================================
struct KtcInputs {
    uint32_t level;     // current level (1 = pressed / closed), if hasLevel
    uint32_t change;    // changed since the previous read, if reported
    uint32_t rise;      // rising edges, for families that report edges only
    bool     hasLevel;
};
typedef void (*KtcDecodeFn)(const KtcCtx& c, KtcInputs& in);

void ktcDecodeKbc16(const KtcCtx& c, KtcInputs& in);
void ktcDecodeKbc24(const KtcCtx& c, KtcInputs& in);
void ktcDecodeKjc(const KtcCtx& c, KtcInputs& in);
void ktcDecodeRiseB0(const KtcCtx& c, KtcInputs& in);
void ktcDecodeRiseB1(const KtcCtx& c, KtcInputs& in);

// ============================================================
//  Identity — type, firmware (major equal, minor >= expected), caps
// ============================================================
KtcVerdict ktcStepIdentity(KtcCtx& c);           // arg: none

// ============================================================
//  Boot handshake
//    RST pulse -> INT low within settleMs -> boot packet
//    (status BOOT_READY, type, tx = 1) -> INT released by the read
//    -> CMD_DISABLE -> (pollable modules) a read shows DISABLED.
//    If RST does not restart the module the operator is asked to
//    unplug and reconnect it instead.
// ============================================================
struct KtcBootCfg {
    uint16_t    settleMs;      // longest expected start-up (e.g. joystick centre calibration)
    bool        pollable;      // a read with nothing pending returns a live packet
    const char* holdOff;       // shown before the reset, e.g. "Hands off the stick." (or nullptr)
};
KtcVerdict ktcStepBoot(KtcCtx& c);

// ============================================================
//  Enable + INT
//    CMD_ENABLE -> ACTIVE (pollable) -> INT idle high for 500 ms ->
//    operator presses and releases one input -> INT asserts, each
//    read releases it, tx counts up by one per INT, and the press
//    plus release produce exactly `packets` packets.
// ============================================================
struct KtcIntCfg {
    const char* prompt;        // e.g. "Press and release AG1."
    uint8_t     bit;           // the input the operator uses
    uint8_t     packets;       // expected packets for one press + release
    bool        pollable;
    KtcDecodeFn decode;
};
KtcVerdict ktcStepEnableInt(KtcCtx& c);

// ============================================================
//  Input walk — prompt each input in turn and wait for it
// ============================================================
struct KtcWalkItem {
    const char* label;         // chip label and prompt text
    uint8_t     bit;           // input bit in the decoded planes
    const char* action;        // "Press", "Set", "Turn" ... (nullptr = "Press")
    bool        noRelease;     // true: do not wait for the level to return to 0 (default: wait)
    const char* releaseText;   // prompt for the return, e.g. "Return MSTR to OPER" (nullptr = "Release <label>")
    const char* prompt;        // full prompt, e.g. "Set SCE to AUX" (nullptr = "<action> <label>")
};
struct KtcWalk {
    uint8_t            count;
    const KtcWalkItem* items;
    KtcDecodeFn        decode;
    uint32_t           watch;      // every bit that must only change when prompted
                                   // (include no-connect bits to catch phantoms)
    uint16_t           pollMs;     // 0 = INT-gated reads; >0 = also read every pollMs
    uint8_t            cols;       // chip grid columns
    void (*prep)(KtcCtx& c);       // optional: run once at the start (e.g. ENABLE)
    const char*        instr;      // instruction text (nullptr = generic)
};
KtcVerdict ktcStepWalk(KtcCtx& c);

// ============================================================
//  LED walk — one position ACTIVE at a time, operator confirms colour
// ============================================================
struct KtcLedItem {
    const char* label;         // button name
    uint8_t     pos;           // LED nibble position
    RGBColor    color;         // expected ACTIVE colour
    const char* colorName;
};
struct KtcLedWalk {
    uint8_t           count;
    const KtcLedItem* items;
    uint8_t           others;  // state for the other positions (KMC_LED_ENABLED or KMC_LED_OFF)
};
KtcVerdict ktcStepLedWalk(KtcCtx& c);

// ============================================================
//  Visual sequence — apply something, operator confirms what they see
// ============================================================
struct KtcVisualItem {
    const char* label;                 // short name for the result note
    const char* prompt;                // what the operator should see
    void (*apply)(KtcCtx& c);          // commands to send (nullptr = none)
    uint16_t    swatch;                // RGB565 preview colour (0 = none)
};
struct KtcVisualSeq {
    uint8_t              count;
    const KtcVisualItem* items;
    void (*before)(KtcCtx& c);         // optional, once at the start
    void (*after)(KtcCtx& c);          // optional, once at the end (also on failure)
};
KtcVerdict ktcStepVisual(KtcCtx& c);

// ============================================================
//  Lifecycle (modules that answer a read at any time)
//    DISABLE -> DISABLED; input still read, no INT
//    SLEEP   -> SLEEPING; input still read (slowly), no INT
//    WAKE    -> ACTIVE; tx +1 (the wake INT)
//    ENABLE  -> ACTIVE
// ============================================================
struct KtcLifeCfg {
    const char* input;         // input the operator uses, e.g. "AG1"
    uint8_t     bit;
    KtcDecodeFn decode;
    const char* darkPrompt;    // asked after DISABLE (e.g. "All button LEDs dark?"), or nullptr
};
KtcVerdict ktcStepLifecycle(KtcCtx& c);

// ============================================================
//  Supply current — idle (DISABLED, LEDs off) and under a load
//  (bulb test, motor, ...). Limits are provisional until measured.
// ============================================================
struct KtcCurrentCfg {
    uint16_t    idleMax_mA;    // idle above this fails (short / wrong part)
    uint16_t    loadRise_mA;   // load must add at least this much (0 = no load test)
    const char* loadName;      // e.g. "bulb test"
    void (*idle)(KtcCtx& c);   // put the module in its idle state
    void (*load)(KtcCtx& c);   // apply the load
    void (*unload)(KtcCtx& c); // remove the load
};
KtcVerdict ktcStepCurrent(KtcCtx& c);

// ============================================================
//  Small command helpers usable as apply/prep/idle callbacks
// ============================================================
void ktcDoEnable(KtcCtx& c);
void ktcDoDisable(KtcCtx& c);
void ktcDoBulbOn(KtcCtx& c);
void ktcDoBulbOff(KtcCtx& c);
void ktcDoLedsOff(KtcCtx& c);
void ktcDoLedsEnabled(KtcCtx& c);
void ktcDoLedsActive(KtcCtx& c);

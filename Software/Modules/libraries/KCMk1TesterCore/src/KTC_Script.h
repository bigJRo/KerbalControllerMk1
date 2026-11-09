/**
 * @file        KTC_Script.h
 * @version     1.0.0
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Script step: a short list of commands, operator actions and
 *              checks, run in order. Used for modules whose inputs drive
 *              their own state (the 7-segment display modules), where each
 *              press must produce one packet with a known state and value.
 *
 *              Packets are read when INT asserts (these modules answer a read
 *              with nothing pending with an empty header, so they are not
 *              polled). The script tracks the latest state byte and value.
 *
 *              Ops:
 *                KS_CMD    run fn (send commands); if state/value are given,
 *                          wait up to 400 ms for a packet that matches
 *                KS_PRESS  prompt; wait for a packet with event bit `bit`,
 *                          then check state/value if given
 *                KS_QUIET  prompt + NEXT; any packet before NEXT fails
 *                KS_ASK    question, PASS / FAIL (failures are collected)
 *                KS_TURN   prompt + NEXT; then the value must equal `value`
 *                          or have changed by lo..hi
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#pragma once
#include "KTC_Steps.h"

#define KS_ANY  (-100000L)

enum KtcScriptOp : uint8_t { KS_CMD, KS_PRESS, KS_QUIET, KS_ASK, KS_TURN };

struct KtcScriptItem {
    KtcScriptOp op;
    const char* text;               // prompt / question (CMD: optional status line)
    void (*fn)(KtcCtx& c);          // KS_CMD: commands to send
    int8_t      bit;                // KS_PRESS: event bit
    int16_t     state;              // expected state byte, -1 = any
    int32_t     value;              // expected value, KS_ANY = any
    int32_t     lo, hi;             // KS_TURN: allowed change when value is KS_ANY
    int8_t      life;               // KS_CMD: expected lifecycle in the packet, -1 = any
};

struct KtcScript {
    uint8_t              count;
    const KtcScriptItem* items;
    uint8_t              eventsOff;   // payload offset of the events byte
    uint8_t              stateOff;    // payload offset of the state byte
    uint8_t              valueOff;    // payload offset of the int16 big-endian value
};

KtcVerdict ktcStepScript(KtcCtx& c);

// Builders (keep the tables readable)
#define KS_DO(fn)                         { KS_CMD,   nullptr, fn, -1, -1, KS_ANY, 0, 0, -1 }
#define KS_DO_EXPECT(fn, st, val, lc)     { KS_CMD,   nullptr, fn, -1, st, val, 0, 0, lc }
#define KS_PRESS_(txt, b, st, val)        { KS_PRESS, txt, nullptr, b, st, val, 0, 0, -1 }
#define KS_QUIET_(txt)                    { KS_QUIET, txt, nullptr, -1, -1, KS_ANY, 0, 0, -1 }
#define KS_ASK_(txt)                      { KS_ASK,   txt, nullptr, -1, -1, KS_ANY, 0, 0, -1 }
#define KS_TURN_TO(txt, val)              { KS_TURN,  txt, nullptr, -1, -1, val, 0, 0, -1 }
#define KS_TURN_BY(txt, lo, hi)           { KS_TURN,  txt, nullptr, -1, -1, KS_ANY, lo, hi, -1 }

/**
 * @file        KTC_Script.cpp
 * @version     1.0.0
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Script step (see KTC_Script.h).
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include "KTC_Script.h"
#include "KTC_Config.h"

// Scratch use: v[0] item index, v[1] value at the start of a TURN, v[2] ASK
// failures (bit mask), v[3] item start time, v[6] last value, v[7] last state,
// m[0] set once a packet has been seen.

static void _scriptShowItem(KtcCtx& c, const KtcScript* s) {
    const KtcScriptItem& it = s->items[c.v[0]];
    char p[100];
    if (it.text) {
        snprintf(p, sizeof(p), "%u/%u  %s", (unsigned)(c.v[0] + 1), (unsigned)s->count, it.text);
        ktcUiInstr(p);
    }
    uint16_t b = 0;
    switch (it.op) {
        case KS_PRESS: b = KTC_BTN_FAIL; break;
        case KS_QUIET: b = KTC_BTN_NEXT | KTC_BTN_FAIL; break;
        case KS_ASK:   b = KTC_BTN_PASS | KTC_BTN_FAIL; break;
        case KS_TURN:  b = KTC_BTN_NEXT | KTC_BTN_FAIL; break;
        default:       b = 0; break;
    }
    if (it.op != KS_CMD) ktcUiButtons(b | KTC_BTN_SKIP | KTC_BTN_ABORT);
    c.v[3] = (int32_t)c.now;
    c.v[1] = c.v[6];
}

static void _scriptLive(KtcCtx& c) {
    if (!c.m[0]) return;
    ktcUiRowf(8, KTC_C_TEXT, "Value %ld   state %02lX", (long)c.v[6], (unsigned long)(c.v[7] & 0xFF));
}

static KtcVerdict _checkPacket(KtcCtx& c, const KtcScriptItem& it, const char* what) {
    if (it.state >= 0 && c.v[7] != it.state)
        return ktcFail(c, "%s: state %02lX, expected %02X", what, (unsigned long)c.v[7], it.state);
    if (it.value != KS_ANY && c.v[6] != it.value)
        return ktcFail(c, "%s: value %ld, expected %ld", what, (long)c.v[6], (long)it.value);
    if (it.life >= 0 && c.lifecycle != (uint8_t)it.life)
        return ktcFail(c, "%s: %s, expected %s", what, ktcLifecycleName(c.lifecycle), ktcLifecycleName(it.life));
    return KTC_RUNNING;
}

static void _next(KtcCtx& c, const KtcScript* s) {
    c.v[0]++;
    if (c.v[0] < s->count) _scriptShowItem(c, s);
}

KtcVerdict ktcStepScript(KtcCtx& c) {
    const KtcScript* s = (const KtcScript*)c.arg;
    if (c.entry) {
        ktcShow(c, nullptr, 0);
        c.autoRead = true;
        c.v[0] = 0; c.v[2] = 0; c.v[6] = 0; c.v[7] = 0; c.m[0] = 0;
        _scriptShowItem(c, s);
    }
    bool pkt = false;
    if (c.fresh) {
        c.v[6] = ktcS16(c, s->valueOff);
        c.v[7] = ktcByte(c, s->stateOff);
        c.m[0] = 1;
        pkt = true;
        _scriptLive(c);
    }
    if (c.v[0] >= s->count) goto done;
    {
        const KtcScriptItem& it = s->items[c.v[0]];
        char what[24];
        snprintf(what, sizeof(what), "item %ld", (long)(c.v[0] + 1));
        if (c.btn == KTC_BTN_FAIL && it.op != KS_ASK) {
            return ktcFail(c, "%s: %s", what, it.text ? it.text : "operator FAIL");
        }
        switch (it.op) {
        case KS_CMD:
            if (!c.m[1]) {                       // send once
                c.m[1] = 1;
                if (it.fn) it.fn(c);
                c.v[3] = (int32_t)c.now;
                if (it.state < 0 && it.value == KS_ANY && it.life < 0) { c.m[1] = 0; _next(c, s); }
                break;
            }
            if (pkt) {
                KtcVerdict v = _checkPacket(c, it, what);
                if (v != KTC_RUNNING) return v;
                c.m[1] = 0;
                _next(c, s);
            } else if (c.now - (uint32_t)c.v[3] > 400) {
                return ktcFail(c, "%s: no packet after the command", what);
            }
            break;
        case KS_PRESS:
            if (pkt && (ktcByte(c, s->eventsOff) & (1u << it.bit))) {
                KtcVerdict v = _checkPacket(c, it, what);
                if (v != KTC_RUNNING) return v;
                _next(c, s);
            }
            break;
        case KS_QUIET:
            if (pkt || ktcIntAsserted()) return ktcFail(c, "%s: module reported while it should not", what);
            if (c.btn == KTC_BTN_NEXT) _next(c, s);
            break;
        case KS_ASK:
            if (c.btn == KTC_BTN_PASS || c.btn == KTC_BTN_FAIL) {
                if (c.btn == KTC_BTN_FAIL) c.v[2] |= 1L << (c.v[0] < 30 ? c.v[0] : 30);
                _next(c, s);
            }
            break;
        case KS_TURN:
            if (pkt) ktcUiRowf(9, KTC_C_DIM, "Change %+ld", (long)(c.v[6] - c.v[1]));
            if (c.btn == KTC_BTN_NEXT) {
                if (it.value != KS_ANY) {
                    if (c.v[6] != it.value)
                        return ktcFail(c, "%s: value %ld, expected %ld", what, (long)c.v[6], (long)it.value);
                } else {
                    int32_t d = c.v[6] - c.v[1];
                    if (d < it.lo || d > it.hi)
                        return ktcFail(c, "%s: changed %+ld, expected %+ld..%+ld", what, (long)d, (long)it.lo, (long)it.hi);
                }
                _next(c, s);
            }
            break;
        }
    }
    if (c.v[0] < s->count) return KTC_RUNNING;
done:
    if (c.v[2]) {
        uint8_t first = 0;
        while (!(c.v[2] & (1L << first))) first++;
        return ktcFail(c, "check %u: %s", first + 1, s->items[first].text);
    }
    return ktcPass(c, "%u items ok", s->count);
}

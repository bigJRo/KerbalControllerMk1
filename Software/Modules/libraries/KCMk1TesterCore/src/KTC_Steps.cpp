/**
 * @file        KTC_Steps.cpp
 * @version     1.0.0
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Reusable test steps (see KTC_Steps.h).
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include "KTC_Steps.h"
#include "KTC_Config.h"

// ============================================================
//  Decoders
// ============================================================
void ktcDecodeKbc16(const KtcCtx& c, KtcInputs& in) {
    in.level = ktcBits(c, 0, 2);
    in.change = ktcBits(c, 2, 2);
    in.rise = 0;
    in.hasLevel = true;
}

void ktcDecodeKbc24(const KtcCtx& c, KtcInputs& in) {
    in.level = ktcBits(c, 0, 3);
    in.change = ktcBits(c, 3, 3);
    in.rise = 0;
    in.hasLevel = true;
}

void ktcDecodeKjc(const KtcCtx& c, KtcInputs& in) {
    in.level = ktcByte(c, 2);
    in.change = ktcByte(c, 1);
    in.rise = 0;
    in.hasLevel = true;
}

void ktcDecodeRiseB0(const KtcCtx& c, KtcInputs& in) {
    in.rise = ktcByte(c, 0);
    in.change = ktcByte(c, 1);
    in.level = 0;
    in.hasLevel = false;
}

void ktcDecodeRiseB1(const KtcCtx& c, KtcInputs& in) {
    in.rise = ktcByte(c, 1);
    in.change = 0;
    in.level = 0;
    in.hasLevel = false;
}

/** Rising edges from a decoded packet, given the previous level plane. */
static uint32_t _rises(const KtcInputs& in, uint32_t prevLevel) {
    if (!in.hasLevel) return in.rise;
    // New level-high bits, plus taps (changed but low now, and low before).
    return (in.level & ~prevLevel) | (in.change & ~in.level & ~prevLevel);
}

// ============================================================
//  Command helpers
// ============================================================
void ktcDoEnable(KtcCtx& c)      { ktcSend(c, KMC_CMD_ENABLE); }
void ktcDoDisable(KtcCtx& c)     { ktcSend(c, KMC_CMD_DISABLE); }
void ktcDoBulbOn(KtcCtx& c)      { ktcSend1(c, KMC_CMD_BULB_TEST, 0x01); }
void ktcDoBulbOff(KtcCtx& c)     { ktcSend1(c, KMC_CMD_BULB_TEST, 0x00); }
void ktcDoLedsOff(KtcCtx& c)     { ktcLedsAll(c, KMC_LED_OFF); }
void ktcDoLedsEnabled(KtcCtx& c) { ktcLedsAll(c, KMC_LED_ENABLED); }
void ktcDoLedsActive(KtcCtx& c)  { ktcLedsAll(c, KMC_LED_ACTIVE); }

// ============================================================
//  Identity
// ============================================================
KtcVerdict ktcStepIdentity(KtcCtx& c) {
    const KtcModuleDef& m = *c.mod;
    ktcShow(c, "Reading the identity reply (CMD_GET_IDENTITY).", 0);
    KtcIdentity id = ktcIdentify(c.addr);
    if (!id.valid) return ktcFail(c, "no identity reply");
    ktcUiRowf(0, KTC_C_TEXT, "Type      0x%02X   expected 0x%02X", id.typeId, m.typeId);
    ktcUiRowf(1, KTC_C_TEXT, "Firmware  %u.%u    expected %u.%u", id.fwMajor, id.fwMinor, m.fwMajor, m.fwMinor);
    ktcUiRowf(2, KTC_C_TEXT, "Caps      0x%02X   expected 0x%02X", id.caps, m.caps);
    if (id.typeId != m.typeId) return ktcFail(c, "type 0x%02X, not 0x%02X", id.typeId, m.typeId);
    if (id.caps & KMC_CAP_FAULT) return ktcFail(c, "fault flag set in caps (0x%02X)", id.caps);
    if (id.caps != m.caps) return ktcFail(c, "caps 0x%02X, expected 0x%02X", id.caps, m.caps);
    if (id.fwMajor != m.fwMajor || id.fwMinor < m.fwMinor)
        return ktcFail(c, "fw %u.%u, tester expects %u.%u", id.fwMajor, id.fwMinor, m.fwMajor, m.fwMinor);
    if (id.fwMinor > m.fwMinor) return ktcPass(c, "fw %u.%u (newer than tester)", id.fwMajor, id.fwMinor);
    return ktcPass(c, "type 0x%02X fw %u.%u caps 0x%02X", id.typeId, id.fwMajor, id.fwMinor, id.caps);
}

// ============================================================
//  Boot handshake
// ============================================================
enum { BOOT_HOLDOFF, BOOT_WAIT_INT, BOOT_RECONNECT, BOOT_READ };

KtcVerdict ktcStepBoot(KtcCtx& c) {
    const KtcBootCfg* cfg = (const KtcBootCfg*)c.arg;
    uint16_t settle = cfg ? cfg->settleMs : 300;
    if (c.entry) {
        c.autoRead = false;
        char instr[96];
        snprintf(instr, sizeof(instr), "%sThe tester pulses RST and checks the boot packet.",
                 (cfg && cfg->holdOff) ? cfg->holdOff : "");
        ktcShow(c, instr, 0);
        ktcPhase(c, BOOT_HOLDOFF);
    }
    switch (c.phase) {
    case BOOT_HOLDOFF:
        if (ktcPhaseMs(c) < ((cfg && cfg->holdOff) ? 1500u : 200u)) return KTC_RUNNING;
        ktcResetPulse(5);
        delay(20);                     // let a stale INT level clear while the part restarts
        ktcUiRow(0, "RST pulsed, waiting for the boot INT...", KTC_C_DIM);
        c.v[0] = 0;                    // 0 = via RST, 1 = via reconnect
        ktcPhase(c, BOOT_WAIT_INT);
        return KTC_RUNNING;

    case BOOT_WAIT_INT:
        if (ktcIntAsserted()) {
            c.v[1] = (int32_t)ktcPhaseMs(c);
            ktcPhase(c, BOOT_READ);
            return KTC_RUNNING;
        }
        if (ktcPhaseMs(c) > (uint32_t)settle + 1500u) {
            ktcPhase(c, BOOT_RECONNECT);
        }
        return KTC_RUNNING;

    case BOOT_RECONNECT:
        if (c.phaseEntry) {
            ktcShow(c, "RST did not restart the module. Unplug it, wait 2 s and plug it back in.",
                    KTC_BTN_FAIL);
            c.v[0] = 1;
            c.v[2] = 0;                // seen unplugged
        }
        if (c.btn == KTC_BTN_FAIL) return ktcFail(c, "no boot INT (RST and reconnect)");
        if (!c.v[2]) {
            if (!ktcProbe(c.addr)) { c.v[2] = 1; ktcUiRow(1, "Module unplugged. Plug it back in.", KTC_C_WARN); }
            return KTC_RUNNING;
        }
        if (ktcIntAsserted() && ktcProbe(c.addr)) {
            delay(settle);             // let a joystick finish its centre calibration
            c.v[1] = -1;
            ktcPhase(c, BOOT_READ);
        }
        if (ktcPhaseMs(c) > 60000u) return ktcFail(c, "no boot INT after reconnect");
        return KTC_RUNNING;

    case BOOT_READ: {
        if (!ktcReadNow(c)) return ktcFail(c, "boot packet unreadable (type byte 0x%02X)", c.pkt[1]);
        ktcUiRowf(1, KTC_C_TEXT, "Boot packet  %02X %02X %02X", c.pkt[0], c.pkt[1], c.pkt[2]);
        if (c.lifecycle != KMC_STATUS_BOOT_READY) {
            if (c.v[0] == 0) {         // RST did nothing: fall back to a reconnect
                ktcPhase(c, BOOT_RECONNECT);
                return KTC_RUNNING;
            }
            return ktcFail(c, "status %s, expected BOOT_READY", ktcLifecycleName(c.lifecycle));
        }
        if (c.pkt[2] != 0x01) return ktcFail(c, "boot tx counter %u, expected 1", c.pkt[2]);
        delay(2);
        if (ktcIntAsserted()) return ktcFail(c, "INT still low after the boot read");
        ktcUiRow(2, "INT released by the read.", KTC_C_TEXT);
        ktcSend(c, KMC_CMD_DISABLE);
        delay(KTC_CMD_SETTLE_MS);
        if (cfg && cfg->pollable) {
            if (!ktcReadNow(c)) return ktcFail(c, "no packet after CMD_DISABLE");
            if (c.lifecycle != KMC_STATUS_DISABLED)
                return ktcFail(c, "status %s after DISABLE", ktcLifecycleName(c.lifecycle));
            ktcUiRow(3, "CMD_DISABLE -> DISABLED.", KTC_C_TEXT);
        } else {
            ktcUiRow(3, "CMD_DISABLE sent.", KTC_C_TEXT);
        }
        if (ktcIntAsserted()) return ktcFail(c, "INT low after CMD_DISABLE");
        if (c.v[0] == 1) return ktcPass(c, "boot ok after reconnect (RST no effect)");
        return ktcPass(c, "boot INT after %ld ms, tx 1", (long)c.v[1]);
    }
    }
    return KTC_RUNNING;
}

// ============================================================
//  Enable + INT
// ============================================================
enum { EI_ENABLE, EI_IDLE, EI_INPUT };

KtcVerdict ktcStepEnableInt(KtcCtx& c) {
    const KtcIntCfg* cfg = (const KtcIntCfg*)c.arg;
    if (c.entry) {
        c.autoRead = false;
        ktcShow(c, "CMD_ENABLE, then INT must stay high with no input.", 0);
        ktcPhase(c, EI_ENABLE);
    }
    switch (c.phase) {
    case EI_ENABLE: {
        ktcSend(c, KMC_CMD_ENABLE);
        delay(50);
        c.v[2] = -1;                                   // last tx, unknown
        for (uint8_t i = 0; i < 10 && ktcIntAsserted(); i++) {   // packets left over from DISABLED
            if (ktcReadNow(c)) c.v[2] = c.pkt[2];
            delay(10);
        }
        if (cfg->pollable) {
            if (!ktcReadNow(c)) return ktcFail(c, "no packet after CMD_ENABLE");
            c.v[2] = c.pkt[2];
        }
        if (c.v[2] >= 0) {
            if (c.lifecycle != KMC_STATUS_ACTIVE)
                return ktcFail(c, "status %s after ENABLE", ktcLifecycleName(c.lifecycle));
            ktcUiRowf(0, KTC_C_TEXT, "ENABLE -> ACTIVE, tx %ld", (long)c.v[2]);
        }
        ktcPhase(c, EI_IDLE);
        return KTC_RUNNING;
    }
    case EI_IDLE:
        if (ktcIntAsserted()) return ktcFail(c, "INT asserted with no input (hands off?)");
        if (ktcPhaseMs(c) >= 500) {
            ktcUiRow(1, "INT idle high: ok", KTC_C_TEXT);
            ktcPhase(c, EI_INPUT);
        }
        return KTC_RUNNING;

    case EI_INPUT:
        if (c.phaseEntry) {
            ktcUiInstr(cfg->prompt);
            c.autoRead = true;
            c.v[3] = 0;        // packets
            c.v[4] = 0;        // input seen
            c.v[5] = 0;        // last packet time
            c.m[0] = 0;        // prev level
            c.v[6] = 0;        // still held
        }
        if (c.fresh) {
            KtcInputs in;
            cfg->decode(c, in);
            uint32_t bit = 1UL << cfg->bit;
            if (c.v[2] >= 0 && c.pkt[2] != (uint8_t)(c.v[2] + 1))
                return ktcFail(c, "tx %u after %ld (expected +1 per INT)", c.pkt[2], (long)c.v[2]);
            c.v[2] = c.pkt[2];
            c.v[3]++;
            c.v[5] = (int32_t)c.now;
            if (_rises(in, c.m[0]) & bit) c.v[4] = 1;
            c.v[6] = in.hasLevel && (in.level & bit);
            if (in.hasLevel) c.m[0] = in.level;
            ktcUiRowf(3, KTC_C_TEXT, "Packets %ld   tx %ld   input %s", (long)c.v[3], (long)c.v[2],
                      c.v[4] ? "seen" : "-");
        }
        if (c.v[3] == 0 && ktcPhaseMs(c) > 20000) return ktcFail(c, "no INT after the input");
        if (c.v[4] && !c.v[6] && c.now - (uint32_t)c.v[5] > 800) {
            if (c.v[3] != cfg->packets)
                return ktcFail(c, "%ld packets for one press+release, expected %u", (long)c.v[3], cfg->packets);
            return ktcPass(c, "%ld packets, tx continuous", (long)c.v[3]);
        }
        if (c.v[3] > 0 && !c.v[4] && c.now - (uint32_t)c.v[5] > 3000)
            return ktcFail(c, "INT came but the input bit never set");
        return KTC_RUNNING;
    }
    return KTC_RUNNING;
}

// ============================================================
//  Input walk
// ============================================================
static const char* _walkLabelForBit(const KtcWalk* w, uint8_t bit, char* buf, uint8_t n) {
    for (uint8_t i = 0; i < w->count; i++)
        if (w->items[i].bit == bit) return w->items[i].label;
    snprintf(buf, n, "bit %u", bit);
    return buf;
}

static void _walkDrawChip(const KtcWalk* w, KtcCtx& c, uint8_t i) {
    uint16_t fill = KTC_C_OFF, outline = 0;
    if (c.m[1] & (1UL << i)) fill = KTC_C_FAIL;
    else if (c.m[0] & (1UL << i)) fill = KTC_C_PASS;
    if ((int32_t)i == c.v[0]) outline = KTC_C_ACCENT;
    ktcUiGridChip(i, w->cols ? w->cols : 4, 24, w->items[i].label, fill, outline);
}

static void _walkPrompt(const KtcWalk* w, KtcCtx& c) {
    char p[64];
    const KtcWalkItem& it = w->items[c.v[0]];
    if (c.v[1] && it.releaseText) snprintf(p, sizeof(p), "%s.", it.releaseText);
    else if (c.v[1]) snprintf(p, sizeof(p), "Release %s.", it.label);
    else if (it.prompt) snprintf(p, sizeof(p), "%s.", it.prompt);
    else snprintf(p, sizeof(p), "%s %s.", it.action ? it.action : "Press", it.label);
    ktcUiRowf(0, KTC_C_ACCENT, "%u/%u  %s", (unsigned)(c.v[0] + 1), (unsigned)w->count, p);
}

KtcVerdict ktcStepWalk(KtcCtx& c) {
    const KtcWalk* w = (const KtcWalk*)c.arg;
    if (c.entry) {
        ktcShow(c, w->instr ? w->instr : "Operate each input when it is named. Tap FAIL if one does nothing.",
                KTC_BTN_FAIL);
        if (w->prep) w->prep(c);
        c.autoRead = true;
        c.v[0] = 0; c.v[1] = 0; c.v[2] = (int32_t)c.now;
        c.m[0] = 0; c.m[1] = 0; c.m[2] = 0;
        // Stuck / phantom check on a fresh read (level families only).
        if (w->pollMs) {
            ktcDrain(c);
            if (ktcReadNow(c)) {
                KtcInputs in;
                w->decode(c, in);
                if (in.hasLevel) {
                    uint32_t stuck = in.level & w->watch;
                    if (stuck) {
                        char b[12];
                        uint8_t bit = 0;
                        while (!(stuck & (1UL << bit))) bit++;
                        return ktcFail(c, "%s reads ON at the start", _walkLabelForBit(w, bit, b, sizeof(b)));
                    }
                    c.m[2] = in.level;
                }
            }
            c.fresh = false;
        }
        for (uint8_t i = 0; i < w->count; i++) _walkDrawChip(w, c, i);
        _walkPrompt(w, c);
    }

    if (c.btn == KTC_BTN_FAIL) {
        c.m[1] |= 1UL << c.v[0];
        c.v[1] = 0;
        uint8_t was = (uint8_t)c.v[0];
        c.v[0]++;
        _walkDrawChip(w, c, was);
        if (c.v[0] < w->count) { _walkDrawChip(w, c, (uint8_t)c.v[0]); _walkPrompt(w, c); }
    }

    if (!c.fresh && w->pollMs && c.now - (uint32_t)c.v[2] >= w->pollMs) {
        c.v[2] = (int32_t)c.now;
        ktcReadNow(c);
    }

    if (c.fresh && c.v[0] < w->count) {
        KtcInputs in;
        w->decode(c, in);
        uint32_t rise = _rises(in, c.m[2]);
        if (in.hasLevel) c.m[2] = in.level;
        const KtcWalkItem& it = w->items[c.v[0]];
        uint32_t bit = 1UL << it.bit;

        // Phantom: a watched bit that belongs to no item.
        uint32_t known = 0;
        for (uint8_t i = 0; i < w->count; i++) known |= 1UL << w->items[i].bit;
        uint32_t phantom = rise & w->watch & ~known;
        if (phantom) {
            uint8_t b = 0;
            while (!(phantom & (1UL << b))) b++;
            return ktcFail(c, "unconnected input bit %u went high", b);
        }
        uint32_t wrong = rise & w->watch & ~bit;
        if (wrong) {
            char b[12];
            uint8_t wb = 0;
            while (!(wrong & (1UL << wb))) wb++;
            char t[48];
            snprintf(t, sizeof(t), "Unexpected input: %s", _walkLabelForBit(w, wb, b, sizeof(b)));
            ktcUiToast(t, KTC_C_WARN);
        }
        bool done = false;
        if (!c.v[1] && (rise & bit)) {
            if (!it.noRelease && in.hasLevel && (in.level & bit)) { c.v[1] = 1; _walkPrompt(w, c); }
            else done = true;
        } else if (c.v[1] && in.hasLevel && !(in.level & bit)) {
            done = true;
        }
        if (done) {
            c.m[0] |= 1UL << c.v[0];
            c.v[1] = 0;
            uint8_t was = (uint8_t)c.v[0];
            c.v[0]++;
            _walkDrawChip(w, c, was);
            if (c.v[0] < w->count) { _walkDrawChip(w, c, (uint8_t)c.v[0]); _walkPrompt(w, c); }
            if (!wrong) ktcUiToast(nullptr);
        }
    }

    if (c.v[0] >= w->count) {
        if (c.m[1]) {
            uint8_t n = 0, first = 0;
            for (uint8_t i = 0; i < w->count; i++)
                if (c.m[1] & (1UL << i)) { if (!n) first = i; n++; }
            if (n == 1) return ktcFail(c, "%s failed", w->items[first].label);
            return ktcFail(c, "%u failed, first %s", n, w->items[first].label);
        }
        return ktcPass(c, "%u inputs ok", w->count);
    }
    return KTC_RUNNING;
}

// ============================================================
//  LED walk
// ============================================================
static void _ledWalkApply(KtcCtx& c, const KtcLedWalk* w) {
    uint8_t st[16];
    for (uint8_t i = 0; i < 16; i++) st[i] = w->others;
    st[w->items[c.v[0]].pos] = KMC_LED_ACTIVE;
    ktcLeds(c, st);
    const KtcLedItem& it = w->items[c.v[0]];
    char p[80];
    snprintf(p, sizeof(p), "%u/%u  Is %s lit %s, and only that one?", (unsigned)(c.v[0] + 1),
             (unsigned)w->count, it.label, it.colorName);
    ktcUiInstr(p);
    ktcUiLiveClear();
    ktcUiSwatch(70, KTC_LIVE_Y + 10, 100, 70, ktcLed565(it.color), it.colorName);
    ktcUiRowf(9, KTC_C_DIM, "position %u, others %s", it.pos, w->others == KMC_LED_OFF ? "off" : "ENABLED");
}

KtcVerdict ktcStepLedWalk(KtcCtx& c) {
    const KtcLedWalk* w = (const KtcLedWalk*)c.arg;
    if (c.entry) {
        ktcShow(c, nullptr, KTC_BTN_PASS | KTC_BTN_FAIL);
        c.v[0] = 0;
        c.m[1] = 0;
        _ledWalkApply(c, w);
        return KTC_RUNNING;
    }
    if (c.btn == KTC_BTN_PASS || c.btn == KTC_BTN_FAIL) {
        if (c.btn == KTC_BTN_FAIL) c.m[1] |= 1UL << c.v[0];
        c.v[0]++;
        if (c.v[0] < w->count) { _ledWalkApply(c, w); return KTC_RUNNING; }
        ktcLedsAll(c, KMC_LED_ENABLED);
        if (c.m[1]) {
            uint8_t n = 0, first = 0;
            for (uint8_t i = 0; i < w->count; i++)
                if (c.m[1] & (1UL << i)) { if (!n) first = i; n++; }
            if (n == 1) return ktcFail(c, "LED %s wrong", w->items[first].label);
            return ktcFail(c, "%u LEDs wrong, first %s", n, w->items[first].label);
        }
        return ktcPass(c, "%u LEDs ok", w->count);
    }
    return KTC_RUNNING;
}

// ============================================================
//  Visual sequence
// ============================================================
static void _visualApply(KtcCtx& c, const KtcVisualSeq* s) {
    const KtcVisualItem& it = s->items[c.v[0]];
    if (it.apply) it.apply(c);
    char p[100];
    snprintf(p, sizeof(p), "%u/%u  %s", (unsigned)(c.v[0] + 1), (unsigned)s->count, it.prompt);
    ktcUiInstr(p);
    ktcUiLiveClear();
    if (it.swatch) ktcUiSwatch(70, KTC_LIVE_Y + 10, 100, 70, it.swatch, it.label);
    else ktcUiTextC(KTC_W / 2, KTC_LIVE_Y + 40, it.label, KTC_C_TEXT, 2);
}

KtcVerdict ktcStepVisual(KtcCtx& c) {
    const KtcVisualSeq* s = (const KtcVisualSeq*)c.arg;
    if (c.entry) {
        ktcShow(c, nullptr, KTC_BTN_PASS | KTC_BTN_FAIL);
        if (s->before) s->before(c);
        c.v[0] = 0;
        c.m[1] = 0;
        _visualApply(c, s);
        return KTC_RUNNING;
    }
    if (c.btn == KTC_BTN_PASS || c.btn == KTC_BTN_FAIL) {
        if (c.btn == KTC_BTN_FAIL) c.m[1] |= 1UL << c.v[0];
        c.v[0]++;
        if (c.v[0] < s->count) { _visualApply(c, s); return KTC_RUNNING; }
        if (s->after) s->after(c);
        if (c.m[1]) {
            uint8_t n = 0, first = 0;
            for (uint8_t i = 0; i < s->count; i++)
                if (c.m[1] & (1UL << i)) { if (!n) first = i; n++; }
            if (n == 1) return ktcFail(c, "%s wrong", s->items[first].label);
            return ktcFail(c, "%u wrong, first %s", n, s->items[first].label);
        }
        return ktcPass(c, "%u checks ok", s->count);
    }
    return KTC_RUNNING;
}

// ============================================================
//  Lifecycle
// ============================================================
enum { LC_DISABLE, LC_DARK, LC_DIS_INPUT, LC_SLEEP, LC_SLP_INPUT, LC_WAKE };

/** Poll the input while INT must stay high. Returns 1 when pressed (and released
 *  for level families), -1 on an INT, 0 while waiting. */
static int8_t _lcWatch(KtcCtx& c, const KtcLifeCfg* cfg, uint16_t pollMs) {
    if (ktcIntAsserted()) return -1;
    if (c.now - (uint32_t)c.v[2] < pollMs) return 0;
    c.v[2] = (int32_t)c.now;
    if (!ktcReadNow(c)) return 0;
    KtcInputs in;
    cfg->decode(c, in);
    uint32_t bit = 1UL << cfg->bit;
    if (_rises(in, c.m[0]) & bit) c.v[3] = 1;
    if (in.hasLevel) c.m[0] = in.level;
    bool held = in.hasLevel && (in.level & bit);
    return (c.v[3] && !held) ? 1 : 0;
}

KtcVerdict ktcStepLifecycle(KtcCtx& c) {
    const KtcLifeCfg* cfg = (const KtcLifeCfg*)c.arg;
    char p[80];
    if (c.entry) {
        c.autoRead = false;
        ktcShow(c, "DISABLE, SLEEP, WAKE and ENABLE. INT must stay high while disabled or asleep.", 0);
        ktcPhase(c, LC_DISABLE);
    }
    switch (c.phase) {
    case LC_DISABLE:
        ktcDrain(c);
        ktcSend(c, KMC_CMD_DISABLE);
        delay(KTC_CMD_SETTLE_MS);
        if (!ktcReadNow(c)) return ktcFail(c, "no packet after DISABLE");
        if (c.lifecycle != KMC_STATUS_DISABLED)
            return ktcFail(c, "status %s after DISABLE", ktcLifecycleName(c.lifecycle));
        ktcUiRow(0, "DISABLE -> DISABLED", KTC_C_TEXT);
        ktcPhase(c, cfg->darkPrompt ? LC_DARK : LC_DIS_INPUT);
        return KTC_RUNNING;

    case LC_DARK:
        if (c.phaseEntry) { ktcUiInstr(cfg->darkPrompt); ktcUiButtons(KTC_BTN_PASS | KTC_BTN_FAIL | KTC_BTN_SKIP | KTC_BTN_ABORT); }
        if (c.btn == KTC_BTN_FAIL) return ktcFail(c, "LEDs not dark when DISABLED");
        if (c.btn == KTC_BTN_PASS) ktcPhase(c, LC_DIS_INPUT);
        return KTC_RUNNING;

    case LC_DIS_INPUT: {
        if (c.phaseEntry) {
            snprintf(p, sizeof(p), "Press and release %s. (Module DISABLED: no INT allowed.)", cfg->input);
            ktcUiInstr(p);
            ktcUiButtons(KTC_BTN_SKIP | KTC_BTN_ABORT);
            c.v[2] = 0; c.v[3] = 0; c.m[0] = 0;
        }
        int8_t r = _lcWatch(c, cfg, 50);
        if (r < 0) return ktcFail(c, "INT asserted while DISABLED");
        if (r > 0) { ktcUiRowf(1, KTC_C_TEXT, "%s read while DISABLED, no INT", cfg->input); ktcPhase(c, LC_SLEEP); }
        else if (ktcPhaseMs(c) > 20000) return ktcFail(c, "%s not read while DISABLED", cfg->input);
        return KTC_RUNNING;
    }

    case LC_SLEEP:
        ktcSend(c, KMC_CMD_SLEEP);
        delay(KTC_CMD_SETTLE_MS);
        if (!ktcReadNow(c)) return ktcFail(c, "no packet after SLEEP");
        if (c.lifecycle != KMC_STATUS_SLEEPING)
            return ktcFail(c, "status %s after SLEEP", ktcLifecycleName(c.lifecycle));
        ktcUiRow(2, "SLEEP -> SLEEPING", KTC_C_TEXT);
        ktcPhase(c, LC_SLP_INPUT);
        return KTC_RUNNING;

    case LC_SLP_INPUT: {
        if (c.phaseEntry) {
            snprintf(p, sizeof(p), "Press and HOLD %s for 1 s, then release. (Asleep: no INT allowed.)", cfg->input);
            ktcUiInstr(p);
            c.v[2] = 0; c.v[3] = 0; c.m[0] = 0;
        }
        int8_t r = _lcWatch(c, cfg, 100);
        if (r < 0) return ktcFail(c, "INT asserted while SLEEPING");
        if (r > 0) { ktcUiRowf(3, KTC_C_TEXT, "%s read while asleep, no INT", cfg->input); ktcPhase(c, LC_WAKE); }
        else if (ktcPhaseMs(c) > 20000) return ktcFail(c, "%s not read while SLEEPING", cfg->input);
        return KTC_RUNNING;
    }

    case LC_WAKE: {
        uint8_t tx = c.pkt[2];
        ktcSend(c, KMC_CMD_WAKE);
        delay(KTC_CMD_SETTLE_MS);
        if (!ktcReadNow(c)) return ktcFail(c, "no packet after WAKE");
        if (c.lifecycle != KMC_STATUS_ACTIVE)
            return ktcFail(c, "status %s after WAKE", ktcLifecycleName(c.lifecycle));
        if (c.pkt[2] != (uint8_t)(tx + 1))
            return ktcFail(c, "WAKE: tx %u -> %u, expected +1", tx, c.pkt[2]);
        ktcUiRow(4, "WAKE -> ACTIVE, wake INT counted", KTC_C_TEXT);
        ktcSend(c, KMC_CMD_ENABLE);
        delay(KTC_CMD_SETTLE_MS);
        ktcDrain(c);
        if (!ktcReadNow(c) || c.lifecycle != KMC_STATUS_ACTIVE)
            return ktcFail(c, "not ACTIVE after ENABLE");
        return ktcPass(c, "DISABLE/SLEEP/WAKE/ENABLE ok");
    }
    }
    return KTC_RUNNING;
}

// ============================================================
//  Supply current
// ============================================================
enum { CU_IDLE, CU_LOAD };

KtcVerdict ktcStepCurrent(KtcCtx& c) {
    const KtcCurrentCfg* cfg = (const KtcCurrentCfg*)c.arg;
    if (c.entry) {
        c.autoRead = false;
        ktcShow(c, "Measuring the module supply current (INA228). Hands off.", 0);
        if (cfg->idle) cfg->idle(c);
        c.v[0] = 0; c.v[1] = 0; c.v[2] = 0; c.v[3] = 0; c.v[4] = 0;
        ktcPhase(c, CU_IDLE);
        return KTC_RUNNING;
    }
    // Sample every 100 ms over the last second of a 2 s window.
    if (c.now - (uint32_t)c.v[4] < 100) return KTC_RUNNING;
    c.v[4] = (int32_t)c.now;
    KtcPower p = ktcReadPower();
    if (!p.ok) return KTC_RUNNING;
    uint32_t ms = ktcPhaseMs(c);
    int32_t mA = (int32_t)(p.amps * 1000.0f + 0.5f);
    if (c.phase == CU_IDLE) {
        ktcUiRowf(0, KTC_C_TEXT, "Idle: %ld mA at %.2f V", (long)mA, (double)p.volts);
        if (ms >= 1000) { c.v[0] += mA; c.v[1]++; }
        if (ms >= 2000) {
            if (c.v[1] == 0) return ktcFail(c, "INA228 not responding");
            c.v[0] /= c.v[1];
            if (cfg->loadRise_mA == 0) {
                if (c.v[0] > cfg->idleMax_mA) return ktcFail(c, "idle %ld mA > %u mA", (long)c.v[0], cfg->idleMax_mA);
                return ktcPass(c, "idle %ld mA", (long)c.v[0]);
            }
            if (cfg->load) cfg->load(c);
            ktcPhase(c, CU_LOAD);
        }
        return KTC_RUNNING;
    }
    ktcUiRowf(1, KTC_C_TEXT, "%s: %ld mA", cfg->loadName ? cfg->loadName : "Load", (long)mA);
    if (ms >= 1000) { c.v[2] += mA; c.v[3]++; }
    if (ms >= 2000) {
        if (cfg->unload) cfg->unload(c);
        if (c.v[3] == 0) return ktcFail(c, "INA228 not responding");
        c.v[2] /= c.v[3];
        ktcUiRowf(2, KTC_C_TEXT, "Idle %ld mA, %s %ld mA", (long)c.v[0], cfg->loadName, (long)c.v[2]);
        if (c.v[0] > cfg->idleMax_mA) return ktcFail(c, "idle %ld mA > %u mA", (long)c.v[0], cfg->idleMax_mA);
        if (c.v[2] - c.v[0] < (int32_t)cfg->loadRise_mA)
            return ktcFail(c, "%s adds %ld mA < %u mA", cfg->loadName, (long)(c.v[2] - c.v[0]), cfg->loadRise_mA);
        return ktcPass(c, "idle %ld mA, %s %ld mA", (long)c.v[0], cfg->loadName, (long)c.v[2]);
    }
    return KTC_RUNNING;
}

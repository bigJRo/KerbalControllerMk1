/**
 * @file        KCMk1_Test_Dual_Encoder.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Bench test for the Dual Encoder module (I2C 0x2D, type 0x0E),
 *              run on the KC-01-9001 v2.0 Module Tester (XIAO RA4M1).
 *
 *              Module (KC-01-1861/1862 with two KC-01-1851/1852 encoder
 *              carriers): two PEC11R encoders with push switches, no LEDs.
 *              Identity fw 2.0, caps 0x04 (ENCODERS).
 *
 *              Packet (7 bytes): header, events (rising edges, latched until
 *              read: bit 0 ENC1 push, bit 1 ENC2 push), change (press and
 *              release), ENC1 delta, ENC2 delta (int8, + = clockwise, one count
 *              per detent, cleared on read).
 *
 *              Steps: identity, boot handshake, enable + INT, push switches,
 *              ENC1 and ENC2 counted both ways (exactly 10 detents each way),
 *              lifecycle, supply current.
 *
 * @note        Board: Seeed XIAO RA4M1. Libraries: KCMk1TesterCore,
 *              KerbalModuleCommon, Adafruit GFX, Adafruit ST7735 and ST7789,
 *              Adafruit FT6206.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include <KCMk1TesterCore.h>

static const KtcWalkItem BUTTONS[] = { { "ENC1 PUSH", 0 }, { "ENC2 PUSH", 1 } };
static const KtcWalk WALK = { 2, BUTTONS, ktcDecodeRiseB0, 0xFF, 50, 2, ktcDoEnable,
    "Push each encoder knob when named." };

static const KtcIntCfg  INT_CFG = { "Press and release ENC1 PUSH.", 0, 2, true, ktcDecodeRiseB0 };
static const KtcLifeCfg LIFE    = { "ENC1 PUSH", 0, ktcDecodeRiseB0, nullptr };

// ------------------------------------------------------------
//  Encoder count: exactly 10 detents clockwise, then 10 anticlockwise
// ------------------------------------------------------------
struct EncCfg { uint8_t enc; const char* name; };
static const EncCfg ENC1 = { 0, "ENC1" };
static const EncCfg ENC2 = { 1, "ENC2" };

static void encPrompt(KtcCtx& c, const void* arg) {
    const EncCfg* e = (const EncCfg*)arg;
    char p[80];
    snprintf(p, sizeof(p), "Turn %s %s exactly 10 clicks, then tap NEXT.", e->name,
             c.phase == 0 ? "clockwise" : "anticlockwise");
    ktcUiInstr(p);
}

static KtcVerdict stepEncoder(KtcCtx& c) {
    const EncCfg* e = (const EncCfg*)c.arg;
    if (c.entry) {
        ktcShow(c, nullptr, KTC_BTN_NEXT | KTC_BTN_FAIL);
        ktcSend(c, KMC_CMD_ENABLE);
        delay(KTC_CMD_SETTLE_MS);
        ktcDrain(c);
        ktcReadNow(c);            // clear deltas left from earlier steps
        c.fresh = false;
        c.v[0] = 0;               // this encoder's total
        c.v[1] = 0;               // the other encoder's total
        c.v[2] = 0;               // last poll
        ktcPhase(c, 0);
        encPrompt(c, e);
    }
    if (c.btn == KTC_BTN_FAIL) return ktcFail(c, "%s: operator FAIL", e->name);
    if (!c.fresh && c.now - (uint32_t)c.v[2] >= 100) { c.v[2] = (int32_t)c.now; ktcReadNow(c); }
    if (c.fresh) {
        c.v[0] += (int8_t)ktcByte(c, 2 + e->enc);
        c.v[1] += (int8_t)ktcByte(c, 3 - e->enc);
        char t[16];
        snprintf(t, sizeof(t), "%+ld", (long)c.v[0]);
        ktcUiFill(0, KTC_LIVE_Y + 20, KTC_W, 32, KTC_C_BG);
        ktcUiTextC(KTC_W / 2, KTC_LIVE_Y + 36, t, KTC_C_TEXT, 3);
        ktcUiRowf(6, KTC_C_DIM, "%s count, other encoder %+ld", e->name, (long)c.v[1]);
    }
    if (c.btn == KTC_BTN_NEXT) {
        int32_t want = c.phase == 0 ? 10 : -10;
        if (c.v[1] != 0) return ktcFail(c, "turning %s moved the other encoder (%+ld)", e->name, (long)c.v[1]);
        if (c.v[0] == -want) return ktcFail(c, "%s counts backwards (%s gives %+ld)", e->name,
                                            c.phase == 0 ? "clockwise" : "anticlockwise", (long)c.v[0]);
        if (c.v[0] != want) return ktcFail(c, "%s counted %+ld, expected %+ld", e->name, (long)c.v[0], (long)want);
        if (c.phase == 1) return ktcPass(c, "%s +10 / -10 exact", e->name);
        c.v[0] = 0;
        c.v[1] = 0;
        ktcPhase(c, 1);
        encPrompt(c, e);
        ktcUiFill(0, KTC_LIVE_Y + 20, KTC_W, 32, KTC_C_BG);
    }
    return KTC_RUNNING;
}

static void curIdle(KtcCtx& c) { ktcSend(c, KMC_CMD_DISABLE); }
static const KtcCurrentCfg CURRENT = { 40, 0, nullptr, curIdle, nullptr, nullptr };
static const KtcBootCfg BOOT = { 300, true, nullptr };

static void live(KtcCtx& c) {
    ktcUiRowf(4, KTC_C_TEXT, "Events %02X  change %02X", ktcByte(c, 0), ktcByte(c, 1));
    ktcUiRowf(5, KTC_C_TEXT, "ENC1 %+d  ENC2 %+d", (int8_t)ktcByte(c, 2), (int8_t)ktcByte(c, 3));
}

static const KtcStep STEPS[] = {
    { "Identity",        ktcStepIdentity,  nullptr },
    { "Boot handshake",  ktcStepBoot,      &BOOT },
    { "Enable and INT",  ktcStepEnableInt, &INT_CFG },
    { "Push switches",   ktcStepWalk,      &WALK },
    { "ENC1 count",      stepEncoder,      &ENC1 },
    { "ENC2 count",      stepEncoder,      &ENC2 },
    { "Lifecycle",       ktcStepLifecycle, &LIFE },
    { "Supply current",  ktcStepCurrent,   &CURRENT },
};

static const KtcModuleDef MODULE = {
    "Dual Encoder", "KC-01-1861/1862 + 2 x KC-01-1851/1852",
    KMC_TYPE_DUAL_ENCODER, 0x2D, KMC_BUTTON_PACKET_SIZE,
    2, 0, KMC_CAP_ENCODERS,
    STEPS, sizeof(STEPS) / sizeof(STEPS[0]),
    nullptr, live,
};

void setup() { ktcBegin(MODULE); }
void loop()  { ktcLoop(); }

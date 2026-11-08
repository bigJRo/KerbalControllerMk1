/**
 * @file        KTC_Kjc.cpp
 * @version     1.0.0
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Joystick-module checks (see KTC_Kjc.h).
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include "KTC_Kjc.h"
#include "KTC_Config.h"

const KtcBootCfg ktcKjcBoot = { 600, true, "Hands off the stick. " };

static int16_t axisOf(const KtcCtx& c, uint8_t a) { return ktcS16(c, 3 + 2 * a); }

// ------------------------------------------------------------
//  LED states and brightness
// ------------------------------------------------------------
static void stBefore(KtcCtx& c) { ktcSend(c, KMC_CMD_ENABLE); ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 32); }
static void stOff(KtcCtx& c)    { ktcLedsAll(c, KMC_LED_OFF); }
static void stEna(KtcCtx& c)    { ktcLedsAll(c, KMC_LED_ENABLED); }
static void stAct(KtcCtx& c)    { ktcLedsAll(c, KMC_LED_ACTIVE); }
static void stWarn(KtcCtx& c)   { ktcLedsAll(c, KMC_LED_WARNING); }
static void stAlert(KtcCtx& c)  { ktcLedsAll(c, KMC_LED_ALERT); }
static void stArmed(KtcCtx& c)  { ktcLedsAll(c, KMC_LED_ARMED); }
static void br0(KtcCtx& c)      { ktcLedsAll(c, KMC_LED_ENABLED); ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 0); }
static void br255(KtcCtx& c)    { ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 255); }
static void stAfter(KtcCtx& c)  { ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 32); ktcLedsAll(c, KMC_LED_ENABLED); }

static void bulbPattern(KtcCtx& c) {
    uint8_t st[16] = {0};
    st[0] = KMC_LED_ACTIVE;   // BTN01 lit, BTN02 off
    ktcLeds(c, st);
}
static const KtcVisualItem STATES[] = {
    { "OFF",            "Both button LEDs off?", stOff, 0 },
    { "ENABLED",        "Both dim white (backlight)?", stEna, 0x8410 },
    { "ACTIVE",         "Both in their ACTIVE colours, as in the LED walk?", stAct, 0 },
    { "WARNING",        "Both steady amber?", stWarn, 0xFA80 },
    { "ALERT",          "Both steady red?", stAlert, 0xF800 },
    { "ARMED",          "Both steady cyan?", stArmed, 0x05BA },
    { "brightness 0",   "Brightness 0: backlight off (LEDs dark)?", br0, 0 },
    { "brightness 255", "Brightness 255: full white?", br255, 0xFFFF },
    { "pattern",        "First button lit in its ACTIVE colour, second off?", bulbPattern, 0 },
    { "bulb on",        "Bulb test: both full white?", ktcDoBulbOn, 0xFFFF },
    { "bulb off",       "Bulb test stopped: the same pattern as before?", ktcDoBulbOff, 0 },
};
const KtcVisualSeq ktcKjcStates = { 11, STATES, stBefore, stAfter };

static void curIdle(KtcCtx& c) { ktcSend(c, KMC_CMD_DISABLE); }
const KtcCurrentCfg ktcKjcCurrent = { 60, 40, "bulb test", curIdle, ktcDoBulbOn, ktcDoBulbOff };

void ktcKjcCleanup(KtcCtx& c) {
    ktcSend1(c, KMC_CMD_BULB_TEST, 0x00);
    ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 32);
    ktcLedsAll(c, KMC_LED_ENABLED);
}

void ktcKjcLive(KtcCtx& c) {
    ktcUiRowf(4, KTC_C_TEXT, "Buttons state %02X  change %02X", ktcByte(c, 2), ktcByte(c, 1));
    ktcUiRowf(5, KTC_C_TEXT, "AXIS1 %6d", axisOf(c, 0));
    ktcUiRowf(6, KTC_C_TEXT, "AXIS2 %6d", axisOf(c, 1));
    ktcUiRowf(7, KTC_C_TEXT, "AXIS3 %6d", axisOf(c, 2));
}

// ------------------------------------------------------------
//  Centre / deadzone
// ------------------------------------------------------------
KtcVerdict ktcStepKjcCentre(KtcCtx& c) {
    if (c.entry) {
        c.autoRead = false;
        ktcShow(c, "Hands off the stick for 2 seconds: all axes must read 0 with no INT.", 0);
        ktcSend(c, KMC_CMD_ENABLE);
        delay(KTC_CMD_SETTLE_MS);
        ktcDrain(c);
        c.v[0] = 0;     // INT count
        c.v[1] = 0;     // last poll
        ktcPhase(c, 0);
        return KTC_RUNNING;
    }
    if (ktcIntAsserted()) {
        c.v[0]++;
        ktcReadNow(c);
        return ktcFail(c, "INT at rest: axes %d %d %d", axisOf(c, 0), axisOf(c, 1), axisOf(c, 2));
    }
    if (c.now - (uint32_t)c.v[1] >= 100) {
        c.v[1] = (int32_t)c.now;
        if (ktcReadNow(c)) {
            for (uint8_t a = 0; a < 3; a++) ktcUiBar(KTC_LIVE_Y + 10 + a * 16, a == 0 ? "AXIS1" : (a == 1 ? "AXIS2" : "AXIS3"),
                                                     axisOf(c, a) / 32767.0f, true, "0 expected");
            for (uint8_t a = 0; a < 3; a++)
                if (axisOf(c, a) != 0) return ktcFail(c, "AXIS%u reads %d at rest", a + 1, axisOf(c, a));
        }
    }
    if (ktcPhaseMs(c) >= 2000) return ktcPass(c, "all axes 0 at rest, no INT");
    return KTC_RUNNING;
}

// ------------------------------------------------------------
//  Axis travel
// ------------------------------------------------------------
enum { AX_POS, AX_BACK1, AX_NEG, AX_BACK2 };

static void axisPrompt(KtcCtx& c, const KtcAxisCfg* a) {
    char p[80];
    switch (c.phase) {
        case AX_POS: snprintf(p, sizeof(p), "%s and hold it there.", a->posMove); break;
        case AX_NEG: snprintf(p, sizeof(p), "%s and hold it there.", a->negMove); break;
        default:     snprintf(p, sizeof(p), "Let go of the stick."); break;
    }
    ktcUiInstr(p);
}

KtcVerdict ktcStepKjcAxis(KtcCtx& c) {
    const KtcAxisCfg* a = (const KtcAxisCfg*)c.arg;
    if (c.entry) {
        ktcShow(c, nullptr, KTC_BTN_FAIL);
        ktcSend(c, KMC_CMD_ENABLE);
        c.autoRead = true;
        c.v[0] = 0;          // sign seen for posMove
        c.v[1] = 0;          // last poll
        c.v[2] = 0;          // min seen through INT reads
        c.v[3] = 0;          // min seen through polled reads
        c.v[4] = 0;          // max (any read)
        c.v[5] = 0;          // centre-hold timer start
        ktcPhase(c, AX_POS);
        axisPrompt(c, a);
    }
    if (c.btn == KTC_BTN_FAIL) return ktcFail(c, "axis %u: operator FAIL", a->axis + 1);

    bool polled = false;
    if (!c.fresh && c.now - (uint32_t)c.v[1] >= 50) {
        c.v[1] = (int32_t)c.now;
        polled = ktcReadNow(c);
    }
    if (!c.fresh) return KTC_RUNNING;

    int16_t v = axisOf(c, a->axis);
    if (polled) { if (v < c.v[3]) c.v[3] = v; }
    else        { if (v < c.v[2]) c.v[2] = v; }
    if (v > c.v[4]) c.v[4] = v;
    char vt[12];
    snprintf(vt, sizeof(vt), "%d", v);
    ktcUiBar(KTC_LIVE_Y + 20, a->axis == 0 ? "AXIS1" : (a->axis == 1 ? "AXIS2" : "AXIS3"), v / 32767.0f, true, vt);
    ktcUiRowf(4, KTC_C_DIM, "min %ld  max %ld", (long)(c.v[2] < c.v[3] ? c.v[2] : c.v[3]), (long)c.v[4]);

    switch (c.phase) {
    case AX_POS:
        if (v >= 30000 || v <= -30000) {
            c.v[0] = v > 0 ? 1 : -1;
            ktcPhase(c, AX_BACK1);
            axisPrompt(c, a);
        }
        break;
    case AX_NEG:
        if ((c.v[0] > 0 && v <= -30000) || (c.v[0] < 0 && v >= 30000)) {
            ktcPhase(c, AX_BACK2);
            axisPrompt(c, a);
        } else if ((c.v[0] > 0 && v >= 30000) || (c.v[0] < 0 && v <= -30000)) {
            ktcUiToast("Same direction as before: move the other way", KTC_C_WARN);
        }
        break;
    case AX_BACK1:
    case AX_BACK2:
        if (v == 0) {
            if (!c.v[5]) c.v[5] = (int32_t)c.now;
        } else c.v[5] = 0;
        if (c.v[5] && c.now - (uint32_t)c.v[5] >= 300) {
            c.v[5] = 0;
            if (c.phase == AX_BACK1) { ktcPhase(c, AX_NEG); axisPrompt(c, a); }
            else {
                if (a->expect && c.v[0] != a->expect)
                    return ktcFail(c, "%s gives %s values", a->posName, c.v[0] > 0 ? "positive" : "negative");
                long mn = c.v[2] < c.v[3] ? c.v[2] : c.v[3];
                if (c.v[3] <= -32767 && c.v[2] > -32767)
                    return ktcPass(c, "%s=%c, %ld..%ld, -32768 sent without INT", a->posName,
                                   c.v[0] > 0 ? '+' : '-', mn, (long)c.v[4]);
                return ktcPass(c, "%s=%c, %ld..%ld", a->posName, c.v[0] > 0 ? '+' : '-', mn, (long)c.v[4]);
            }
        }
        if (ktcPhaseMs(c) > 15000) return ktcFail(c, "axis %u does not return to 0", a->axis + 1);
        break;
    }
    return KTC_RUNNING;
}

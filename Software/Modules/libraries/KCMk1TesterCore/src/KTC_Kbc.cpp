/**
 * @file        KTC_Kbc.cpp
 * @version     1.0.0
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Button-module checks (see KTC_Kbc.h).
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include "KTC_Kbc.h"

const KtcBootCfg ktcKbcBoot = { 300, true, nullptr };

// ------------------------------------------------------------
//  LED states
// ------------------------------------------------------------
static void stOff(KtcCtx& c)     { ktcLedsAll(c, KMC_LED_OFF); }
static void stEna(KtcCtx& c)     { ktcLedsAll(c, KMC_LED_ENABLED); }
static void stAct(KtcCtx& c)     { ktcLedsAll(c, KMC_LED_ACTIVE); }
static void stAlt(KtcCtx& c)     { ktcLedsAll(c, KMC_LED_ACTIVE_ALT); }
static void stWarn(KtcCtx& c)    { ktcLedsAll(c, KMC_LED_WARNING); }
static void stAlert(KtcCtx& c)   { ktcLedsAll(c, KMC_LED_ALERT); }
static void stArmed(KtcCtx& c)   { ktcLedsAll(c, KMC_LED_ARMED); }
static void stPartial(KtcCtx& c) { ktcLedsAll(c, KMC_LED_PARTIAL_DEPLOY); }
static void stCut(KtcCtx& c)     { ktcLedsAll(c, KMC_LED_CUT); }
static void stBefore(KtcCtx& c)  { ktcSend(c, KMC_CMD_ENABLE); ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, KTC_KBC_BRIGHTNESS_DEFAULT); }
static void stAfter(KtcCtx& c)   { ktcLedsAll(c, KMC_LED_ENABLED); }

static const KtcVisualItem STATES_EXT[] = {
    { "OFF",        "All 12 button LEDs off?", stOff, 0 },
    { "ENABLED",    "All 12 dim white (backlight)?", stEna, 0x8410 },
    { "ACTIVE",     "All 12 in their ACTIVE colours, as in the LED walk?", stAct, 0 },
    { "ACTIVE_ALT", "All 12 the same as ACTIVE? (no alternate colours on this module)", stAlt, 0 },
    { "WARNING",    "All 12 flashing amber, about once a second?", stWarn, 0xFA80 },
    { "ALERT",      "All 12 flashing red, fast (about 3 per second)?", stAlert, 0xF800 },
    { "ARMED",      "All 12 steady cyan?", stArmed, 0x05BA },
    { "PARTIAL",    "All 12 steady amber?", stPartial, 0xFA80 },
    { "CUT",        "All 12 steady red?", stCut, 0xF800 },
};
const KtcVisualSeq ktcKbcStatesCore     = { 4, STATES_EXT, stBefore, stAfter };
const KtcVisualSeq ktcKbcStatesExtended = { 9, STATES_EXT, stBefore, stAfter };

// ------------------------------------------------------------
//  Brightness (ENABLED only)
// ------------------------------------------------------------
static void br0(KtcCtx& c)   { ktcLedsAll(c, KMC_LED_ENABLED); ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 0); }
static void br32(KtcCtx& c)  { ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 32); }
static void br128(KtcCtx& c) { ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 128); }
static void br255(KtcCtx& c) { ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 255); }
static void brAct(KtcCtx& c) { ktcLedsAll(c, KMC_LED_ACTIVE); }
static void brAfter(KtcCtx& c) { ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, KTC_KBC_BRIGHTNESS_DEFAULT); ktcLedsAll(c, KMC_LED_ENABLED); }

static const KtcVisualItem BRIGHT[] = {
    { "brightness 0",   "Brightness 0: backlight off (LEDs look dark)?", br0, 0 },
    { "brightness 32",  "Brightness 32 (default): dim white?", br32, 0x2104 },
    { "brightness 128", "Brightness 128: clearly brighter white?", br128, 0x8410 },
    { "brightness 255", "Brightness 255: full white?", br255, 0xFFFF },
    { "ACTIVE at 255",  "ACTIVE colours unchanged by brightness (full colour)?", brAct, 0 },
};
const KtcVisualSeq ktcKbcBrightness = { 5, BRIGHT, stBefore, brAfter };

// ------------------------------------------------------------
//  Bulb test
// ------------------------------------------------------------
static void bulbPattern(KtcCtx& c) {
    uint8_t st[16];
    for (uint8_t i = 0; i < 16; i++) st[i] = (i % 2) ? KMC_LED_OFF : KMC_LED_ACTIVE;
    ktcLeds(c, st);
}
static void bulbBefore(KtcCtx& c) { stBefore(c); }
static const KtcVisualItem BULB[] = {
    { "pattern",  "Every other button lit in its ACTIVE colour, the rest off?", bulbPattern, 0 },
    { "bulb on",  "Bulb test: all 12 full white?", ktcDoBulbOn, 0xFFFF },
    { "bulb off", "Bulb test stopped: the same every-other pattern as before?", ktcDoBulbOff, 0 },
};
const KtcVisualSeq ktcKbcBulb = { 3, BULB, bulbBefore, stAfter };

// ------------------------------------------------------------
//  Supply current: dark and DISABLED vs bulb test (12 x full white)
// ------------------------------------------------------------
static void curIdle(KtcCtx& c) { ktcSend(c, KMC_CMD_DISABLE); }
const KtcCurrentCfg ktcKbcCurrent = { 80, 150, "bulb test", curIdle, ktcDoBulbOn, ktcDoBulbOff };

void ktcKbcCleanup(KtcCtx& c) {
    ktcSend1(c, KMC_CMD_BULB_TEST, 0x00);
    ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, KTC_KBC_BRIGHTNESS_DEFAULT);
    ktcLedsAll(c, KMC_LED_ENABLED);
}

static void liveBits(KtcCtx& c, uint8_t bytes) {
    uint32_t lv = ktcBits(c, 0, bytes), ch = ktcBits(c, bytes, bytes);
    ktcUiRowf(4, KTC_C_TEXT, "Level  %0*lX", bytes * 2, (unsigned long)lv);
    ktcUiRowf(5, KTC_C_TEXT, "Change %0*lX", bytes * 2, (unsigned long)ch);
    char on[48];
    int o = 0;
    for (uint8_t b = 0; b < bytes * 8 && o < 40; b++)
        if (lv & (1UL << b)) o += snprintf(on + o, sizeof(on) - o, "%u ", b);
    on[o] = '\0';
    ktcUiRowf(6, KTC_C_ACCENT, "On: %s", o ? on : "-");
}
void ktcKbcLive16(KtcCtx& c) { liveBits(c, 2); }
void ktcKbcLive24(KtcCtx& c) { liveBits(c, 3); }

/**
 * @file        KCMk1_Test_PreWarp_Time.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Bench test for the Pre-Warp Time module (I2C 0x2B, type 0x0C),
 *              run on the KC-01-9001 v2.0 Module Tester (XIAO RA4M1).
 *              Replaces the serial-menu KCMk1_PreWarp_Tester.
 *
 *              Module (KC-01-1841/1842 7-segment board, Kerbal7SegmentCore):
 *              4-digit display showing the pre-warp time in minutes (0-9999),
 *              encoder with push (sets 0), and three preset keycaps: 5 MIN,
 *              1 HR (60) and 1 DAY (360, one Kerbin day). A preset flashes its
 *              keycap GOLD for 150 ms. The module drives its own LEDs.
 *              Identity fw 1.0, caps 0x10.
 *
 *              Packet (8 bytes): header, events (rising edges: bit 0 5 MIN,
 *              1 1 HR, 2 1 DAY, 3 knob push), change (= events), state
 *              (always 0), value int16 big-endian, 0-9999. A read with
 *              nothing pending returns an empty header, so the tester reads
 *              only on INT.
 *
 *              Steps: identity, boot handshake, enable + INT, presets and
 *              knob push, encoder, encoder speed and limits, display digits
 *              and brightness (incl. the 9999 clamp), bulb test, lifecycle,
 *              supply current.
 *
 * @note        Board: Seeed XIAO RA4M1. Libraries: KCMk1TesterCore,
 *              KerbalModuleCommon, Adafruit GFX, Adafruit ST7735 and ST7789,
 *              Adafruit FT6206.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include <KCMk1TesterCore.h>

#define P_EVENTS  0
#define P_STATE   2
#define P_VALUE   3

static void setValue(KtcCtx& c, int16_t v) {
    uint8_t p[2] = { (uint8_t)((uint16_t)v >> 8), (uint8_t)(v & 0xFF) };
    ktcSend(c, KMC_CMD_SET_VALUE, p, 2);
}

// DISABLE clears the value; ENABLE reports ACTIVE, value 0.
static void resetModule(KtcCtx& c)   { ktcSend(c, KMC_CMD_DISABLE); delay(KTC_CMD_SETTLE_MS); ktcSend(c, KMC_CMD_ENABLE); }
static void v9990(KtcCtx& c)   { setValue(c, 9990); }
static void v5(KtcCtx& c)      { setValue(c, 5); }
static void v5000(KtcCtx& c)   { setValue(c, 5000); }
static void v8888(KtcCtx& c)   { setValue(c, 8888); }
static void v1234(KtcCtx& c)   { setValue(c, 1234); }
static void v7(KtcCtx& c)      { setValue(c, 7); }
static void v12000(KtcCtx& c)  { setValue(c, 12000); }
static void v360(KtcCtx& c)    { setValue(c, 360); }
static void brMin(KtcCtx& c)   { ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 0x00); }
static void brMax(KtcCtx& c)   { ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 0xF0); }
static void brNorm(KtcCtx& c)  { ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 0x80); }
static void cmdDisable(KtcCtx& c) { ktcSend(c, KMC_CMD_DISABLE); }
static void cmdEnable(KtcCtx& c)  { ktcSend(c, KMC_CMD_ENABLE); }
static void cmdSleep(KtcCtx& c)  { ktcSend(c, KMC_CMD_SLEEP); }
static void cmdWake(KtcCtx& c)    { ktcSend(c, KMC_CMD_WAKE); }

#define ACTIVE  KMC_STATUS_ACTIVE

static const KtcScriptItem PRESETS[] = {
    KS_DO_EXPECT(resetModule, 0x00, 0, ACTIVE),
    KS_ASK_("Display shows 0 and all three keycaps dim (backlight)?"),
    KS_PRESS_("Press 5 MIN.", 0, 0x00, 5),
    KS_ASK_("Display shows 5, and 5 MIN flashed GOLD?"),
    KS_PRESS_("Press 1 HR.", 1, 0x00, 60),
    KS_PRESS_("Press 1 DAY.", 2, 0x00, 360),
    KS_ASK_("Display shows 360?"),
    KS_PRESS_("Push the knob.", 3, 0x00, 0),
    KS_ASK_("Display shows 0?"),
};
static const KtcScript PRESETS_S = { sizeof(PRESETS) / sizeof(PRESETS[0]), PRESETS, P_EVENTS, P_STATE, P_VALUE };

static const KtcScriptItem ENC[] = {
    KS_DO_EXPECT(resetModule, 0x00, 0, ACTIVE),
    KS_TURN_TO("Turn the knob clockwise 5 clicks, slowly, then tap NEXT.", 5),
    KS_TURN_TO("Turn the knob anticlockwise 3 clicks, slowly, then tap NEXT.", 2),
    KS_ASK_("Display shows 2?"),
};
static const KtcScript ENC_S = { sizeof(ENC) / sizeof(ENC[0]), ENC, P_EVENTS, P_STATE, P_VALUE };

static const KtcScriptItem SPEED[] = {
    KS_DO_EXPECT(resetModule, 0x00, 0, ACTIVE),
    KS_DO_EXPECT(v9990, 0x00, 9990, ACTIVE),
    KS_TURN_TO("Spin the knob clockwise, 20 clicks or more, then tap NEXT.", 9999),
    KS_DO_EXPECT(v5, 0x00, 5, ACTIVE),
    KS_TURN_TO("Turn the knob anticlockwise 10 clicks or more, then tap NEXT.", 0),
    KS_DO_EXPECT(v5000, 0x00, 5000, ACTIVE),
    KS_TURN_BY("Spin the knob clockwise quickly, 20 clicks in one go, then tap NEXT.", 50, 9999),
};
static const KtcScript SPEED_S = { sizeof(SPEED) / sizeof(SPEED[0]), SPEED, P_EVENTS, P_STATE, P_VALUE };

static const KtcScriptItem DISP[] = {
    KS_DO_EXPECT(resetModule, 0x00, 0, ACTIVE),
    KS_DO_EXPECT(v8888, 0x00, 8888, ACTIVE),
    KS_ASK_("Display shows 8888 (every segment, no decimal points)?"),
    KS_DO_EXPECT(v1234, 0x00, 1234, ACTIVE),
    KS_ASK_("Display shows 1234?"),
    KS_DO_EXPECT(v7, 0x00, 7, ACTIVE),
    KS_ASK_("Display shows 7 in the right digit only (no leading zeros)?"),
    KS_DO_EXPECT(v12000, 0x00, 9999, ACTIVE),
    KS_ASK_("SET_VALUE 12000 is clamped: display shows 9999?"),
    KS_DO(brMin),
    KS_ASK_("Brightness minimum: display dim but still lit?"),
    KS_DO(brMax),
    KS_ASK_("Brightness maximum: display bright?"),
    KS_DO(brNorm),
};
static const KtcScript DISP_S = { sizeof(DISP) / sizeof(DISP[0]), DISP, P_EVENTS, P_STATE, P_VALUE };

static const KtcScriptItem BULB[] = {
    KS_DO_EXPECT(resetModule, 0x00, 0, ACTIVE),
    KS_DO(ktcDoBulbOn),
    KS_ASK_("Bulb test: 8.8.8.8. (all segments and points) and all three keycaps white?"),
    KS_DO(ktcDoBulbOff),
    KS_ASK_("Bulb test stopped: display 0 and the keycaps dim again?"),
};
static const KtcScript BULB_S = { sizeof(BULB) / sizeof(BULB[0]), BULB, P_EVENTS, P_STATE, P_VALUE };

static const KtcScriptItem LIFE[] = {
    KS_DO_EXPECT(resetModule, 0x00, 0, ACTIVE),
    KS_DO_EXPECT(v360, 0x00, 360, ACTIVE),
    KS_DO(cmdDisable),
    KS_ASK_("DISABLED: display and keycaps dark?"),
    KS_QUIET_("Press 5 MIN and turn the knob: nothing may happen. Then tap NEXT."),
    KS_DO_EXPECT(cmdEnable, 0x00, 0, ACTIVE),
    KS_ASK_("ENABLE: display shows 0 (cleared by DISABLE), keycaps dim?"),
    KS_DO(cmdSleep),
    KS_QUIET_("Asleep: press 5 MIN. Nothing may happen. Then tap NEXT."),
    KS_DO_EXPECT(cmdWake, 0x00, 0, ACTIVE),
};
static const KtcScript LIFE_S = { sizeof(LIFE) / sizeof(LIFE[0]), LIFE, P_EVENTS, P_STATE, P_VALUE };

static const KtcBootCfg BOOT    = { 300, false, nullptr };
static const KtcIntCfg  INT_CFG = { "Press and release 5 MIN.", 0, 1, false, ktcDecodeRiseB0 };

static void curLoad(KtcCtx& c) { ktcSend1(c, KMC_CMD_BULB_TEST, 0x01); }
static const KtcCurrentCfg CURRENT = { 60, 20, "bulb test", cmdDisable, curLoad, ktcDoBulbOff };

static void cleanup(KtcCtx& c) {
    ktcSend1(c, KMC_CMD_BULB_TEST, 0x00);
    ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 0x80);
}

static void live(KtcCtx& c) {
    ktcUiRowf(4, KTC_C_TEXT, "Events %02X  change %02X  state %02X", ktcByte(c, 0), ktcByte(c, 1), ktcByte(c, 2));
    ktcUiRowf(5, KTC_C_TEXT, "Pre-warp %d min", ktcS16(c, P_VALUE));
}

static const KtcStep STEPS[] = {
    { "Identity",         ktcStepIdentity,  nullptr },
    { "Boot handshake",   ktcStepBoot,      &BOOT },
    { "Enable and INT",   ktcStepEnableInt, &INT_CFG },
    { "Presets",          ktcStepScript,    &PRESETS_S },
    { "Encoder",          ktcStepScript,    &ENC_S },
    { "Encoder speed",    ktcStepScript,    &SPEED_S },
    { "Display",          ktcStepScript,    &DISP_S },
    { "Bulb test",        ktcStepScript,    &BULB_S },
    { "Lifecycle",        ktcStepScript,    &LIFE_S },
    { "Supply current",   ktcStepCurrent,   &CURRENT },
};

static const KtcModuleDef MODULE = {
    "Pre-Warp Time", "KC-01-1841/1842 7-segment board",
    KMC_TYPE_PRE_WARP_TIME, 0x2B, KMC_DISPLAY_PACKET_SIZE,
    1, 0, KMC_CAP_DISPLAY,
    STEPS, sizeof(STEPS) / sizeof(STEPS[0]),
    cleanup, live,
};

void setup() { ktcBegin(MODULE); }
void loop()  { ktcLoop(); }

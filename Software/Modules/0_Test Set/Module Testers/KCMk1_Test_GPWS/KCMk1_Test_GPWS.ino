/**
 * @file        KCMk1_Test_GPWS.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Bench test for the GPWS input panel (I2C 0x2A, type 0x0B), run
 *              on the KC-01-9001 v2.0 Module Tester (XIAO RA4M1). Replaces the
 *              serial-menu KCMk1_GPWS_Tester.
 *
 *              Module (KC-01-1841/1842 7-segment board, Kerbal7SegmentCore):
 *              4-digit display showing the threshold in metres (0-9999,
 *              default 200), encoder with push (resets to 200), and three
 *              keycaps: ENA (GPWS mode OFF -> ACTIVE green -> PROX amber ->
 *              OFF), PROX and RNDZ (mutually exclusive, only when GPWS is on).
 *              The module drives its own LEDs. Identity fw 1.0, caps 0x10.
 *
 *              Packet (8 bytes): header, events (rising edges: bit 0 ENA,
 *              1 PROX, 2 RNDZ, 3 knob push), change, state (bits 1:0 mode,
 *              bit 2 PROX, bit 3 RNDZ), threshold int16 big-endian. A read
 *              with nothing pending returns an empty header, so the tester
 *              reads only on INT.
 *
 *              Steps: identity, boot handshake, enable + INT, ENA mode cycle,
 *              PROX and RNDZ, encoder and knob push, encoder speed and limits,
 *              display digits and brightness, bulb test, lifecycle, supply
 *              current.
 *
 * @note        Board: Seeed XIAO RA4M1. Libraries: KCMk1TesterCore,
 *              KerbalModuleCommon, Adafruit GFX, Adafruit ST7735 and ST7789,
 *              Adafruit FT6206.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include <KCMk1TesterCore.h>

// Payload offsets
#define P_EVENTS  0
#define P_STATE   2
#define P_VALUE   3

static void setValue(KtcCtx& c, int16_t v) {
    uint8_t p[2] = { (uint8_t)((uint16_t)v >> 8), (uint8_t)(v & 0xFF) };
    ktcSend(c, KMC_CMD_SET_VALUE, p, 2);
}

// Back to a known state: DISABLE clears mode, flags and threshold; ENABLE
// reports ACTIVE, state 0, threshold 200.
static void resetModule(KtcCtx& c)   { ktcSend(c, KMC_CMD_DISABLE); delay(KTC_CMD_SETTLE_MS); ktcSend(c, KMC_CMD_ENABLE); }
static void v9990(KtcCtx& c)   { setValue(c, 9990); }
static void v5(KtcCtx& c)      { setValue(c, 5); }
static void v5000(KtcCtx& c)   { setValue(c, 5000); }
static void v8888(KtcCtx& c)   { setValue(c, 8888); }
static void v1234(KtcCtx& c)   { setValue(c, 1234); }
static void v7(KtcCtx& c)      { setValue(c, 7); }
static void v200(KtcCtx& c)    { setValue(c, 200); }
static void brMin(KtcCtx& c)   { ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 0x00); }
static void brMax(KtcCtx& c)   { ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 0xF0); }
static void brNorm(KtcCtx& c)  { ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 0x80); }
static void cmdDisable(KtcCtx& c) { ktcSend(c, KMC_CMD_DISABLE); }
static void cmdEnable(KtcCtx& c)  { ktcSend(c, KMC_CMD_ENABLE); }
static void cmdSleep(KtcCtx& c)  { ktcSend(c, KMC_CMD_SLEEP); }
static void cmdWake(KtcCtx& c)    { ktcSend(c, KMC_CMD_WAKE); }

#define ACTIVE  KMC_STATUS_ACTIVE

// ------------------------------------------------------------
//  ENA: OFF -> ACTIVE (green) -> PROX (amber) -> OFF
// ------------------------------------------------------------
static const KtcScriptItem MODE[] = {
    KS_DO_EXPECT(resetModule, 0x00, 200, ACTIVE),
    KS_ASK_("All three keycaps dim (backlight) and the display blank?"),
    KS_PRESS_("Press ENA.", 0, 0x01, 200),
    KS_ASK_("ENA lit GREEN and the display shows 200?"),
    KS_PRESS_("Press ENA again.", 0, 0x02, 200),
    KS_ASK_("ENA lit AMBER and the display still 200?"),
    KS_PRESS_("Press ENA again.", 0, 0x00, 200),
    KS_ASK_("ENA dim again and the display blank?"),
};
static const KtcScript MODE_S = { sizeof(MODE) / sizeof(MODE[0]), MODE, P_EVENTS, P_STATE, P_VALUE };

// ------------------------------------------------------------
//  PROX / RNDZ: ignored while GPWS is off, mutually exclusive when on
// ------------------------------------------------------------
static const KtcScriptItem PROX[] = {
    KS_DO_EXPECT(resetModule, 0x00, 200, ACTIVE),
    KS_QUIET_("GPWS is off: press PROX and RNDZ. Nothing may happen. Then tap NEXT."),
    KS_PRESS_("Press ENA.", 0, 0x01, 200),
    KS_PRESS_("Press PROX.", 1, 0x06, 200),
    KS_ASK_("ENA AMBER and PROX GREEN, RNDZ dim?"),
    KS_PRESS_("Press RNDZ.", 2, 0x0A, 200),
    KS_ASK_("RNDZ GREEN, PROX dim, ENA still AMBER?"),
    KS_PRESS_("Press RNDZ again.", 2, 0x02, 200),
    KS_ASK_("RNDZ and PROX dim, ENA still AMBER?"),
};
static const KtcScript PROX_S = { sizeof(PROX) / sizeof(PROX[0]), PROX, P_EVENTS, P_STATE, P_VALUE };

// ------------------------------------------------------------
//  Encoder: one count per click when turned slowly; knob push -> 200
// ------------------------------------------------------------
static const KtcScriptItem ENC[] = {
    KS_DO_EXPECT(resetModule, 0x00, 200, ACTIVE),
    KS_PRESS_("Press ENA (the display shows 200).", 0, 0x01, 200),
    KS_TURN_TO("Turn the knob clockwise 5 clicks, slowly, then tap NEXT.", 205),
    KS_ASK_("Display shows 205?"),
    KS_PRESS_("Push the knob.", 3, 0x01, 200),
    KS_TURN_TO("Turn the knob anticlockwise 5 clicks, slowly, then tap NEXT.", 195),
    KS_ASK_("Display shows 195?"),
};
static const KtcScript ENC_S = { sizeof(ENC) / sizeof(ENC[0]), ENC, P_EVENTS, P_STATE, P_VALUE };

// ------------------------------------------------------------
//  Encoder acceleration and the 0 / 9999 limits
// ------------------------------------------------------------
static const KtcScriptItem SPEED[] = {
    KS_DO_EXPECT(resetModule, 0x00, 200, ACTIVE),
    KS_PRESS_("Press ENA.", 0, 0x01, 200),
    KS_DO_EXPECT(v9990, 0x01, 9990, ACTIVE),
    KS_TURN_TO("Spin the knob clockwise, 20 clicks or more, then tap NEXT.", 9999),
    KS_DO_EXPECT(v5, 0x01, 5, ACTIVE),
    KS_TURN_TO("Turn the knob anticlockwise 10 clicks or more, then tap NEXT.", 0),
    KS_DO_EXPECT(v5000, 0x01, 5000, ACTIVE),
    KS_TURN_BY("Spin the knob clockwise quickly, 20 clicks in one go, then tap NEXT.", 50, 9999),
};
static const KtcScript SPEED_S = { sizeof(SPEED) / sizeof(SPEED[0]), SPEED, P_EVENTS, P_STATE, P_VALUE };

// ------------------------------------------------------------
//  Display digits and brightness (GPWS on, so the display is lit)
// ------------------------------------------------------------
static const KtcScriptItem DISP[] = {
    KS_DO_EXPECT(resetModule, 0x00, 200, ACTIVE),
    KS_PRESS_("Press ENA.", 0, 0x01, 200),
    KS_DO_EXPECT(v8888, 0x01, 8888, ACTIVE),
    KS_ASK_("Display shows 8888 (every segment, no decimal points)?"),
    KS_DO_EXPECT(v1234, 0x01, 1234, ACTIVE),
    KS_ASK_("Display shows 1234?"),
    KS_DO_EXPECT(v7, 0x01, 7, ACTIVE),
    KS_ASK_("Display shows 7 in the right digit only (no leading zeros)?"),
    KS_DO(brMin),
    KS_ASK_("Brightness minimum: display dim but still lit?"),
    KS_DO(brMax),
    KS_ASK_("Brightness maximum: display bright?"),
    KS_DO(brNorm),
    KS_DO_EXPECT(v200, 0x01, 200, ACTIVE),
};
static const KtcScript DISP_S = { sizeof(DISP) / sizeof(DISP[0]), DISP, P_EVENTS, P_STATE, P_VALUE };

// ------------------------------------------------------------
//  Bulb test (no packet)
// ------------------------------------------------------------
static const KtcScriptItem BULB[] = {
    KS_DO_EXPECT(resetModule, 0x00, 200, ACTIVE),
    KS_PRESS_("Press ENA.", 0, 0x01, 200),
    KS_DO(ktcDoBulbOn),
    KS_ASK_("Bulb test: 8.8.8.8. (all segments and points) and all three keycaps white?"),
    KS_DO(ktcDoBulbOff),
    KS_ASK_("Bulb test stopped: display 200 and ENA GREEN again?"),
};
static const KtcScript BULB_S = { sizeof(BULB) / sizeof(BULB[0]), BULB, P_EVENTS, P_STATE, P_VALUE };

// ------------------------------------------------------------
//  Lifecycle: dark and silent while DISABLED / SLEEPING
// ------------------------------------------------------------
static const KtcScriptItem LIFE[] = {
    KS_DO_EXPECT(resetModule, 0x00, 200, ACTIVE),
    KS_PRESS_("Press ENA.", 0, 0x01, 200),
    KS_DO(cmdDisable),
    KS_ASK_("DISABLED: display and keycaps dark?"),
    KS_QUIET_("Press ENA and turn the knob: nothing may happen. Then tap NEXT."),
    KS_DO_EXPECT(cmdEnable, 0x00, 200, ACTIVE),
    KS_ASK_("ENABLE: keycaps dim, display blank (GPWS off)?"),
    KS_DO(cmdSleep),
    KS_QUIET_("Asleep: press ENA. Nothing may happen. Then tap NEXT."),
    KS_DO_EXPECT(cmdWake, 0x00, 200, ACTIVE),
};
static const KtcScript LIFE_S = { sizeof(LIFE) / sizeof(LIFE[0]), LIFE, P_EVENTS, P_STATE, P_VALUE };

static const KtcBootCfg BOOT    = { 300, false, nullptr };
static const KtcIntCfg  INT_CFG = { "Press and release ENA.", 0, 1, false, ktcDecodeRiseB0 };

static void curLoad(KtcCtx& c) { ktcSend1(c, KMC_CMD_BULB_TEST, 0x01); }
static const KtcCurrentCfg CURRENT = { 60, 20, "bulb test", cmdDisable, curLoad, ktcDoBulbOff };

static void cleanup(KtcCtx& c) {
    ktcSend1(c, KMC_CMD_BULB_TEST, 0x00);
    ktcSend1(c, KMC_CMD_SET_BRIGHTNESS, 0x80);
}

static void live(KtcCtx& c) {
    ktcUiRowf(4, KTC_C_TEXT, "Events %02X  change %02X  state %02X", ktcByte(c, 0), ktcByte(c, 1), ktcByte(c, 2));
    ktcUiRowf(5, KTC_C_TEXT, "Threshold %d m", ktcS16(c, P_VALUE));
}

static const KtcStep STEPS[] = {
    { "Identity",         ktcStepIdentity,  nullptr },
    { "Boot handshake",   ktcStepBoot,      &BOOT },
    { "Enable and INT",   ktcStepEnableInt, &INT_CFG },
    { "ENA mode cycle",   ktcStepScript,    &MODE_S },
    { "PROX and RNDZ",    ktcStepScript,    &PROX_S },
    { "Encoder",          ktcStepScript,    &ENC_S },
    { "Encoder speed",    ktcStepScript,    &SPEED_S },
    { "Display",          ktcStepScript,    &DISP_S },
    { "Bulb test",        ktcStepScript,    &BULB_S },
    { "Lifecycle",        ktcStepScript,    &LIFE_S },
    { "Supply current",   ktcStepCurrent,   &CURRENT },
};

static const KtcModuleDef MODULE = {
    "GPWS", "KC-01-1841/1842 7-segment board",
    KMC_TYPE_GPWS_INPUT, 0x2A, KMC_DISPLAY_PACKET_SIZE,
    1, 0, KMC_CAP_DISPLAY,
    STEPS, sizeof(STEPS) / sizeof(STEPS[0]),
    cleanup, live,
};

void setup() { ktcBegin(MODULE); }
void loop()  { ktcLoop(); }

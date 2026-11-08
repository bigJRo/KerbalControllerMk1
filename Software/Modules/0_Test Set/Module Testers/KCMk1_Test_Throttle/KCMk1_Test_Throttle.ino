/**
 * @file        KCMk1_Test_Throttle.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Bench test for the Throttle module (I2C 0x2C, type 0x0D), run on
 *              the KC-01-9001 v2.0 Module Tester (XIAO RA4M1).
 *
 *              Module (KC-01-1871/1872): motorised fader with a capacitive
 *              touch sensor (AT42QT1010), four throttle buttons (100%, UP,
 *              DOWN, 0%) and one shared backlight output for the five panel
 *              switches. Identity fw 2.0, caps 0x20 (MOTORIZED).
 *
 *              Packet (7 bytes): header, flags (bit 0 ENABLED, 1 PRECISION,
 *              2 TOUCH, 3 MOTOR_MOVING), button events (rising edges only:
 *              bit 0 100%, 1 UP, 2 DOWN, 3 0%), value uint16 big-endian
 *              0..32767.
 *
 *              Steps: identity, boot handshake, enable + INT on a button
 *              press, button walk, lever travel and touch sensor, motor seek
 *              (0 / 100 / 50 %, with peak supply current), touch blocks the
 *              motor, precision mode, lever hold, backlights and bulb test,
 *              lifecycle, supply current.
 *
 * @note        The motor needs the module's motor supply. Keep hands off the
 *              lever unless a step asks for it. A seek that does not arrive
 *              within 4 s is stopped by the tester.
 *
 * @note        Throttle firmware 2.2 (I2C.cpp i2cSyncINT) clears the button
 *              INT-pending flag when it collects the events, so a button press
 *              with the lever still raises no INT. "Enable and INT" fails on
 *              that firmware with "no INT after the input"; the button walk
 *              still checks the buttons by polling.
 *
 * @note        Board: Seeed XIAO RA4M1. Libraries: KCMk1TesterCore,
 *              KerbalModuleCommon, Adafruit GFX, Adafruit ST7735 and ST7789,
 *              Adafruit FT6206.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include <KCMk1TesterCore.h>

// ------------------------------------------------------------
//  Packet fields
// ------------------------------------------------------------
#define F_ENABLED    0x01
#define F_PRECISION  0x02
#define F_TOUCH      0x04
#define F_MOVING     0x08

static uint8_t  flagsOf(const KtcCtx& c) { return ktcByte(c, 0); }
static uint16_t valueOf(const KtcCtx& c) { return ktcU16(c, 2); }

static void setThrottle(KtcCtx& c, uint16_t v) {
    uint8_t p[2] = { (uint8_t)(v >> 8), (uint8_t)(v & 0xFF) };
    ktcSend(c, KMC_CMD_SET_THROTTLE, p, 2);
}

/** Poll a packet every 50 ms (the throttle answers a read at any time). */
static bool pollPacket(KtcCtx& c, uint8_t slot) {
    if (c.fresh) return true;
    if (c.now - (uint32_t)c.v[slot] < 50) return false;
    c.v[slot] = (int32_t)c.now;
    return ktcReadNow(c);
}

static void showValue(KtcCtx& c) {
    char t[24];
    uint8_t f = flagsOf(c);
    snprintf(t, sizeof(t), "%u", valueOf(c));
    ktcUiBar(KTC_LIVE_Y + 6, "Lever", valueOf(c) / 32767.0f, false, t);
    ktcUiRowf(2, KTC_C_TEXT, "%s %s %s %s", (f & F_ENABLED) ? "ENABLED" : "-------",
              (f & F_PRECISION) ? "PRECISION" : "---------", (f & F_TOUCH) ? "TOUCH" : "-----",
              (f & F_MOVING) ? "MOVING" : "------");
}

// ------------------------------------------------------------
//  Buttons
// ------------------------------------------------------------
static const KtcWalkItem BUTTONS[] = {
    { "100%", 0 }, { "UP", 1 }, { "DOWN", 2 }, { "0%", 3 },
};
static const KtcWalk WALK = { 4, BUTTONS, ktcDecodeRiseB1, 0x0F, 50, 4, ktcDoEnable,
    "Hands off the lever. Press each button when named." };

static const KtcBootCfg BOOT = { 300, true, "Hands off the lever. " };
static const KtcIntCfg INT_CFG = { "Hands off the lever. Press and release 100%.", 0, 1, true, ktcDecodeRiseB1 };

// ------------------------------------------------------------
//  Lever travel and touch sensor
// ------------------------------------------------------------
static KtcVerdict stepLever(KtcCtx& c) {
    static const char* const P[] = { "Slide the lever fully DOWN and hold it there.",
                                     "Slide the lever fully UP and hold it there.",
                                     "Let go of the lever." };
    if (c.entry) {
        ktcShow(c, P[0], KTC_BTN_FAIL);
        ktcSend(c, KMC_CMD_ENABLE);
        c.v[3] = 0;   // time the end was reached without touch
        ktcPhase(c, 0);
    }
    if (c.btn == KTC_BTN_FAIL) return ktcFail(c, "operator FAIL");
    if (!pollPacket(c, 2)) return KTC_RUNNING;
    showValue(c);
    uint16_t v = valueOf(c);
    bool touch = flagsOf(c) & F_TOUCH;
    bool atEnd = (c.phase == 0 && v <= 300) || (c.phase == 1 && v >= 32400);
    if (c.phase < 2 && atEnd) {
        if (touch) {
            ktcPhase(c, c.phase + 1);
            ktcUiInstr(P[c.phase]);
            c.v[3] = 0;
        } else {
            if (!c.v[3]) c.v[3] = (int32_t)c.now;
            if (c.now - (uint32_t)c.v[3] > 1500) return ktcFail(c, "lever held but TOUCH flag not set");
        }
    } else if (c.phase == 2 && !touch) {
        if (!c.v[3]) c.v[3] = (int32_t)c.now;
        if (c.now - (uint32_t)c.v[3] > 300) return ktcPass(c, "0..32767 reached, touch sensor ok");
    } else if (c.phase == 2) c.v[3] = 0;
    if (ktcPhaseMs(c) > 20000)
        return ktcFail(c, c.phase == 0 ? "never reached 0 (min > 300)" :
                          c.phase == 1 ? "never reached 32767 (max < 32400)" : "TOUCH stays set after let go");
    return KTC_RUNNING;
}

// ------------------------------------------------------------
//  Motor seek: 0 %, 100 %, 50 %, hands off
// ------------------------------------------------------------
static KtcVerdict stepMotor(KtcCtx& c) {
    static const uint16_t TARGET[3] = { 0, 32767, 16384 };
    static const uint16_t TOL[3]    = { 500, 500, 900 };
    if (c.entry) {
        ktcShow(c, "Hands off the lever: the motor drives it to 0 %, 100 % and 50 %.", 0);
        ktcSend(c, KMC_CMD_ENABLE);
        ktcSend1(c, KMC_CMD_SET_LEVER_HOLD, 0);
        c.v[4] = 0;          // seek index
        c.v[5] = 0;          // peak mA
        c.v[6] = 0;          // seen moving
        c.v[7] = 0;          // last power sample
        setThrottle(c, TARGET[0]);
        ktcPhase(c, 0);
    }
    if (c.now - (uint32_t)c.v[7] >= 50) {
        c.v[7] = (int32_t)c.now;
        KtcPower p = ktcReadPower();
        if (p.ok && p.amps * 1000.0f > c.v[5]) c.v[5] = (int32_t)(p.amps * 1000.0f);
    }
    if (!pollPacket(c, 2)) return KTC_RUNNING;
    showValue(c);
    uint8_t i = (uint8_t)c.v[4];
    ktcUiRowf(4, KTC_C_TEXT, "Seek %u: target %u  (%lu ms)", i + 1, TARGET[i], (unsigned long)ktcPhaseMs(c));
    ktcUiRowf(5, KTC_C_DIM, "Peak supply current %ld mA", (long)c.v[5]);
    uint16_t v = valueOf(c);
    bool moving = flagsOf(c) & F_MOVING;
    if (moving) c.v[6] = 1;
    int32_t err = (int32_t)v - (int32_t)TARGET[i];
    if (err < 0) err = -err;
    if (!moving && err <= TOL[i] && ktcPhaseMs(c) > 100) {
        if (++c.v[4] >= 3) {
            if (!c.v[6]) return ktcFail(c, "MOVING flag never set");
            return ktcPass(c, "0/100/50 %% reached, peak %ld mA", (long)c.v[5]);
        }
        setThrottle(c, TARGET[c.v[4]]);
        ktcPhase(c, 0);
        return KTC_RUNNING;
    }
    if (ktcPhaseMs(c) > 4000) {
        setThrottle(c, v);   // stop the motor where it is
        return ktcFail(c, "no arrival at %u (at %u%s)", TARGET[i], v, moving ? ", still driving" : "");
    }
    return KTC_RUNNING;
}

// ------------------------------------------------------------
//  Touch blocks the motor (lever hold off)
// ------------------------------------------------------------
static KtcVerdict stepTouchBlock(KtcCtx& c) {
    if (c.entry) {
        ktcShow(c, "Hold the lever near the middle and keep touching it.", KTC_BTN_FAIL);
        ktcSend(c, KMC_CMD_ENABLE);
        ktcSend1(c, KMC_CMD_SET_LEVER_HOLD, 0);
        c.v[3] = 0;
        ktcPhase(c, 0);
    }
    if (c.btn == KTC_BTN_FAIL) return ktcFail(c, "operator FAIL");
    if (!pollPacket(c, 2)) return KTC_RUNNING;
    showValue(c);
    uint16_t v = valueOf(c);
    bool touch = flagsOf(c) & F_TOUCH, moving = flagsOf(c) & F_MOVING;
    switch (c.phase) {
    case 0:
        if (touch) { if (!c.v[3]) c.v[3] = (int32_t)c.now; } else c.v[3] = 0;
        if (c.v[3] && c.now - (uint32_t)c.v[3] > 500) {
            c.v[4] = v;
            setThrottle(c, v > 16384 ? 0 : 32767);     // try to drive away from the hand
            ktcUiRow(4, "Target sent while touched...", KTC_C_DIM);
            ktcPhase(c, 1);
        }
        if (ktcPhaseMs(c) > 20000) return ktcFail(c, "TOUCH flag never set");
        break;
    case 1:
        if (moving) { setThrottle(c, v); return ktcFail(c, "motor drove while the lever was touched"); }
        if (!touch) return ktcFail(c, "touch lost: keep touching for 1 s");
        if (ktcPhaseMs(c) > 1000) { ktcUiInstr("Let go of the lever."); ktcPhase(c, 2); }
        break;
    case 2:
        if (moving) { setThrottle(c, v); return ktcFail(c, "motor moved after let go (target not dropped)"); }
        if (!touch && ktcPhaseMs(c) > 1500) return ktcPass(c, "target refused while touched");
        if (ktcPhaseMs(c) > 20000) return ktcFail(c, "TOUCH stays set after let go");
        break;
    }
    return KTC_RUNNING;
}

// ------------------------------------------------------------
//  Precision mode: +-3276 about the anchor over the full travel
// ------------------------------------------------------------
static KtcVerdict stepPrecision(KtcCtx& c) {
    if (c.entry) {
        ktcShow(c, "Hands off: the lever goes to 50 %, then precision mode starts.", KTC_BTN_FAIL);
        ktcSend(c, KMC_CMD_ENABLE);
        ktcSend1(c, KMC_CMD_SET_LEVER_HOLD, 0);
        setThrottle(c, 16384);
        ktcPhase(c, 0);
    }
    if (c.btn == KTC_BTN_FAIL) { ktcSend1(c, KMC_CMD_SET_PRECISION, 0); return ktcFail(c, "operator FAIL"); }
    if (!pollPacket(c, 2)) return KTC_RUNNING;
    showValue(c);
    uint16_t v = valueOf(c);
    uint8_t f = flagsOf(c);
    int32_t anchor = c.v[4];
    switch (c.phase) {
    case 0:   // at 50 %
        if (!(f & F_MOVING) && v > 15500 && v < 17300 && ktcPhaseMs(c) > 100) {
            c.v[4] = v;
            ktcSend1(c, KMC_CMD_SET_PRECISION, 1);
            ktcPhase(c, 1);
        } else if (ktcPhaseMs(c) > 4000) return ktcFail(c, "lever did not reach 50 %%");
        break;
    case 1:   // centring, then the operator moves it
        if (!(f & F_PRECISION)) { if (ktcPhaseMs(c) > 300) return ktcFail(c, "PRECISION flag not set"); break; }
        if (!(f & F_MOVING) && ktcPhaseMs(c) > 200) {
            if (v < anchor - 400 || v > anchor + 400) return ktcFail(c, "output %u moved from anchor %ld", v, (long)anchor);
            ktcUiRowf(4, KTC_C_TEXT, "Anchor %ld", (long)anchor);
            ktcUiInstr("Slide the lever fully UP and hold it there.");
            ktcPhase(c, 2);
        }
        break;
    case 2:
        if (v > anchor + 4000) return ktcFail(c, "%u: more than +3276 (precision not applied)", v);
        if ((f & F_TOUCH) && v >= anchor + 3000) { ktcUiInstr("Slide the lever fully DOWN and hold it there."); ktcPhase(c, 3); }
        break;
    case 3:
        if (v + 4000 < anchor) return ktcFail(c, "%u: more than -3276 (precision not applied)", v);
        if ((f & F_TOUCH) && v + 3000 <= anchor) { c.v[5] = v; ktcUiInstr("Let go of the lever."); ktcPhase(c, 4); }
        break;
    case 4:
        if (!(f & F_TOUCH) && ktcPhaseMs(c) > 300) {
            c.v[5] = v;
            ktcSend1(c, KMC_CMD_SET_PRECISION, 0);
            ktcUiRow(5, "Precision off: lever drives to match", KTC_C_DIM);
            ktcPhase(c, 5);
        }
        break;
    case 5:
        if ((f & F_PRECISION) && ktcPhaseMs(c) > 300) return ktcFail(c, "PRECISION flag still set");
        if (!(f & F_MOVING) && ktcPhaseMs(c) > 300) {
            int32_t d = (int32_t)v - c.v[5];
            if (d < -900 || d > 900) return ktcFail(c, "exit: output %ld became %u", (long)c.v[5], v);
            return ktcPass(c, "anchor %ld, range +-3276, exit ok", (long)anchor);
        }
        if (ktcPhaseMs(c) > 4000) { setThrottle(c, v); return ktcFail(c, "no arrival after exit"); }
        break;
    }
    if (c.phase >= 2 && c.phase <= 4 && ktcPhaseMs(c) > 20000) {
        ktcSend1(c, KMC_CMD_SET_PRECISION, 0);
        return ktcFail(c, "range not reached");
    }
    return KTC_RUNNING;
}

// ------------------------------------------------------------
//  Lever hold: the motor pushes back for 0.5 s, then returns on let go
// ------------------------------------------------------------
static KtcVerdict stepHold(KtcCtx& c) {
    if (c.entry) {
        ktcShow(c, "Hands off: lever hold on, the lever goes to 25 %.", KTC_BTN_FAIL);
        ktcSend(c, KMC_CMD_ENABLE);
        ktcSend1(c, KMC_CMD_SET_LEVER_HOLD, 1);
        setThrottle(c, 8192);
        c.v[4] = 0; c.v[5] = 0; c.v[6] = 0;
        ktcPhase(c, 0);
    }
    if (c.btn == KTC_BTN_FAIL) { ktcSend1(c, KMC_CMD_SET_LEVER_HOLD, 0); return ktcFail(c, "operator FAIL"); }
    if (!pollPacket(c, 2)) return KTC_RUNNING;
    showValue(c);
    uint16_t v = valueOf(c);
    uint8_t f = flagsOf(c);
    switch (c.phase) {
    case 0:
        if (!(f & F_MOVING) && v > 7400 && v < 9000 && ktcPhaseMs(c) > 100) {
            ktcUiInstr("Push the lever UP firmly for 2 s, then let go.");
            ktcPhase(c, 1);
        } else if (ktcPhaseMs(c) > 4000) { ktcSend1(c, KMC_CMD_SET_LEVER_HOLD, 0); return ktcFail(c, "no arrival at 25 %%"); }
        break;
    case 1:   // touched: pushing back, then giving up
        if (f & F_TOUCH) {
            if (!c.v[4]) c.v[4] = (int32_t)c.now;
            uint32_t held = c.now - (uint32_t)c.v[4];
            if ((f & F_MOVING) && held < 450) c.v[5] = 1;         // pushed back
            if (!(f & F_MOVING) && held > 700) c.v[6] = 1;        // let go of the hand
        } else if (c.v[4]) {
            ktcUiRowf(4, KTC_C_TEXT, "Push-back %s, released %s", c.v[5] ? "yes" : "no", c.v[6] ? "yes" : "no");
            ktcPhase(c, 2);
        }
        if (ktcPhaseMs(c) > 20000) { ktcSend1(c, KMC_CMD_SET_LEVER_HOLD, 0); return ktcFail(c, "lever never touched"); }
        break;
    case 2:   // back to 25 %
        if (!(f & F_MOVING) && v > 7400 && v < 9000 && ktcPhaseMs(c) > 200) {
            ktcSend1(c, KMC_CMD_SET_LEVER_HOLD, 0);
            if (!c.v[5]) return ktcFail(c, "no push-back while held");
            if (!c.v[6]) return ktcFail(c, "motor kept pushing past 0.5 s");
            return ktcPass(c, "push-back 0.5 s, returned to 25 %%");
        }
        if (ktcPhaseMs(c) > 4000) { setThrottle(c, v); ktcSend1(c, KMC_CMD_SET_LEVER_HOLD, 0); return ktcFail(c, "did not return to 25 %%"); }
        break;
    }
    return KTC_RUNNING;
}

// ------------------------------------------------------------
//  Backlights and bulb test (one shared output for five switches)
// ------------------------------------------------------------
static void lampEnable(KtcCtx& c)  { ktcSend(c, KMC_CMD_ENABLE); }
static void lampDisable(KtcCtx& c) { ktcSend(c, KMC_CMD_DISABLE); }
static const KtcVisualItem LAMPS[] = {
    { "enabled",  "ENABLE: are the five switch backlights on?", lampEnable, 0 },
    { "disabled", "DISABLE: backlights off, and the lever drives to 0 %?", lampDisable, 0 },
    { "bulb on",  "Bulb test: backlights on?", ktcDoBulbOn, 0 },
    { "bulb off", "Bulb test stopped: backlights off again (still disabled)?", ktcDoBulbOff, 0 },
    { "enabled",  "ENABLE: backlights on again?", lampEnable, 0 },
};
static const KtcVisualSeq LAMP_SEQ = { 5, LAMPS, nullptr, nullptr };

// ------------------------------------------------------------
//  Lifecycle: no INT while DISABLED or SLEEPING, WAKE counts one INT
// ------------------------------------------------------------
static KtcVerdict stepLifecycle(KtcCtx& c) {
    if (c.entry) {
        c.autoRead = false;
        ktcShow(c, nullptr, 0);
        ktcPhase(c, 0);
    }
    switch (c.phase) {
    case 0:
        ktcDrain(c);
        ktcSend(c, KMC_CMD_DISABLE);
        delay(KTC_CMD_SETTLE_MS);
        if (!ktcReadNow(c) || c.lifecycle != KMC_STATUS_DISABLED) return ktcFail(c, "not DISABLED after DISABLE");
        if (flagsOf(c) & F_ENABLED) return ktcFail(c, "ENABLED flag set while DISABLED");
        ktcUiInstr("Press 100% now, then tap NEXT. (Module DISABLED: no INT allowed.)");
        ktcUiButtons(KTC_BTN_NEXT | KTC_BTN_SKIP | KTC_BTN_ABORT);
        ktcPhase(c, 1);
        break;
    case 1:
    case 3:
        if (ktcIntAsserted()) return ktcFail(c, "INT while %s", c.phase == 1 ? "DISABLED" : "SLEEPING");
        if (c.btn == KTC_BTN_NEXT) ktcPhase(c, c.phase + 1);
        break;
    case 2:
        ktcSend(c, KMC_CMD_ENABLE);
        delay(KTC_CMD_SETTLE_MS);
        ktcDrain(c);
        ktcSend(c, KMC_CMD_SLEEP);
        delay(KTC_CMD_SETTLE_MS);
        if (!ktcReadNow(c) || c.lifecycle != KMC_STATUS_SLEEPING) return ktcFail(c, "not SLEEPING after SLEEP");
        ktcUiInstr("Press 100% now, then tap NEXT. (Asleep: no INT allowed.)");
        ktcPhase(c, 3);
        break;
    case 4: {
        uint8_t tx = c.pkt[2];
        ktcSend(c, KMC_CMD_WAKE);
        delay(KTC_CMD_SETTLE_MS);
        if (!ktcReadNow(c) || c.lifecycle != KMC_STATUS_ACTIVE) return ktcFail(c, "not ACTIVE after WAKE");
        if (c.pkt[2] != (uint8_t)(tx + 1)) return ktcFail(c, "WAKE: tx %u -> %u, expected +1", tx, c.pkt[2]);
        if (!(flagsOf(c) & F_ENABLED)) return ktcFail(c, "ENABLED flag clear after WAKE");
        return ktcPass(c, "DISABLE/SLEEP quiet, WAKE counted");
    }
    }
    return KTC_RUNNING;
}

// ------------------------------------------------------------
//  Supply current: dark and still vs backlights on
// ------------------------------------------------------------
static void curIdle(KtcCtx& c) { ktcSend(c, KMC_CMD_DISABLE); }
static const KtcCurrentCfg CURRENT = { 80, 3, "backlights", curIdle, ktcDoBulbOn, ktcDoBulbOff };

static void cleanup(KtcCtx& c) {
    ktcSend1(c, KMC_CMD_SET_PRECISION, 0);
    ktcSend1(c, KMC_CMD_SET_LEVER_HOLD, 0);
    ktcSend1(c, KMC_CMD_BULB_TEST, 0x00);
}

static void live(KtcCtx& c) {
    ktcUiRowf(4, KTC_C_TEXT, "Flags %02X  events %02X", flagsOf(c), ktcByte(c, 1));
    ktcUiRowf(5, KTC_C_TEXT, "Value %u", valueOf(c));
}

static const KtcStep STEPS[] = {
    { "Identity",         ktcStepIdentity,  nullptr },
    { "Boot handshake",   ktcStepBoot,      &BOOT },
    { "Enable and INT",   ktcStepEnableInt, &INT_CFG },
    { "Buttons",          ktcStepWalk,      &WALK },
    { "Lever and touch",  stepLever,        nullptr },
    { "Motor seek",       stepMotor,        nullptr },
    { "Touch blocks motor", stepTouchBlock, nullptr },
    { "Precision mode",   stepPrecision,    nullptr },
    { "Lever hold",       stepHold,         nullptr },
    { "Backlights",       ktcStepVisual,    &LAMP_SEQ },
    { "Lifecycle",        stepLifecycle,    nullptr },
    { "Supply current",   ktcStepCurrent,   &CURRENT },
};

static const KtcModuleDef MODULE = {
    "Throttle", "KC-01-1871/1872 throttle board",
    KMC_TYPE_THROTTLE, 0x2C, KMC_THROTTLE_PACKET_SIZE,
    2, 0, KMC_CAP_MOTORIZED,
    STEPS, sizeof(STEPS) / sizeof(STEPS[0]),
    cleanup, live,
};

void setup() { ktcBegin(MODULE); }
void loop()  { ktcLoop(); }

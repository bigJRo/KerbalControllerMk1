/***************************************************************************************
   Screen_LNCH_AscentAP.ino — Ascent Autopilot interface (chrome + draw + touch)

   Touch UI for the Simpit ascent autopilot. Renders the autopilot status echoed
   from Controller_Main (AscentStatus, in the shared `state` struct) and lets the
   pilot edit parameters (on-screen numeric keypad + toggles) and ARM/DISARM by
   touch, at any time.

   Pilot edits go to a local "pending" buffer that is shown in preference to the
   autopilot-confirmed value and staged on the outbound command channel; when the
   autopilot echoes the new value back, the pending flag is cleared.

   LAYOUT (1024x600, content 0..CONTENT_W, y from TITLE_TOP):
     banner: <PHASE, coloured>            body   <ARMED/DISARMED>
     MISSION (inputs) | VEH PROFILE (inputs) | GUIDANCE (outputs)
     Tgt Ap / Incl /  | Loft / Roll /        | Pitch Hdg Thr
     Launch           | Max-G  + [ARM]       | G q ApA PeA
   Boxed fields are the touch targets. Tapping one opens the keypad (or toggles).
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

// ── Palette / fonts ─────────────────────────────────────────────────────────────────
static const uint16_t AP_LBL = KDC_LABEL_COLOR;
static const uint16_t AP_VAL = TFT_DARK_GREEN;
static const uint16_t AP_HDR = TFT_WHITE;
static const uint16_t AP_BOX = TFT_GREY;
static const uint16_t AP_EDT = TFT_SKY;          // pending (unconfirmed) value colour
static const uint16_t AP_GUARD = TFT_ORANGE;
#define AP_F_LBL  (&Roboto_Black_24)
#define AP_F_VAL  (&Roboto_Black_32)
#define AP_F_HDR  (&Roboto_Black_28)
#define AP_F_BANN (&Roboto_Black_36)
#define AP_F_ARM  (&Roboto_Black_28)

// ── Geometry ──────────────────────────────────────────────────────────────────────
static const int16_t AP_BANNER_Y = TITLE_TOP;
static const int16_t AP_BANNER_H = 64;
static const int16_t AP_COL_Y    = AP_BANNER_Y + AP_BANNER_H + 6;   // 132
static const int16_t AP_ROW_Y0   = AP_COL_Y + 40;                   // 172
static const int16_t AP_ROW_H    = 58;
static const int16_t AP_COL_BOT  = 590;
static const int16_t AP_C1X = 6,   AP_C2X = 322, AP_C3X = 638;
static const int16_t AP_COLW = 298;
static const int16_t AP_DIV1_X = 314, AP_DIV2_X = 630;
static const int16_t AP_VALW = 176;
static const int16_t AP_VALH = AP_ROW_H - 8;
static const int16_t AP_ARM_H = 136;
static const int16_t AP_ARM_Y = SCREEN_H - 4 - AP_ARM_H;           // 460
static const int16_t AP_ARM_X = AP_C2X, AP_ARM_W = AP_COLW;
static inline int16_t apRowY(uint8_t i) { return AP_ROW_Y0 + i * AP_ROW_H; }

// ── Field table ───────────────────────────────────────────────────────────────────
enum {
  AP_TGTALT = 0, AP_INCL, AP_DIR,                          // mission (inputs)
  AP_LOFT, AP_ROLL, AP_MAXG,                               // profile (inputs)
  AP_PITCH, AP_HDG, AP_THR, AP_G, AP_Q, AP_APA, AP_PE,     // guidance (outputs)
  AP_SLOT_COUNT
};
static const uint8_t AP_ARM_SLOT   = 20;
static const uint8_t AP_PHASE_SLOT = 21;
static const uint8_t AP_BODY_SLOT  = 22;

struct ApCell { int16_t x; uint8_t row; const char *label; bool edit; };
static const ApCell AP_CELLS[AP_SLOT_COUNT] = {
  { AP_C1X, 0, "TGT AP", true  },
  { AP_C1X, 1, "INC",    true  },
  { AP_C1X, 2, "LAUNCH", true  },
  { AP_C2X, 0, "LOFT",   true  },
  { AP_C2X, 1, "ROLL",   true  },
  { AP_C2X, 2, "MAX-G",  true  },
  { AP_C3X, 0, "PITCH",  false },
  { AP_C3X, 1, "HDG",    false },
  { AP_C3X, 2, "THRTL",  false },
  { AP_C3X, 3, "G",      false },
  { AP_C3X, 4, "Q",      false },
  { AP_C3X, 5, "APA",    false },
  { AP_C3X, 6, "PEA",    false },
};
static inline int16_t apValX(uint8_t slot) { return AP_CELLS[slot].x + AP_COLW - AP_VALW; }
static inline int16_t apValY(uint8_t slot) { return apRowY(AP_CELLS[slot].row) + 4; }

// Editable-field descriptors (indexed by AP_EDITS order). kind: 0 numeric, 1 toggle
// N/S, 2 numeric+OFF (roll hold), 3 numeric+OFF (max-G, 0=off).
struct ApEdit { uint8_t slot, kind; const char *name, *units; float mn, mx; };
static const ApEdit AP_EDITS[6] = {
  { AP_TGTALT, 0, "TARGET APOAPSIS",  "m",    0.0f, 2000000000.0f },
  { AP_INCL,   0, "INCLINATION",      "\xB0", 0.0f, 180.0f },
  { AP_DIR,    1, "LAUNCH DIRECTION", "",     0.0f, 0.0f },
  { AP_LOFT,   0, "LOFT EXPONENT",    "",     0.5f, 2.0f },
  { AP_ROLL,   2, "ROLL HOLD",        "\xB0", -180.0f, 180.0f },
  { AP_MAXG,   3, "MAX-G",            "g",    0.0f, 20.0f },
};

// ── Pending (pilot-entered) overrides — shown until the autopilot echoes back ────────
static bool  apDTgt=false;  static float apTgt;
static bool  apDInc=false;  static float apInc;
static bool  apDDir=false;  static bool  apDir;
static bool  apDLof=false;  static float apLof;
static bool  apDRol=false;  static bool  apRolEn;  static float apRolDeg;
static bool  apDMxg=false;  static float apMxg;
static int8_t apArmOvr = -1;   // -1 use state, 0 disarmed, 1 armed (commanded)

static float apGTgt(){ return apDTgt ? apTgt    : state.apTargetAlt; }
static float apGInc(){ return apDInc ? apInc    : state.apInclination; }
static bool  apGDir(){ return apDDir ? apDir    : state.apSoutherly; }
static float apGLof(){ return apDLof ? apLof    : state.apLoft; }
static bool  apGRolEn(){ return apDRol ? apRolEn  : state.apRollEnable; }
static float apGRolDeg(){ return apDRol ? apRolDeg : state.apRollDeg; }
static float apGMxg(){ return apDMxg ? apMxg    : state.apMaxG; }
static uint16_t apEditColor(bool dirty){ return dirty ? AP_EDT : AP_VAL; }

// Two different questions, deliberately kept apart — conflating them has produced a
// bug twice now.
//
//   apArmCommanded()  — what the pilot has asked for: the queued ARM/DISARM tap while
//                       one is in flight, the autopilot's state otherwise. Drives what
//                       the NEXT tap does, so a second tap reverses a pending command
//                       rather than repeating it, and a lost acknowledgement cannot
//                       lock the button out.
//   apArmedAnnunciated() — what the autopilot actually reports. Everything that TELLS
//                       the pilot whether the vehicle is armed reads this and only
//                       this: the ARM button, the ARMED/DISARMED banner, and the
//                       sidebar ASC key. An annunciation must never say DISARMED while
//                       the autopilot is still flying the vehicle, so a tap alone is
//                       not enough to change it — Controller_Main has to echo it back.
//                       The wait is shown as a pending cue, not by lying about state.
static bool apArmCommanded(){ return apArmOvr >= 0 ? (apArmOvr == 1) : state.apArmed; }
static bool apArmPending()  { return apArmOvr >= 0; }

bool apArmedAnnunciated() { return state.apArmed; }

// ── Outbound command channel ─────────────────────────────────────────────────────────
// The command queue (apEnqueueCmd / apPumpCommandQueue / apFillOutboundCmd /
// apAckCommand) and the numeric keypad moved to ConsoleShared.ino when the AIRCRAFT
// and ROVER autopilot consoles arrived; all three consoles share one queue and one
// keypad. Byte-level layout is in Documents/Developer/Ascent_Autopilot_Interface.md.

// Clear pending (cyan) flags once Controller_Main echoes the accepted value back in the
// AscentStatus frame. Called each loop from updateI2CState().
void apReconcilePending() {
  // Target uses a relative tolerance: a float32 ULP already exceeds 1 m above ~8.4e6 m,
  // so a fixed 1 m window would never close for high (interplanetary) target apoapses.
  float tgtTol = fmaxf(1.0f, fabsf(apTgt) * 5.0e-4f);
  if (apDTgt && fabsf(state.apTargetAlt   - apTgt)    < tgtTol)  apDTgt = false;
  if (apDInc && fabsf(state.apInclination - apInc)    < 0.05f)   apDInc = false;
  if (apDDir && state.apSoutherly == apDir)                      apDDir = false;
  if (apDLof && fabsf(state.apLoft        - apLof)    < 0.005f)  apDLof = false;
  if (apDRol && state.apRollEnable == apRolEn &&
      (!apRolEn || fabsf(state.apRollDeg  - apRolDeg) < 0.5f))   apDRol = false;
  if (apDMxg && fabsf(state.apMaxG        - apMxg)    < 0.05f)   apDMxg = false;
  // Clears the pending cue once Controller_Main reports the commanded armed state.
  if (apArmOvr >= 0 && state.apArmed == (apArmOvr == 1))         apArmOvr = -1;
}

static bool apScreenRefresh = false;

// ── Phase name / colour ─────────────────────────────────────────────────────────────
const char *apPhaseName(uint8_t p) {
  switch (p) {
    case 1: return "VERTICAL"; case 2: return "GRAVITY TURN"; case 3: return "COAST";
    case 4: return "CIRCULARIZE"; case 5: return "COMPLETE"; case 6: return "ABORT";
    default: return "IDLE";
  }
}
static uint16_t apPhaseColor(uint8_t p) {
  switch (p) {
    case 1: case 2: return TFT_DARK_GREEN;
    case 3: case 4: return TFT_CYAN;
    case 5:         return TFT_SKY;
    case 6:         return TFT_RED;
    default:        return TFT_DARK_GREY;
  }
}

// Pick a legible text colour for an arbitrary RGB565 background by its perceived
// luminance (Rec. 601). White washes out on the light phase colours (CYAN ~179,
// SKY ~177); on those we use TFT_DARK_GREY — the standard dark-on-light button text
// used elsewhere (AIRBRK on CYAN, ROVR REV on YELLOW). The dark phases (DARK_GREEN
// ~73, RED ~76) keep white. Threshold 140 splits the two groups with margin;
// channels are scaled 5/6/5 -> 0..255 before weighting.
static uint16_t apTextOn(uint16_t bg) {
  uint16_t r = ((bg >> 11) & 0x1F) * 255 / 31;
  uint16_t g = ((bg >> 5)  & 0x3F) * 255 / 63;
  uint16_t b = ( bg        & 0x1F) * 255 / 31;
  uint16_t lum = (uint16_t)((r * 299 + g * 587 + b * 114) / 1000);
  return (lum > 140) ? TFT_DARK_GREY : TFT_WHITE;
}

// ── CHROME (static) ─────────────────────────────────────────────────────────────────
static void chromeScreen_LNCHAP(KCM_TFT &tft) {
  // Any full repaint (screen entry, display reset) starts with the keypad closed — the
  // modal is transient and must never persist across a leave/return, or the panel would
  // come back frozen with taps misrouted into a stale keypad.
  kpForceClose();
  tft.fillRect(0, AP_BANNER_Y + AP_BANNER_H, CONTENT_W, 2, TFT_GREY);
  tft.drawLine(AP_DIV1_X, AP_COL_Y, AP_DIV1_X, AP_COL_BOT, TFT_GREY);
  tft.drawLine(AP_DIV2_X, AP_COL_Y, AP_DIV2_X, AP_COL_BOT, TFT_GREY);
  textLeft(tft, AP_F_HDR, AP_C1X, AP_COL_Y, AP_COLW, 32, "MISSION",     AP_HDR, TFT_BLACK);
  textLeft(tft, AP_F_HDR, AP_C2X, AP_COL_Y, AP_COLW, 32, "VEH PROFILE", AP_HDR, TFT_BLACK);
  textLeft(tft, AP_F_HDR, AP_C3X, AP_COL_Y, AP_COLW, 32, "GUIDANCE",    AP_HDR, TFT_BLACK);
  for (uint8_t s = 0; s < AP_SLOT_COUNT; s++) {
    const ApCell &c = AP_CELLS[s];
    int16_t y = apRowY(c.row);
    textLeft(tft, AP_F_LBL, c.x + 2, y, AP_COLW - AP_VALW, AP_ROW_H, c.label, AP_LBL, TFT_BLACK);
    if (c.edit) tft.drawRect(apValX(s), apValY(s), AP_VALW - 2, AP_VALH, AP_BOX);
  }
  textLeft(tft, &Roboto_Black_16, AP_C1X + 2, apRowY(3) + 8, AP_COLW - 8, 20,
           "Boxed fields - tap to edit", TFT_DARK_GREY, TFT_BLACK);
}

// ── Value helper (flicker-free) ─────────────────────────────────────────────────────
static void apPut(KCM_TFT &tft, uint8_t slot, const String &val, uint16_t fg) {
  RowCache &rc = rowCache[screen_LNCHAP][slot];
  if (rc.value == val && rc.fg == fg) return;
  printValue(tft, AP_F_VAL, apValX(slot), apValY(slot), AP_VALW - 2, AP_VALH,
             "", val, fg, TFT_BLACK, TFT_BLACK, printState[screen_LNCHAP][slot]);
  rc.value = val; rc.fg = fg; rc.bg = TFT_BLACK;
}
// C-string form: the cache compare allocates nothing; the String is built only for a
// value about to be drawn. Every per-frame value on the console comes through here.
static void apPut(KCM_TFT &tft, uint8_t slot, const char *val, uint16_t fg) {
  RowCache &rc = rowCache[screen_LNCHAP][slot];
  if (rc.fg == fg && rc.value == val) return;
  apPut(tft, slot, String(val), fg);
}

// ── DRAW (dynamic) ──────────────────────────────────────────────────────────────────
static void drawScreen_LNCHAP(KCM_TFT &tft) {
  if (kpTakeClosed()) apScreenRefresh = true;   // repaint the panel the modal covered
  if (apScreenRefresh) {
    tft.fillRect(0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP, TFT_BLACK);
    chromeScreen_LNCHAP(tft);
    for (uint8_t r = 0; r < ROW_COUNT; r++) rowCache[screen_LNCHAP][r].value = "\x01";
    apScreenRefresh = false;
  }
  if (kpIsOpen()) {
    kpDraw(tft);
    return;                       // freeze the panel behind the modal while editing
  }

  char buf[48];

  // Phase banner
  {
    uint16_t pc = apPhaseColor(state.apPhase);
    const char *pn = apPhaseName(state.apPhase);
    RowCache &rc = rowCache[screen_LNCHAP][AP_PHASE_SLOT];
    if (rc.fg != pc || rc.value != pn) {
      tft.fillRect(0, AP_BANNER_Y, 640, AP_BANNER_H, TFT_BLACK);
      textLeft(tft, AP_F_BANN, 8, AP_BANNER_Y, 632, AP_BANNER_H, pn, pc, TFT_BLACK);
      rc.value = pn; rc.fg = pc;
    }
  }
  {
    bool armed = apArmedAnnunciated();
    const char *bs = state.gameSOI.c_str();
    // Truth, always. A queued tap appends "..." rather than flipping the word, so the
    // banner cannot read DISARMED while the autopilot is still armed.
    char as[16];
    snprintf(as, sizeof(as), "%s%s", armed ? "ARMED" : "DISARMED", apArmPending() ? "..." : "");
    uint16_t ac = armed ? TFT_NEON_GREEN : TFT_DARK_GREY;
    RowCache &rc = rowCache[screen_LNCHAP][AP_BODY_SLOT];
    char combo[64];
    snprintf(combo, sizeof(combo), "%s|%s", bs, as);
    if (rc.fg != ac || rc.value != combo) {
      tft.fillRect(648, AP_BANNER_Y, CONTENT_W - 648, AP_BANNER_H, TFT_BLACK);
      textRight(tft, &Roboto_Black_20, 648, AP_BANNER_Y + 4,  CONTENT_W - 648 - 6, 24, bs, AP_LBL, TFT_BLACK);
      textRight(tft, &Roboto_Black_28, 648, AP_BANNER_Y + 30, CONTENT_W - 648 - 6, 30, as, ac, TFT_BLACK);
      rc.value = combo; rc.fg = ac;
    }
  }

  // Mission (inputs — pending shown in AP_EDT colour)
  { formatAltBuf(apGTgt(), buf, sizeof(buf)); apPut(tft, AP_TGTALT, buf, apEditColor(apDTgt)); }
  { snprintf(buf, sizeof(buf), "%.1f\xB0", apGInc()); apPut(tft, AP_INCL, buf, apEditColor(apDInc)); }
  apPut(tft, AP_DIR, apGDir() ? "SOUTH" : "NORTH", apEditColor(apDDir));

  // Vehicle profile (inputs)
  { char b2[12]; dtostrf(apGLof(), 1, 2, b2); apPut(tft, AP_LOFT, b2, apEditColor(apDLof)); }
  if (apGRolEn()) { snprintf(buf, sizeof(buf), "%+.0f\xB0", apGRolDeg()); apPut(tft, AP_ROLL, buf, apEditColor(apDRol)); }
  else            { apPut(tft, AP_ROLL, "OFF", apDRol ? AP_EDT : TFT_DARK_GREY); }
  if (apGMxg() > 0.0f) { snprintf(buf, sizeof(buf), "%.1f g", apGMxg()); apPut(tft, AP_MAXG, buf, apEditColor(apDMxg)); }
  else                 { apPut(tft, AP_MAXG, "OFF", apDMxg ? AP_EDT : TFT_DARK_GREY); }

  // Guidance (outputs)
  { snprintf(buf, sizeof(buf), "%+.0f\xB0", state.apCmdPitch); apPut(tft, AP_PITCH, buf, AP_VAL); }
  { int16_t h = (int16_t)roundf(state.apCmdHeading) % 360; if (h < 0) h += 360;
    snprintf(buf, sizeof(buf), "%03d\xB0", h); apPut(tft, AP_HDG, buf, AP_VAL); }
  {
    int16_t pct = (int16_t)roundf(state.apCmdThrottle * 100.0f);
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    RowCache &rc = rowCache[screen_LNCHAP][AP_THR];
    char v[8];
    snprintf(v, sizeof(v), "%d", pct);
    if (rc.value != v) {
      int16_t bx = apValX(AP_THR), by = apValY(AP_THR), bw = AP_VALW - 2, bh = AP_VALH;
      int16_t barW = 80, fillW = (int16_t)(barW * pct / 100);
      tft.fillRect(bx, by, bw, bh, TFT_BLACK);
      int16_t barY = by + (bh - 18) / 2;
      tft.drawRect(bx, barY, barW, 18, TFT_GREY);
      if (fillW > 2) tft.fillRect(bx + 1, barY + 1, fillW - 2, 16, TFT_DARK_GREEN);
      snprintf(buf, sizeof(buf), "%d%%", pct);
      textRight(tft, AP_F_VAL, bx + barW, by, bw - barW, bh, buf, AP_VAL, TFT_BLACK);
      rc.value = v;
    }
  }
  {
    uint16_t gc = AP_VAL;
    float mg = apGMxg();
    if (mg > 0.0f) { if (state.gForce >= mg) gc = TFT_RED; else if (state.gForce >= 0.9f * mg) gc = TFT_YELLOW; }
    snprintf(buf, sizeof(buf), "%.1f g", state.gForce);
    apPut(tft, AP_G, buf, gc);
  }
  if (state.apDynPressure >= 1000.0f) snprintf(buf, sizeof(buf), "%.1f kPa", state.apDynPressure / 1000.0f);
  else                                snprintf(buf, sizeof(buf), "%.0f Pa",  state.apDynPressure);
  apPut(tft, AP_Q, buf, AP_VAL);
  formatAltBuf(state.apoapsis,  buf, sizeof(buf)); apPut(tft, AP_APA, buf, TFT_LIGHT_GREY);
  formatAltBuf(state.periapsis, buf, sizeof(buf)); apPut(tft, AP_PE,  buf, TFT_LIGHT_GREY);

  // ARM control
  {
    bool armed   = apArmedAnnunciated();   // the autopilot's state, never the tap
    bool pending = apArmPending();         // a tap queued, not yet echoed back
    uint16_t pc = apPhaseColor(state.apPhase);
    const char *txt = armed ? apPhaseName(state.apPhase) : "ARM";
    // Armed: legible text auto-picked for the phase-colour background (white washes
    // out on the light CYAN/SKY phases). Disarmed: orange guard text on off-black.
    uint16_t fg  = armed ? apTextOn(pc) : AP_GUARD;
    uint16_t bg  = armed ? pc           : TFT_OFF_BLACK;
    // While a command is in flight the border goes to the pending colour the editable
    // fields already use, and the hint says which way it is going. The fill and legend
    // stay on the real state: the pilot gets immediate confirmation that the tap
    // registered — which a touchscreen owes them, having no detent — without the panel
    // claiming a state the autopilot has not reached.
    uint16_t bdr = pending ? AP_EDT : (armed ? TFT_WHITE : AP_GUARD);
    RowCache &rc = rowCache[screen_LNCHAP][AP_ARM_SLOT];
    char key[32];
    snprintf(key, sizeof(key), "%s%s%s", armed ? "A:" : "D:", pending ? "P:" : "-:", txt);
    if (rc.value != key) {
      ButtonLabel btn = { txt, fg, fg, bg, bg, bdr, bdr };
      drawButton(tft, AP_ARM_X, AP_ARM_Y, AP_ARM_W, AP_ARM_H, btn, AP_F_ARM, false);
      const char *hint = pending ? (apArmCommanded() ? "ARMING..." : "DISARMING...")
                                 : (armed ? "tap to DISARM" : "tap to ARM");
      textCenter(tft, &Roboto_Black_16, AP_ARM_X, AP_ARM_Y + AP_ARM_H - 24, AP_ARM_W, 18,
                 hint, pending ? AP_EDT : (armed ? apTextOn(pc) : AP_GUARD), bg);
      rc.value = key;
    }
  }
}

// ── Keypad open / commit ────────────────────────────────────────────────────────────
// Commit from the shared keypad: `off` is the OFF key (roll hold / max-G only); the
// value is already clamped to the field's range.
static void apKeypadCommit(int8_t idx, float v, bool off) {
  const ApEdit &e = AP_EDITS[idx];
  if (off) {
    if (e.kind == 2)      { apRolEn = false; apDRol = apEnqueueCmd(AP_CMD_SET_ROLL, AP_ROLL_OFF); }
    else if (e.kind == 3) { apMxg = 0.0f;    apDMxg = apEnqueueCmd(AP_CMD_SET_MAXG, 0.0f); }
    return;
  }
  switch (e.slot) {
    // The pending cue is raised only for a command that was actually queued (see
    // apEnqueueCmd): a cue for one that was not would never clear.
    case AP_TGTALT: apTgt = v; apDTgt = apEnqueueCmd(AP_CMD_SET_TARGET_ALT, v);  break;  // metres
    case AP_INCL:   apInc = v; apDInc = apEnqueueCmd(AP_CMD_SET_INCLINATION, v); break;
    case AP_LOFT:   apLof = v; apDLof = apEnqueueCmd(AP_CMD_SET_LOFT, v);        break;
    case AP_ROLL:   apRolDeg = v; apRolEn = true; apDRol = apEnqueueCmd(AP_CMD_SET_ROLL, v); break;
    case AP_MAXG:   apMxg = v; apDMxg = apEnqueueCmd(AP_CMD_SET_MAXG, v);        break;
  }
}

static void apOpenKeypad(int8_t editIdx) {
  const ApEdit &e = AP_EDITS[editIdx];
  KpField f = { e.name, e.units, e.mn, e.mx, (e.kind == 2 || e.kind == 3), true, 0 };
  kpOpen(f, editIdx, apKeypadCommit);
}

// Public: routed here for any content-area tap while the AP screen is active.
void apScreenTouch(uint16_t x, uint16_t y) {
  if (kpIsOpen()) { kpTouch(x, y); return; }

  // ARM / DISARM
  if (x >= AP_ARM_X && x < AP_ARM_X + AP_ARM_W && y >= AP_ARM_Y && y < AP_ARM_Y + AP_ARM_H) {
    // Toggle from the COMMANDED state, so a second tap while one is in flight reverses
    // that command instead of re-sending it, and a lost echo cannot lock the button.
    bool wantArm = !apArmCommanded();
    // The pending cue is raised only if a command really went out. In demo mode (and on
    // unit 1, where the command channel is compiled out) nothing is queued and nothing
    // will ever acknowledge it, so a cue raised here would sit on the button forever.
    if (apEnqueueCmd(wantArm ? AP_CMD_ARM : AP_CMD_DISARM, 0.0f)) {
      apArmOvr = wantArm ? 1 : 0;
      rowCache[screen_LNCHAP][AP_ARM_SLOT].value = "\x01";   // force ARM + banner redraw
      rowCache[screen_LNCHAP][AP_BODY_SLOT].value = "\x01";
    }
    return;
  }
  // Editable fields
  for (uint8_t i = 0; i < 6; i++) {
    const ApEdit &e = AP_EDITS[i];
    int16_t cx = AP_CELLS[e.slot].x, cy = apRowY(AP_CELLS[e.slot].row);
    if (x >= cx && x < cx + AP_COLW && y >= cy && y < cy + AP_ROW_H) {
      if (e.kind == 1) {                             // Launch N/S toggle
        apDir = !apGDir();
        apDDir = apEnqueueCmd(AP_CMD_SET_LAUNCH_DIR, apDir ? 1.0f : 0.0f);
        rowCache[screen_LNCHAP][e.slot].value = "\x01";
      } else {
        apOpenKeypad(i);
      }
      return;
    }
  }
}

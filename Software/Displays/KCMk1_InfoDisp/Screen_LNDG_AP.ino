/***************************************************************************************
   Screen_LNDG_AP.ino — Landing Autopilot console (chrome + draw + touch)

   Touch console for the landing autopilot on Controller_Main: DESC (descent-rate hold),
   HOVR (radar-altitude hold), BRAKE (armed suicide burn) and ENTRY (re-entry attitude
   hold). Renders the 44-byte landing status frame (state.ld*). Design:
   Mission_Autopilot.md §5, §7.2.

   LAYOUT (the shared console grid — ConsoleShared.ino):
     banner: <engaged modes | A/P OFF>  <IGN IN m:ss / FIRING / reason>  body · vessel  <ENGAGED>
     DESCENT             | OPTIONS              | DESCENT DATA
     [DESC]  [rate]      | ATT REF [RETRO/RADIAL]| RDR ALT
     [HOVR]  [altitude]  | TWR     [MEAS|value] | V/S
     [BRAKE] ign alt     | MARGIN  [m]          | H SPD
     [ENTRY] [AOA]       | ROLL    [entry roll] | ACCEL (+ source)
     hint                |                      | IGN ALT (orange when marginal)
     legend              | [A/P OFF]            | T-IMP
                         |                      | THRTL
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

static const uint16_t LD_LBL = KDC_LABEL_COLOR;
static const uint16_t LD_VAL = TFT_DARK_GREEN;
static const uint16_t LD_EDT = TFT_SKY;
#define LD_F_LBL  (&Roboto_Black_24)
#define LD_F_BANN (&Roboto_Black_36)

// Modes: 0 DESC, 1 HOVR, 2 BRAKE (ldMode 1..3), 3 ENTRY (ldEntry).
static const uint8_t LD_MODES = 4;
static const char *const LD_MODE_LBL[LD_MODES]  = { "DESC", "HOVR", "BRAKE", "ENTRY" };
static const uint8_t     LD_ENGAGE_OP[LD_MODES] = { LD_CMD_ENGAGE_DESC, LD_CMD_ENGAGE_HOVR, LD_CMD_ENGAGE_BRAKE, LD_CMD_ENGAGE_ENTRY };
// Options: 0 ATT REF (toggle), 1 TWR, 2 MARGIN, 3 ROLL
static const char *const LD_OPT_LBL[4] = { "ATT REF", "TWR", "MARGIN", "ROLL" };
static const uint8_t     LD_OPT_OP[4]  = { LD_CMD_SET_ATT_REF, LD_CMD_SET_TWR, LD_CMD_SET_MARGIN, LD_CMD_SET_ENTRY_ROLL };

static const uint8_t LD_SLOT_BTN  = 0;    // 0..3
static const uint8_t LD_SLOT_VAL  = 4;    // 4..7  rate / alt / ign alt / aoa
static const uint8_t LD_SLOT_OPT  = 8;    // 8..11
static const uint8_t LD_SLOT_DATA = 12;   // 12..18
static const uint8_t LD_SLOT_BANN = 19;

static const uint32_t LD_PENDING_TIMEOUT_MS = 3000;

static int8_t   ldPend[LD_MODES];  static uint32_t ldPendMs[LD_MODES];
static bool     ldDirty[LD_MODES]; static float ldPendVal[LD_MODES];      // rate, alt, (none), aoa
static bool     ldOptDirty[4];     static float ldOptPend[4];
static int8_t   ldPendRef = -1;    static uint32_t ldPendRefMs = 0;
static bool     ldScreenRefresh = false;

static bool ldEngaged(uint8_t i)   { return i < 3 ? (state.ldMode == (uint8_t)(i + 1)) : (state.ldEntry != 0); }
static bool ldCommanded(uint8_t i) { return ldPend[i] >= 0 ? (ldPend[i] == 1) : ldEngaged(i); }
static bool ldRadial()             { return (state.ldFlags & 0x08) != 0; }
static float ldEcho(uint8_t i)     { return i == 0 ? state.ldDescRate : i == 1 ? state.ldHovrAlt : state.ldEntryAoa; }
static float ldOptEcho(uint8_t i)  { return i == 1 ? state.ldTwr : i == 2 ? state.ldMargin : state.ldEntryRoll; }

static void ldReconcile() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < LD_MODES; i++) {
    if (ldPend[i] >= 0 && (ldEngaged(i) == (ldPend[i] == 1) || now - ldPendMs[i] > LD_PENDING_TIMEOUT_MS)) ldPend[i] = -1;
    if (i != 2 && ldDirty[i] && fabsf(ldEcho(i) - ldPendVal[i]) < 0.1f) ldDirty[i] = false;
  }
  for (uint8_t i = 1; i < 4; i++) if (ldOptDirty[i] && fabsf(ldOptEcho(i) - ldOptPend[i]) < 0.05f) ldOptDirty[i] = false;
  if (ldPendRef >= 0 && (ldRadial() == (ldPendRef == 1) || now - ldPendRefMs > LD_PENDING_TIMEOUT_MS)) ldPendRef = -1;
}

static float ldHorizontalSpeed() {
  float h2 = state.surfaceVel * state.surfaceVel - state.verticalVel * state.verticalVel;
  return h2 > 0.0f ? sqrtf(h2) : 0.0f;
}
static void ldFmtTime(float s, char *buf, size_t n) {
  if (s < 0.0f || s > 35999.0f) { snprintf(buf, n, "---"); return; }
  uint32_t t = (uint32_t)(s + 0.5f);
  snprintf(buf, n, "%lu:%02lu", (unsigned long)(t / 60), (unsigned long)(t % 60));
}

// ── Chrome ───────────────────────────────────────────────────────────────────────────
void chromeScreen_LNDGAP(KCM_TFT &tft) {
  kpForceClose();
  conDrawColumns(tft, "DESCENT", "OPTIONS", "DESCENT DATA");
  for (uint8_t i = 0; i < LD_MODES; i++) conDrawValueBox(tft, CON_C1X, i);
  for (uint8_t i = 0; i < 4; i++) {
    textLeft(tft, LD_F_LBL, CON_C2X + 4, conRowY(i), CON_COLW - CON_VALW, CON_ROW_H, LD_OPT_LBL[i], LD_LBL, TFT_BLACK);
    if (i > 0) conDrawValueBox(tft, CON_C2X, i);
  }
  static const char *const DATA_LBL[7] = { "RDR ALT", "V/S", "H SPD", "ACCEL", "IGN ALT", "T-IMP", "THRTL" };
  for (uint8_t r = 0; r < 7; r++)
    textLeft(tft, LD_F_LBL, CON_C3X + 4, conRowY(r), CON_COLW - CON_VALW, CON_ROW_H, DATA_LBL[r], LD_LBL, TFT_BLACK);
  textLeft(tft, &Roboto_Black_16, CON_C1X + 4, conRowY(4) + 8,  CON_COLW - 8, 20, "BRAKE fires at IGN altitude, then", TFT_DARK_GREY, TFT_BLACK);
  textLeft(tft, &Roboto_Black_16, CON_C1X + 4, conRowY(4) + 28, CON_COLW - 8, 20, "hands the descent to DESC at its rate.", TFT_DARK_GREY, TFT_BLACK);
  int16_t ly = conRowY(5) + 12, lx = CON_C1X + 2;
  tft.drawRect(lx, ly, 92, 28, TFT_GREY);             textCenter(tft, &Roboto_Black_16, lx, ly, 92, 28, "OFF", TFT_WHITE, TFT_BLACK);
  tft.drawRect(lx + 100, ly, 92, 28, TFT_CYAN);       textCenter(tft, &Roboto_Black_16, lx + 100, ly, 92, 28, "PENDING", TFT_CYAN, TFT_BLACK);
  tft.fillRect(lx + 200, ly, 92, 28, TFT_DARK_GREEN); textCenter(tft, &Roboto_Black_16, lx + 200, ly, 92, 28, "ENGAGED", TFT_WHITE, TFT_DARK_GREEN);
  ButtonLabel b = { "A/P OFF", TFT_WHITE, TFT_WHITE, TFT_OFF_BLACK, TFT_OFF_BLACK, TFT_GREY, TFT_GREY };
  drawButton(tft, CON_C2X, CON_BIG_Y, CON_COLW, CON_BIG_H, b, &Roboto_Black_28, false);
  textCenter(tft, &Roboto_Black_16, CON_C2X, CON_BIG_Y + CON_BIG_H - 26, CON_COLW, 18, "disconnect all, throttle stays as set", TFT_GREY, TFT_OFF_BLACK);
}

// ── Draw ─────────────────────────────────────────────────────────────────────────────
void drawScreen_LNDGAP(KCM_TFT &tft) {
  if (kpTakeClosed()) ldScreenRefresh = true;
  if (ldScreenRefresh) {
    tft.fillRect(0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP, TFT_BLACK);
    chromeScreen_LNDGAP(tft);
    invalidateRowCache(screen_LNDGAP);
    ldScreenRefresh = false;
  }
  if (kpIsOpen()) { kpDraw(tft); return; }

  char buf[48];
  bool anyPending = ldPendRef >= 0;
  for (uint8_t i = 0; i < LD_MODES; i++) if (ldPend[i] >= 0) anyPending = true;
  bool brakeArmed = (state.ldFlags & 0x02) != 0, firing = (state.ldFlags & 0x04) != 0, marginal = (state.ldFlags & 0x40) != 0;
  float tImpact = (state.verticalVel < -0.1f) ? state.radarAlt / -state.verticalVel : -1.0f;
  float tIgn = (brakeArmed && state.verticalVel < -0.1f) ? (state.radarAlt - state.ldIgnAlt) / -state.verticalVel : -1.0f;

  // Banner
  {
    char modes[40] = "";
    for (uint8_t i = 0; i < LD_MODES; i++)
      if (ldEngaged(i)) { if (modes[0]) strlcat(modes, "  ", sizeof(modes)); strlcat(modes, LD_MODE_LBL[i], sizeof(modes)); }
    bool engaged = modes[0] != '\0';
    bool showReason = (state.ldReason != HP_REASON_NONE && state.ldReasonAge <= 5);
    char note[24] = "";
    if (showReason) strlcpy(note, hpReasonLabel(state.ldReason), sizeof(note));
    else if (firing) strlcpy(note, "FIRING", sizeof(note));
    else if (brakeArmed && tIgn >= 0.0f) { char t[12]; ldFmtTime(tIgn, t, sizeof(t)); snprintf(note, sizeof(note), "IGN IN %s", t); }
    char key[112];
    snprintf(key, sizeof(key), "%s|%s|%s|%d|%s", engaged ? modes : "A/P OFF", note, state.gameSOI.c_str(), anyPending, state.vesselName.c_str());
    RowCache &rc = rowCache[screen_LNDGAP][LD_SLOT_BANN];
    if (rc.value != key) {
      tft.fillRect(0, CON_BANNER_Y, CONTENT_W, CON_BANNER_H, TFT_BLACK);
      textLeft(tft, LD_F_BANN, 8, CON_BANNER_Y, 316, CON_BANNER_H, engaged ? modes : "A/P OFF", engaged ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);
      if (note[0]) textLeft(tft, &Roboto_Black_24, 330, CON_BANNER_Y, 300, CON_BANNER_H, note, TFT_ORANGE, TFT_BLACK);
      snprintf(buf, sizeof(buf), "%s \xB7 %s", state.gameSOI.c_str(), state.vesselName.c_str());
      textRight(tft, &Roboto_Black_20, 648, CON_BANNER_Y + 4,  CONTENT_W - 648 - 6, 24, buf, LD_LBL, TFT_BLACK);
      snprintf(buf, sizeof(buf), "%s%s", engaged ? "ENGAGED" : "A/P OFF", anyPending ? "..." : "");
      textRight(tft, &Roboto_Black_28, 648, CON_BANNER_Y + 30, CONTENT_W - 648 - 6, 30, buf, engaged ? TFT_NEON_GREEN : TFT_DARK_GREY, TFT_BLACK);
      rc.value = key;
    }
  }

  // Mode buttons + boxes
  int16_t vx = conValX(CON_C1X) + 6, vw = CON_VALW - 10;
  for (uint8_t i = 0; i < LD_MODES; i++) {
    ConBtnState st = ldEngaged(i) ? CON_BTN_ON : (ldPend[i] >= 0 ? CON_BTN_PENDING : CON_BTN_OFF);
    RowCache &rc = rowCache[screen_LNDGAP][LD_SLOT_BTN + i];
    char k[4]; snprintf(k, sizeof(k), "%d", (int)st);
    if (rc.value != k) { conDrawModeButton(tft, CON_C1X + 2, conRowY(i) + 4, CON_BTN_W, CON_VALH, LD_MODE_LBL[i], st); rc.value = k; }
    uint16_t fg;
    switch (i) {
      case 0: snprintf(buf, sizeof(buf), "%+.1f m/s", ldDirty[0] ? ldPendVal[0] : state.ldDescRate); fg = ldDirty[0] ? LD_EDT : (ldEngaged(0) ? LD_VAL : TFT_DARK_GREY); break;
      case 1: formatAltBuf(ldDirty[1] ? ldPendVal[1] : state.ldHovrAlt, buf, sizeof(buf)); fg = ldDirty[1] ? LD_EDT : (ldEngaged(1) ? LD_VAL : TFT_DARK_GREY); break;
      case 2: if (state.ldIgnAlt > 0.0f) formatAltBuf(state.ldIgnAlt, buf, sizeof(buf)); else snprintf(buf, sizeof(buf), "---");
              fg = marginal ? TFT_ORANGE : (ldEngaged(2) ? LD_VAL : TFT_DARK_GREY); break;
      default: snprintf(buf, sizeof(buf), "AOA %.0f\xB0", ldDirty[3] ? ldPendVal[3] : state.ldEntryAoa); fg = ldDirty[3] ? LD_EDT : (ldEngaged(3) ? LD_VAL : TFT_DARK_GREY); break;
    }
    conPut(tft, screen_LNDGAP, LD_SLOT_VAL + i, vx, conRowY(i) + 4, vw, CON_VALH, buf, fg);
  }

  // Options
  {
    bool radial = ldPendRef >= 0 ? (ldPendRef == 1) : ldRadial();
    RowCache &rc = rowCache[screen_LNDGAP][LD_SLOT_OPT];
    char k[8]; snprintf(k, sizeof(k), "%d%d", radial, ldPendRef >= 0);
    if (rc.value != k) {
      int16_t x = conValX(CON_C2X), y = conRowY(0) + 4, w = CON_VALW - 2, h = CON_VALH;
      tft.fillRect(x, y, w, h, TFT_DARK_GREEN);
      tft.drawRect(x, y, w, h, ldPendRef >= 0 ? TFT_CYAN : TFT_GREY);
      textCenter(tft, &Roboto_Black_24, x, y, w, h, radial ? "RADIAL" : "RETRO", ldPendRef >= 0 ? TFT_CYAN : TFT_WHITE, TFT_DARK_GREEN);
      rc.value = k;
    }
  }
  int16_t ox = conValX(CON_C2X) + 6;
  { float twr = ldOptDirty[1] ? ldOptPend[1] : state.ldTwr;
    if (twr > 0.0f) snprintf(buf, sizeof(buf), "%.2f", twr); else snprintf(buf, sizeof(buf), state.ldAccelSource == 1 ? "MEAS" : "EST");
    conPut(tft, screen_LNDGAP, LD_SLOT_OPT + 1, ox, conRowY(1) + 4, vw, CON_VALH, buf, ldOptDirty[1] ? LD_EDT : LD_VAL); }
  snprintf(buf, sizeof(buf), "%.0f m", ldOptDirty[2] ? ldOptPend[2] : state.ldMargin);
  conPut(tft, screen_LNDGAP, LD_SLOT_OPT + 2, ox, conRowY(2) + 4, vw, CON_VALH, buf, ldOptDirty[2] ? LD_EDT : LD_VAL);
  snprintf(buf, sizeof(buf), "%.0f\xB0", ldOptDirty[3] ? ldOptPend[3] : state.ldEntryRoll);
  conPut(tft, screen_LNDGAP, LD_SLOT_OPT + 3, ox, conRowY(3) + 4, vw, CON_VALH, buf, ldOptDirty[3] ? LD_EDT : LD_VAL);

  // Descent data
  int16_t dx = conValX(CON_C3X) + 6;
  formatAltBuf(state.radarAlt, buf, sizeof(buf));               conPut(tft, screen_LNDGAP, LD_SLOT_DATA + 0, dx, conRowY(0) + 4, vw, CON_VALH, buf, LD_VAL);
  snprintf(buf, sizeof(buf), "%+.0f m/s", state.verticalVel);   conPut(tft, screen_LNDGAP, LD_SLOT_DATA + 1, dx, conRowY(1) + 4, vw, CON_VALH, buf, (tImpact >= 0.0f && tImpact < 30.0f) ? TFT_ORANGE : LD_VAL);
  snprintf(buf, sizeof(buf), "%.0f m/s", ldHorizontalSpeed());  conPut(tft, screen_LNDGAP, LD_SLOT_DATA + 2, dx, conRowY(2) + 4, vw, CON_VALH, buf, LD_VAL);
  { static const char *const SRC[3] = { "EST", "MEAS", "TWR" };
    if (state.ldAccelEst > 0.0f) snprintf(buf, sizeof(buf), "%.1f %s", state.ldAccelEst, SRC[state.ldAccelSource < 3 ? state.ldAccelSource : 0]); else snprintf(buf, sizeof(buf), "---");
    conPut(tft, screen_LNDGAP, LD_SLOT_DATA + 3, dx, conRowY(3) + 4, vw, CON_VALH, buf, state.ldAccelEst > 0.0f ? LD_VAL : TFT_DARK_GREY); }
  if (state.ldIgnAlt > 0.0f) formatAltBuf(state.ldIgnAlt, buf, sizeof(buf)); else snprintf(buf, sizeof(buf), "---");
  conPut(tft, screen_LNDGAP, LD_SLOT_DATA + 4, dx, conRowY(4) + 4, vw, CON_VALH, buf, marginal ? TFT_ORANGE : (state.ldIgnAlt > 0.0f ? LD_VAL : TFT_DARK_GREY));
  ldFmtTime(tImpact, buf, sizeof(buf));                          conPut(tft, screen_LNDGAP, LD_SLOT_DATA + 5, dx, conRowY(5) + 4, vw, CON_VALH, buf, (tImpact >= 0.0f && tImpact < 30.0f) ? TFT_ORANGE : (tImpact >= 0.0f ? LD_VAL : TFT_DARK_GREY));
  snprintf(buf, sizeof(buf), "%.0f %%", state.ldCmdThrottle * 100.0f);
  conPut(tft, screen_LNDGAP, LD_SLOT_DATA + 6, dx, conRowY(6) + 4, vw, CON_VALH, buf, state.ldMode ? LD_VAL : TFT_GREY);
}

// ── Keypad commits ───────────────────────────────────────────────────────────────────
static void ldModeCommit(int8_t idx, float v, bool off) {
  (void)off;
  if (idx < 0 || idx >= LD_MODES || idx == 2) return;
  ldPendVal[idx] = v;
  uint8_t op = idx == 0 ? LD_CMD_SET_DESC_RATE : idx == 1 ? LD_CMD_SET_HOVR_ALT : LD_CMD_SET_ENTRY_AOA;
  ldDirty[idx] = apEnqueueCmd(op, v);
}
static void ldOptCommit(int8_t idx, float v, bool off) {
  if (idx < 1 || idx > 3) return;
  if (off && idx == 1) v = 0.0f;              // TWR OFF = measured / estimated
  ldOptPend[idx] = v;
  ldOptDirty[idx] = apEnqueueCmd(LD_OPT_OP[idx], v);
}

// ── Touch ────────────────────────────────────────────────────────────────────────────
void lndgApScreenTouch(uint16_t x, uint16_t y) {
  if (kpIsOpen()) { kpTouch(x, y); return; }
  uint32_t now = millis();

  if (y >= CON_BIG_Y && y < CON_BIG_Y + CON_BIG_H && x >= CON_C2X && x < CON_C2X + CON_COLW) {
    if (apEnqueueCmd(HP_CMD_AP_OFF, 0.0f))
      for (uint8_t i = 0; i < LD_MODES; i++) if (ldEngaged(i)) { ldPend[i] = 0; ldPendMs[i] = now; }
    return;
  }

  for (uint8_t r = 0; r < 4; r++) {
    int16_t cy = conRowY(r);
    if (y < cy || y >= cy + CON_ROW_H) continue;
    if (x >= CON_C1X + 2 && x < CON_C1X + 2 + CON_BTN_W) {
      bool want = !ldCommanded(r);
      if (apEnqueueCmd(LD_ENGAGE_OP[r], want ? 1.0f : 0.0f)) { ldPend[r] = want ? 1 : 0; ldPendMs[r] = now; }
      return;
    }
    if (x >= conValX(CON_C1X) && x < conValX(CON_C1X) + CON_VALW && r != 2) {
      KpField f = (r == 0) ? KpField{ "DESCENT RATE", "m/s", -50.0f, 20.0f, false, true, 1 }
                : (r == 1) ? KpField{ "HOVER ALTITUDE (AGL)", "m", 2.0f, 5000.0f, false, false, 0 }
                           : KpField{ "ENTRY ANGLE OF ATTACK", "\xB0", -30.0f, 40.0f, false, true, 0 };
      kpOpen(f, (int8_t)r, ldModeCommit);
      return;
    }
    if (x >= conValX(CON_C2X) && x < conValX(CON_C2X) + CON_VALW) {
      if (r == 0) {
        bool want = !(ldPendRef >= 0 ? (ldPendRef == 1) : ldRadial());
        if (apEnqueueCmd(LD_CMD_SET_ATT_REF, want ? 1.0f : 0.0f)) { ldPendRef = want ? 1 : 0; ldPendRefMs = now; }
        return;
      }
      KpField f = (r == 1) ? KpField{ "TWR OVERRIDE (OFF = MEAS)", "", 0.0f, 20.0f, true, false, 2 }
                : (r == 2) ? KpField{ "IGNITION MARGIN", "m", 0.0f, 5000.0f, false, false, 0 }
                           : KpField{ "ENTRY ROLL", "\xB0", -180.0f, 180.0f, false, true, 0 };
      kpOpen(f, (int8_t)r, ldOptCommit);
      return;
    }
  }
}

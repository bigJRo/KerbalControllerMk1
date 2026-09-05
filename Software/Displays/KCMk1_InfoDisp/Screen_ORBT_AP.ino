/***************************************************************************************
   Screen_ORBT_AP.ino — Orbital Autopilot console (chrome + draw + touch)

   Touch console for the burn autopilot on Controller_Main: maneuver node executor,
   apoapsis / periapsis burns, plane change and the rendezvous approach-rate hold.
   Renders the 52-byte orbital status frame (state.ob*). Design: Mission_Autopilot.md §4.

   LAYOUT (the shared console grid — ConsoleShared.ino):
     banner: <armed mode | A/P OFF>  <phase / reason>   body · vessel  <ARMED / EXECUTING / A/P OFF>
     BURN               | TARGET / OPTIONS     | PLAN
     [NODE] node ΔV     | [APPR] [rate]        | ΔV TOT
     [AP]   [target Ap] | HOLD AT [distance]   | ΔV REM
     [PE]   [target Pe] | WARP    [AUTO/OFF]   | T-IGN
     [INC]  [target i]  | STAGE   [AUTO/OFF]   | BURN
     hint               |                      | ACCEL
     legend             | [EXEC/WARP] [A/P OFF]| STG ΔV
                        |                      | RANGE

   Burns are two-step: arming a mode plans it and fills the PLAN column; EXEC starts
   the executor. Once aligned with WARP on, the button reads WARP and a second tap
   warps to ignition minus the lead — the executor never warps unasked. A/P OFF aborts.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

static const uint16_t OB_LBL = KDC_LABEL_COLOR;
static const uint16_t OB_VAL = TFT_DARK_GREEN;
static const uint16_t OB_EDT = TFT_SKY;
#define OB_F_LBL  (&Roboto_Black_24)
#define OB_F_BANN (&Roboto_Black_36)

// Burn modes: 0 NODE, 1 AP, 2 PE, 3 INC (state.obMode 1..4). Approach is mode 4 here.
static const uint8_t OB_BURNS = 4;
static const char *const OB_BURN_LBL[OB_BURNS] = { "NODE", "AP", "PE", "INC" };
static const uint8_t     OB_ARM_OP[OB_BURNS]   = { OB_CMD_ARM_NODE, OB_CMD_ARM_AP, OB_CMD_ARM_PE, OB_CMD_ARM_INC };
static const uint8_t     OB_SET_OP[OB_BURNS]   = { 0, OB_CMD_SET_AP, OB_CMD_SET_PE, OB_CMD_SET_INC };

// Row-cache slots
static const uint8_t OB_SLOT_BTN   = 0;    // 0..3 burn buttons, 4 APPR
static const uint8_t OB_SLOT_VAL   = 5;    // 5..8 burn boxes, 9 appr rate, 10 hold at, 11 warp, 12 stage
static const uint8_t OB_SLOT_PLAN  = 13;   // 13..19
static const uint8_t OB_SLOT_EXEC  = 20;
static const uint8_t OB_SLOT_BANN  = 21;

static const int16_t OB_BIG_W = 145;
static const int16_t OB_EXEC_X = CON_C2X;
static const int16_t OB_OFF_X  = CON_C2X + 153;

static const uint32_t OB_PENDING_TIMEOUT_MS = 3000;

static int8_t   obPendArm[OB_BURNS];   // -1 none, 0 disarm sent, 1 arm sent
static uint32_t obPendArmMs[OB_BURNS];
static bool     obDirtyTgt[OB_BURNS];  static float obPendTgt[OB_BURNS];
static int8_t   obPendAppr = -1;       static uint32_t obPendApprMs = 0;
static bool     obDirtyRate = false, obDirtyDist = false; static float obPendRate = 0.0f, obPendDist = 0.0f;
static int8_t   obPendWarp = -1, obPendStage = -1; static uint32_t obPendWarpMs = 0, obPendStageMs = 0;
static uint8_t  obExecPendPhase = 0;   static uint32_t obExecPendMs = 0;   // phase we expect after EXEC
static bool     obScreenRefresh = false;

static bool obArmedMode(uint8_t i)  { return state.obMode == (uint8_t)(i + 1) && (state.obFlags & 0x01); }
static bool obApprEngaged()         { return (state.obFlags & 0x40) != 0; }
static bool obWarpAuto()            { return (state.obFlags & 0x04) != 0; }
static bool obStageAuto()           { return (state.obFlags & 0x08) != 0; }
static float obTgtEcho(uint8_t i)   { return i == 1 ? state.obTargetAp : i == 2 ? state.obTargetPe : state.obTargetInc; }

const char *obPhaseName(uint8_t p) {
  switch (p) {
    case 1: return "PLANNED"; case 2: return "ALIGNING"; case 3: return "WARP READY"; case 4: return "WARPING";
    case 5: return "BURNING"; case 6: return "DONE"; case 7: return "ABORT"; default: return "";
  }
}

static void obReconcile() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < OB_BURNS; i++) {
    if (obPendArm[i] >= 0 && (obArmedMode(i) == (obPendArm[i] == 1) || now - obPendArmMs[i] > OB_PENDING_TIMEOUT_MS)) obPendArm[i] = -1;
    if (obDirtyTgt[i] && fabsf(obTgtEcho(i) - obPendTgt[i]) < (i == 3 ? 0.05f : 0.5f)) obDirtyTgt[i] = false;
  }
  if (obPendAppr >= 0 && (obApprEngaged() == (obPendAppr == 1) || now - obPendApprMs > OB_PENDING_TIMEOUT_MS)) obPendAppr = -1;
  if (obDirtyRate && fabsf(state.obApprRate - obPendRate) < 0.05f) obDirtyRate = false;
  if (obDirtyDist && fabsf(state.obApprDist - obPendDist) < 0.5f) obDirtyDist = false;
  if (obPendWarp >= 0  && (obWarpAuto()  == (obPendWarp == 1)  || now - obPendWarpMs  > OB_PENDING_TIMEOUT_MS)) obPendWarp = -1;
  if (obPendStage >= 0 && (obStageAuto() == (obPendStage == 1) || now - obPendStageMs > OB_PENDING_TIMEOUT_MS)) obPendStage = -1;
  if (obExecPendPhase && (state.obPhase >= obExecPendPhase || state.obPhase == 7 || now - obExecPendMs > OB_PENDING_TIMEOUT_MS)) obExecPendPhase = 0;
}

static void obFmtTime(float s, char *buf, size_t n) {
  bool neg = s < 0.0f; if (neg) s = -s;
  uint32_t t = (uint32_t)(s + 0.5f);
  if (t >= 3600) snprintf(buf, n, "%s%lu:%02lu:%02lu", neg ? "-" : "", (unsigned long)(t / 3600), (unsigned long)((t / 60) % 60), (unsigned long)(t % 60));
  else           snprintf(buf, n, "%s%lu:%02lu", neg ? "-" : "", (unsigned long)(t / 60), (unsigned long)(t % 60));
}

// ── Chrome ───────────────────────────────────────────────────────────────────────────
void chromeScreen_ORBTAP(KCM_TFT &tft) {
  kpForceClose();
  conDrawColumns(tft, "BURN", "TARGET / OPTIONS", "PLAN");
  for (uint8_t i = 0; i < OB_BURNS; i++) conDrawValueBox(tft, CON_C1X, i);
  conDrawValueBox(tft, CON_C2X, 0);
  textLeft(tft, OB_F_LBL, CON_C2X + 4, conRowY(1), CON_COLW - CON_VALW, CON_ROW_H, "HOLD AT", OB_LBL, TFT_BLACK);
  conDrawValueBox(tft, CON_C2X, 1);
  textLeft(tft, OB_F_LBL, CON_C2X + 4, conRowY(2), CON_COLW - CON_VALW, CON_ROW_H, "WARP", OB_LBL, TFT_BLACK);
  textLeft(tft, OB_F_LBL, CON_C2X + 4, conRowY(3), CON_COLW - CON_VALW, CON_ROW_H, "STAGE", OB_LBL, TFT_BLACK);
  static const char *const PLAN_LBL[7] = { "\x94V TOT", "\x94V REM", "T-IGN", "BURN", "ACCEL", "STG \x94V", "RANGE" };
  for (uint8_t r = 0; r < 7; r++)
    textLeft(tft, OB_F_LBL, CON_C3X + 4, conRowY(r), CON_COLW - CON_VALW, CON_ROW_H, PLAN_LBL[r], OB_LBL, TFT_BLACK);
  textLeft(tft, &Roboto_Black_16, CON_C1X + 4, conRowY(4) + 8,  CON_COLW - 8, 20, "Arm one burn, review the PLAN column,", TFT_DARK_GREY, TFT_BLACK);
  textLeft(tft, &Roboto_Black_16, CON_C1X + 4, conRowY(4) + 28, CON_COLW - 8, 20, "then EXEC. A/P OFF aborts at any phase.", TFT_DARK_GREY, TFT_BLACK);
  int16_t ly = conRowY(5) + 12, lx = CON_C1X + 2;
  tft.drawRect(lx, ly, 92, 28, TFT_GREY);             textCenter(tft, &Roboto_Black_16, lx, ly, 92, 28, "OFF", TFT_WHITE, TFT_BLACK);
  tft.drawRect(lx + 100, ly, 92, 28, TFT_CYAN);       textCenter(tft, &Roboto_Black_16, lx + 100, ly, 92, 28, "PENDING", TFT_CYAN, TFT_BLACK);
  tft.fillRect(lx + 200, ly, 92, 28, TFT_DARK_GREEN); textCenter(tft, &Roboto_Black_16, lx + 200, ly, 92, 28, "ENGAGED", TFT_WHITE, TFT_DARK_GREEN);
  ButtonLabel b = { "A/P OFF", TFT_WHITE, TFT_WHITE, TFT_OFF_BLACK, TFT_OFF_BLACK, TFT_GREY, TFT_GREY };
  drawButton(tft, OB_OFF_X, CON_BIG_Y, OB_BIG_W, CON_BIG_H, b, &Roboto_Black_28, false);
  textCenter(tft, &Roboto_Black_16, OB_OFF_X, CON_BIG_Y + CON_BIG_H - 26, OB_BIG_W, 18, "abort / disarm", TFT_GREY, TFT_OFF_BLACK);
}

// ── Draw ─────────────────────────────────────────────────────────────────────────────
static void obToggleBox(KCM_TFT &tft, uint8_t slot, uint8_t row, bool on, bool pending) {
  RowCache &rc = rowCache[screen_ORBTAP][slot];
  char k[8]; snprintf(k, sizeof(k), "%d%d", on, pending);
  if (rc.value == k) return;
  int16_t x = conValX(CON_C2X), y = conRowY(row) + 4, w = CON_VALW - 2, h = CON_VALH;
  tft.fillRect(x, y, w, h, on ? TFT_DARK_GREEN : TFT_BLACK);
  tft.drawRect(x, y, w, h, pending ? TFT_CYAN : TFT_GREY);
  textCenter(tft, &Roboto_Black_24, x, y, w, h, on ? "AUTO" : "OFF", pending ? TFT_CYAN : TFT_WHITE, on ? TFT_DARK_GREEN : TFT_BLACK);
  rc.value = k;
}

void drawScreen_ORBTAP(KCM_TFT &tft) {
  if (kpTakeClosed()) obScreenRefresh = true;
  if (obScreenRefresh) {
    tft.fillRect(0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP, TFT_BLACK);
    chromeScreen_ORBTAP(tft);
    invalidateRowCache(screen_ORBTAP);
    obScreenRefresh = false;
  }
  if (kpIsOpen()) { kpDraw(tft); return; }

  char buf[48];
  bool armed = (state.obFlags & 0x01) != 0, executing = (state.obFlags & 0x02) != 0;
  bool anyPending = obPendAppr >= 0 || obExecPendPhase != 0;
  for (uint8_t i = 0; i < OB_BURNS; i++) if (obPendArm[i] >= 0) anyPending = true;

  // Banner
  {
    const char *modeName = armed ? OB_BURN_LBL[state.obMode ? state.obMode - 1 : 0] : (obApprEngaged() ? "APPR" : "A/P OFF");
    bool engaged = armed || obApprEngaged();
    bool showReason = (state.obReason != HP_REASON_NONE && state.obReasonAge <= 5);
    const char *note = showReason ? hpReasonLabel(state.obReason) : obPhaseName(state.obPhase);
    char key[112];
    snprintf(key, sizeof(key), "%s|%s|%s|%d|%d|%s", modeName, note, state.gameSOI.c_str(), anyPending, executing, state.vesselName.c_str());
    RowCache &rc = rowCache[screen_ORBTAP][OB_SLOT_BANN];
    if (rc.value != key) {
      tft.fillRect(0, CON_BANNER_Y, CONTENT_W, CON_BANNER_H, TFT_BLACK);
      textLeft(tft, OB_F_BANN, 8, CON_BANNER_Y, 316, CON_BANNER_H, modeName, engaged ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);
      if (note[0]) textLeft(tft, &Roboto_Black_24, 330, CON_BANNER_Y, 300, CON_BANNER_H, note, showReason ? TFT_ORANGE : (state.obPhase == 6 ? TFT_SKY : TFT_ORANGE), TFT_BLACK);
      snprintf(buf, sizeof(buf), "%s \xB7 %s", state.gameSOI.c_str(), state.vesselName.c_str());
      textRight(tft, &Roboto_Black_20, 648, CON_BANNER_Y + 4,  CONTENT_W - 648 - 6, 24, buf, OB_LBL, TFT_BLACK);
      snprintf(buf, sizeof(buf), "%s%s", executing ? "EXECUTING" : engaged ? (armed ? "ARMED" : "ENGAGED") : "A/P OFF", anyPending ? "..." : "");
      textRight(tft, &Roboto_Black_28, 648, CON_BANNER_Y + 30, CONTENT_W - 648 - 6, 30, buf, engaged ? TFT_NEON_GREEN : TFT_DARK_GREY, TFT_BLACK);
      rc.value = key;
    }
  }

  // Burn buttons + boxes
  int16_t vx = conValX(CON_C1X) + 6, vw = CON_VALW - 10;
  for (uint8_t i = 0; i < OB_BURNS; i++) {
    ConBtnState st = obArmedMode(i) ? CON_BTN_ON : (obPendArm[i] >= 0 ? CON_BTN_PENDING : CON_BTN_OFF);
    RowCache &rc = rowCache[screen_ORBTAP][OB_SLOT_BTN + i];
    char k[4]; snprintf(k, sizeof(k), "%d", (int)st);
    if (rc.value != k) { conDrawModeButton(tft, CON_C1X + 2, conRowY(i) + 4, CON_BTN_W, CON_VALH, OB_BURN_LBL[i], st); rc.value = k; }
    uint16_t fg;
    if (i == 0) {
      bool node = (state.obFlags & 0x20) != 0 || state.mnvrDeltaV > 0.01f;
      if (node) snprintf(buf, sizeof(buf), "%.0f m/s", state.mnvrDeltaV); else snprintf(buf, sizeof(buf), "NO NODE");
      fg = obArmedMode(0) ? OB_VAL : TFT_DARK_GREY;
    } else {
      float v = obDirtyTgt[i] ? obPendTgt[i] : obTgtEcho(i);
      if (i == 3) snprintf(buf, sizeof(buf), "%.1f\xB0", v); else formatAltBuf(v, buf, sizeof(buf));
      fg = obDirtyTgt[i] ? OB_EDT : (obArmedMode(i) ? OB_VAL : TFT_DARK_GREY);
    }
    conPut(tft, screen_ORBTAP, OB_SLOT_VAL + i, vx, conRowY(i) + 4, vw, CON_VALH, buf, fg);
  }

  // APPR + options
  {
    ConBtnState st = obApprEngaged() ? CON_BTN_ON : (obPendAppr >= 0 ? CON_BTN_PENDING : CON_BTN_OFF);
    RowCache &rc = rowCache[screen_ORBTAP][OB_SLOT_BTN + 4];
    char k[4]; snprintf(k, sizeof(k), "%d", (int)st);
    if (rc.value != k) { conDrawModeButton(tft, CON_C2X + 2, conRowY(0) + 4, CON_BTN_W, CON_VALH, "APPR", st); rc.value = k; }
  }
  int16_t ox = conValX(CON_C2X) + 6;
  snprintf(buf, sizeof(buf), "%+.1f m/s", obDirtyRate ? obPendRate : state.obApprRate);
  conPut(tft, screen_ORBTAP, OB_SLOT_VAL + 4, ox, conRowY(0) + 4, vw, CON_VALH, buf, obDirtyRate ? OB_EDT : (obApprEngaged() ? OB_VAL : TFT_DARK_GREY));
  snprintf(buf, sizeof(buf), "%.0f m", obDirtyDist ? obPendDist : state.obApprDist);
  conPut(tft, screen_ORBTAP, OB_SLOT_VAL + 5, ox, conRowY(1) + 4, vw, CON_VALH, buf, obDirtyDist ? OB_EDT : OB_VAL);
  obToggleBox(tft, OB_SLOT_VAL + 6, 2, obPendWarp  >= 0 ? (obPendWarp  == 1) : obWarpAuto(),  obPendWarp  >= 0);
  obToggleBox(tft, OB_SLOT_VAL + 7, 3, obPendStage >= 0 ? (obPendStage == 1) : obStageAuto(), obPendStage >= 0);

  // EXEC / WARP button: green outline when a tap does something, grey otherwise
  {
    bool canExec = (state.obPhase == 1), canWarp = (state.obPhase == 3 && obWarpAuto());
    const char *txt = canWarp ? "WARP" : "EXEC";
    const char *hint = canWarp ? "warp to ignition" : canExec ? "start the burn" : executing ? obPhaseName(state.obPhase) : "arm a burn first";
    bool pending = obExecPendPhase != 0;
    RowCache &rc = rowCache[screen_ORBTAP][OB_SLOT_EXEC];
    char key[40]; snprintf(key, sizeof(key), "%s|%s|%d", txt, hint, pending);
    if (rc.value != key) {
      uint16_t col = pending ? TFT_CYAN : (canExec || canWarp) ? TFT_NEON_GREEN : TFT_GREY;
      ButtonLabel b = { txt, col, col, TFT_OFF_BLACK, TFT_OFF_BLACK, col, col };
      drawButton(tft, OB_EXEC_X, CON_BIG_Y, OB_BIG_W, CON_BIG_H, b, &Roboto_Black_28, false);
      textCenter(tft, &Roboto_Black_16, OB_EXEC_X, CON_BIG_Y + CON_BIG_H - 26, OB_BIG_W, 18, hint, TFT_GREY, TFT_OFF_BLACK);
      rc.value = key;
    }
  }

  // PLAN
  int16_t px = conValX(CON_C3X) + 6;
  bool plan = armed && state.obDvTotal > 0.0f;
  if (plan) snprintf(buf, sizeof(buf), "%.0f m/s", state.obDvTotal); else snprintf(buf, sizeof(buf), "---");
  conPut(tft, screen_ORBTAP, OB_SLOT_PLAN + 0, px, conRowY(0) + 4, vw, CON_VALH, buf, plan ? OB_VAL : TFT_DARK_GREY);
  if (plan) snprintf(buf, sizeof(buf), "%.0f m/s", state.obDvRemaining); else snprintf(buf, sizeof(buf), "---");
  conPut(tft, screen_ORBTAP, OB_SLOT_PLAN + 1, px, conRowY(1) + 4, vw, CON_VALH, buf, plan ? (state.obPhase == 5 ? TFT_NEON_GREEN : OB_VAL) : TFT_DARK_GREY);
  if (plan) obFmtTime(-state.obTIgnition, buf, sizeof(buf)); else snprintf(buf, sizeof(buf), "---");
  conPut(tft, screen_ORBTAP, OB_SLOT_PLAN + 2, px, conRowY(2) + 4, vw, CON_VALH, buf, plan ? (state.obTIgnition < 30.0f ? TFT_ORANGE : OB_VAL) : TFT_DARK_GREY);
  if (plan) obFmtTime(state.obBurnDuration, buf, sizeof(buf)); else snprintf(buf, sizeof(buf), "---");
  conPut(tft, screen_ORBTAP, OB_SLOT_PLAN + 3, px, conRowY(3) + 4, vw, CON_VALH, buf, plan ? OB_VAL : TFT_DARK_GREY);
  if (state.obAccelEst > 0.0f) snprintf(buf, sizeof(buf), "%.1f m/s\xB2", state.obAccelEst); else snprintf(buf, sizeof(buf), "---");
  conPut(tft, screen_ORBTAP, OB_SLOT_PLAN + 4, px, conRowY(4) + 4, vw, CON_VALH, buf, state.obAccelEst > 0.0f ? OB_VAL : TFT_DARK_GREY);
  snprintf(buf, sizeof(buf), "%.0f m/s", state.stageDeltaV);
  conPut(tft, screen_ORBTAP, OB_SLOT_PLAN + 5, px, conRowY(5) + 4, vw, CON_VALH, buf, (plan && state.stageDeltaV < state.obDvTotal) ? TFT_ORANGE : OB_VAL);
  if (state.targetAvailable) formatAltBuf(state.tgtDistance, buf, sizeof(buf)); else snprintf(buf, sizeof(buf), "---");
  conPut(tft, screen_ORBTAP, OB_SLOT_PLAN + 6, px, conRowY(6) + 4, vw, CON_VALH, buf, state.targetAvailable ? OB_VAL : TFT_DARK_GREY);
}

// ── Keypad commits ───────────────────────────────────────────────────────────────────
static void obTargetCommit(int8_t idx, float v, bool off) {
  (void)off;
  if (idx < 1 || idx >= OB_BURNS) return;
  obPendTgt[idx] = v;
  obDirtyTgt[idx] = apEnqueueCmd(OB_SET_OP[idx], v);
}
static void obApprCommit(int8_t idx, float v, bool off) {
  (void)off;
  if (idx == 0) { obPendRate = v; obDirtyRate = apEnqueueCmd(OB_CMD_SET_APPR_RATE, v); }
  else          { obPendDist = v; obDirtyDist = apEnqueueCmd(OB_CMD_SET_APPR_DIST, v); }
}

// ── Touch ────────────────────────────────────────────────────────────────────────────
void orbtApScreenTouch(uint16_t x, uint16_t y) {
  if (kpIsOpen()) { kpTouch(x, y); return; }
  uint32_t now = millis();

  if (y >= CON_BIG_Y && y < CON_BIG_Y + CON_BIG_H) {
    if (x >= OB_EXEC_X && x < OB_EXEC_X + OB_BIG_W) {
      if (state.obPhase == 1 || (state.obPhase == 3 && obWarpAuto())) {
        if (apEnqueueCmd(OB_CMD_EXEC, 0.0f)) { obExecPendPhase = (state.obPhase == 1) ? 2 : 4; obExecPendMs = now; }
      }
      return;
    }
    if (x >= OB_OFF_X && x < OB_OFF_X + OB_BIG_W) {
      if (apEnqueueCmd(HP_CMD_AP_OFF, 0.0f)) {
        for (uint8_t i = 0; i < OB_BURNS; i++) if (obArmedMode(i)) { obPendArm[i] = 0; obPendArmMs[i] = now; }
        if (obApprEngaged()) { obPendAppr = 0; obPendApprMs = now; }
      }
      return;
    }
  }

  // Burn column
  for (uint8_t r = 0; r < OB_BURNS; r++) {
    int16_t cy = conRowY(r);
    if (y < cy || y >= cy + CON_ROW_H) continue;
    if (x >= CON_C1X + 2 && x < CON_C1X + 2 + CON_BTN_W) {
      bool want = !(obPendArm[r] >= 0 ? (obPendArm[r] == 1) : obArmedMode(r));
      if (apEnqueueCmd(OB_ARM_OP[r], want ? 1.0f : 0.0f)) { obPendArm[r] = want ? 1 : 0; obPendArmMs[r] = now; }
      return;
    }
    if (r >= 1 && x >= conValX(CON_C1X) && x < conValX(CON_C1X) + CON_VALW) {
      if (state.obPhase == 5) return;   // no edits during a burn
      KpField f = (r == 1) ? KpField{ "TARGET APOAPSIS", "m", 0.0f, 2000000000.0f, false, false, 0 }
                : (r == 2) ? KpField{ "TARGET PERIAPSIS", "m", 0.0f, 2000000000.0f, false, false, 0 }
                           : KpField{ "TARGET INCLINATION", "\xB0", 0.0f, 180.0f, false, false, 1 };
      kpOpen(f, (int8_t)r, obTargetCommit);
      return;
    }
  }
  // Target / options column
  for (uint8_t r = 0; r < 4; r++) {
    int16_t cy = conRowY(r);
    if (y < cy || y >= cy + CON_ROW_H || x < CON_C2X || x >= CON_C2X + CON_COLW) continue;
    if (r == 0 && x < CON_C2X + 2 + CON_BTN_W) {
      bool want = !(obPendAppr >= 0 ? (obPendAppr == 1) : obApprEngaged());
      if (apEnqueueCmd(OB_CMD_ENGAGE_APPR, want ? 1.0f : 0.0f)) { obPendAppr = want ? 1 : 0; obPendApprMs = now; }
      return;
    }
    if (x < conValX(CON_C2X)) return;
    switch (r) {
      case 0: { KpField f = { "APPROACH RATE", "m/s", -20.0f, 5.0f, false, true, 1 }; kpOpen(f, 0, obApprCommit); break; }
      case 1: { KpField f = { "HOLD DISTANCE", "m", 5.0f, 5000.0f, false, false, 0 }; kpOpen(f, 1, obApprCommit); break; }
      case 2: { bool want = !(obPendWarp  >= 0 ? (obPendWarp  == 1) : obWarpAuto());  if (apEnqueueCmd(OB_CMD_SET_WARP,      want ? 1.0f : 0.0f)) { obPendWarp  = want ? 1 : 0; obPendWarpMs  = now; } break; }
      case 3: { bool want = !(obPendStage >= 0 ? (obPendStage == 1) : obStageAuto()); if (apEnqueueCmd(OB_CMD_SET_AUTOSTAGE, want ? 1.0f : 0.0f)) { obPendStage = want ? 1 : 0; obPendStageMs = now; } break; }
    }
    return;
  }
}

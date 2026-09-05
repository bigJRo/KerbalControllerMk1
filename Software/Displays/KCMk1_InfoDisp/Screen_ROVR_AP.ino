/***************************************************************************************
   Screen_ROVR_AP.ino — Rover Autopilot console (chrome + draw + touch)

   Touch console for the hold-mode autopilot's rover modes (Controller_Main). Renders
   the 28-byte rover status frame (state.rv*) and lets the pilot engage CRUISE / HDG /
   TGT, edit the cruise speed, heading and guard limits, and tap A/P OFF.
   Design: Documents/Developer/Hold_Mode_Autopilot.md §5.

   LAYOUT (the shared console grid — ConsoleShared.ino):
     banner: <CRUISE  HDG | A/P OFF>  <guard / reason>   body · vessel  <ENGAGED / A/P OFF>
     DRIVE               | GUARD LIMITS       | DRIVE DATA
     [CRUISE] [speed]    | SPEED  [limit]     | SPEED    live, signed
     [HDG]    [heading]  | SLOPE  [limit]     | HDG      live
     [TGT]    BRG xxx°   | ROLL   [limit]     | TGT BRG  live
     hint                | hint               | PITCH    live
                         |                    | ROLL     live
     legend              | [A/P OFF]          | WHL THR  commanded
                         |                    | BRAKES   live
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

static const uint16_t RA_LBL = KDC_LABEL_COLOR;
static const uint16_t RA_VAL = TFT_DARK_GREEN;
static const uint16_t RA_EDT = TFT_SKY;
#define RA_F_LBL  (&Roboto_Black_24)
#define RA_F_BANN (&Roboto_Black_36)

// Modes: 0 CRUISE, 1 HDG, 2 TGT (no setpoint). Guards: 0 SPEED, 1 SLOPE, 2 ROLL.
static const uint8_t RA_MODES  = 3;
static const uint8_t RA_GUARDS = 3;
static const char *const RA_MODE_LBL[RA_MODES]   = { "CRUISE", "HDG", "TGT" };
static const uint8_t     RA_ENGAGE_OP[RA_MODES]  = { HP_CMD_ENGAGE_CRUISE, HP_CMD_ENGAGE_RHDG, HP_CMD_ENGAGE_RTGT };
static const uint8_t     RA_FLAG_BIT[RA_MODES]   = { 0x01, 0x02, 0x04 };
static const char *const RA_GUARD_LBL[RA_GUARDS] = { "SPEED", "SLOPE", "ROLL" };
static const uint8_t     RA_GUARD_OP[RA_GUARDS]  = { HP_CMD_SET_MAXSPD, HP_CMD_SET_MAXSLOPE, HP_CMD_SET_MAXROLL };

// Row-cache slots
static const uint8_t RA_SLOT_BTN   = 0;    // 0..2
static const uint8_t RA_SLOT_VAL   = 3;    // 3..5  cruise speed / heading / target bearing
static const uint8_t RA_SLOT_GUARD = 6;    // 6..8
static const uint8_t RA_SLOT_DATA  = 9;    // 9..15
static const uint8_t RA_SLOT_BANN  = 16;

static const uint32_t RA_PENDING_TIMEOUT_MS = 3000;

static int8_t   raPend[RA_MODES];
static uint32_t raPendMs[RA_MODES];
static bool     raDirty[2];          // cruise speed, heading
static float    raPendVal[2];
static bool     raGuardDirty[RA_GUARDS];
static float    raGuardPend[RA_GUARDS];
static bool     raScreenRefresh = false;

static bool raEngaged(uint8_t i)   { return (state.rvFlags & RA_FLAG_BIT[i]) != 0; }
static bool raCommanded(uint8_t i) { return raPend[i] >= 0 ? (raPend[i] == 1) : raEngaged(i); }
static float raEcho(uint8_t i)     { return i == 0 ? state.rvCruise : state.rvHdg; }
static float raGuardEcho(uint8_t i){ return i == 0 ? state.rvMaxSpeed : i == 1 ? state.rvMaxSlope : state.rvMaxRoll; }

static void rvReconcile() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < RA_MODES; i++) {
    if (raPend[i] < 0) continue;
    if (raEngaged(i) == (raPend[i] == 1) || now - raPendMs[i] > RA_PENDING_TIMEOUT_MS) raPend[i] = -1;
  }
  for (uint8_t i = 0; i < 2; i++)
    if (raDirty[i] && fabsf(raEcho(i) - raPendVal[i]) < 0.1f) raDirty[i] = false;
  for (uint8_t i = 0; i < RA_GUARDS; i++)
    if (raGuardDirty[i] && fabsf(raGuardEcho(i) - raGuardPend[i]) < 0.1f) raGuardDirty[i] = false;
}

// Signed ground speed: more than 90 deg between the nose and the surface-velocity
// vector means reverse (the ROVER screen's derivation, and the master's).
static float raSignedSpeed() {
  if (state.surfaceVel < 0.3f) return state.surfaceVel;
  float d = state.srfVelHeading - state.heading;
  while (d > 180.0f) d -= 360.0f;
  while (d < -180.0f) d += 360.0f;
  return (fabsf(d) > 90.0f) ? -state.surfaceVel : state.surfaceVel;
}

static void raHdgBuf(float v, char *buf, size_t n) {
  int16_t h = (int16_t)roundf(v) % 360; if (h < 0) h += 360;
  snprintf(buf, n, "%03d\xB0", h);
}

// ── Chrome ───────────────────────────────────────────────────────────────────────────
void chromeScreen_ROVRAP(KCM_TFT &tft) {
  kpForceClose();
  conDrawColumns(tft, "DRIVE", "GUARD LIMITS", "DRIVE DATA");
  for (uint8_t i = 0; i < RA_MODES; i++)  conDrawValueBox(tft, CON_C1X, i);
  for (uint8_t i = 0; i < RA_GUARDS; i++) {
    textLeft(tft, RA_F_LBL, CON_C2X + 4, conRowY(i), CON_COLW - CON_VALW, CON_ROW_H, RA_GUARD_LBL[i], RA_LBL, TFT_BLACK);
    conDrawValueBox(tft, CON_C2X, i);
  }
  static const char *const DATA_LBL[7] = { "SPEED", "HDG", "TGT BRG", "PITCH", "ROLL", "WHL THR", "BRAKES" };
  for (uint8_t r = 0; r < 7; r++)
    textLeft(tft, RA_F_LBL, CON_C3X + 4, conRowY(r), CON_COLW - CON_VALW, CON_ROW_H, DATA_LBL[r], RA_LBL, TFT_BLACK);
  textLeft(tft, &Roboto_Black_16, CON_C1X + 4, conRowY(3) + 8,  CON_COLW - 8, 20, "TGT steers to the target bearing;", TFT_DARK_GREY, TFT_BLACK);
  textLeft(tft, &Roboto_Black_16, CON_C1X + 4, conRowY(3) + 28, CON_COLW - 8, 20, "HDG and TGT are exclusive.", TFT_DARK_GREY, TFT_BLACK);
  textLeft(tft, &Roboto_Black_16, CON_C2X + 4, conRowY(3) + 8,  CON_COLW - 8, 20, "Slope guard scales the cruise", TFT_DARK_GREY, TFT_BLACK);
  textLeft(tft, &Roboto_Black_16, CON_C2X + 4, conRowY(3) + 28, CON_COLW - 8, 20, "setpoint; roll guard brakes.", TFT_DARK_GREY, TFT_BLACK);
  int16_t ly = conRowY(5) + 12, lx = CON_C1X + 2;
  tft.drawRect(lx, ly, 92, 28, TFT_GREY);             textCenter(tft, &Roboto_Black_16, lx, ly, 92, 28, "OFF", TFT_WHITE, TFT_BLACK);
  tft.drawRect(lx + 100, ly, 92, 28, TFT_CYAN);       textCenter(tft, &Roboto_Black_16, lx + 100, ly, 92, 28, "PENDING", TFT_CYAN, TFT_BLACK);
  tft.fillRect(lx + 200, ly, 92, 28, TFT_DARK_GREEN); textCenter(tft, &Roboto_Black_16, lx + 200, ly, 92, 28, "ENGAGED", TFT_WHITE, TFT_DARK_GREEN);
  ButtonLabel b = { "A/P OFF", TFT_WHITE, TFT_WHITE, TFT_OFF_BLACK, TFT_OFF_BLACK, TFT_GREY, TFT_GREY };
  drawButton(tft, CON_C2X, CON_BIG_Y, CON_COLW, CON_BIG_H, b, &Roboto_Black_28, false);
  textCenter(tft, &Roboto_Black_16, CON_C2X, CON_BIG_Y + CON_BIG_H - 26, CON_COLW, 18, "disconnect all, brakes stay as set", TFT_GREY, TFT_OFF_BLACK);
}

// ── Draw ─────────────────────────────────────────────────────────────────────────────
void drawScreen_ROVRAP(KCM_TFT &tft) {
  if (kpTakeClosed()) raScreenRefresh = true;
  if (raScreenRefresh) {
    tft.fillRect(0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP, TFT_BLACK);
    chromeScreen_ROVRAP(tft);
    invalidateRowCache(screen_ROVRAP);
    raScreenRefresh = false;
  }
  if (kpIsOpen()) { kpDraw(tft); return; }

  char buf[48];
  bool anyPending = false;
  for (uint8_t i = 0; i < RA_MODES; i++) if (raPend[i] >= 0) anyPending = true;
  bool slopeGuard = (state.rvFlags & 0x10) != 0;

  // Banner
  {
    char modes[32] = "";
    for (uint8_t i = 0; i < RA_MODES; i++)
      if (raEngaged(i)) { if (modes[0]) strlcat(modes, "  ", sizeof(modes)); strlcat(modes, RA_MODE_LBL[i], sizeof(modes)); }
    bool engaged = modes[0] != '\0';
    bool showReason = (state.rvReason != HP_REASON_NONE && state.rvReasonAge <= 5);
    const char *note = showReason ? hpReasonLabel(state.rvReason) : (slopeGuard ? "SLOPE LIMIT" : "");
    char key[96];
    snprintf(key, sizeof(key), "%s|%s|%s|%d|%s", engaged ? modes : "A/P OFF", note, state.gameSOI.c_str(), anyPending, state.vesselName.c_str());
    RowCache &rc = rowCache[screen_ROVRAP][RA_SLOT_BANN];
    if (rc.value != key) {
      tft.fillRect(0, CON_BANNER_Y, CONTENT_W, CON_BANNER_H, TFT_BLACK);
      textLeft(tft, RA_F_BANN, 8, CON_BANNER_Y, 316, CON_BANNER_H, engaged ? modes : "A/P OFF",
               engaged ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);
      if (note[0]) textLeft(tft, &Roboto_Black_24, 330, CON_BANNER_Y, 300, CON_BANNER_H, note, TFT_ORANGE, TFT_BLACK);
      snprintf(buf, sizeof(buf), "%s \xB7 %s", state.gameSOI.c_str(), state.vesselName.c_str());
      textRight(tft, &Roboto_Black_20, 648, CON_BANNER_Y + 4,  CONTENT_W - 648 - 6, 24, buf, RA_LBL, TFT_BLACK);
      snprintf(buf, sizeof(buf), "%s%s", engaged ? "ENGAGED" : "A/P OFF", anyPending ? "..." : "");
      textRight(tft, &Roboto_Black_28, 648, CON_BANNER_Y + 30, CONTENT_W - 648 - 6, 30, buf,
                engaged ? TFT_NEON_GREEN : TFT_DARK_GREY, TFT_BLACK);
      rc.value = key;
    }
  }

  // Mode buttons
  for (uint8_t i = 0; i < RA_MODES; i++) {
    ConBtnState st = raEngaged(i) ? CON_BTN_ON : (raPend[i] >= 0 ? CON_BTN_PENDING : CON_BTN_OFF);
    RowCache &rc = rowCache[screen_ROVRAP][RA_SLOT_BTN + i];
    char k[4]; snprintf(k, sizeof(k), "%d", (int)st);
    if (rc.value != k) {
      conDrawModeButton(tft, CON_C1X + 2, conRowY(i) + 4, CON_BTN_W, CON_VALH, RA_MODE_LBL[i], st);
      rc.value = k;
    }
  }
  int16_t vx = conValX(CON_C1X) + 6, vw = CON_VALW - 10;
  snprintf(buf, sizeof(buf), "%.1f m/s", raDirty[0] ? raPendVal[0] : state.rvCruise);
  conPut(tft, screen_ROVRAP, RA_SLOT_VAL + 0, vx, conRowY(0) + 4, vw, CON_VALH, buf, raDirty[0] ? RA_EDT : (raEngaged(0) ? RA_VAL : TFT_DARK_GREY));
  raHdgBuf(raDirty[1] ? raPendVal[1] : state.rvHdg, buf, sizeof(buf));
  conPut(tft, screen_ROVRAP, RA_SLOT_VAL + 1, vx, conRowY(1) + 4, vw, CON_VALH, buf, raDirty[1] ? RA_EDT : (raEngaged(1) ? RA_VAL : TFT_DARK_GREY));
  if (state.targetAvailable) { char h[12]; raHdgBuf(state.tgtHeading, h, sizeof(h)); snprintf(buf, sizeof(buf), "BRG %s", h); }
  else                       snprintf(buf, sizeof(buf), "NO TGT");
  conPut(tft, screen_ROVRAP, RA_SLOT_VAL + 2, vx, conRowY(2) + 4, vw, CON_VALH, buf, raEngaged(2) ? RA_VAL : TFT_DARK_GREY);

  // Guard limits
  int16_t gx = conValX(CON_C2X) + 6;
  for (uint8_t i = 0; i < RA_GUARDS; i++) {
    float v = raGuardDirty[i] ? raGuardPend[i] : raGuardEcho(i);
    if (i == 0) snprintf(buf, sizeof(buf), "%.1f m/s", v); else snprintf(buf, sizeof(buf), "%.0f\xB0", v);
    conPut(tft, screen_ROVRAP, RA_SLOT_GUARD + i, gx, conRowY(i) + 4, vw, CON_VALH, buf, raGuardDirty[i] ? RA_EDT : RA_VAL);
  }

  // Drive data
  int16_t dx = conValX(CON_C3X) + 6;
  snprintf(buf, sizeof(buf), "%+.1f m/s", raSignedSpeed());   conPut(tft, screen_ROVRAP, RA_SLOT_DATA + 0, dx, conRowY(0) + 4, vw, CON_VALH, buf, slopeGuard ? TFT_ORANGE : RA_VAL);
  raHdgBuf(state.heading, buf, sizeof(buf));                  conPut(tft, screen_ROVRAP, RA_SLOT_DATA + 1, dx, conRowY(1) + 4, vw, CON_VALH, buf, RA_VAL);
  if (state.targetAvailable) raHdgBuf(state.tgtHeading, buf, sizeof(buf)); else snprintf(buf, sizeof(buf), "---");
  conPut(tft, screen_ROVRAP, RA_SLOT_DATA + 2, dx, conRowY(2) + 4, vw, CON_VALH, buf, state.targetAvailable ? RA_VAL : TFT_DARK_GREY);
  snprintf(buf, sizeof(buf), "%+.1f\xB0", state.pitch);       conPut(tft, screen_ROVRAP, RA_SLOT_DATA + 3, dx, conRowY(3) + 4, vw, CON_VALH, buf, slopeGuard ? TFT_ORANGE : RA_VAL);
  snprintf(buf, sizeof(buf), "%+.1f\xB0", state.roll);        conPut(tft, screen_ROVRAP, RA_SLOT_DATA + 4, dx, conRowY(4) + 4, vw, CON_VALH, buf, RA_VAL);
  snprintf(buf, sizeof(buf), "%+.0f %%", state.rvCmdWheel * 100.0f);
  conPut(tft, screen_ROVRAP, RA_SLOT_DATA + 5, dx, conRowY(5) + 4, vw, CON_VALH, buf, raEngaged(0) ? RA_VAL : TFT_DARK_GREY);
  bool brakes = (state.rvFlags & 0x08) != 0 || state.brakes_on;
  conPut(tft, screen_ROVRAP, RA_SLOT_DATA + 6, dx, conRowY(6) + 4, vw, CON_VALH, brakes ? "ON" : "OFF", brakes ? TFT_YELLOW : TFT_GREY);
}

// ── Keypad commits ───────────────────────────────────────────────────────────────────
static void raSetpointCommit(int8_t idx, float v, bool off) {
  (void)off;
  if (idx < 0 || idx > 1) return;
  raPendVal[idx] = v;
  raDirty[idx] = apEnqueueCmd(idx == 0 ? HP_CMD_SET_CRUISE : HP_CMD_SET_RHDG, v);
}
static void raGuardCommit(int8_t idx, float v, bool off) {
  (void)off;
  if (idx < 0 || idx >= RA_GUARDS) return;
  raGuardPend[idx] = v;
  raGuardDirty[idx] = apEnqueueCmd(RA_GUARD_OP[idx], v);
}

// ── Touch ────────────────────────────────────────────────────────────────────────────
void rovrApScreenTouch(uint16_t x, uint16_t y) {
  if (kpIsOpen()) { kpTouch(x, y); return; }
  uint32_t now = millis();

  if (y >= CON_BIG_Y && y < CON_BIG_Y + CON_BIG_H && x >= CON_C2X && x < CON_C2X + CON_COLW) {
    if (apEnqueueCmd(HP_CMD_AP_OFF, 0.0f))
      for (uint8_t i = 0; i < RA_MODES; i++) if (raEngaged(i)) { raPend[i] = 0; raPendMs[i] = now; }
    return;
  }

  for (uint8_t r = 0; r < 3; r++) {
    int16_t cy = conRowY(r);
    if (y < cy || y >= cy + CON_ROW_H) continue;
    // Drive column: mode button or setpoint box
    if (x >= CON_C1X + 2 && x < CON_C1X + 2 + CON_BTN_W) {
      bool want = !raCommanded(r);
      if (apEnqueueCmd(RA_ENGAGE_OP[r], want ? 1.0f : 0.0f)) { raPend[r] = want ? 1 : 0; raPendMs[r] = now; }
      return;
    }
    if (x >= conValX(CON_C1X) && x < conValX(CON_C1X) + CON_VALW && r < 2) {
      KpField f = (r == 0) ? KpField{ "CRUISE SPEED", "m/s", -10.0f, 60.0f, false, true, 1 }
                           : KpField{ "HEADING", "\xB0", 0.0f, 359.9f, false, false, 0 };
      kpOpen(f, (int8_t)r, raSetpointCommit);
      return;
    }
    // Guard column
    if (x >= conValX(CON_C2X) && x < conValX(CON_C2X) + CON_VALW) {
      KpField f = (r == 0) ? KpField{ "SPEED LIMIT", "m/s", 1.0f, 60.0f, false, false, 1 }
                : (r == 1) ? KpField{ "SLOPE LIMIT", "\xB0", 5.0f, 45.0f, false, false, 0 }
                           : KpField{ "ROLL LIMIT",  "\xB0", 5.0f, 60.0f, false, false, 0 };
      kpOpen(f, (int8_t)r, raGuardCommit);
      return;
    }
  }
}

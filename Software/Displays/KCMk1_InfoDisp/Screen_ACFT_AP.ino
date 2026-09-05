/***************************************************************************************
   Screen_ACFT_AP.ino — Aircraft Autopilot console (chrome + draw + touch)

   Touch console for the hold-mode autopilot's aircraft modes, which run on
   Controller_Main. Renders the status echoed over I2C (state.hp*, the 44-byte
   aircraft frame) and lets the pilot engage / disengage modes, edit setpoints and
   tap LVL / A/P OFF. Design: Documents/Developer/Hold_Mode_Autopilot.md §4.

   LAYOUT (content 0..CONTENT_W, the ascent console's grid — ConsoleShared.ino):
     banner: <engaged modes | A/P OFF>  <reason>      body · vessel  <ENGAGED / A/P OFF>
     PITCH            | LATERAL / THRUST   | FLIGHT DATA
     [ATT] [setpoint] | [ROLL] [setpoint]  | PITCH  live
     [AOA] [setpoint] | [HDG]  [setpoint]  | ROLL   live
     [V/S] [setpoint] | [IAS]  [setpoint]  | HDG    live
     [ALT] [setpoint] | [MACH] [setpoint]  | V/S    live
     hint             |                    | ALT    live
     legend           | [LVL]  [A/P OFF]   | IAS    live
                      |                    | MACH   live

   Mode buttons are three-state: off (grey outline), pending (cyan outline — tap sent,
   not yet echoed), engaged (green fill). Tapping a mode engages it and the master
   captures the current value as the setpoint; tapping a setpoint box opens the
   shared keypad and applies whether or not the mode is engaged. Pending cues clear
   when the master's echo matches, exactly as on the ascent console. Commanded
   throttle is deliberately not shown: while the autothrottle is engaged the physical
   lever IS the throttle readout.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

static const uint16_t HA_LBL = KDC_LABEL_COLOR;
static const uint16_t HA_VAL = TFT_DARK_GREEN;
static const uint16_t HA_EDT = TFT_SKY;
#define HA_F_LBL  (&Roboto_Black_24)
#define HA_F_BANN (&Roboto_Black_36)

// ── Mode table ───────────────────────────────────────────────────────────────────────
// group: 0 pitch, 1 lateral, 2 thrust. modeVal: the value the group's mode byte takes
// when this mode is engaged (HpPitchMode / HpLatMode / HpThrMode on the master).
struct HaMode {
  int16_t     x;          // column x
  uint8_t     row;
  const char *label;      // button caption
  uint8_t     engageOp, setOp;
  const char *name;       // keypad header
  const char *units;
  float       mn, mx;
  bool        sign;
  uint8_t     dec;
  uint8_t     group, modeVal;
};
static const uint8_t HA_MODES = 8;
static const HaMode HA[HA_MODES] = {
  { CON_C1X, 0, "ATT",  HP_CMD_ENGAGE_ATT,  HP_CMD_SET_ATT,  "PITCH ATTITUDE",  "\xB0",  -45.0f,   45.0f, true,  1, 0, 1 },
  { CON_C1X, 1, "AOA",  HP_CMD_ENGAGE_AOA,  HP_CMD_SET_AOA,  "ANGLE OF ATTACK", "\xB0",  -10.0f,   25.0f, true,  1, 0, 2 },
  { CON_C1X, 2, "V/S",  HP_CMD_ENGAGE_VS,   HP_CMD_SET_VS,   "VERTICAL SPEED",  "m/s",  -100.0f,  100.0f, true,  0, 0, 3 },
  { CON_C1X, 3, "ALT",  HP_CMD_ENGAGE_ALT,  HP_CMD_SET_ALT,  "ALTITUDE (ASL)",  "m",       0.0f, 70000.0f, false, 0, 0, 4 },
  { CON_C2X, 0, "ROLL", HP_CMD_ENGAGE_ROLL, HP_CMD_SET_ROLL, "BANK ANGLE",      "\xB0",  -60.0f,   60.0f, true,  1, 1, 1 },
  { CON_C2X, 1, "HDG",  HP_CMD_ENGAGE_HDG,  HP_CMD_SET_HDG,  "HEADING",         "\xB0",    0.0f,  359.9f, false, 0, 1, 2 },
  { CON_C2X, 2, "IAS",  HP_CMD_ENGAGE_IAS,  HP_CMD_SET_IAS,  "INDICATED AIRSPEED", "m/s", 20.0f, 1500.0f, false, 0, 2, 1 },
  { CON_C2X, 3, "MACH", HP_CMD_ENGAGE_MACH, HP_CMD_SET_MACH, "MACH NUMBER",     "",       0.10f,    6.0f, false, 2, 2, 2 },
};

// Row-cache slots (ROW_COUNT = 24)
static const uint8_t HA_SLOT_BTN  = 0;    // 0..7  mode buttons
static const uint8_t HA_SLOT_VAL  = 8;    // 8..15 setpoint boxes
static const uint8_t HA_SLOT_DATA = 16;   // 16..22 flight data
static const uint8_t HA_SLOT_BANN = 23;

static const int16_t HA_BIG_W = 145;                     // LVL / A/P OFF, side by side
static const int16_t HA_LVL_X = CON_C2X;
static const int16_t HA_OFF_X = CON_C2X + 153;

static const uint32_t HA_PENDING_TIMEOUT_MS = 3000;      // a refused engage is never echoed

// ── Pending state ────────────────────────────────────────────────────────────────────
static int8_t   haPend[HA_MODES];        // -1 none, 0 disengage sent, 1 engage sent
static uint32_t haPendMs[HA_MODES];
static bool     haDirty[HA_MODES];       // setpoint edit awaiting echo
static float    haPendVal[HA_MODES];
static bool     haScreenRefresh = false;

static uint8_t haGroupMode(uint8_t group) {
  return group == 0 ? state.hpPitchMode : group == 1 ? state.hpLatMode : state.hpThrMode;
}
static bool haEngaged(uint8_t i)   { return haGroupMode(HA[i].group) == HA[i].modeVal; }
static bool haCommanded(uint8_t i) { return haPend[i] >= 0 ? (haPend[i] == 1) : haEngaged(i); }

static float haEcho(uint8_t i) {
  switch (i) {
    case 0: return state.hpAtt;  case 1: return state.hpAoa;  case 2: return state.hpVs;   case 3: return state.hpAlt;
    case 4: return state.hpRoll; case 5: return state.hpHdg;  case 6: return state.hpIas;  default: return state.hpMach;
  }
}
static float haTol(uint8_t i) {
  switch (i) { case 2: case 6: return 0.1f; case 3: return 0.5f; case 7: return 0.005f; default: return 0.05f; }
}
static float haShown(uint8_t i) { return haDirty[i] ? haPendVal[i] : haEcho(i); }

bool hpAnyEngagedAnnunciated() { return (state.hpFlags & 0x01) != 0 || (state.rvFlags & 0x07) != 0; }

const char *hpReasonLabel(uint8_t r) {
  switch (r) {
    case HP_REASON_STICK: return "STICK";  case HP_REASON_LEVER: return "A/T OFF: LEVER";
    case HP_REASON_BRAKES: return "BRAKES"; case HP_REASON_AIRBORNE: return "AIRBORNE";
    case HP_REASON_ROLL_LIMIT: return "ROLL LIMIT"; case HP_REASON_NO_ATMO: return "NO ATMO";
    case HP_REASON_TELEMETRY: return "TELEMETRY"; case HP_REASON_ASCENT: return "ASCENT ARMED";
    case HP_REASON_REFUSED: return "REFUSED"; default: return "";
  }
}

// Retire pending cues once the master's echo agrees. Called each loop from
// updateI2CState() for both hold-mode consoles (the rover half lives in its own tab).
static void haReconcile() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < HA_MODES; i++) {
    if (haPend[i] >= 0) {
      if (haEngaged(i) == (haPend[i] == 1)) haPend[i] = -1;
      else if (now - haPendMs[i] > HA_PENDING_TIMEOUT_MS) haPend[i] = -1;   // refused / lost
    }
    if (haDirty[i] && fabsf(haEcho(i) - haPendVal[i]) < haTol(i)) haDirty[i] = false;
  }
}
static void rvReconcile();   // Screen_ROVR_AP.ino
void hpReconcilePending() { haReconcile(); rvReconcile(); }

// ── Formatting ───────────────────────────────────────────────────────────────────────
static void haFmtSetpoint(uint8_t i, float v, char *buf, size_t n) {
  switch (i) {
    case 0: case 1: case 4: snprintf(buf, n, "%+.1f\xB0", v); break;
    case 2: snprintf(buf, n, "%+.0f m/s", v); break;
    case 3: formatAltBuf(v, buf, n); break;
    case 5: { int16_t h = (int16_t)roundf(v) % 360; if (h < 0) h += 360; snprintf(buf, n, "%03d\xB0", h); break; }
    case 6: snprintf(buf, n, "%.0f m/s", v); break;
    default: snprintf(buf, n, "%.2f", v); break;
  }
}

// ── Chrome ───────────────────────────────────────────────────────────────────────────
void chromeScreen_ACFTAP(KCM_TFT &tft) {
  kpForceClose();
  conDrawColumns(tft, "PITCH", "LATERAL / THRUST", "FLIGHT DATA");
  for (uint8_t i = 0; i < HA_MODES; i++) conDrawValueBox(tft, HA[i].x, HA[i].row);
  static const char *const DATA_LBL[7] = { "PITCH", "ROLL", "HDG", "V/S", "ALT", "IAS", "MACH" };
  for (uint8_t r = 0; r < 7; r++)
    textLeft(tft, HA_F_LBL, CON_C3X + 4, conRowY(r), CON_COLW - CON_VALW, CON_ROW_H, DATA_LBL[r], HA_LBL, TFT_BLACK);
  textLeft(tft, &Roboto_Black_16, CON_C1X + 4, conRowY(4) + 8,  CON_COLW - 8, 20, "Tap a mode to engage and capture", TFT_DARK_GREY, TFT_BLACK);
  textLeft(tft, &Roboto_Black_16, CON_C1X + 4, conRowY(4) + 28, CON_COLW - 8, 20, "the current value. Tap a box to edit.", TFT_DARK_GREY, TFT_BLACK);
  // State legend
  int16_t ly = conRowY(5) + 12, lx = CON_C1X + 2;
  tft.drawRect(lx, ly, 92, 28, TFT_GREY);           textCenter(tft, &Roboto_Black_16, lx, ly, 92, 28, "OFF", TFT_WHITE, TFT_BLACK);
  tft.drawRect(lx + 100, ly, 92, 28, TFT_CYAN);     textCenter(tft, &Roboto_Black_16, lx + 100, ly, 92, 28, "PENDING", TFT_CYAN, TFT_BLACK);
  tft.fillRect(lx + 200, ly, 92, 28, TFT_DARK_GREEN); textCenter(tft, &Roboto_Black_16, lx + 200, ly, 92, 28, "ENGAGED", TFT_WHITE, TFT_DARK_GREEN);
  // LVL / A/P OFF
  { ButtonLabel b = { "LVL", TFT_WHITE, TFT_WHITE, TFT_OFF_BLACK, TFT_OFF_BLACK, TFT_GREY, TFT_GREY };
    drawButton(tft, HA_LVL_X, CON_BIG_Y, HA_BIG_W, CON_BIG_H, b, &Roboto_Black_28, false);
    textCenter(tft, &Roboto_Black_16, HA_LVL_X, CON_BIG_Y + CON_BIG_H - 26, HA_BIG_W, 18, "wings level, V/S 0", TFT_GREY, TFT_OFF_BLACK); }
  { ButtonLabel b = { "A/P OFF", TFT_WHITE, TFT_WHITE, TFT_OFF_BLACK, TFT_OFF_BLACK, TFT_GREY, TFT_GREY };
    drawButton(tft, HA_OFF_X, CON_BIG_Y, HA_BIG_W, CON_BIG_H, b, &Roboto_Black_28, false);
    textCenter(tft, &Roboto_Black_16, HA_OFF_X, CON_BIG_Y + CON_BIG_H - 26, HA_BIG_W, 18, "disconnect all", TFT_GREY, TFT_OFF_BLACK); }
}

// ── Draw ─────────────────────────────────────────────────────────────────────────────
void drawScreen_ACFTAP(KCM_TFT &tft) {
  if (kpTakeClosed()) haScreenRefresh = true;
  if (haScreenRefresh) {
    tft.fillRect(0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP, TFT_BLACK);
    chromeScreen_ACFTAP(tft);
    invalidateRowCache(screen_ACFTAP);
    haScreenRefresh = false;
  }
  if (kpIsOpen()) { kpDraw(tft); return; }

  char buf[48];
  bool anyPending = false;
  for (uint8_t i = 0; i < HA_MODES; i++) if (haPend[i] >= 0) anyPending = true;

  // Banner: engaged modes (left), reason (middle), body · vessel + ENGAGED (right)
  {
    char modes[40] = "";
    for (uint8_t i = 0; i < HA_MODES; i++)
      if (haEngaged(i)) { if (modes[0]) strlcat(modes, "  ", sizeof(modes)); strlcat(modes, HA[i].label, sizeof(modes)); }
    bool engaged = modes[0] != '\0';
    const char *reason = (state.hpReason != HP_REASON_NONE && state.hpReasonAge <= 5) ? hpReasonLabel(state.hpReason)
                       : ((state.hpFlags & 0x02) && !(state.hpFlags & 0x08)) ? "LEVER OFF" : "";
    uint16_t reasonCol = (state.hpReason != HP_REASON_NONE && state.hpReasonAge <= 5) ? TFT_ORANGE : TFT_GREY;
    char key[96];
    snprintf(key, sizeof(key), "%s|%s|%s|%d|%s", engaged ? modes : "A/P OFF", reason, state.gameSOI.c_str(), anyPending, state.vesselName.c_str());
    RowCache &rc = rowCache[screen_ACFTAP][HA_SLOT_BANN];
    if (rc.value != key) {
      tft.fillRect(0, CON_BANNER_Y, CONTENT_W, CON_BANNER_H, TFT_BLACK);
      textLeft(tft, HA_F_BANN, 8, CON_BANNER_Y, 316, CON_BANNER_H, engaged ? modes : "A/P OFF",
               engaged ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);
      if (reason[0]) textLeft(tft, &Roboto_Black_24, 330, CON_BANNER_Y, 300, CON_BANNER_H, reason, reasonCol, TFT_BLACK);
      snprintf(buf, sizeof(buf), "%s \xB7 %s", state.gameSOI.c_str(), state.vesselName.c_str());
      textRight(tft, &Roboto_Black_20, 648, CON_BANNER_Y + 4,  CONTENT_W - 648 - 6, 24, buf, HA_LBL, TFT_BLACK);
      snprintf(buf, sizeof(buf), "%s%s", engaged ? "ENGAGED" : "A/P OFF", anyPending ? "..." : "");
      textRight(tft, &Roboto_Black_28, 648, CON_BANNER_Y + 30, CONTENT_W - 648 - 6, 30, buf,
                engaged ? TFT_NEON_GREEN : TFT_DARK_GREY, TFT_BLACK);
      rc.value = key;
    }
  }

  // Mode buttons + setpoints
  for (uint8_t i = 0; i < HA_MODES; i++) {
    ConBtnState st = haEngaged(i) ? CON_BTN_ON : (haPend[i] >= 0 ? CON_BTN_PENDING : CON_BTN_OFF);
    RowCache &rc = rowCache[screen_ACFTAP][HA_SLOT_BTN + i];
    char k[4]; snprintf(k, sizeof(k), "%d", (int)st);
    if (rc.value != k) {
      conDrawModeButton(tft, HA[i].x + 2, conRowY(HA[i].row) + 4, CON_BTN_W, CON_VALH, HA[i].label, st);
      rc.value = k;
    }
    haFmtSetpoint(i, haShown(i), buf, sizeof(buf));
    uint16_t fg = haDirty[i] ? HA_EDT : (haEngaged(i) ? HA_VAL : TFT_DARK_GREY);
    conPut(tft, screen_ACFTAP, HA_SLOT_VAL + i, conValX(HA[i].x) + 6, conRowY(HA[i].row) + 4, CON_VALW - 10, CON_VALH, buf, fg);
  }

  // Flight data
  int16_t dx = conValX(CON_C3X) + 6, dw = CON_VALW - 10;
  snprintf(buf, sizeof(buf), "%+.1f\xB0", state.pitch);        conPut(tft, screen_ACFTAP, HA_SLOT_DATA + 0, dx, conRowY(0) + 4, dw, CON_VALH, buf, HA_VAL);
  snprintf(buf, sizeof(buf), "%+.1f\xB0", state.roll);         conPut(tft, screen_ACFTAP, HA_SLOT_DATA + 1, dx, conRowY(1) + 4, dw, CON_VALH, buf, HA_VAL);
  { int16_t h = (int16_t)roundf(state.heading) % 360; if (h < 0) h += 360;
    snprintf(buf, sizeof(buf), "%03d\xB0", h);                  conPut(tft, screen_ACFTAP, HA_SLOT_DATA + 2, dx, conRowY(2) + 4, dw, CON_VALH, buf, HA_VAL); }
  snprintf(buf, sizeof(buf), "%+.1f m/s", state.verticalVel);  conPut(tft, screen_ACFTAP, HA_SLOT_DATA + 3, dx, conRowY(3) + 4, dw, CON_VALH, buf, HA_VAL);
  formatAltBuf(state.altitude, buf, sizeof(buf));              conPut(tft, screen_ACFTAP, HA_SLOT_DATA + 4, dx, conRowY(4) + 4, dw, CON_VALH, buf, HA_VAL);
  snprintf(buf, sizeof(buf), "%.0f m/s", state.IAS);           conPut(tft, screen_ACFTAP, HA_SLOT_DATA + 5, dx, conRowY(5) + 4, dw, CON_VALH, buf, HA_VAL);
  snprintf(buf, sizeof(buf), "%.2f", state.machNumber);        conPut(tft, screen_ACFTAP, HA_SLOT_DATA + 6, dx, conRowY(6) + 4, dw, CON_VALH, buf, HA_VAL);
}

// ── Keypad commit ────────────────────────────────────────────────────────────────────
static void haKeypadCommit(int8_t idx, float v, bool off) {
  (void)off;
  if (idx < 0 || idx >= HA_MODES) return;
  haPendVal[idx] = v;
  haDirty[idx] = apEnqueueCmd(HA[idx].setOp, v);   // cue only for a command that went out
}

// ── Touch ────────────────────────────────────────────────────────────────────────────
void acftApScreenTouch(uint16_t x, uint16_t y) {
  if (kpIsOpen()) { kpTouch(x, y); return; }
  uint32_t now = millis();

  // LVL: wings level + V/S 0 (thrust group untouched)
  if (y >= CON_BIG_Y && y < CON_BIG_Y + CON_BIG_H) {
    if (x >= HA_LVL_X && x < HA_LVL_X + HA_BIG_W) {
      if (apEnqueueCmd(HP_CMD_LVL, 0.0f)) {
        haPend[4] = 1; haPendMs[4] = now;   // ROLL
        haPend[2] = 1; haPendMs[2] = now;   // V/S
      }
      return;
    }
    if (x >= HA_OFF_X && x < HA_OFF_X + HA_BIG_W) {
      if (apEnqueueCmd(HP_CMD_AP_OFF, 0.0f))
        for (uint8_t i = 0; i < HA_MODES; i++) if (haEngaged(i)) { haPend[i] = 0; haPendMs[i] = now; }
      return;
    }
  }

  // Mode buttons / setpoint boxes
  for (uint8_t i = 0; i < HA_MODES; i++) {
    int16_t cy = conRowY(HA[i].row);
    if (y < cy || y >= cy + CON_ROW_H) continue;
    if (x >= HA[i].x + 2 && x < HA[i].x + 2 + CON_BTN_W) {
      // Toggle from the COMMANDED state so a second tap reverses a command in flight.
      bool want = !haCommanded(i);
      if (apEnqueueCmd(HA[i].engageOp, want ? 1.0f : 0.0f)) { haPend[i] = want ? 1 : 0; haPendMs[i] = now; }
      return;
    }
    if (x >= conValX(HA[i].x) && x < conValX(HA[i].x) + CON_VALW) {
      KpField f = { HA[i].name, HA[i].units, HA[i].mn, HA[i].mx, false, HA[i].sign, HA[i].dec };
      kpOpen(f, (int8_t)i, haKeypadCommit);
      return;
    }
  }
}

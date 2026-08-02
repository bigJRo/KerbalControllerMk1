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
static const uint16_t AP_LBL = TFT_LIGHT_GREY;
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
  { AP_C1X, 0, "Tgt Ap:", true  },
  { AP_C1X, 1, "Incl:",   true  },
  { AP_C1X, 2, "Launch:", true  },
  { AP_C2X, 0, "Loft:",   true  },
  { AP_C2X, 1, "Roll:",   true  },
  { AP_C2X, 2, "Max-G:",  true  },
  { AP_C3X, 0, "Pitch:",  false },
  { AP_C3X, 1, "Hdg:",    false },
  { AP_C3X, 2, "Thr:",    false },
  { AP_C3X, 3, "G:",      false },
  { AP_C3X, 4, "q:",      false },
  { AP_C3X, 5, "ApA:",    false },
  { AP_C3X, 6, "PeA:",    false },
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
static bool  apGArmed(){ return apArmOvr >= 0 ? (apArmOvr == 1) : state.apArmed; }
static uint16_t apEditColor(bool dirty){ return dirty ? AP_EDT : AP_VAL; }

// ── Outbound command channel (InfoDisp -> Controller_Main) ────────────────────────────
// Pilot edits and ARM/DISARM taps queue here as (opcode, float-payload) commands. The
// I2C master (Controller_Main) reads the head command from the outbound packet, applies
// it to the autopilot, then acknowledges it by echoing the command's sequence number in
// its next inbound packet, which pops the queue. Pending (cyan) values clear separately,
// in apReconcilePending(), once Controller_Main echoes the accepted value back in the
// AscentStatus frame — so the UI confirms the round-trip, not merely command delivery.
enum {
  AP_CMD_NOP             = 0x00,
  AP_CMD_SET_TARGET_ALT  = 0x01,   // payload = metres
  AP_CMD_SET_INCLINATION = 0x02,   // payload = degrees
  AP_CMD_SET_LAUNCH_DIR  = 0x03,   // payload = 0 north / 1 south
  AP_CMD_SET_LOFT        = 0x04,   // payload = exponent
  AP_CMD_SET_ROLL        = 0x05,   // payload = degrees, or AP_ROLL_OFF to disable hold
  AP_CMD_SET_MAXG        = 0x06,   // payload = g (0 = off)
  AP_CMD_ARM             = 0x10,   // payload = 0
  AP_CMD_DISARM          = 0x11,   // payload = 0
};
static const float AP_ROLL_OFF = 1.0e9f;   // roll-hold disable sentinel (outside +/-180)

struct ApCmd { uint8_t op; float payload; };
static const uint8_t AP_CMDQ_LEN = 16;
static ApCmd   apCmdQ[AP_CMDQ_LEN];
static uint8_t apCmdHead = 0, apCmdTail = 0;   // ring-buffer indices (empty when equal)
static uint8_t apCmdCurSeq = 0;                // seq of head command in flight (0 = none)
static uint8_t apCmdSeqCtr = 0;                // monotonic seq generator, wraps 1..255

static void apEnqueueCmd(uint8_t op, float payload) {
  if (demoMode) return;                        // no master to drain the queue in demo
  uint8_t next = (uint8_t)((apCmdTail + 1) % AP_CMDQ_LEN);
  if (next == apCmdHead) return;               // full — drop (queue holds 15, never happens)
  apCmdQ[apCmdTail].op = op;
  apCmdQ[apCmdTail].payload = payload;
  apCmdTail = next;
}

// Assign a sequence number to a newly-exposed head command. Main-thread only; called
// from updateI2CState() before outbound change-detection.
void apPumpCommandQueue() {
  if (apCmdCurSeq == 0 && apCmdHead != apCmdTail) {
    if (++apCmdSeqCtr == 0) apCmdSeqCtr = 1;   // seq is 1..255; 0 means "no command"
    apCmdCurSeq = apCmdSeqCtr;
  }
}

// Write the head command into a 6-byte field: [seq][op][payload float, little-endian].
// Pure read — safe to call for both the live and candidate packet buffers.
void apFillOutboundCmd(uint8_t *out6) {
  if (apCmdCurSeq != 0) {
    out6[0] = apCmdCurSeq;
    out6[1] = apCmdQ[apCmdHead].op;
    memcpy(&out6[2], &apCmdQ[apCmdHead].payload, 4);
  } else {
    out6[0] = 0;
    out6[1] = AP_CMD_NOP;
    memset(&out6[2], 0, 4);
  }
}

// Master acknowledged sequence `ackSeq` (0 = no ack); pop the head if it matches the
// in-flight command so the next queued command is exposed on the following pump.
void apAckCommand(uint8_t ackSeq) {
  if (ackSeq != 0 && ackSeq == apCmdCurSeq) {
    apCmdHead = (uint8_t)((apCmdHead + 1) % AP_CMDQ_LEN);
    apCmdCurSeq = 0;
  }
}

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
  if (apArmOvr >= 0 && state.apArmed == (apArmOvr == 1))         apArmOvr = -1;
}

// ── Keypad state ────────────────────────────────────────────────────────────────────
static bool    apKpOpen = false;
static bool    apKpRedraw = false;
static bool    apScreenRefresh = false;
static int8_t  apKpEdit = -1;         // index into AP_EDITS
static char    apKpBuf[12];
static uint8_t apKpLen = 0;

static const int16_t KP_W = 480, KP_H = 384;
static const int16_t KP_X = (CONTENT_W - KP_W) / 2;                 // 230
static const int16_t KP_Y = TITLE_TOP + (SCREEN_H - TITLE_TOP - KP_H) / 2;   // ~139
static const int16_t KP_HDR_H = 80;
static const int16_t KAX = KP_X + 6, KAY = KP_Y + KP_HDR_H;
static const int16_t KAW = KP_W - 12, KAH = KP_H - KP_HDR_H - 6;
static const int16_t AP_KEY_W = KAW / 4, AP_KEY_H = KAH / 4;
static const int16_t KP_CX = KP_X + KP_W - 74, KP_CY = KP_Y + 8, KP_CW = 66, KP_CH = 40;  // CANCEL

// Key labels (row-major). "" = disabled slot.
static const char *const KP_KEYS[4][4] = {
  { "7",   "8", "9", "DEL" },
  { "4",   "5", "6", "CLR" },
  { "1",   "2", "3", "OFF" },
  { "+/-", "0", ".", "ENT" },
};

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

// ── CHROME (static) ─────────────────────────────────────────────────────────────────
static void chromeScreen_LNCHAP(KCM_TFT &tft) {
  // Any full repaint (screen entry, display reset) starts with the keypad closed — the
  // modal is transient and must never persist across a leave/return, or the panel would
  // come back frozen with taps misrouted into a stale keypad.
  apKpOpen = false; apKpRedraw = false; apKpLen = 0; apKpEdit = -1;
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

// Format the live keypad entry with thousands separators in the integer part,
// preserving a leading sign and any decimal portion being typed.
static String apCommaEntry(const char *buf) {
  String work(buf), sign;
  if (work.length() && work[0] == '-') { sign = "-"; work = work.substring(1); }
  int dot = work.indexOf('.');
  String ip = (dot >= 0) ? work.substring(0, dot) : work;
  String fp = (dot >= 0) ? work.substring(dot)    : String("");
  String out; int len = ip.length();
  for (int i = 0; i < len; i++) {
    if (i > 0 && (len - i) % 3 == 0) out += ",";
    out += ip[i];
  }
  return sign + out + fp;
}

// ── KEYPAD draw ─────────────────────────────────────────────────────────────────────
static void apDrawKeypad(KCM_TFT &tft) {
  const ApEdit &e = AP_EDITS[apKpEdit];
  tft.fillRect(KP_X, KP_Y, KP_W, KP_H, TFT_OFF_BLACK);
  tft.drawRect(KP_X, KP_Y, KP_W, KP_H, TFT_SKY);
  tft.drawRect(KP_X + 1, KP_Y + 1, KP_W - 2, KP_H - 2, TFT_SKY);
  // Header: field name, range, live entry
  textLeft(tft, &Roboto_Black_24, KP_X + 10, KP_Y + 4, KP_W - 90, 28, e.name, TFT_WHITE, TFT_OFF_BLACK);
  { char r[48];
    if (e.mx >= 100000.0f) snprintf(r, sizeof(r), "enter in %s (min %g)", e.units, e.mn);
    else                   snprintf(r, sizeof(r), "range %g to %g %s", e.mn, e.mx, e.units);
    textLeft(tft, &Roboto_Black_16, KP_X + 10, KP_Y + 34, KP_W - 90, 20, r, TFT_LIGHT_GREY, TFT_OFF_BLACK); }
  { String ent = (apKpLen ? apCommaEntry(apKpBuf) : String("_")) + " " + e.units;
    textRight(tft, &Roboto_Black_28, KP_X + 6, KP_Y + 50, KP_W - 12, 28, ent, TFT_SKY, TFT_OFF_BLACK); }
  // CANCEL
  tft.fillRect(KP_CX, KP_CY, KP_CW, KP_CH, TFT_OFF_BLACK);
  tft.drawRect(KP_CX, KP_CY, KP_CW, KP_CH, TFT_RED);
  textCenter(tft, &Roboto_Black_24, KP_CX, KP_CY, KP_CW, KP_CH, "X", TFT_RED, TFT_OFF_BLACK);
  // Keys
  bool offOk = (e.kind == 2 || e.kind == 3);
  for (uint8_t r = 0; r < 4; r++)
    for (uint8_t c = 0; c < 4; c++) {
      const char *k = KP_KEYS[r][c];
      bool dis = (strcmp(k, "OFF") == 0 && !offOk);
      int16_t kx = KAX + c * AP_KEY_W, ky = KAY + r * AP_KEY_H;
      uint16_t kb = dis ? TFT_OFF_BLACK : TFT_GREY;
      uint16_t kf = dis ? TFT_DARK_GREY : (strcmp(k, "ENT") == 0 ? TFT_NEON_GREEN :
                    strcmp(k, "OFF") == 0 ? TFT_ORANGE : TFT_WHITE);
      tft.drawRect(kx, ky, AP_KEY_W - 4, AP_KEY_H - 4, kb);
      textCenter(tft, &Roboto_Black_28, kx, ky, AP_KEY_W - 4, AP_KEY_H - 4, k, kf, TFT_OFF_BLACK);
    }
}

// ── DRAW (dynamic) ──────────────────────────────────────────────────────────────────
static void drawScreen_LNCHAP(KCM_TFT &tft) {
  if (apScreenRefresh) {
    tft.fillRect(0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP, TFT_BLACK);
    chromeScreen_LNCHAP(tft);
    for (uint8_t r = 0; r < ROW_COUNT; r++) rowCache[screen_LNCHAP][r].value = "\x01";
    apScreenRefresh = false;
  }
  if (apKpOpen) {
    if (apKpRedraw) { apDrawKeypad(tft); apKpRedraw = false; }
    return;                       // freeze the panel behind the modal while editing
  }

  char buf[24];

  // Phase banner
  {
    uint16_t pc = apPhaseColor(state.apPhase);
    String pn = apPhaseName(state.apPhase);
    RowCache &rc = rowCache[screen_LNCHAP][AP_PHASE_SLOT];
    if (rc.value != pn || rc.fg != pc) {
      tft.fillRect(0, AP_BANNER_Y, 640, AP_BANNER_H, TFT_BLACK);
      textLeft(tft, AP_F_BANN, 8, AP_BANNER_Y, 632, AP_BANNER_H, pn.c_str(), pc, TFT_BLACK);
      rc.value = pn; rc.fg = pc;
    }
  }
  {
    bool armed = apGArmed();
    String bs = state.gameSOI;
    String as = armed ? "ARMED" : "DISARMED";
    uint16_t ac = armed ? TFT_NEON_GREEN : TFT_DARK_GREY;
    RowCache &rc = rowCache[screen_LNCHAP][AP_BODY_SLOT];
    String combo = bs + "|" + as;
    if (rc.value != combo || rc.fg != ac) {
      tft.fillRect(648, AP_BANNER_Y, CONTENT_W - 648, AP_BANNER_H, TFT_BLACK);
      textRight(tft, &Roboto_Black_20, 648, AP_BANNER_Y + 4,  CONTENT_W - 648 - 6, 24, bs.c_str(), AP_LBL, TFT_BLACK);
      textRight(tft, &Roboto_Black_28, 648, AP_BANNER_Y + 30, CONTENT_W - 648 - 6, 30, as.c_str(), ac, TFT_BLACK);
      rc.value = combo; rc.fg = ac;
    }
  }

  // Mission (inputs — pending shown in AP_EDT colour)
  apPut(tft, AP_TGTALT, formatAlt(apGTgt()), apEditColor(apDTgt));
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
    String v = String(pct);
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
  apPut(tft, AP_APA, formatAlt(state.apoapsis),  TFT_LIGHT_GREY);
  apPut(tft, AP_PE,  formatAlt(state.periapsis), TFT_LIGHT_GREY);

  // ARM control
  {
    bool armed = apGArmed();
    uint16_t pc = apPhaseColor(state.apPhase);
    const char *txt = armed ? apPhaseName(state.apPhase) : "ARM";
    uint16_t fg  = armed ? TFT_WHITE : AP_GUARD;
    uint16_t bg  = armed ? pc        : TFT_OFF_BLACK;
    uint16_t bdr = armed ? TFT_WHITE : AP_GUARD;
    RowCache &rc = rowCache[screen_LNCHAP][AP_ARM_SLOT];
    String key = String(armed ? "A:" : "D:") + txt;
    if (rc.value != key) {
      ButtonLabel btn = { txt, fg, fg, bg, bg, bdr, bdr };
      drawButton(tft, AP_ARM_X, AP_ARM_Y, AP_ARM_W, AP_ARM_H, btn, AP_F_ARM, false);
      const char *hint = armed ? "tap to DISARM" : "tap to ARM";
      textCenter(tft, &Roboto_Black_16, AP_ARM_X, AP_ARM_Y + AP_ARM_H - 24, AP_ARM_W, 18,
                 hint, armed ? TFT_WHITE : AP_GUARD, bg);
      rc.value = key;
    }
  }
}

// ── Keypad open / commit ────────────────────────────────────────────────────────────
static void apOpenKeypad(int8_t editIdx) {
  apKpEdit = editIdx; apKpLen = 0; apKpBuf[0] = '\0';
  apKpOpen = true; apKpRedraw = true;
}
static void apCloseKeypad() { apKpOpen = false; apScreenRefresh = true; }

// Commit the entered value into the pending buffer for the current field.
static void apCommitKeypad() {
  const ApEdit &e = AP_EDITS[apKpEdit];
  float v = (apKpLen ? atof(apKpBuf) : 0.0f);
  if (v < e.mn) v = e.mn; if (v > e.mx) v = e.mx;
  switch (e.slot) {
    case AP_TGTALT: apTgt = v; apDTgt = true; apEnqueueCmd(AP_CMD_SET_TARGET_ALT, v);  break;  // metres
    case AP_INCL:   apInc = v; apDInc = true; apEnqueueCmd(AP_CMD_SET_INCLINATION, v); break;
    case AP_LOFT:   apLof = v; apDLof = true; apEnqueueCmd(AP_CMD_SET_LOFT, v);        break;
    case AP_ROLL:   apRolDeg = v; apRolEn = true; apDRol = true; apEnqueueCmd(AP_CMD_SET_ROLL, v); break;
    case AP_MAXG:   apMxg = v; apDMxg = true; apEnqueueCmd(AP_CMD_SET_MAXG, v);        break;
  }
  apCloseKeypad();
}

// ── Touch entry point (called from TouchEvents.ino) ─────────────────────────────────
static void apKeypadTouch(uint16_t x, uint16_t y) {
  // CANCEL
  if (x >= KP_CX && x < KP_CX + KP_CW && y >= KP_CY && y < KP_CY + KP_CH) { apCloseKeypad(); return; }
  // Key grid
  if (x < KAX || y < KAY) return;
  int16_t c = (x - KAX) / AP_KEY_W, r = (y - KAY) / AP_KEY_H;
  if (c < 0 || c > 3 || r < 0 || r > 3) return;
  const char *k = KP_KEYS[r][c];
  const ApEdit &e = AP_EDITS[apKpEdit];
  if (strcmp(k, "ENT") == 0) { apCommitKeypad(); return; }
  if (strcmp(k, "CLR") == 0) { apKpLen = 0; apKpBuf[0] = '\0'; apKpRedraw = true; return; }
  if (strcmp(k, "DEL") == 0) { if (apKpLen) { apKpBuf[--apKpLen] = '\0'; apKpRedraw = true; } return; }
  if (strcmp(k, "OFF") == 0) {
    if (e.kind == 2)      { apRolEn = false; apDRol = true; apEnqueueCmd(AP_CMD_SET_ROLL, AP_ROLL_OFF); apCloseKeypad(); }
    else if (e.kind == 3) { apMxg = 0.0f;    apDMxg = true; apEnqueueCmd(AP_CMD_SET_MAXG, 0.0f);        apCloseKeypad(); }
    return;
  }
  if (strcmp(k, "+/-") == 0) {                      // sign toggle
    if (apKpLen && apKpBuf[0] == '-') { memmove(apKpBuf, apKpBuf + 1, apKpLen); apKpLen--; }
    else if (apKpLen < sizeof(apKpBuf) - 1) { memmove(apKpBuf + 1, apKpBuf, apKpLen + 1); apKpBuf[0] = '-'; apKpLen++; }
    apKpRedraw = true; return;
  }
  // digit or '.'
  if (apKpLen < sizeof(apKpBuf) - 1) {
    if (k[0] == '.' && strchr(apKpBuf, '.')) return;   // one decimal point
    apKpBuf[apKpLen++] = k[0]; apKpBuf[apKpLen] = '\0'; apKpRedraw = true;
  }
}

// Public: routed here for any content-area tap while the AP screen is active.
void apScreenTouch(uint16_t x, uint16_t y) {
  if (apKpOpen) { apKeypadTouch(x, y); return; }

  // ARM / DISARM
  if (x >= AP_ARM_X && x < AP_ARM_X + AP_ARM_W && y >= AP_ARM_Y && y < AP_ARM_Y + AP_ARM_H) {
    bool wantArm = !apGArmed();
    apArmOvr = wantArm ? 1 : 0;
    rowCache[screen_LNCHAP][AP_ARM_SLOT].value = "\x01";   // force ARM + banner redraw
    rowCache[screen_LNCHAP][AP_BODY_SLOT].value = "\x01";
    apEnqueueCmd(wantArm ? AP_CMD_ARM : AP_CMD_DISARM, 0.0f);
    return;
  }
  // Editable fields
  for (uint8_t i = 0; i < 6; i++) {
    const ApEdit &e = AP_EDITS[i];
    int16_t cx = AP_CELLS[e.slot].x, cy = apRowY(AP_CELLS[e.slot].row);
    if (x >= cx && x < cx + AP_COLW && y >= cy && y < cy + AP_ROW_H) {
      if (e.kind == 1) {                             // Launch N/S toggle
        apDir = !apGDir(); apDDir = true;
        rowCache[screen_LNCHAP][e.slot].value = "\x01";
        apEnqueueCmd(AP_CMD_SET_LAUNCH_DIR, apDir ? 1.0f : 0.0f);
      } else {
        apOpenKeypad(i);
      }
      return;
    }
  }
}

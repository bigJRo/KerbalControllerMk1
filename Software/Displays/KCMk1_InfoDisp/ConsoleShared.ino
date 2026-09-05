/***************************************************************************************
   ConsoleShared.ino -- Infrastructure shared by the three autopilot consoles
   (ASCENT AUTOPILOT, AIRCRAFT AUTOPILOT, ROVER AUTOPILOT).

   Three things live here so that every console uses one copy:
     1. The outbound command queue (InfoDisp -> Controller_Main). One command in flight
        at a time, sequence-numbered, acknowledged by the master echoing the sequence.
        Moved out of Screen_LNCH_AscentAP.ino unchanged when the second and third
        consoles arrived; the byte-level contract is unchanged
        (Documents/Developer/Ascent_Autopilot_Interface.md).
     2. The numeric keypad modal. A console opens it with a KpField descriptor and a
        commit callback; the keypad owns the touch surface until ENT / OFF / CANCEL.
     3. Drawing helpers for the shared console grid: column headers, mode buttons in
        their three states, value boxes, cached value text.

   This tab sorts after AAA_* (so TITLE_TOP and the row helpers exist) and before the
   Screen_* tabs that call it. API declarations are in KCMk1_InfoDisp.h.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

// ── Palette (the ascent console's) ────────────────────────────────────────────────────
static const uint16_t CON_LBL = KDC_LABEL_COLOR;
static const uint16_t CON_HDR = TFT_WHITE;
static const uint16_t CON_BOX = TFT_GREY;
static const uint16_t CON_EDT = TFT_SKY;
#define CON_F_HDR (&Roboto_Black_28)
#define CON_F_VAL (&Roboto_Black_32)
#define CON_F_BTN (&Roboto_Black_24)


/***************************************************************************************
   1. OUTBOUND COMMAND CHANNEL
   Pilot edits and mode taps queue here as (opcode, float-payload) commands. The I2C
   master reads the head command from the outbound packet, applies it, then
   acknowledges it by echoing the command's sequence number in its next inbound packet,
   which pops the queue. Pending (cyan) cues clear separately, once the master echoes the
   accepted value or mode back in a status frame — so the UI confirms the round trip,
   not merely command delivery.
****************************************************************************************/
struct ApCmd { uint8_t op; float payload; };
static const uint8_t AP_CMDQ_LEN = 16;
static ApCmd   apCmdQ[AP_CMDQ_LEN];
static uint8_t apCmdHead = 0, apCmdTail = 0;   // ring-buffer indices (empty when equal)
static uint8_t apCmdCurSeq = 0;                // seq of head command in flight (0 = none)
static uint8_t apCmdSeqCtr = 0;                // monotonic seq generator, wraps 1..255

// Returns true when the command was actually queued. Callers that show a pending cue
// must gate it on this: a cue for a command that was never sent would never clear.
bool apEnqueueCmd(uint8_t op, float payload) {
#if INFO_DISP_IS_PFD_UNIT
  // Only the mission panel (unit 2) owns the autopilots. Unit 1 has no console key, so
  // it should never reach here — this is the backstop that makes the single-editor rule
  // a property of the command channel rather than of the navigation table.
  (void)op; (void)payload;
  return false;
#else
  // No master to drain the queue in demo — the demo applies the command to its own
  // autopilot models and publishes the result on the next frame, so the console's
  // round trip closes and its pending cue clears exactly as it does in flight.
  if (demoMode) return apDemoApplyCommand(op, payload) || hpDemoApplyCommand(op, payload);
  uint8_t next = (uint8_t)((apCmdTail + 1) % AP_CMDQ_LEN);
  if (next == apCmdHead) return false;         // full — drop (queue holds 15, never happens)
  apCmdQ[apCmdTail].op = op;
  apCmdQ[apCmdTail].payload = payload;
  apCmdTail = next;
  return true;
#endif
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


/***************************************************************************************
   2. NUMERIC KEYPAD
****************************************************************************************/
static bool       kpOpenFlag   = false;
static bool       kpRedraw     = false;
static bool       kpClosedFlag = false;
static KpField    kpField      = { "", "", 0.0f, 0.0f, false, false, 0 };
static int8_t     kpIdx        = -1;
static KpCommitFn kpCommit     = nullptr;
static char       kpBuf[12];
static uint8_t    kpLen        = 0;

static const int16_t KP_W = 480, KP_H = 384;
static const int16_t KP_X = (CONTENT_W - KP_W) / 2;                        // 230
static const int16_t KP_Y = TITLE_TOP + (SCREEN_H - TITLE_TOP - KP_H) / 2; // ~139
static const int16_t KP_HDR_H = 80;
static const int16_t KAX = KP_X + 6, KAY = KP_Y + KP_HDR_H;
static const int16_t KAW = KP_W - 12, KAH = KP_H - KP_HDR_H - 6;
static const int16_t KP_KEY_W = KAW / 4, KP_KEY_H = KAH / 4;
static const int16_t KP_CX = KP_X + KP_W - 74, KP_CY = KP_Y + 8, KP_CW = 66, KP_CH = 40;  // CANCEL

// Key labels (row-major).
static const char *const KP_KEYS[4][4] = {
  { "7",   "8", "9", "DEL" },
  { "4",   "5", "6", "CLR" },
  { "1",   "2", "3", "OFF" },
  { "+/-", "0", ".", "ENT" },
};

void kpOpen(const KpField &f, int8_t idx, KpCommitFn onCommit) {
  kpField = f; kpIdx = idx; kpCommit = onCommit;
  kpLen = 0; kpBuf[0] = '\0';
  kpOpenFlag = true; kpRedraw = true;
}

static void kpClose() { kpOpenFlag = false; kpClosedFlag = true; }

bool kpIsOpen()     { return kpOpenFlag; }
bool kpTakeClosed() { bool c = kpClosedFlag; kpClosedFlag = false; return c; }
void kpForceClose() { kpOpenFlag = false; kpRedraw = false; kpClosedFlag = false; kpLen = 0; kpIdx = -1; }

// Format the live entry with thousands separators in the integer part, preserving a
// leading sign and any decimal portion being typed.
static String kpCommaEntry(const char *buf) {
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

void kpDraw(KCM_TFT &tft) {
  if (!kpOpenFlag || !kpRedraw) return;
  kpRedraw = false;
  const KpField &e = kpField;
  tft.fillRect(KP_X, KP_Y, KP_W, KP_H, TFT_OFF_BLACK);
  tft.drawRect(KP_X, KP_Y, KP_W, KP_H, TFT_SKY);
  tft.drawRect(KP_X + 1, KP_Y + 1, KP_W - 2, KP_H - 2, TFT_SKY);
  // Header: field name, range, live entry
  textLeft(tft, &Roboto_Black_24, KP_X + 10, KP_Y + 4, KP_W - 90, 28, e.name, TFT_WHITE, TFT_OFF_BLACK);
  { char r[48];
    if (e.mx >= 100000.0f) snprintf(r, sizeof(r), "enter in %s (min %g)", e.units, e.mn);
    else                   snprintf(r, sizeof(r), "range %g to %g %s", e.mn, e.mx, e.units);
    textLeft(tft, &Roboto_Black_16, KP_X + 10, KP_Y + 34, KP_W - 90, 20, r, TFT_LIGHT_GREY, TFT_OFF_BLACK); }
  { String ent = (kpLen ? kpCommaEntry(kpBuf) : String("_")) + " " + e.units;
    textRight(tft, &Roboto_Black_28, KP_X + 6, KP_Y + 50, KP_W - 12, 28, ent, TFT_SKY, TFT_OFF_BLACK); }
  // CANCEL
  tft.fillRect(KP_CX, KP_CY, KP_CW, KP_CH, TFT_OFF_BLACK);
  tft.drawRect(KP_CX, KP_CY, KP_CW, KP_CH, TFT_RED);
  textCenter(tft, &Roboto_Black_24, KP_CX, KP_CY, KP_CW, KP_CH, "X", TFT_RED, TFT_OFF_BLACK);
  // Keys
  for (uint8_t r = 0; r < 4; r++)
    for (uint8_t c = 0; c < 4; c++) {
      const char *k = KP_KEYS[r][c];
      bool dis = (strcmp(k, "OFF") == 0 && !e.allowOff) || (strcmp(k, "+/-") == 0 && !e.allowSign);
      int16_t kx = KAX + c * KP_KEY_W, ky = KAY + r * KP_KEY_H;
      uint16_t kb = dis ? TFT_OFF_BLACK : TFT_GREY;
      uint16_t kf = dis ? TFT_DARK_GREY : (strcmp(k, "ENT") == 0 ? TFT_NEON_GREEN :
                    strcmp(k, "OFF") == 0 ? TFT_ORANGE : TFT_WHITE);
      tft.drawRect(kx, ky, KP_KEY_W - 4, KP_KEY_H - 4, kb);
      textCenter(tft, &Roboto_Black_28, kx, ky, KP_KEY_W - 4, KP_KEY_H - 4, k, kf, TFT_OFF_BLACK);
    }
}

static void kpDoCommit(bool off) {
  float v = (kpLen ? atof(kpBuf) : 0.0f);
  if (v < kpField.mn) v = kpField.mn;
  if (v > kpField.mx) v = kpField.mx;
  KpCommitFn fn = kpCommit;
  int8_t idx = kpIdx;
  kpClose();
  if (fn) fn(idx, v, off);
}

void kpTouch(uint16_t x, uint16_t y) {
  if (!kpOpenFlag) return;
  // CANCEL
  if (x >= KP_CX && x < KP_CX + KP_CW && y >= KP_CY && y < KP_CY + KP_CH) { kpClose(); return; }
  // Key grid
  if (x < KAX || y < KAY) return;
  int16_t c = (x - KAX) / KP_KEY_W, r = (y - KAY) / KP_KEY_H;
  if (c < 0 || c > 3 || r < 0 || r > 3) return;
  const char *k = KP_KEYS[r][c];
  if (strcmp(k, "ENT") == 0) { kpDoCommit(false); return; }
  if (strcmp(k, "CLR") == 0) { kpLen = 0; kpBuf[0] = '\0'; kpRedraw = true; return; }
  if (strcmp(k, "DEL") == 0) { if (kpLen) { kpBuf[--kpLen] = '\0'; kpRedraw = true; } return; }
  if (strcmp(k, "OFF") == 0) { if (kpField.allowOff) kpDoCommit(true); return; }
  if (strcmp(k, "+/-") == 0) {                      // sign toggle
    if (!kpField.allowSign) return;
    if (kpLen && kpBuf[0] == '-') { memmove(kpBuf, kpBuf + 1, kpLen); kpLen--; }
    else if (kpLen < sizeof(kpBuf) - 1) { memmove(kpBuf + 1, kpBuf, kpLen + 1); kpBuf[0] = '-'; kpLen++; }
    kpRedraw = true; return;
  }
  // digit or '.'
  if (kpLen < sizeof(kpBuf) - 1) {
    if (k[0] == '.' && strchr(kpBuf, '.')) return;   // one decimal point
    kpBuf[kpLen++] = k[0]; kpBuf[kpLen] = '\0'; kpRedraw = true;
  }
}


/***************************************************************************************
   3. DRAWING HELPERS
****************************************************************************************/
// Pick a legible text colour for an arbitrary RGB565 background by its perceived
// luminance (Rec. 601). White washes out on the light colours (CYAN, SKY); on those we
// use TFT_DARK_GREY — the standard dark-on-light button text used elsewhere.
uint16_t conTextOn(uint16_t bg) {
  uint16_t r = ((bg >> 11) & 0x1F) * 255 / 31;
  uint16_t g = ((bg >> 5)  & 0x3F) * 255 / 63;
  uint16_t b = ( bg        & 0x1F) * 255 / 31;
  uint16_t lum = (uint16_t)((r * 299 + g * 587 + b * 114) / 1000);
  return (lum > 140) ? TFT_DARK_GREY : TFT_WHITE;
}

// Banner rule, column dividers and the three column headers.
void conDrawColumns(KCM_TFT &tft, const char *h1, const char *h2, const char *h3) {
  tft.fillRect(0, CON_BANNER_Y + CON_BANNER_H, CONTENT_W, 2, TFT_GREY);
  tft.drawLine(CON_DIV1_X, CON_COL_Y, CON_DIV1_X, CON_COL_BOT, TFT_GREY);
  tft.drawLine(CON_DIV2_X, CON_COL_Y, CON_DIV2_X, CON_COL_BOT, TFT_GREY);
  textLeft(tft, CON_F_HDR, CON_C1X, CON_COL_Y, CON_COLW, 32, h1, CON_HDR, TFT_BLACK);
  textLeft(tft, CON_F_HDR, CON_C2X, CON_COL_Y, CON_COLW, 32, h2, CON_HDR, TFT_BLACK);
  textLeft(tft, CON_F_HDR, CON_C3X, CON_COL_Y, CON_COLW, 32, h3, CON_HDR, TFT_BLACK);
}

// A mode button in one of its three states. Off: grey outline, white caption.
// Pending: cyan 3 px outline, cyan caption (tap sent, not yet echoed). Engaged: green fill.
void conDrawModeButton(KCM_TFT &tft, int16_t x, int16_t y, int16_t w, int16_t h,
                       const char *label, ConBtnState st) {
  tft.fillRect(x, y, w, h, TFT_BLACK);
  uint16_t fg;
  switch (st) {
    case CON_BTN_ON:
      tft.fillRect(x, y, w, h, TFT_DARK_GREEN);
      fg = TFT_WHITE;
      break;
    case CON_BTN_PENDING:
      tft.drawRect(x, y, w, h, TFT_CYAN);
      tft.drawRect(x + 1, y + 1, w - 2, h - 2, TFT_CYAN);
      tft.drawRect(x + 2, y + 2, w - 4, h - 4, TFT_CYAN);
      fg = TFT_CYAN;
      break;
    default:
      tft.drawRect(x, y, w, h, TFT_GREY);
      fg = TFT_WHITE;
      break;
  }
  textCenter(tft, CON_F_BTN, x, y, w, h, label, fg, st == CON_BTN_ON ? TFT_DARK_GREEN : TFT_BLACK);
}

// The outlined value box at the right of a row.
void conDrawValueBox(KCM_TFT &tft, int16_t colX, uint8_t row) {
  tft.drawRect(conValX(colX), conRowY(row) + 4, CON_VALW - 2, CON_VALH, CON_BOX);
}

// Cached value draw. The cache compare allocates nothing; the String is built only for
// a value about to be drawn.
void conPut(KCM_TFT &tft, ScreenType screen, uint8_t slot, int16_t x, int16_t y,
            int16_t w, int16_t h, const char *val, uint16_t fg) {
  RowCache &rc = rowCache[(uint8_t)screen][slot];
  if (rc.fg == fg && rc.value == val) return;
  String v(val);
  printValue(tft, CON_F_VAL, x, y, w, h, "", v, fg, TFT_BLACK, TFT_BLACK, printState[(uint8_t)screen][slot]);
  rc.value = v; rc.fg = fg; rc.bg = TFT_BLACK;
}

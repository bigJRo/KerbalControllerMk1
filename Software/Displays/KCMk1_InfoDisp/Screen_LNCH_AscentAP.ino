/***************************************************************************************
   Screen_LNCH_AscentAP.ino — Ascent Autopilot interface (chrome + draw)

   Touch UI for the Simpit ascent autopilot. Renders the autopilot status echoed
   from Controller_Main (AscentStatus, carried in the shared `state` struct) and —
   in a later pass — lets the pilot edit parameters via an on-screen keypad and
   ARM/DISARM by touch. This file is the display + layout (M1). Touch handling and
   the on-screen keypad live in Screen_LNCH_AscentAP_Touch.ino / TouchEvents.ino.

   LAYOUT (1024x600, content 0..CONTENT_W, y from TITLE_TOP):
     ┌ ASCENT AUTOPILOT ──────────────────────────────────┐
     │  <PHASE, coloured>                body    <ARMED>   │  banner
     ├───────────────┬───────────────┬────────────────────┤
     │ MISSION       │ VEH PROFILE   │ GUIDANCE            │
     │ Tgt Ap  [box] │ Loft   [box]  │ Pitch    +42°       │
     │  Actual       │ Roll   [box]  │ Hdg       090°      │
     │ Incl    [box] │ Max-G  [box]  │ Thr   ▓▓▓░ 78%      │
     │ Launch  [box] │               │ G / q / PeA         │
     ├───────────────┴───────────────┴────────────────────┤
     │                  [ ARM / phase ]                    │  guarded control
     └─────────────────────────────────────────────────────┘
   Boxed fields are the disarmed-only touch targets (keypad / toggle).
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

// ── Palette ───────────────────────────────────────────────────────────────────────
static const uint16_t AP_LBL = TFT_LIGHT_GREY;   // field labels
static const uint16_t AP_VAL = TFT_DARK_GREEN;   // default readout value
static const uint16_t AP_HDR = TFT_WHITE;        // column headers
static const uint16_t AP_BOX = TFT_GREY;         // editable-field border
static const uint16_t AP_GUARD = TFT_ORANGE;     // guarded ARM (disarmed) accent

// ── Geometry ──────────────────────────────────────────────────────────────────────
static const int16_t AP_BANNER_Y = TITLE_TOP;                 // 62
static const int16_t AP_BANNER_H = 64;
static const int16_t AP_COL_Y    = AP_BANNER_Y + AP_BANNER_H + 6;   // 132 — headers
static const int16_t AP_ROW_Y0   = AP_COL_Y + 32;                   // 164 — first field row
static const int16_t AP_ROW_H    = 48;
static const int16_t AP_COL_BOT  = AP_ROW_Y0 + 6 * AP_ROW_H;        // 452 — column bottom (6 rows)
static const int16_t AP_C1X = 6,   AP_C2X = 322, AP_C3X = 638;
static const int16_t AP_COLW = 298;
static const int16_t AP_DIV1_X = 314, AP_DIV2_X = 630;
static const int16_t AP_VALW = 160;                                 // value sub-cell width (right side of a cell)
static const int16_t AP_ARM_Y = AP_COL_BOT + 8;                     // 460
static const int16_t AP_ARM_H = SCREEN_H - AP_ARM_Y - 4;            // ~136
static const int16_t AP_ARM_X = 6, AP_ARM_W = CONTENT_W - 12;

// Row Y for field row i (0-based within a column)
static inline int16_t apRowY(uint8_t i) { return AP_ROW_Y0 + i * AP_ROW_H; }

// ── Field table ───────────────────────────────────────────────────────────────────
// slot = rowCache index; also the touch-target id (M2). editable => boxed + keypad/toggle.
enum {
  AP_TGTALT = 0, AP_ACTAP, AP_INCL, AP_DIR,          // mission (col 1)
  AP_LOFT, AP_ROLL, AP_MAXG,                          // profile (col 2)
  AP_PITCH, AP_HDG, AP_THR, AP_G, AP_Q, AP_PE,        // guidance (col 3)
  AP_SLOT_COUNT
};
static const uint8_t AP_ARM_SLOT   = 20;   // ARM button cache slot
static const uint8_t AP_PHASE_SLOT = 21;   // phase banner cache slot
static const uint8_t AP_BODY_SLOT  = 22;   // body / armed banner cache slot

struct ApCell { int16_t x, y; const char *label; bool edit; };
static const ApCell AP_CELLS[AP_SLOT_COUNT] = {
  { AP_C1X, 0,           "Tgt Ap:", true  },   // AP_TGTALT
  { AP_C1X, 0,           "Actual:", false },   // AP_ACTAP
  { AP_C1X, 0,           "Incl:",   true  },   // AP_INCL
  { AP_C1X, 0,           "Launch:", true  },   // AP_DIR
  { AP_C2X, 0,           "Loft:",   true  },   // AP_LOFT
  { AP_C2X, 0,           "Roll:",   true  },   // AP_ROLL
  { AP_C2X, 0,           "Max-G:",  true  },   // AP_MAXG
  { AP_C3X, 0,           "Pitch:",  false },   // AP_PITCH
  { AP_C3X, 0,           "Hdg:",    false },   // AP_HDG
  { AP_C3X, 0,           "Thr:",    false },   // AP_THR
  { AP_C3X, 0,           "G:",      false },   // AP_G
  { AP_C3X, 0,           "q:",      false },   // AP_Q
  { AP_C3X, 0,           "PeA:",    false },   // AP_PE
};
// Row index (within its column) per slot.
static inline uint8_t apCellRow(uint8_t slot) {
  switch (slot) {
    case AP_TGTALT: return 0; case AP_ACTAP: return 1; case AP_INCL: return 2; case AP_DIR: return 3;
    case AP_LOFT:   return 0; case AP_ROLL:  return 1; case AP_MAXG: return 2;
    case AP_PITCH:  return 0; case AP_HDG:   return 1; case AP_THR:  return 2;
    case AP_G:      return 3; case AP_Q:     return 4; case AP_PE:   return 5;
    default: return 0;
  }
}
// Value sub-cell rect for a slot (right side of its cell).
static inline int16_t apValX(uint8_t slot) { return AP_CELLS[slot].x + AP_COLW - AP_VALW; }
static inline int16_t apValY(uint8_t slot) { return apRowY(apCellRow(slot)) + 3; }
static const int16_t  AP_VALH = AP_ROW_H - 6;

// ── Phase name / colour ─────────────────────────────────────────────────────────────
const char *apPhaseName(uint8_t p) {
  switch (p) {
    case 1: return "VERTICAL";
    case 2: return "GRAVITY TURN";
    case 3: return "COAST";
    case 4: return "CIRCULARIZE";
    case 5: return "COMPLETE";
    case 6: return "ABORT";
    default: return "IDLE";
  }
}
static uint16_t apPhaseColor(uint8_t p) {
  switch (p) {
    case 1: case 2: return TFT_DARK_GREEN;   // powered ascent
    case 3: case 4: return TFT_CYAN;         // coast / circularize
    case 5:         return TFT_SKY;          // complete
    case 6:         return TFT_RED;          // abort
    default:        return TFT_DARK_GREY;    // idle
  }
}

// ── CHROME (static) ─────────────────────────────────────────────────────────────────
static void chromeScreen_LNCHAP(KCM_TFT &tft) {
  // Banner divider + column dividers
  tft.fillRect(0, AP_BANNER_Y + AP_BANNER_H, CONTENT_W, 2, TFT_GREY);
  tft.drawLine(AP_DIV1_X, AP_COL_Y, AP_DIV1_X, AP_COL_BOT, TFT_GREY);
  tft.drawLine(AP_DIV2_X, AP_COL_Y, AP_DIV2_X, AP_COL_BOT, TFT_GREY);

  // Column headers
  textLeft(tft, &Roboto_Black_24, AP_C1X, AP_COL_Y, AP_COLW, 28, "MISSION",     AP_HDR, TFT_BLACK);
  textLeft(tft, &Roboto_Black_24, AP_C2X, AP_COL_Y, AP_COLW, 28, "VEH PROFILE", AP_HDR, TFT_BLACK);
  textLeft(tft, &Roboto_Black_24, AP_C3X, AP_COL_Y, AP_COLW, 28, "GUIDANCE",    AP_HDR, TFT_BLACK);

  // Field labels + editable boxes
  for (uint8_t s = 0; s < AP_SLOT_COUNT; s++) {
    const ApCell &c = AP_CELLS[s];
    int16_t y = apRowY(apCellRow(s));
    textLeft(tft, &Roboto_Black_20, c.x + 2, y + 8, AP_COLW - AP_VALW, 24, c.label, AP_LBL, TFT_BLACK);
    if (c.edit) tft.drawRect(apValX(s), apValY(s), AP_VALW - 2, AP_VALH, AP_BOX);
  }

  // "disarmed-only" hint above the profile-editable region
  textLeft(tft, &Roboto_Black_16, AP_C1X, AP_COL_BOT + 2, CONTENT_W - 12, 18,
           "Boxed fields are editable while DISARMED — tap to set.", TFT_DARK_GREY, TFT_BLACK);
}

// ── Value helper (flicker-free) ─────────────────────────────────────────────────────
static void apPut(KCM_TFT &tft, uint8_t slot, const String &val, uint16_t fg) {
  RowCache &rc = rowCache[screen_LNCHAP][slot];
  if (rc.value == val && rc.fg == fg) return;
  printValue(tft, &Roboto_Black_28, apValX(slot), apValY(slot), AP_VALW - 2, AP_VALH,
             "", val, fg, TFT_BLACK, TFT_BLACK, printState[screen_LNCHAP][slot]);
  rc.value = val; rc.fg = fg; rc.bg = TFT_BLACK;
}

// ── DRAW (dynamic) ──────────────────────────────────────────────────────────────────
static void drawScreen_LNCHAP(KCM_TFT &tft) {
  char buf[24];

  // ── Phase banner ──
  {
    uint16_t pc = apPhaseColor(state.apPhase);
    String pn = apPhaseName(state.apPhase);
    RowCache &rc = rowCache[screen_LNCHAP][AP_PHASE_SLOT];
    if (rc.value != pn || rc.fg != pc) {
      tft.fillRect(0, AP_BANNER_Y, 640, AP_BANNER_H, TFT_BLACK);
      textLeft(tft, &Roboto_Black_36, 8, AP_BANNER_Y + 14, 632, 44, pn.c_str(), pc, TFT_BLACK);
      rc.value = pn; rc.fg = pc;
    }
  }
  // Body + armed indicator (right of banner)
  {
    String bs = state.gameSOI;
    String as = state.apArmed ? "ARMED" : "DISARMED";
    uint16_t ac = state.apArmed ? TFT_NEON_GREEN : TFT_DARK_GREY;
    RowCache &rc = rowCache[screen_LNCHAP][AP_BODY_SLOT];
    String combo = bs + "|" + as;
    if (rc.value != combo || rc.fg != ac) {
      tft.fillRect(648, AP_BANNER_Y, CONTENT_W - 648, AP_BANNER_H, TFT_BLACK);
      textRight(tft, &Roboto_Black_20, 648, AP_BANNER_Y + 6,  CONTENT_W - 648 - 6, 22, bs.c_str(), AP_LBL, TFT_BLACK);
      textRight(tft, &Roboto_Black_28, 648, AP_BANNER_Y + 30, CONTENT_W - 648 - 6, 30, as.c_str(), ac, TFT_BLACK);
      rc.value = combo; rc.fg = ac;
    }
  }

  // ── Mission ──
  apPut(tft, AP_TGTALT, formatAlt(state.apTargetAlt), AP_VAL);
  apPut(tft, AP_ACTAP,  formatAlt(state.apoapsis),    TFT_LIGHT_GREY);
  snprintf(buf, sizeof(buf), "%.1f\xB0", state.apInclination);
  apPut(tft, AP_INCL, buf, AP_VAL);
  apPut(tft, AP_DIR, state.apSoutherly ? "SOUTH" : "NORTH", AP_VAL);

  // ── Vehicle profile ──
  { char b2[12]; dtostrf(state.apLoft, 1, 2, b2); apPut(tft, AP_LOFT, b2, AP_VAL); }
  if (state.apRollEnable) { snprintf(buf, sizeof(buf), "%+.0f\xB0", state.apRollDeg); apPut(tft, AP_ROLL, buf, AP_VAL); }
  else                    { apPut(tft, AP_ROLL, "OFF", TFT_DARK_GREY); }
  if (state.apMaxG > 0.0f) { snprintf(buf, sizeof(buf), "%.1f g", state.apMaxG); apPut(tft, AP_MAXG, buf, AP_VAL); }
  else                     { apPut(tft, AP_MAXG, "OFF", TFT_DARK_GREY); }

  // ── Guidance ──
  snprintf(buf, sizeof(buf), "%+.0f\xB0", state.apCmdPitch);         apPut(tft, AP_PITCH, buf, AP_VAL);
  { int16_t h = (int16_t)roundf(state.apCmdHeading) % 360; if (h < 0) h += 360;
    snprintf(buf, sizeof(buf), "%03d\xB0", h); apPut(tft, AP_HDG, buf, AP_VAL); }

  // Throttle: bar + percent, custom draw with a cache
  {
    int16_t pct = (int16_t)roundf(state.apCmdThrottle * 100.0f);
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    RowCache &rc = rowCache[screen_LNCHAP][AP_THR];
    String v = String(pct);
    if (rc.value != v) {
      int16_t bx = apValX(AP_THR), by = apValY(AP_THR), bw = AP_VALW - 2, bh = AP_VALH;
      int16_t barW = 70, fillW = (int16_t)(barW * pct / 100);
      tft.fillRect(bx, by, bw, bh, TFT_BLACK);
      tft.drawRect(bx, by + (bh - 16) / 2, barW, 16, TFT_GREY);
      if (fillW > 2) tft.fillRect(bx + 1, by + (bh - 16) / 2 + 1, fillW - 2, 14, TFT_DARK_GREEN);
      snprintf(buf, sizeof(buf), "%d%%", pct);
      textRight(tft, &Roboto_Black_28, bx + barW, by, bw - barW, bh, buf, AP_VAL, TFT_BLACK);
      rc.value = v;
    }
  }

  // G-force, coloured against the Max-G cap
  {
    uint16_t gc = AP_VAL;
    if (state.apMaxG > 0.0f) {
      if (state.gForce >= state.apMaxG)          gc = TFT_RED;
      else if (state.gForce >= 0.9f * state.apMaxG) gc = TFT_YELLOW;
    }
    snprintf(buf, sizeof(buf), "%.1f g", state.gForce);
    apPut(tft, AP_G, buf, gc);
  }
  // Dynamic pressure
  if (state.apDynPressure >= 1000.0f) snprintf(buf, sizeof(buf), "%.1f kPa", state.apDynPressure / 1000.0f);
  else                                snprintf(buf, sizeof(buf), "%.0f Pa",  state.apDynPressure);
  apPut(tft, AP_Q, buf, AP_VAL);
  // Periapsis (actual)
  apPut(tft, AP_PE, formatAlt(state.periapsis), TFT_LIGHT_GREY);

  // ── ARM control ──
  {
    bool armed = state.apArmed;
    uint16_t pc = apPhaseColor(state.apPhase);
    // Disarmed: guarded amber "ARM". Armed: lit in phase colour, labelled with the phase.
    const char *txt = armed ? apPhaseName(state.apPhase) : "ARM";
    uint16_t fg  = armed ? TFT_WHITE  : AP_GUARD;
    uint16_t bg  = armed ? pc         : TFT_OFF_BLACK;
    uint16_t bdr = armed ? TFT_WHITE  : AP_GUARD;
    RowCache &rc = rowCache[screen_LNCHAP][AP_ARM_SLOT];
    String key = String(armed ? "A:" : "D:") + txt;
    if (rc.value != key) {
      ButtonLabel btn = { txt, fg, fg, bg, bg, bdr, bdr };
      drawButton(tft, AP_ARM_X, AP_ARM_Y, AP_ARM_W, AP_ARM_H, btn, &Roboto_Black_36, false);
      // Sub-label so the action is unambiguous
      const char *hint = armed ? "tap to DISARM" : "tap to ARM";
      textCenter(tft, &Roboto_Black_16, AP_ARM_X, AP_ARM_Y + AP_ARM_H - 24, AP_ARM_W, 18,
                 hint, armed ? TFT_WHITE : AP_GUARD, bg);
      rc.value = key;
    }
  }
}

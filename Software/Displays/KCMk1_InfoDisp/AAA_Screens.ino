/***************************************************************************************
   AAA_Screens.ino -- Shared screen infrastructure for Kerbal Controller Mk1 Information Display
   Must compile before Screen_*.ino tabs (AAA_ prefix ensures correct sort order).

   Sidebar buttons (8, top-to-bottom) — decoupled from ScreenType via SB_BTN_SCREEN.
   Multi-mode buttons CYCLE their modes when pressed while already active, and the
   button caption shows the active mode (sbButtonLabel); a first press from another
   screen goes to the button's context/primary mode. Context auto-select still runs on
   scene/vessel change; a press latches a manual override (see TouchEvents.ino). Title-
   bar taps no longer switch anything. The ASC (Ascent Autopilot) button is parked at
   the bottom, physically separated and drawn in a distinct purple, since it is an
   interactive console rather than a display screen.
     0  LNCH      Launch — LNCH / PRE (auto on pad) / ASC <-> CIRC (press cycles)
     1  PFD       Primary Flight Display — SPC / ACFT / ROVR (press cycles; context default)
     2  ORB       Orbit — ORB <-> ORB+ (Advanced Elements)
     3  VEH       Vehicle Information
     4  MNVR      Maneuver Information
     5  TGT/DOCK  Target / Docking — TGT <-> DOCK (context default: DOCK when near a target)
     6  LNDG/ENTR Landing — DESC (powered descent) <-> ENTR (re-entry)
     7  ASC       Ascent Autopilot (touch console for the Simpit ascent autopilot)

   Layout (1024x600):
     Title bar  : 62px (58px text + 4px rule)
     Data rows  : text screens fill the content height evenly
     Sidebar    : 84px right-hand column, 10 labelled buttons

   Update pattern (mirrors Annunciator):
     Chrome (labels)  : printDispChrome() — called once per screen transition.
     Values           : printValue()      — called only when state != prev.
                        Draws only the right-hand value region, label untouched.
     Colour changes   : tracked via prev colour fields per row.
     This avoids all unnecessary SPI traffic and eliminates label flicker entirely.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

/***************************************************************************************
   LAYOUT CONSTANTS
****************************************************************************************/
const uint16_t CONTENT_W = SCREEN_W - SIDEBAR_W;

const uint16_t TITLE_H = 58;
const uint16_t TITLE_RULE_H = 4;
const uint16_t TITLE_TOP = TITLE_H + TITLE_RULE_H;

const tFont *TITLE_FONT = &Roboto_Black_36;
const tFont *ROW_FONT = &Roboto_Black_40;  // all screen row labels
const uint16_t ROW_PAD = 2;

const uint16_t COL_LABEL = TFT_WHITE;
const uint16_t COL_VALUE = TFT_DARK_GREEN;
const uint16_t COL_BACK = TFT_BLACK;
const uint16_t COL_NO_BDR = TFT_BLACK;

const uint8_t SB_BTN_COUNT = 8;
// Multi-mode button indices. Each of these cycles its modes on a repeated press and
// shows the active mode's label; a single-mode button just selects its screen.
const uint8_t SB_LNCH_BTN    = 0;   // LNCH: PRE (auto on pad) / ASC <-> CIRC (manual)
const uint8_t SB_PFD_BTN     = 1;   // PFD:  SPC -> ACFT -> ROVR
const uint8_t SB_ORB_BTN     = 2;   // ORB:  ORB <-> ORB+
const uint8_t SB_TGTDOCK_BTN = 5;   // TGT/DOCK
const uint8_t SB_LNDG_BTN    = 6;   // LNDG/ENTR: DESC <-> ENTR
inline uint16_t sbBtnH() {
  return SCREEN_H / SB_BTN_COUNT;
}
inline uint16_t sbBtnY(uint8_t btn) {
  return btn * sbBtnH();
}

// Sidebar button -> canonical (primary) screen, physical top-to-bottom order. Several
// buttons cover multiple screens: PFD (SCFT/ACFT/ROVR), ORB (ORB/ORB+), TGT/DOCK
// (TGT/DOCK), LNDG/ENTR (LNDG/LNDGRE). SB_BTN_SCREEN holds each button's primary
// screen; the tap handler in TouchEvents.ino cycles the alternates.
const ScreenType SB_BTN_SCREEN[SB_BTN_COUNT] = {
  screen_LNCH,     // 0 LNCH      (PRE / ASC / CIRC)
  screen_SCFT,     // 1 PFD       (SPC default; ACFT / ROVR by context or cycle)
  screen_ORB,      // 2 ORB       (ORB / ORB+)
  screen_VEH,      // 3 VEH
  screen_MNVR,     // 4 MNVR
  screen_TGT,      // 5 TGT/DOCK  (TGT default; DOCK by context or cycle)
  screen_LNDG,     // 6 LNDG/ENTR (DESC default; ENTR by cycle)
  screen_LNCHAP    // 7 ASC — Ascent Autopilot; parked at the bottom, physically separated
                   //   from the display-nav cluster and drawn in a distinct colour
};

// Base labels (shown when the button is NOT the active screen). When a multi-mode
// button IS active, sbButtonLabel() substitutes the active mode's label instead.
const char *const SB_BTN_IDS[SB_BTN_COUNT] = {
  "LNCH", "PFD", "ORB", "VEH", "MNVR", "TGT", "LNDG", "ASC"
};

// Which sidebar button should highlight for the active screen. Multi-screen buttons
// map all their screens to one index; every other screen maps 1:1.
uint8_t screenToButton(ScreenType s) {
  switch (s) {
    case screen_SCFT: case screen_ACFT:   case screen_ROVR:   return SB_PFD_BTN;
    case screen_ORB:  case screen_ORBADV:                     return SB_ORB_BTN;
    case screen_TGT:  case screen_DOCK:                       return SB_TGTDOCK_BTN;
    case screen_LNDG: case screen_LNDGRE:                     return SB_LNDG_BTN;
    default: break;
  }
  for (uint8_t i = 0; i < SB_BTN_COUNT; i++)
    if (SB_BTN_SCREEN[i] == s) return i;
  return 0xFF;   // no button (shouldn't happen — every screen maps)
}

// Sidebar button caption for the current state: the active mode's label when this
// button owns the active screen, otherwise the base label. Mirrors the labelling
// rules in the sidebar-nav design (LNCH/PRE/ASC/CIRC, PFD/SPC/ACFT/ROVR, ORB/ORB+,
// TGT/DOCK, LNDG/DESC/ENTR).
const char *sbButtonLabel(uint8_t i) {
  if (screenToButton(activeScreen) != i) return SB_BTN_IDS[i];
  switch (i) {
    case SB_LNCH_BTN:
      return _lnchPrelaunchMode ? "PRE" : (_lnchOrbitalMode ? "CIRC" : "ASC");
    case SB_PFD_BTN:
      return (activeScreen == screen_ROVR) ? "ROVR"
           : (activeScreen == screen_ACFT) ? "ACFT" : "SPC";
    case SB_ORB_BTN:
      return (activeScreen == screen_ORBADV) ? "ORB+" : "ORB";
    case SB_TGTDOCK_BTN:
      return (activeScreen == screen_DOCK) ? "DOCK" : "TGT";
    case SB_LNDG_BTN:
      return (activeScreen == screen_LNDGRE) ? "ENTR" : "DESC";
    default:
      return SB_BTN_IDS[i];
  }
}

const char *const SCREEN_TITLES[SCREEN_COUNT] = {
  "LAUNCH",
  "ORBIT",
  "SPACECRAFT",
  "MANEUVER",
  "TARGET",
  "DOCKING",
  "POWERED DESCENT",
  "VEHICLE INFO",
  "AIRCRAFT",
  "ROVER",
  "ORBIT ADVANCED",
  "RE-ENTRY",
  "ASCENT AUTOPILOT"
};

const ButtonLabel btnScreenOff = {
  "", TFT_WHITE, TFT_WHITE, TFT_OFF_BLACK, TFT_NAVY, TFT_GREY, TFT_GREY
};
const ButtonLabel btnScreenOn = {
  "", TFT_WHITE, TFT_WHITE, TFT_CORNELL, TFT_CORNELL, TFT_GREY, TFT_WHITE
};

/***************************************************************************************
   PREV STATE SHADOW
   Mirrors the fields actually displayed. Values are redrawn only when changed.
   Colour fields track the last-drawn colour so a colour change triggers a redraw
   even if the formatted string is identical.
   Initialised to sentinel values in drawStaticScreen() to force first-draw.
****************************************************************************************/
// RowCache struct defined in KCMk1_InfoDisp.h (shared with TouchEvents.ino)
RowCache rowCache[SCREEN_COUNT][ROW_COUNT];
PrintState printState[SCREEN_COUNT][ROW_COUNT];

/***************************************************************************************
   ROW GEOMETRY
   rowH/rowY computed from per-screen row count so rows fill the content area evenly.
****************************************************************************************/
const uint16_t CONTENT_H = SCREEN_H - TITLE_TOP;  // 538px (600 - 62)

inline uint16_t rowHFor(uint8_t nRows) {
  return CONTENT_H / nRows;
}
inline uint16_t rowYFor(uint8_t row, uint8_t nRows) {
  return TITLE_TOP + row * rowHFor(nRows) + ROW_PAD;
}
inline uint16_t rowX() {
  return ROW_PAD;
}
inline uint16_t rowW() {
  return CONTENT_W - ROW_PAD * 2;
}

// All screens use NR=8; call rowYFor(row, NR) and rowHFor(NR) explicitly.

// ── Shared reticle geometry (MNVR / DOCK / TGT) ──────────────────────────────────────
// The three attitude-reticle screens share an identical black disc + readout panel +
// bottom bar. Only the angular scale (±20° vs ±60°) and the ring labels differ per
// screen. Define the common geometry once so a re-layout touches a single place.
const uint16_t RETICLE_RP_W  = 360;                              // right readout panel width
const uint16_t RETICLE_RP_X  = SCREEN_W - SIDEBAR_W - RETICLE_RP_W;  // 580 — panel left edge
const uint16_t RETICLE_CX    = (RETICLE_RP_X - 2) / 2;          // 289 — disc centre in left region
const uint16_t RETICLE_CY    = 300;                             // disc centre y
const uint16_t RETICLE_R     = 210;                             // disc radius
const uint16_t RETICLE_BAR_W = 450;                             // bottom bar width (centred under disc)

/***************************************************************************************
   SHARED RETICLE DOT LAYER (TGT + DOCK)
   The moving marker layer that sits on top of the shared reticle base. TGT and DOCK
   draw a byte-identical dot layer (same four markers, colours, dot radii, clamp,
   erase/repair regions, in-view/antipodal logic and inner-crosshair repair); they
   differ only in angular SCALE and the four ring-label strings, both carried in
   ReticleGeom. Extracted here (concatenated before Screen_DOCK/Screen_TGT) so one
   definition serves both. Each caller passes its OWN ReticleDotCache by reference,
   keeping the erase-before-redraw state per-screen and byte-identical to before.
****************************************************************************************/

// Derived angles both screens feed to the dot layer. Byte-identical math in both
// (previously duplicated in drawScreen_TGT / drawScreen_DOCK). Calls eadiHdgDelta
// directly — the per-screen _tgtWrapErr/_dockWrapErr wrappers delegate to it identically.
ReticleAngles reticleComputeAngles() {
  ReticleAngles a;
  // Nose→target: where is the target relative to your nose?
  a.priBrg  = eadiHdgDelta(state.heading - state.tgtHeading, 0.0f);
  a.priElv  = state.pitch - state.tgtPitch;
  // Velocity→target: is your velocity vector pointed at the target bearing?
  a.velBrg  = eadiHdgDelta(state.tgtHeading - state.tgtVelHeading, 0.0f);
  a.velElv  = state.tgtPitch - state.tgtVelPitch;
  // Opposites (antipodal): anti-target and retrograde, same convention.
  a.antiBrg  = eadiHdgDelta(state.heading    - (state.tgtHeading    + 180.0f), 0.0f);
  a.antiElv  = state.pitch    + state.tgtPitch;
  a.retroBrg = eadiHdgDelta(state.tgtHeading - (state.tgtVelHeading + 180.0f), 0.0f);
  a.retroElv = state.tgtPitch + state.tgtVelPitch;
  return a;
}

// Clamp a dot to within the scope boundary (widest marker = prograde ring + spoke).
void reticleClampDot(const ReticleGeom &g, int16_t &sx, int16_t &sy) {
  float dx = sx - g.cx, dy = sy - g.cy;
  float dist = sqrtf(dx*dx + dy*dy);
  float maxR = (float)(g.r - g.dotRVel * 3 / 2 - 2);
  if (dist > maxR && dist > 0.5f) {
    float scale = maxR / dist;
    sx = g.cx + (int16_t)(dx * scale);
    sy = g.cy + (int16_t)(dy * scale);
  }
}

// Repair scope chrome after a fillRect dot erase. Shared reticle restore (rings /
// cardinals / crosshair / centre-dot / good-zone) then redraw the ring degree label(s)
// whose bbox overlaps the erase box, PLUS the innermost one when the good-zone refill
// (radius r/4) painted over it (marker inside that ring). Ring radii are r/4, r/2,
// 3r/4, r — identical in both screens; only the label text differs (g.lbl).
void reticleRepairDotChrome(KCM_TFT &tft, const ReticleGeom &g,
                            int16_t bx, int16_t by, uint8_t bh) {
  int16_t boxX0 = bx, boxX1 = bx + 2*bh, boxY0 = by, boxY1 = by + 2*bh;

  float d = reticleRepair(tft, g.cx, g.cy, g.r, 18, bx, by, bh);

  const uint16_t lblR[4] = { (uint16_t)(g.r / 4), (uint16_t)(g.r / 2),
                             (uint16_t)((g.r * 3) / 4), (uint16_t)g.r };
  bool fontSet = false;
  for (uint8_t i = 0; i < 4; i++) {
    int16_t lx = g.cx + 3, ly = g.cy - lblR[i] + 3;
    bool boxHit      = (boxX1 >= lx && boxX0 <= lx + 26 && boxY1 >= ly && boxY0 <= ly + 20);
    bool goodZoneHit = (i == 0 && d <= (float)(g.r / 4));
    if (boxHit || goodZoneHit) {
      if (!fontSet) { tft.setFont(Roboto_Black_16); tft.setTextColor(TFT_LIGHT_GREY); fontSet = true; }
      tft.setCursor(lx, ly);
      tft.print(g.lbl[i]);
    }
  }
}

// Erase-phase for one reticle marker: if it should be hidden, or has moved >1px, erase
// its cached position and repair chrome, then advance the cache. The draw phase runs
// after ALL erases (so a moving marker never clips a neighbour) and redraws at the cache
// (no sub-pixel smear). File-scope static — internal to reticleUpdateDots.
static void reticleEraseDot(KCM_TFT &tft, const ReticleGeom &g,
                            int16_t curX, int16_t curY,
                            int16_t &prevX, int16_t &prevY, bool visible) {
  const uint8_t EH = g.eraseHalf;
  if (!visible) {
    if (prevX != 9999) {
      tft.fillRect(prevX - EH, prevY - EH, EH*2+1, EH*2+1, TFT_BLACK);
      reticleRepairDotChrome(tft, g, prevX - EH, prevY - EH, EH);
      prevX = prevY = 9999;
    }
    return;
  }
  if (prevX == 9999 || abs(curX - prevX) > 1 || abs(curY - prevY) > 1) {
    if (prevX != 9999) {
      tft.fillRect(prevX - EH, prevY - EH, EH*2+1, EH*2+1, TFT_BLACK);
      reticleRepairDotChrome(tft, g, prevX - EH, prevY - EH, EH);
    }
    prevX = curX; prevY = curY;
  }
}

// Update scope dots — erase old, repair chrome, draw new.
//   primary marker: solid target/port diamond (violet)
//   vel marker:     hollow prograde circle (neon green)
// Angular convention: sx = cx + (-brg * scale); sy = cy + (elv * scale).
// Anti-target/retrograde show only inside the FOV; the matching primary is suppressed
// while its opposite is shown. Erase phase runs for all markers, then the draw phase
// (bottom-to-top: opposites, primary, velocity on top). Finally the inner crosshair
// gap segments + centre dot are redrawn (the vel circle can clip them near centre).
void reticleUpdateDots(KCM_TFT &tft, const ReticleGeom &g, ReticleDotCache &c,
                       float priBrg, float priElv, float velBrg, float velElv,
                       float antiBrg, float antiElv, float retroBrg, float retroElv) {
  // Primary dot: where is the target/port relative to your nose?
  int16_t pSX = g.cx + (int16_t)(-priBrg * g.scale);
  int16_t pSY = g.cy + (int16_t)( priElv * g.scale);
  reticleClampDot(g, pSX, pSY);

  // Vel dot: where is your velocity vector relative to the target bearing?
  int16_t vSX = g.cx + (int16_t)(-velBrg * g.scale);
  int16_t vSY = g.cy + (int16_t)( velElv * g.scale);
  reticleClampDot(g, vSX, vSY);

  // Opposite (antipodal) marker positions + in-view test.
  const float maxR = (float)(g.r - g.dotRVel * 3 / 2 - 2);
  int16_t aSX = g.cx + (int16_t)(-antiBrg  * g.scale), aSY = g.cy + (int16_t)(antiElv  * g.scale);
  int16_t rSX = g.cx + (int16_t)(-retroBrg * g.scale), rSY = g.cy + (int16_t)(retroElv * g.scale);
  bool antiInView  = ((float)(aSX-g.cx)*(aSX-g.cx) + (float)(aSY-g.cy)*(aSY-g.cy)) <= maxR*maxR;
  bool retroInView = ((float)(rSX-g.cx)*(rSX-g.cx) + (float)(rSY-g.cy)*(rSY-g.cy)) <= maxR*maxR;

  // Erase phase (all markers) then draw phase, so a moving marker's erase never clips a
  // neighbour. Draw order bottom-to-top: opposites, primary, velocity on top.
  reticleEraseDot(tft, g, aSX, aSY, c.antiX,    c.antiY,    antiInView);
  reticleEraseDot(tft, g, rSX, rSY, c.retroX,   c.retroY,   retroInView);
  reticleEraseDot(tft, g, pSX, pSY, c.primaryX, c.primaryY, !antiInView);
  reticleEraseDot(tft, g, vSX, vSY, c.velX,     c.velY,     !retroInView);

  if (c.antiX    != 9999) drawAntiTargetMarker(tft, c.antiX,    c.antiY,    g.dotRPrimary, TFT_VIOLET);
  if (c.retroX   != 9999) drawRetrogradeMarker(tft, c.retroX,   c.retroY,   g.dotRVel,     TFT_NEON_GREEN);
  if (c.primaryX != 9999) drawTargetMarker(tft,     c.primaryX, c.primaryY, g.dotRPrimary, TFT_VIOLET);
  if (c.velX     != 9999) drawProgradeMarker(tft,   c.velX,     c.velY,     g.dotRVel,     TFT_NEON_GREEN);

  // Redraw crosshair inner segments — the vel circle can clip them near centre.
  {
    static const uint16_t gp = 18;   // matches reticleDrawBase gap
    tft.drawLine(g.cx - gp + 2, g.cy, g.cx - 4, g.cy, TFT_GREY);
    tft.drawLine(g.cx + 4,      g.cy, g.cx + gp - 2, g.cy, TFT_GREY);
    tft.drawLine(g.cx, g.cy - gp + 2, g.cx, g.cy - 4, TFT_GREY);
    tft.drawLine(g.cx, g.cy + 4,      g.cx, g.cy + gp - 2, TFT_GREY);
  }
  tft.fillCircle(g.cx, g.cy, 2, TFT_GREY);
}

/***************************************************************************************
   VALUE FORMATTERS
****************************************************************************************/
String fmtNum(float v) {
  if (fabsf(v) < 0.05f) v = 0.0f;  // snap -0.0 and sub-rounding noise to zero
  if (v >= 1000.0f || v <= -1000.0f) return formatSep(v);
  char buf[16];
  dtostrf(v, 1, 1, buf);
  return String(buf);
}
String fmtUnit(float v, const char *unit) {
  return fmtNum(v) + " " + unit;
}
String fmtMs(float v) {
  return fmtUnit(v, "m/s");
}

// formatTime() removed — formatting improvements merged into library formatTime() (#5C)
// All call sites now call formatTime() directly.

/***************************************************************************************
   KSP BODY COLORS — single source of truth, used by ORB and LNCH (Circularization)

   Both screens render a top-down orbit diagram with body and atmosphere rings, so
   they need to agree on per-body colors. These functions are the canonical lookup;
   callers should never inline their own table. If KSP ever adds a body, update only
   here and both screens update together.

   Returns TFT_GREY for unknown bodies (safe fallback). atmoColor returns 0 (no draw)
   for bodies with no significant atmosphere.
****************************************************************************************/
uint16_t kspBodyColor(const char *name) {
  if (!name || name[0] == '\0') return TFT_GREY;
  if      (strcmp(name, "Kerbol") == 0) return TFT_YELLOW;
  else if (strcmp(name, "Moho")   == 0) return TFT_UPS_BROWN;
  else if (strcmp(name, "Eve")    == 0) return TFT_PURPLE;
  else if (strcmp(name, "Gilly")  == 0) return TFT_TAN;
  else if (strcmp(name, "Kerbin") == 0) return TFT_OCEAN;
  else if (strcmp(name, "Mun")    == 0) return TFT_SILVER;
  else if (strcmp(name, "Minmus") == 0) return TFT_MINT;
  else if (strcmp(name, "Duna")   == 0) return TFT_CORNELL;
  else if (strcmp(name, "Ike")    == 0) return TFT_DARK_GREY;
  else if (strcmp(name, "Dres")   == 0) return TFT_GREY;
  else if (strcmp(name, "Jool")   == 0) return TFT_SAP_GREEN;
  else if (strcmp(name, "Laythe") == 0) return TFT_FRENCH_BLUE;
  else if (strcmp(name, "Vall")   == 0) return TFT_AQUA;
  else if (strcmp(name, "Tylo")   == 0) return TFT_BROWN;
  else if (strcmp(name, "Bop")    == 0) return TFT_DARK_RED;
  else if (strcmp(name, "Pol")    == 0) return TFT_OLIVE;
  else if (strcmp(name, "Eeloo")  == 0) return TFT_WHITE;
  return TFT_GREY;
}

uint16_t kspAtmoColor(const char *name) {
  if (!name || name[0] == '\0') return 0;
  if      (strcmp(name, "Eve")    == 0) return TFT_VIOLET;
  else if (strcmp(name, "Kerbin") == 0) return TFT_AQUA;
  else if (strcmp(name, "Duna")   == 0) return TFT_RED;
  else if (strcmp(name, "Jool")   == 0) return TFT_NEON_GREEN;
  else if (strcmp(name, "Laythe") == 0) return TFT_SKY;
  return 0;
}

/***************************************************************************************
   DRAW SIDEBAR
****************************************************************************************/
void drawSidebar(KCM_TFT &tft) {
  const uint16_t divX = SCREEN_W - SIDEBAR_W;
  tft.drawLine(divX, 0, divX, SCREEN_H - 1, TFT_GREY);
  uint16_t bx = divX + 1;
  uint16_t bw = SIDEBAR_W - 1;
  uint16_t bh = sbBtnH();
  uint8_t  activeBtn = screenToButton(activeScreen);
  for (uint8_t i = 0; i < SB_BTN_COUNT; i++) {
    ButtonLabel btn = (i == activeBtn) ? btnScreenOn : btnScreenOff;
    btn.text = sbButtonLabel(i);   // active mode's label when this button owns the screen
    // Ascent Autopilot button carries a distinct purple identity so it reads as a
    // different kind of control (an interactive console, not a display screen). The
    // sidebar draws with isOn=true, so override backgroundColorOn: brighter violet
    // when it is the active screen, dimmer purple when idle.
    if (SB_BTN_SCREEN[i] == screen_LNCHAP)
      btn.backgroundColorOn = (i == activeBtn) ? TFT_VIOLET : TFT_PURPLE;
    uint16_t by = sbBtnY(i);
    uint16_t h  = bh;
    // Last button tiles to row 599 (the panel's final scanline), where its bottom
    // border is hidden by the bezel. Clamp its height so the border lands on
    // SCREEN_H-2 and stays visible.
    if (i == SB_BTN_COUNT - 1) h = (SCREEN_H - 1) - by;
    drawButton(tft, bx, by, bw, h, btn, &Roboto_Black_20, true);
  }
}

/***************************************************************************************
   DRAW TITLE BAR (chrome — once per transition)
****************************************************************************************/
void drawTitleBar(KCM_TFT &tft, ScreenType s) {
  tft.fillRect(0, 0, CONTENT_W, TITLE_TOP, TFT_BLACK);
  textCenter(tft, TITLE_FONT, 0, 0, CONTENT_W, TITLE_H,
             SCREEN_TITLES[(uint8_t)s], TFT_WHITE, TFT_BLACK);
  tft.fillRect(0, TITLE_H, CONTENT_W, TITLE_RULE_H, TFT_GREY);
}

void drawTitleBar(KCM_TFT &tft, const String &title) {
  tft.fillRect(0, 0, CONTENT_W, TITLE_TOP, TFT_BLACK);
  textCenter(tft, TITLE_FONT, 0, 0, CONTENT_W, TITLE_H,
             title, TFT_WHITE, TFT_BLACK);
  tft.fillRect(0, TITLE_H, CONTENT_W, TITLE_RULE_H, TFT_GREY);
}

/***************************************************************************************
   ROW PRIMITIVES
   Core versions accept explicit font and nRows for screens with non-standard layout.
   Convenience wrappers use ROW_FONT. All screens use NR=8 rows.
****************************************************************************************/
// Split-column overload (#51) — explicit x, w for left/right half-row cells
void drawValue(KCM_TFT &tft, uint8_t screen, uint8_t row,
               uint16_t x, uint16_t w,
               const char *label, String value,
               uint16_t fg, uint16_t bg,
               const tFont *font, uint8_t nRows) {
  RowCache &c = rowCache[screen][row];
  if (c.value == value && c.fg == fg && c.bg == bg) return;
  printValue(tft, font,
             x, rowYFor(row, nRows), w, rowHFor(nRows),
             label, value, fg, bg, COL_BACK,
             printState[screen][row]);
  c.value = value;
  c.fg = fg;
  c.bg = bg;
}

// Right-panel cache-checked draw with an explicit cache `slot` distinct from the
// geometry `row` (needed by screens whose half-width cells share a Y row but need two
// cache slots) and an explicit column x/w. Y/H derive from rowYFor/rowHFor(nRows).
// greyDashes: render a "---" no-data value in dark grey regardless of `fg`. This backs
// the per-screen mnvr/tgt/dock/rp/acft/att value helpers (previously duplicated inline).
void drawPanelValue(KCM_TFT &tft, uint8_t screen, uint8_t slot, uint8_t row,
                    uint16_t x, uint16_t w,
                    const char *label, String value,
                    uint16_t fg, uint16_t bg,
                    const tFont *font, uint8_t nRows, bool greyDashes) {
  uint16_t drawFg = (greyDashes && value == "---") ? TFT_DARK_GREY : fg;
  RowCache &c = rowCache[screen][slot];
  if (c.value == value && c.fg == drawFg && c.bg == bg) return;
  printValue(tft, font,
             x, rowYFor(row, nRows), w, rowHFor(nRows),
             label, value, drawFg, bg, COL_BACK,
             printState[screen][slot]);
  c.value = value;
  c.fg = drawFg;
  c.bg = bg;
}

/***************************************************************************************
   ROW-CACHE INVALIDATION
   Reset the cached value/fg/bg for a screen's rows to sentinels that differ from any
   real draw, forcing the next updateScreen() to repaint every row. Centralised here so
   drawStaticScreen() and the vessel-switch handler share one definition (previously
   duplicated with divergent field coverage).
****************************************************************************************/
void invalidateRowCache(ScreenType s) {
  for (uint8_t r = 0; r < ROW_COUNT; r++) {
    rowCache[(uint8_t)s][r].value = "\x01";
    rowCache[(uint8_t)s][r].fg    = 0x0001;
    rowCache[(uint8_t)s][r].bg    = 0x0001;
  }
}

void invalidateAllRowCache() {
  for (uint8_t s = 0; s < SCREEN_COUNT; s++) invalidateRowCache((ScreenType)s);
}


/***************************************************************************************
   SAS-MODE NAVBALL LABEL / PALETTE
   Shared by the SPACECRAFT / AIRCRAFT / ROVER PFD screens (previously triplicated
   verbatim). Maps the Simpit SAS mode to the button label + fg/bg palette.
   255 (SAS off) and any unknown value fall back to the dimmed "SAS" chip.
****************************************************************************************/
void sasNavballLabel(uint8_t mode, const char *&v, uint16_t &fg, uint16_t &bg) {
  switch (mode) {
    case 0:  v = "STAB"; fg = TFT_WHITE;     bg = TFT_DARK_GREEN; break;
    case 1:  v = "PRO";  fg = TFT_DARK_GREY; bg = TFT_NEON_GREEN; break;
    case 2:  v = "RETR"; fg = TFT_DARK_GREY; bg = TFT_NEON_GREEN; break;
    case 3:  v = "NRM";  fg = TFT_WHITE;     bg = TFT_MAGENTA;    break;
    case 4:  v = "ANRM"; fg = TFT_WHITE;     bg = TFT_MAGENTA;    break;
    case 5:  v = "RAD+"; fg = TFT_DARK_GREY; bg = TFT_SKY;        break;
    case 6:  v = "RAD-"; fg = TFT_DARK_GREY; bg = TFT_SKY;        break;
    case 7:  v = "TGT";  fg = TFT_WHITE;     bg = TFT_VIOLET;     break;
    case 8:  v = "ATGT"; fg = TFT_WHITE;     bg = TFT_VIOLET;     break;
    case 9:  v = "MNVR"; fg = TFT_WHITE;     bg = TFT_BLUE;       break;
    default: v = "SAS";  fg = TFT_DARK_GREY; bg = TFT_OFF_BLACK;  break;  // 255 = off / unknown
  }
}


/***************************************************************************************
   SCREEN CHROME FUNCTIONS — labels drawn once on transition
****************************************************************************************/


void drawStaticScreen(KCM_TFT &tft, ScreenType s) {
  tft.fillScreen(TFT_BLACK);
  drawSidebar(tft);

  // Dynamic titles.
  // ORB/ORBADV and LNDG/LNDGRE are now sibling sidebar screens (no title toggle).
  // Their mode flags are derived from the active screen so the shared chrome/draw
  // dispatchers (chromeScreen_ORB/OrbAdv, chromeScreen_LNDG) select the right layout.
  if (s == screen_ORB) {
    _orbAdvancedMode = false;
    drawTitleBar(tft, "ORBIT");
  } else if (s == screen_ORBADV) {
    _orbAdvancedMode = true;
    drawTitleBar(tft, "ORBIT ADVANCED");
  } else if (s == screen_LNDG) {
    _lndgReentryMode = false;
    drawTitleBar(tft, "POWERED DESCENT");
  } else if (s == screen_LNDGRE) {
    _lndgReentryMode = true;
    drawTitleBar(tft, "RE-ENTRY");
  } else if (s == screen_LNCH) {
    drawTitleBar(tft, _lnchOrbitalMode ? "CIRCULARIZATION" : "ASCENT");
    // Red dot indicates manual override of auto phase switch (set by the LNCH button)
    if (_lnchManualOverride)
      tft.fillCircle(CONTENT_W - 14, 29, 6, TFT_RED);
  } else if (s == screen_DOCK) {
    // Vessel name from Simpit reflects the active/combined vessel after docking.
    String dockTitle = String("DOCKING [ ") + state.vesselName + " ]";
    drawTitleBar(tft, dockTitle);
  } else {
    // Mode switching is via the sidebar buttons (see TouchEvents.ino) — the title
    // bar no longer toggles, so no title-tap indicator is drawn.
    drawTitleBar(tft, s);
  }

  switch (s) {
    case screen_LNCH:   chromeScreen_LNCH(tft); break;
    case screen_ORB:    chromeScreen_ORB(tft); break;
    case screen_ORBADV: chromeScreen_OrbAdv(tft); break;
    case screen_ROVR:   chromeScreen_ROVR(tft); break;
    case screen_SCFT:   chromeScreen_SCFT(tft); break;
    case screen_MNVR:   chromeScreen_MNVR(tft); break;
    case screen_TGT:
      _tgtChromDrawn = false;
      chromeScreen_TGT(tft);
      break;
    case screen_DOCK:
      _dockChromDrawn = false;
      chromeScreen_DOCK(tft);
      break;
    case screen_VEH:    chromeScreen_VEH(tft); break;
    case screen_LNDG:   chromeScreen_LNDG(tft); break;   // _lndgReentryMode set false above
    case screen_LNDGRE: chromeScreen_LNDG(tft); break;   // _lndgReentryMode set true above
    case screen_ACFT:   chromeScreen_ACFT(tft); break;
    case screen_LNCHAP: chromeScreen_LNCHAP(tft); break;
    default: break;
  }

  // Invalidate all row cache slots so first updateScreen() draws everything fresh
  invalidateRowCache(s);
}

void updateScreen(KCM_TFT &tft, ScreenType s) {
  switch (s) {
    case screen_LNCH:   drawScreen_LNCH(tft); break;
    case screen_ORB:    drawScreen_ORB(tft); break;
    case screen_ORBADV: drawScreen_OrbAdv(tft); break;
    case screen_ROVR:   drawScreen_ROVR(tft); break;
    case screen_SCFT:   drawScreen_SCFT(tft); break;
    case screen_MNVR:   drawScreen_MNVR(tft); break;
    case screen_TGT:    drawScreen_TGT(tft); break;
    case screen_DOCK:   drawScreen_DOCK(tft); break;
    case screen_VEH:    drawScreen_VEH(tft); break;
    // Re-assert the LNDG mode flag every frame — a vessel switch can reset it
    // (SimpitHandler) while this screen stays active; keep chrome and draw in sync.
    case screen_LNDG:   _lndgReentryMode = false; drawScreen_LNDG(tft); break;
    case screen_LNDGRE: _lndgReentryMode = true;  drawScreen_LNDG(tft); break;
    case screen_ACFT:   drawScreen_ACFT(tft); break;
    case screen_LNCHAP: drawScreen_LNCHAP(tft); break;
    default: break;
  }
}

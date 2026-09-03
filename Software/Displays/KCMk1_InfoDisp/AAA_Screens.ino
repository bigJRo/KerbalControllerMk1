/***************************************************************************************
   AAA_Screens.ino -- Shared screen infrastructure for Kerbal Controller Mk1 Information Display
   Must compile before Screen_*.ino tabs (AAA_ prefix ensures correct sort order).

   Sidebar buttons (6, top-to-bottom) — decoupled from ScreenType via SB_BTN_SCREEN.
   Multi-mode buttons CYCLE their modes when pressed while already active, and the
   button caption shows the active mode (sbButtonLabel); a first press from another
   screen goes to the button's context/primary mode. The context ladder runs every
   frame; a press latches the manual selection until the situation it was set against
   passes, or the vessel or scene changes (see AAA_Globals.ino). Title-bar taps do
   not switch anything.
   On unit 2 the ASC (Ascent Autopilot) button is parked at the bottom, below the
   display-nav cluster, since it is an interactive console rather than a display screen;
   it turns green while the autopilot is armed. Unit 1 carries VEH in that slot.
     0  PFD       Primary Flight Display — SPC -> ACFT -> ROVR (press cycles; context
                  default). Unit 2 keeps VEH in this ring; unit 1 gives it button 5.
     1  LNCH      Launch — LNCH / PRE (auto on pad) / ASC <-> CIRC (press cycles)
     2  ORB       Orbit — ORB -> ORB+ (Advanced Elements) -> MNVR (Maneuver) (press cycles)
     3  TGT/DOCK  Target / Docking / Navigation — TGT -> DOCK -> NAV (press cycles;
                  context default: DOCK when near a target, NAV in atmospheric flight)
     4  LNDG/ENTR Landing — DESC (powered descent) <-> ENTR (re-entry)
     5  per unit  Unit 1: VEH (Vehicle Info, single-mode).
                  Unit 2: ASC (Ascent Autopilot console).

   Button order is identical on both units so one reach serves either panel. PFD is
   pinned to the top: it is the screen a pilot returns to from anywhere, it is unit 1's
   whole role, and the top key is the easiest to find without looking. The remaining
   five keep the original mission-phase progression, LNCH through LNDG, with the
   single-mode key (VEH / ASC) parked at the bottom.

   Layout (1024x600):
     Title bar  : 62px (58px text + 4px rule)
     Data rows  : text screens fill the content height evenly
     Sidebar    : 84px column on the unit's outboard edge, 6 labelled buttons

   Update pattern (mirrors Annunciator):
     Chrome (labels)  : printDispChrome() — called once per screen transition.
     Values           : printValue()      — called only when state != prev.
                        Draws only the right-hand value region, label untouched.
     Colour changes   : tracked via prev colour fields per row.
     Nothing that has not changed is re-rasterised, and labels never flicker.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

/***************************************************************************************
   LAYOUT CONSTANTS
****************************************************************************************/
// CONTENT_W / CONTENT_X / SIDEBAR_X now live in KCMk1_InfoDisp.h — the touch helpers
// there need them, and the header is processed before any tab.

const uint16_t TITLE_H = 58;
const uint16_t TITLE_RULE_H = 4;
const uint16_t TITLE_TOP = TITLE_H + TITLE_RULE_H;

const tFont *TITLE_FONT = &Roboto_Black_36;
const uint16_t ROW_PAD = 2;

const uint16_t COL_LABEL = KDC_LABEL_COLOR;   // the panel-wide readout label colour (KerbalDisplayCommon)
const uint16_t COL_VALUE = TFT_DARK_GREEN;
const uint16_t COL_BACK = TFT_BLACK;
const uint16_t COL_NO_BDR = TFT_BLACK;

const uint8_t SB_BTN_COUNT = 6;
// Multi-mode button indices. Each of these cycles its modes on a repeated press and
// shows the active mode's label; a single-mode button just selects its screen.
const uint8_t SB_PFD_BTN     = 0;   // PFD:  SPC -> ACFT -> ROVR (-> VEH on unit 2)
const uint8_t SB_LNCH_BTN    = 1;   // LNCH: PRE (auto on pad) / ASC <-> CIRC (manual)
const uint8_t SB_ORB_BTN     = 2;   // ORB:  ORB -> ORB+ -> MNVR
const uint8_t SB_TGTDOCK_BTN = 3;   // TGT/DOCK
const uint8_t SB_LNDG_BTN    = 4;   // LNDG/ENTR: DESC <-> ENTR
// Button 5 is single-mode on both units (VEH on unit 1, ASC on unit 2), so it needs
// no index constant — the tap handler's default branch selects SB_BTN_SCREEN[5].
inline uint16_t sbBtnH() {
  return SCREEN_H / SB_BTN_COUNT;
}
inline uint16_t sbBtnY(uint8_t btn) {
  return btn * sbBtnH();
}

// Sidebar button -> canonical (primary) screen, physical top-to-bottom order. Several
// buttons cover multiple screens: PFD (SCFT/ACFT/ROVR, +VEH on unit 2), ORB (ORB/ORB+/MNVR),
// TGT/DOCK (TGT/DOCK), LNDG/ENTR (LNDG/LNDGRE). SB_BTN_SCREEN holds each button's
// primary screen; the tap handler in TouchEvents.ino cycles the alternates.
// The first five buttons are identical on both units: every screen stays reachable
// from either panel, so losing one display never costs a screen class. Only the sixth
// differs, and only in what a first press lands on.
//
// Unit 2 keeps the Ascent Autopilot console. It is the only console on either panel
// that sends commands, and two of them driving one autopilot is an ambiguity worth
// designing out rather than discovering during a gravity turn — the pending-edit
// reconcile assumes a single editor, and ARM/DISARM has no business having two
// sources. apEnqueueCmd() enforces the same rule at the command channel.
//
// Unit 1 gets VEHICLE INFO in that slot instead. It is on-role for the vehicle-type
// panel, it is where the ladder already routes a recoverable vessel, and promoting it
// out of the PFD ring shortens that ring from four modes to three — VEH was the
// deepest cycle on the panel that uses cycling most.
const ScreenType SB_BTN_SCREEN[SB_BTN_COUNT] = {
  screen_SCFT,     // 0 PFD       (SPC default; ACFT / ROVR by context or cycle)
  screen_LNCH,     // 1 LNCH      (PRE / ASC / CIRC)
  screen_ORB,      // 2 ORB       (ORB / ORB+ / MNVR)
  screen_TGT,      // 3 TGT/DOCK  (TGT default; DOCK by context or cycle)
  screen_LNDG,     // 4 LNDG/ENTR (DESC default; ENTR by cycle)
#if INFO_DISP_IS_PFD_UNIT
  screen_VEH       // 5 VEH — Vehicle Info, single-mode
#else
  screen_LNCHAP    // 5 ASC — Ascent Autopilot; parked at the bottom, below the
                   //   display-nav cluster. Green while the autopilot is armed.
#endif
};

// Base labels (shown when the button is NOT the active screen). When a multi-mode
// button IS active, sbButtonLabel() substitutes the active mode's label instead.
const char *const SB_BTN_IDS[SB_BTN_COUNT] = {
  "PFD", "LNCH", "ORB", "TGT", "LNDG",
#if INFO_DISP_IS_PFD_UNIT
  "VEH"
#else
  "ASC"
#endif
};

// Which sidebar button should highlight for the active screen. Multi-screen buttons
// map all their screens to one index; every other screen maps 1:1.
uint8_t screenToButton(ScreenType s) {
  switch (s) {
#if !INFO_DISP_IS_PFD_UNIT
    // Unit 2 keeps VEH inside the PFD ring; unit 1 gives it its own button, so it
    // falls through to the SB_BTN_SCREEN lookup below and highlights button 5.
    case screen_VEH:                                                       return SB_PFD_BTN;
#endif
    case screen_SCFT: case screen_ACFT: case screen_ROVR:                  return SB_PFD_BTN;
    case screen_ORB:  case screen_ORBADV: case screen_MNVR:                return SB_ORB_BTN;
    case screen_TGT:  case screen_DOCK: case screen_NAV:                    return SB_TGTDOCK_BTN;
    case screen_LNDG: case screen_LNDGRE:                                  return SB_LNDG_BTN;
    default: break;
  }
  for (uint8_t i = 0; i < SB_BTN_COUNT; i++)
    if (SB_BTN_SCREEN[i] == s) return i;
  return 0xFF;   // no button (shouldn't happen — every screen maps)
}

// Sidebar button caption for the current state: the active mode's label when this
// button owns the active screen, otherwise the base label. Mirrors the labelling
// rules in the sidebar-nav design (LNCH/PRE/ASC/CIRC, PFD/SPC/ACFT/ROVR/VEH,
// ORB/ORB+/MNVR, TGT/DOCK, LNDG/DESC/ENTR).
const char *sbButtonLabel(uint8_t i) {
  if (screenToButton(activeScreen) != i) return SB_BTN_IDS[i];
  switch (i) {
    case SB_LNCH_BTN:
      return _lnchPrelaunchMode ? "PRE" : (_lnchOrbitalMode ? "CIRC" : "ASC");
    case SB_PFD_BTN:
      // VEH only appears in this ring on unit 2; unit 1 labels its own button.
      return (activeScreen == screen_VEH)  ? "VEH"
           : (activeScreen == screen_ROVR) ? "ROVR"
           : (activeScreen == screen_ACFT) ? "ACFT" : "SPC";
    case SB_ORB_BTN:
      return (activeScreen == screen_ORBADV) ? "ORB+"
           : (activeScreen == screen_MNVR)   ? "MNVR" : "ORB";
    case SB_TGTDOCK_BTN:
      return (activeScreen == screen_DOCK) ? "DOCK"
           : (activeScreen == screen_NAV)  ? "NAV" : "TGT";
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
  "ASCENT AUTOPILOT",
  "NAVIGATION"
};

/***************************************************************************************
   SIDEBAR KEY PALETTE
   The sidebar is chrome, not data, so it is achromatic: selection is shown by reverse
   video — white-on-black unselected, black-on-grey selected — which is the convention
   every bezel-key flight deck uses (the Garmin G1000 softkey row, the closest analogue
   to this sidebar, is exactly this) and which FAA display guidance assumes when it
   assigns light grey to inactive soft-button labels.

   It previously filled unselected keys navy and the selected key TFT_CORNELL, which is
   a red (#B51C19). That put the vocabulary's highest-urgency colour permanently on
   screen, on every screen, marking the least urgent fact the panel knows — the page you
   are already looking at — while this same panel uses white-on-red for the dV, G-load
   and ground-proximity alarms. Chrome does not get to spend the alerting colours.

   Keeping the keys neutral also leaves room to colour a key by the state of the screen
   behind it, the way an EICAS flags a page you are not on. Nothing does that yet; the
   ASC armed state below is the first instance.
****************************************************************************************/
const ButtonLabel btnScreenOff = {
  "", TFT_WHITE, TFT_WHITE, TFT_BLACK, TFT_BLACK, TFT_GREY, TFT_GREY
};
const ButtonLabel btnScreenOn = {
  "", TFT_BLACK, TFT_BLACK, TFT_GREY,  TFT_GREY,  TFT_GREY, TFT_WHITE
};

/***************************************************************************************
   PREV STATE SHADOW
   Mirrors the fields actually displayed. Values are redrawn only when changed.
   Colour fields track the last-drawn colour so a colour change triggers a redraw
   even if the formatted string is identical.
   Initialised to sentinel values in drawStaticScreen() to force first-draw.
****************************************************************************************/
// RowCache struct defined in KCMk1_InfoDisp.h
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
const uint16_t RETICLE_RP_X  = CONTENT_W - RETICLE_RP_W;             // 580 — panel left edge
const uint16_t RETICLE_CX    = (RETICLE_RP_X - 2) / 2;          // 289 — disc centre in left region
const uint16_t RETICLE_CY    = 300;                             // disc centre y
const uint16_t RETICLE_R     = 210;                             // disc radius
const uint16_t RETICLE_BAR_W = 450;                             // bottom bar width (centred under disc)

/***************************************************************************************
   RETICLE ANGLES — the AppState adapter for the shared reticle marker layer
   The marker layer itself (ReticleGeom / ReticleDotCache / ReticleAngles, plus
   reticleProject / reticleClampDot / reticleEraseDot / reticleRepairDotChrome /
   reticleUpdateDots) now lives in KerbalDisplayCommon ≥ 3.3.0, beside the
   reticleDrawBase + reticleRepair chrome it draws on, so MNVR / DOCK / TGT all run
   one implementation. Nothing there reads telemetry.
   This function is the seam: it is the only reticle code that touches the global
   `state`, turning vessel/target telemetry into the library's ReticleAngles via
   kspBodyAxes + kspBoresightAngles.
****************************************************************************************/

// Every plotted pair is boresight-relative (+right, +up) from kspBoresightAngles, so a
// marker's distance from the crosshair is its TRUE angular offset from the nose at any
// attitude, and the axes are built with the vessel roll — the project-wide rule that
// EVERY boresight display is body-referenced. Screen up is the craft's roof, so the
// pilot's instinct is the same on every one of them:
//   position markers (node / port / target) — steer TOWARD them: marker up and right
//     means pitch up and yaw right, and it walks back to the crosshair.
//   velocity markers (prograde)             — thrust AWAY from them: marker up and
//     right means thrust left and down, and it walks back to the crosshair.
// Those responses are opposite because of what the symbols mean, not because of the
// frame; the body frame is what puts both of them in the axes the controls use.
//
// appRight/appUp are the readout pair: the relative velocity decomposed about the TARGET
// axis rather than the nose, so "is my approach path aimed at the port, and which way is
// it off" is answered exactly, in the pilot's own right/up sense (the target axes carry
// the vessel roll). Every angular readout on the three reticle screens now comes from
// this block, so the numbers, the colour bands and the markers cannot disagree.
ReticleAngles reticleComputeAngles() {
  ReticleAngles a;
  const KspBodyAxes ax = kspBodyAxes(state.heading, state.pitch, state.roll);

  // Where is the target/port relative to the nose?
  kspBoresightAngles(ax, state.tgtHeading, state.tgtPitch, a.priRight, a.priUp);
  // Where is the craft going relative to where it is pointing?
  kspBoresightAngles(ax, state.tgtVelHeading, state.tgtVelPitch, a.velRight, a.velUp);
  // Opposites: anti-target and retrograde (heading + 180, pitch negated).
  kspBoresightAngles(ax, state.tgtHeading    + 180.0f, -state.tgtPitch,
                     a.antiRight,  a.antiUp);
  kspBoresightAngles(ax, state.tgtVelHeading + 180.0f, -state.tgtVelPitch,
                     a.retroRight, a.retroUp);

  // Readout only — approach-path error, measured about the TARGET axis: how far right
  // and above the approach path is the relative velocity actually pointing?
  const KspBodyAxes tax = kspBodyAxes(state.tgtHeading, state.tgtPitch, state.roll);
  kspBoresightAngles(tax, state.tgtVelHeading, state.tgtVelPitch, a.appRight, a.appUp);
  return a;
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
   DRAWING REGIONS
   Every screen lays itself out in content space: x from 0 to CONTENT_W. On unit 2 that
   is also panel space, because the sidebar is on the right and the content starts at 0.
   On unit 1 the sidebar is on the left, so content has to land 84 px further right.

   Rather than add CONTENT_X to ~700 drawing calls — every one of them a chance for a
   silent 84 px error — the shift is applied once, at the canvas origin. The RA8876
   addresses the canvas as a linear framebuffer of stride canvasImageWidth; because
   that stride stays SCREEN_W, advancing the base address by CONTENT_X pixels shifts
   every row by exactly CONTENT_X and nothing else. Drawing at content x=0 lands at
   panel x=CONTENT_X on every scanline, with no shear.

   Two details this has to get right, both learned from canvasTo() in KCM_Display.h:
     - currentPage must move with canvasImageStartAddress. The driver's writeRect()
       text path blits relative to currentPage while the 2D geometry engine uses the
       canvas address; leaving them apart puts text and geometry on different origins.
     - the active window must clamp the content region to CONTENT_W. Without it, a
       fill that runs to the full panel width would spill past the end of the row and
       wrap into the left edge of the next one.

   On unit 2 CONTENT_X is 0 and canvasContentRegion() is exactly the full-panel setup
   the double buffer already installs, so that unit's register writes are unchanged.
****************************************************************************************/
// The canvas base must stay 4-byte aligned. At 16 bpp that makes SIDEBAR_W even.
static_assert((SIDEBAR_W * 2) % 4 == 0, "SIDEBAR_W must keep the canvas base 4-byte aligned");

void canvasPanelRegion(KCM_TFT &tft) {
  const uint32_t base = infoDB.backAddr();
  tft.canvasImageStartAddress(base);
  tft.canvasImageWidth(SCREEN_W);
  tft.currentPage = base;             // keep text blits on the same origin as geometry
  tft.activeWindowXY(0, 0);
  tft.activeWindowWH(SCREEN_W, SCREEN_H);
}

void canvasContentRegion(KCM_TFT &tft) {
  if (CONTENT_X == 0) { canvasPanelRegion(tft); return; }   // unit 2 — nothing to shift
  const uint32_t base = infoDB.backAddr() + (uint32_t)CONTENT_X * 2UL;   // 16 bpp
  tft.canvasImageStartAddress(base);
  tft.canvasImageWidth(SCREEN_W);
  tft.currentPage = base;
  tft.activeWindowXY(0, 0);
  tft.activeWindowWH(CONTENT_W, SCREEN_H);   // clamp so a full-width fill cannot wrap
}


/***************************************************************************************
   DRAW SIDEBAR
   Drawn in panel space, not content space — it is the one piece of chrome that lives
   outside the content region. Restores the content region on exit so the caller can
   carry on drawing screen content without knowing this happened.
****************************************************************************************/
void drawSidebar(KCM_TFT &tft) {
  canvasPanelRegion(tft);
  // Clear the strip first. drawStaticScreen()'s fillScreen runs in the content region,
  // which on unit 1 stops at the sidebar, and the buttons do not quite cover the strip
  // themselves — the last one is clamped a row short so its bottom border stays visible
  // (see below). Without this the final scanline would keep stale pixels from whatever
  // was last on this page. On unit 2 the fillScreen already covered it; the extra fill
  // costs one rect per screen entry.
  tft.fillRect(SIDEBAR_X, 0, SIDEBAR_W, SCREEN_H, TFT_BLACK);
  tft.drawLine(SIDEBAR_DIV_X, 0, SIDEBAR_DIV_X, SCREEN_H - 1, TFT_GREY);
  uint16_t bx = SIDEBAR_BTN_X;
  uint16_t bw = SIDEBAR_W - 1;
  uint16_t bh = sbBtnH();
  uint8_t  activeBtn = screenToButton(activeScreen);
  for (uint8_t i = 0; i < SB_BTN_COUNT; i++) {
    ButtonLabel btn = (i == activeBtn) ? btnScreenOn : btnScreenOff;
    btn.text = sbButtonLabel(i);   // active mode's label when this button owns the screen
    // Ascent Autopilot key: green while the autopilot is armed. Engaged-mode green is
    // the standard assignment, so the one sidebar key with state worth annunciating is
    // the one key that carries colour — and it carries it only while that state holds.
    // This replaces a fixed purple/violet identity, which said "different kind of
    // control" in a hue the flight-deck vocabulary does not assign, and said it
    // identically whether the autopilot was idle or flying the vehicle. What kind of
    // control it is, is already carried by its position: parked at the bottom, below
    // the display-nav cluster. The sidebar draws with isOn=true, so only the ...On
    // fields are overridden; the border is left alone, so it still reads white when
    // this key owns the active screen and grey when it does not.
    if (SB_BTN_SCREEN[i] == screen_LNCHAP && apArmedAnnunciated()) {
      btn.backgroundColorOn = TFT_DARK_GREEN;
      btn.fontColorOn       = TFT_WHITE;
    }
    uint16_t by = sbBtnY(i);
    uint16_t h  = bh;
    // Last button tiles to row 599 (the panel's final scanline), where its bottom
    // border is hidden by the bezel. Clamp its height so the border lands on
    // SCREEN_H-2 and stays visible.
    if (i == SB_BTN_COUNT - 1) h = (SCREEN_H - 1) - by;
    drawButton(tft, bx, by, bw, h, btn, &Roboto_Black_20, true);
  }
  canvasContentRegion(tft);
}

/***************************************************************************************
   AUTO / MAN CHIP
   Says whether the screen in front of the pilot was chosen by the context ladder or
   held by hand. With the ladders running continuously this is the difference between
   "this panel will follow the mission" and "this panel is where I put it", and there
   is no way to infer it from the content — so it is stated, in words, rather than left
   to a coloured dot.

   MAN covers both overrides that can hold a screen: the panel-level selection latch,
   and the LAUNCH screen's own ASC/CIRC phase override, which can outlive the latch
   when the latch auto-releases.

   MAN is dark green — engaged-mode green, the same assignment the ASC key uses while
   the autopilot is armed. A held selection is a mode the pilot has engaged, so it
   reads as engaged rather than as an exception, and no alerting colour is spent.
   AUTO is grey: the resting state, stated but not asserted.

   Both states are outlined rather than filled, matching the sidebar keys, so the chip
   reads as a status badge and not as something to press.
****************************************************************************************/
static const uint16_t CHIP_W = 58, CHIP_H = 22, CHIP_R = 5;
static int8_t _chipShown = -1;    // -1 = unknown/needs redraw, 0 = AUTO, 1 = MAN

void invalidateModeChip() { _chipShown = -1; }

// drawRoundRectOutline lives in KerbalDisplayCommon. It was local to this chip with a
// note to promote it if a second caller appeared; the reference chips below are the
// second and third.


/***************************************************************************************
   REFERENCE CHIP  (SPACECRAFT velocity reference, AIRCRAFT altitude datum)
   Says which of a two-state mode the screen is using, and whether the pilot pinned it.

   The Shuttle annunciated its ADI's attitude reference beside the instrument because a
   ball whose frame is not stated is ambiguous; the same is true of a prograde marker
   that silently follows an altitude threshold. Both screens put the chip mirrored from
   their TRIM flag, at the heading tape's left edge.

   COLOUR follows the AUTO/MAN chip exactly: grey while the automatic rule is in force,
   dark green while the pilot holds it. A held reference is a mode the pilot engaged, so
   it reads as engaged rather than as an exception, and no alerting colour is spent.
   Cyan is deliberately NOT used -- that is reserved for pilot-ENTERED values, and this
   is a mode, not a value. Outlined rather than filled, like the mode chip and the
   sidebar keys, so it reads as status.

   The chip is smaller than the ~40 px a finger wants, so the HIT BOX is larger than the
   drawn shape -- see refChipHit.
****************************************************************************************/
static const int16_t REF_CHIP_W = 52, REF_CHIP_H = 24, REF_CHIP_R = 5;
// Mirrored from TRIM, which is right-aligned to the heading tape's right edge just above
// it. Both attitude screens share the tape geometry, so one pair of constants serves both.
static const int16_t REF_CHIP_X = 120;   // heading tape left edge (112) + 8
static const int16_t REF_CHIP_Y = 475;   // level with TRIM

void drawRefChip(KCM_TFT &tft, const char *text, bool held) {
  const uint16_t col = held ? TFT_DARK_GREEN : TFT_GREY;
  tft.fillRect(REF_CHIP_X, REF_CHIP_Y, REF_CHIP_W, REF_CHIP_H, TFT_BLACK);
  drawRoundRectOutline(tft, REF_CHIP_X, REF_CHIP_Y, REF_CHIP_W, REF_CHIP_H, REF_CHIP_R, col);
  textCenter(tft, &Roboto_Black_16, REF_CHIP_X, REF_CHIP_Y, REF_CHIP_W, REF_CHIP_H,
             text, col, TFT_BLACK);
}

// Touch target, generously larger than the 52x24 chip. Nothing else on either screen
// consumes a touch in this region, so the slack costs nothing.
bool refChipHit(int16_t cx, int16_t cy) {
  return cx >= REF_CHIP_X - 26 && cx < REF_CHIP_X + REF_CHIP_W + 26 &&
         cy >= REF_CHIP_Y - 22 && cy < REF_CHIP_Y + REF_CHIP_H + 22;
}

static bool panelInAuto() {
  if (_manualScreenLatch) return false;
  if (activeScreen == screen_LNCH && _lnchManualOverride) return false;
  return true;
}

void updateModeChip(KCM_TFT &tft) {
  const int8_t want = panelInAuto() ? 0 : 1;
  if (want == _chipShown) return;
  _chipShown = want;
  const uint16_t x = CONTENT_W - CHIP_W - 6, y = 6;
  const uint16_t col = want ? TFT_DARK_GREEN : TFT_GREY;
  tft.fillRect(x, y, CHIP_W, CHIP_H, TFT_BLACK);
  drawRoundRectOutline(tft, x, y, CHIP_W, CHIP_H, CHIP_R, col);
  textCenter(tft, &Roboto_Black_16, x, y, CHIP_W, CHIP_H,
             want ? "MAN" : "AUTO", col, TFT_BLACK);
}


/***************************************************************************************
   SIDEBAR STATE REFRESH
   drawSidebar() is chrome: it runs from drawStaticScreen(), which runs only on a screen
   change. That is fine for a key whose colour depends only on which screen is active,
   and wrong for a key whose colour depends on live state — the ASC key would not turn
   green until the pilot happened to switch screens, which during an ascent is exactly
   when they are not touching the panel. Called once per steady-state frame; it repaints
   the 84 px strip only when a state colour actually changes.

   The armed state read here is apArmedAnnunciated(), the same value the ARM button and
   the ARMED/DISARMED banner render — the autopilot's own state, never an unacknowledged
   tap. The key going green therefore means the vehicle is being flown by the autopilot,
   not that someone asked for it to be.

   Unit 1 has no state-coloured key (its sixth button is VEH), so this compiles out.
****************************************************************************************/
void updateSidebar(KCM_TFT &tft) {
#if INFO_DISP_IS_MISSION_UNIT
  static bool prevArmed = false;
  const bool armed = apArmedAnnunciated();
  if (armed == prevArmed) return;
  prevArmed = armed;
  drawSidebar(tft);
#else
  (void)tft;
#endif
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
   Convenience wrappers take the font from their caller. All screens use NR=8 rows.
****************************************************************************************/
// Split-column overload (#51) — explicit x, w for left/right half-row cells
void drawValue(KCM_TFT &tft, uint8_t screen, uint8_t row,
               uint16_t x, uint16_t w,
               const char *label, const String &value,
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
                    const char *label, const String &value,
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
  invalidateModeChip();   // the title bar is about to be repainted over the chip

  // LNDG/LNDGRE share one chrome/draw path (chromeScreen_LNDG), selected by a mode
  // flag derived from the active screen here and re-asserted in updateScreen().
  if (s == screen_LNDG)        _lndgReentryMode = false;
  else if (s == screen_LNDGRE) _lndgReentryMode = true;

  // Dynamic titles; everything else takes its title from SCREEN_TITLES.
  if (s == screen_LNCH) {
    drawTitleBar(tft, _lnchOrbitalMode ? "CIRCULARIZATION" : "ASCENT");
    // The manual-override red dot that used to sit here is gone: the panel-level
    // AUTO/MAN chip (updateModeChip) covers this screen's phase override too, and one
    // explicit indicator beats a coloured dot whose meaning has to be remembered.
  } else if (s == screen_DOCK) {
    // Vessel name from Simpit reflects the active/combined vessel after docking.
    String dockTitle = String("DOCKING [ ") + state.vesselName + " ]";
    drawTitleBar(tft, dockTitle);
  } else {
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
    case screen_NAV:    chromeScreen_NAV(tft); break;
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
    // The LNDG mode flag is derived from the screen; asserting it here as well as in
    // drawStaticScreen() means nothing else can leave chrome and draw disagreeing.
    case screen_LNDG:   _lndgReentryMode = false; drawScreen_LNDG(tft); break;
    case screen_LNDGRE: _lndgReentryMode = true;  drawScreen_LNDG(tft); break;
    case screen_ACFT:   drawScreen_ACFT(tft); break;
    case screen_LNCHAP: drawScreen_LNCHAP(tft); break;
    case screen_NAV:    drawScreen_NAV(tft); break;
    default: break;
  }
}

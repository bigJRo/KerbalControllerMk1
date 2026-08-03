/***************************************************************************************
   ScreenMain.ino -- Main bar graph screen for Kerbal Controller Mk1 Resource Display

   Layout (three regions, left to right):
     Gauge column : GAUGE_COL_W px — four fixed 270° ring gauges (EC/O2/Food/H2O),
                    always present in both TOTAL and STAGE mode
     Bar region   : vertical resource bars for the remaining (user-selected)
                    resources, dynamically sized based on slotCount
     Right region : SIDEBAR_W px button column, 4 buttons edge-to-edge, full height

   Sidebar buttons (top to bottom):
     0. TOTL / STG  -- toggles stageMode. TOTL=dark green, STG=crimson.
     1. DFLT        -- resets slots to the STD default group
     2. SEL         -- navigates to the resource selection screen
     3. DATA        -- navigates to the numerical resource detail screen

   Button labels are horizontal, centred, using SB_BTN_FONT (Roboto_Black_28).
   Bottom three buttons (DFLT, SEL, DATA) use a navy background.

   Each bar:
     - Always drawn in the resource's designated fixed color, regardless of fill
       level. Low-level warning is conveyed solely by the percentage text color.
     - Two-fillRect technique: black from top to empty boundary, then color below
     - Resource label centred below the bar in white (font scales with bar width)
     - Percentage centred above the bar in white (font scales with bar width)
     - Percentage color: >30% white, 11-30% yellow (caution), 0-10% red (critical)
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


/***************************************************************************************
   LAYOUT CONSTANTS -- MAIN SCREEN
****************************************************************************************/
static const uint16_t SCREEN_W     = KCM_SCREEN_W;   // #3A from SystemConfig
static const uint16_t SCREEN_H     = KCM_SCREEN_H;   // #3A from SystemConfig
static const uint16_t AXIS_W       = 44;   // px reserved on left for Y-axis labels + ticks
static const uint16_t SIDEBAR_W    = 84;   // px -- width of right-hand nav button column (matches InfoDisp)
static const tFont   *SB_BTN_FONT  = &Roboto_Black_24;  // nav-button label font (fits 4 chars at 84px)

// Fixed ring-gauge column on the far left (EC/O2/Food/H2O), present in both modes.
// The bar region is what remains between the gauge column and the sidebar.
static const uint16_t GAUGE_COL_W  = 120;  // px -- width of the left gauge column
static const uint16_t GAUGE_R      = 56;   // ring outer radius
static const uint16_t GAUGE_THICK  = 13;   // ring band thickness
static inline uint16_t gaugeRowH()  { return SCREEN_H / GAUGE_COUNT; }   // vertical slot per gauge

static inline uint16_t barRegionW() { return SCREEN_W - SIDEBAR_W - GAUGE_COL_W; }
static inline uint16_t barAreaW()   { return barRegionW() - AXIS_W; }
static const uint16_t LABEL_H      = 44;
static const uint16_t PERC_H       = 36;
static const uint16_t BAR_TOP      = PERC_H;
static const uint16_t BAR_BOTTOM   = SCREEN_H - LABEL_H;
static const uint16_t BAR_H        = BAR_BOTTOM - BAR_TOP;
static const uint16_t BAR_PAD      = 8;

// Sidebar: 4 buttons, no padding, edge-to-edge, full height
static const uint8_t  SB_BTN_COUNT = 4;

// Sidebar geometry computed at runtime via inline functions so barRegionW()
// always reflects the current SIDEBAR_W value without duplication.
static inline uint16_t sbX()    { return SCREEN_W - SIDEBAR_W; }
static inline uint16_t sbBtnH() { return SCREEN_H / SB_BTN_COUNT; }
static inline uint16_t sbBtnY(uint8_t btn) { return btn * sbBtnH(); }


/***************************************************************************************
   SIDEBAR BUTTON DEFINITIONS
   Mode button uses two ButtonLabel instances — one per stageMode state.
   Bottom three buttons share a navy background.
   All buttons have a TFT_GREY border for visual separation.
****************************************************************************************/
static const ButtonLabel btnModeTotal = {
  "TOTL",
  TFT_WHITE, TFT_WHITE,
  TFT_OFF_BLACK, TFT_DARK_GREEN,
  TFT_GREY, TFT_GREY
};
static const ButtonLabel btnModeStage = {
  "STG",
  TFT_WHITE, TFT_WHITE,
  TFT_OFF_BLACK, TFT_CORNELL,
  TFT_GREY, TFT_GREY
};
static const ButtonLabel btnReset = {
  "DFLT",
  TFT_WHITE, TFT_WHITE,
  TFT_NAVY, TFT_NAVY,
  TFT_GREY, TFT_GREY
};
static const ButtonLabel btnSelect = {
  "SEL",
  TFT_WHITE, TFT_WHITE,
  TFT_NAVY, TFT_NAVY,
  TFT_GREY, TFT_GREY
};
static const ButtonLabel btnDetail = {
  "DATA",
  TFT_WHITE, TFT_WHITE,
  TFT_NAVY, TFT_NAVY,
  TFT_GREY, TFT_GREY
};


/***************************************************************************************
   DRAW SIDEBAR
****************************************************************************************/
static void drawSidebar(KCM_TFT &tft) {
  tft.drawLine(sbX(), 0, sbX(), SCREEN_H, TFT_GREY);
  uint16_t bx = sbX() + 1;
  uint16_t bw = SIDEBAR_W - 1;

  // Button 0: TOTAL / STAGE mode toggle -- always drawn "on" (illuminated)
  const ButtonLabel &modeBtn = stageMode ? btnModeStage : btnModeTotal;
  drawButton(tft, bx, sbBtnY(0), bw, sbBtnH(), modeBtn, SB_BTN_FONT, true);

  // Buttons 1-3: action buttons, drawn "on" so their background color always shows
  drawButton(tft, bx, sbBtnY(1), bw, sbBtnH(), btnReset,  SB_BTN_FONT, true);
  drawButton(tft, bx, sbBtnY(2), bw, sbBtnH(), btnSelect, SB_BTN_FONT, true);
  drawButton(tft, bx, sbBtnY(3), bw, sbBtnH(), btnDetail, SB_BTN_FONT, true);
}


/***************************************************************************************
   REDRAW MODE BUTTON ONLY
   Called when stageMode toggles to avoid a full screen redraw.
****************************************************************************************/
void redrawStageModeButton(KCM_TFT &tft) {
  const ButtonLabel &modeBtn = stageMode ? btnModeStage : btnModeTotal;
  drawButton(tft, sbX() + 1, sbBtnY(0), SIDEBAR_W - 1, sbBtnH(), modeBtn, SB_BTN_FONT, true);
}


/***************************************************************************************
   BAR GEOMETRY HELPERS
   Bars occupy barAreaW() (the bar region minus the left axis strip).
   barX() offsets all bars to the right of the axis.
****************************************************************************************/
static uint16_t barWidth() {
  if (slotCount == 0) return 0;
  uint16_t totalPad = BAR_PAD * (slotCount + 1);
  return (barAreaW() - totalPad) / slotCount;
}

static uint16_t barX(uint8_t index) {
  return GAUGE_COL_W + AXIS_W + BAR_PAD + index * (barWidth() + BAR_PAD);
}


/***************************************************************************************
   DRAW Y-AXIS
   Delegates to the library's drawLabelledAxis(). See KerbalDisplayCommon.h.
****************************************************************************************/
static void drawAxis(KCM_TFT &tft) {
  drawLabelledAxis(tft,
                   GAUGE_COL_W, AXIS_W,
                   BAR_TOP, BAR_BOTTOM,
                   &Roboto_Black_12,
                   TFT_LIGHT_GREY, TFT_BLACK);
}


/***************************************************************************************
   BAR FONT SELECTOR
   Returns the largest font that fits comfortably within the current bar width.
   Called on each update pass so font scales automatically as slot count changes.
****************************************************************************************/
static const tFont* barFont() {
  uint16_t bw = barWidth();
  if (bw >= 60) return &Roboto_Black_20;
  if (bw >= 40) return &Roboto_Black_16;
  return &Roboto_Black_12;
}


/***************************************************************************************
   FIXED RING GAUGES (left column)
   Four 270° ring gauges (gap at the bottom) for EC / O2 / Food / Water, stacked
   vertically. The ring fills clockwise from bottom-left (0%) over the top to
   bottom-right (100%); the abbreviation + percentage sit in the centre hole.
   The ring band always renders in the resource colour; only the percentage text
   is threshold-coloured. Redraws are gated on the integer percentage and drawn as
   deltas (grow in resource colour / shrink in track colour) so they don't flicker.
****************************************************************************************/
static uint8_t _prevGaugePerc[GAUGE_COUNT];   // last integer % drawn; 255 = force repaint

static inline void _gaugeCenter(uint8_t idx, int16_t &cx, int16_t &cy) {
  cx = (int16_t)(GAUGE_COL_W / 2);
  cy = (int16_t)(idx * gaugeRowH() + gaugeRowH() / 2);
}

// Level (0..1) for a gauge; maxVal == 0 (resource absent) parks at 0.
static float _gaugeLevel(uint8_t idx) {
  float m = gauges[idx].maxVal;
  float l = (m > 0.0f) ? (gauges[idx].current / m) : 0.0f;
  return constrain(l, 0.0f, 1.0f);
}

// Draw the ring band for fill fractions [p0,p1] in `color`, as overlapping dots
// along the 270° arc centreline. p=0 -> 225° (bottom-left), sweeping clockwise
// (decreasing angle) 270° to p=1 -> -45° (bottom-right).
static void _gaugeArc(KCM_TFT &tft, int16_t cx, int16_t cy,
                      float p0, float p1, uint16_t color) {
  if (p1 <= p0) return;
  const float rc = (float)GAUGE_R - (float)GAUGE_THICK / 2.0f;
  int steps = (int)ceilf((p1 - p0) * 270.0f / 1.2f);   // ~1.2° per dot
  if (steps < 1) steps = 1;
  int16_t r = (int16_t)(GAUGE_THICK / 2);
  for (int s = 0; s <= steps; s++) {
    float p   = p0 + (p1 - p0) * (float)s / (float)steps;
    float ang = (225.0f - 270.0f * p) * 0.01745329f;   // deg -> rad
    int16_t px = cx + (int16_t)lroundf(rc * cosf(ang));
    int16_t py = cy - (int16_t)lroundf(rc * sinf(ang));
    tft.fillCircle(px, py, r, color);
  }
}

// Static abbreviation label (top half of the centre hole) — drawn once.
static void drawGaugeLabel(KCM_TFT &tft, uint8_t idx) {
  int16_t cx, cy; _gaugeCenter(idx, cx, cy);
  const char *lbl = resLabel(gauges[idx].type);
  tft.setFont(Roboto_Black_16);
  tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
  int16_t lw = getFontStringWidth(&Roboto_Black_16, lbl);
  tft.setCursor(cx - lw / 2, cy - (int16_t)Roboto_Black_16.cap_height - 3);
  tft.print(lbl);
}

// Percentage value (bottom half of the centre hole) — threshold-coloured, redrawn
// on change. Clears only its own line so the label above is untouched.
static void drawGaugeValue(KCM_TFT &tft, uint8_t idx, uint8_t perc) {
  int16_t cx, cy; _gaugeCenter(idx, cx, cy);
  char pbuf[6];
  snprintf(pbuf, sizeof(pbuf), "%d%%", perc);
  uint16_t fore, back;
  thresholdColor((uint16_t)perc,
                 (uint16_t)10, TFT_RED,    TFT_BLACK,
                 (uint16_t)30, TFT_YELLOW, TFT_BLACK,
                              TFT_WHITE,  TFT_BLACK,
                 fore, back);
  int16_t vh = (int16_t)Roboto_Black_20.cap_height + 4;
  tft.fillRect(cx - 30, cy + 2, 60, vh, back);
  tft.setFont(Roboto_Black_20);
  tft.setTextColor(fore, back);
  int16_t vw = getFontStringWidth(&Roboto_Black_20, pbuf);
  tft.setCursor(cx - vw / 2, cy + 2);
  tft.print(pbuf);
}

// Full gauge draw (track + fill + label + value) — on screen entry / mode change.
static void drawGaugeFull(KCM_TFT &tft, uint8_t idx) {
  int16_t cx, cy; _gaugeCenter(idx, cx, cy);
  uint8_t perc = (uint8_t)(_gaugeLevel(idx) * 100.0f);
  _gaugeArc(tft, cx, cy, 0.0f, 1.0f, TFT_DARK_GREY);                           // track
  if (perc > 0) _gaugeArc(tft, cx, cy, 0.0f, perc / 100.0f, resColor(gauges[idx].type));
  drawGaugeLabel(tft, idx);
  drawGaugeValue(tft, idx, perc);
  _prevGaugePerc[idx] = perc;
}

// Draw the whole gauge column + its divider (called from drawStaticMain).
static void drawGaugeColumn(KCM_TFT &tft) {
  tft.drawLine(GAUGE_COL_W, 0, GAUGE_COL_W, SCREEN_H, TFT_GREY);
  for (uint8_t i = 0; i < GAUGE_COUNT; i++) drawGaugeFull(tft, i);
}

// Per-frame gauge update — delta arc + value, gated on integer % (flicker-free).
static void updateGaugeColumn(KCM_TFT &tft) {
  for (uint8_t i = 0; i < GAUGE_COUNT; i++) {
    uint8_t perc = (uint8_t)(_gaugeLevel(i) * 100.0f);
    if (perc == _prevGaugePerc[i]) continue;
    int16_t cx, cy; _gaugeCenter(i, cx, cy);
    float prevP = _prevGaugePerc[i] / 100.0f;
    float curP  = perc / 100.0f;
    if (perc > _prevGaugePerc[i])
      _gaugeArc(tft, cx, cy, prevP, curP, resColor(gauges[i].type));   // grow
    else
      _gaugeArc(tft, cx, cy, curP, prevP, TFT_DARK_GREY);              // shrink
    drawGaugeValue(tft, i, perc);
    _prevGaugePerc[i] = perc;
  }
}


/***************************************************************************************
   DRAW STATIC CHROME -- main screen
****************************************************************************************/
static float   _prevLevel[MAX_SLOTS];
static uint8_t _prevPerc[MAX_SLOTS];   // last integer % drawn per bar; 255 = force repaint
static bool    _prevStageMode = false;

void drawStaticMain(KCM_TFT &tft) {
  tft.fillScreen(TFT_BLACK);
  drawSidebar(tft);
  drawGaugeColumn(tft);
  drawAxis(tft);

  // Reset update-pass state so all bars and percentage labels repaint on first pass
  for (uint8_t i = 0; i < MAX_SLOTS; i++) { _prevLevel[i] = -1.0f; _prevPerc[i] = 255; }
  _prevStageMode = stageMode;

  const tFont *font = barFont();
  uint16_t bw = barWidth();
  for (uint8_t i = 0; i < slotCount; i++) {
    uint16_t x = barX(i);
    tft.drawRect(x, BAR_TOP, bw, BAR_H, TFT_GREY);
    if (slots[i].type != RES_NONE) {
      tft.setFont(*font);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      const char *lbl = resLabel(slots[i].type);
      int16_t lblW = getFontStringWidth(font, lbl);
      int16_t lblH = (int16_t)font->cap_height;
      tft.setCursor(x + (bw - lblW) / 2, BAR_BOTTOM + (LABEL_H - lblH) / 2);
      tft.print(lbl);
    }
  }
}


/***************************************************************************************
   DRAW ONE BAR -- flicker-free two-fillRect technique
****************************************************************************************/
static void drawBar(KCM_TFT &tft, uint16_t x, uint16_t bw, uint16_t fillH, uint16_t color) {
  uint16_t emptyH = (BAR_H - 2) - fillH;
  uint16_t innerX = x + 1;
  uint16_t innerW = bw - 2;

  if (emptyH > 0) tft.fillRect(innerX, BAR_TOP + 1,           innerW, emptyH, TFT_BLACK);
  if (fillH  > 0) tft.fillRect(innerX, BAR_TOP + 1 + emptyH,  innerW, fillH,  color);
  tft.drawRect(x, BAR_TOP, bw, BAR_H, TFT_GREY);
}


/***************************************************************************************
   UPDATE PASS
****************************************************************************************/
void updateScreenMain(KCM_TFT &tft) {
  if (stageMode != _prevStageMode) {
    _prevStageMode = stageMode;
    redrawStageModeButton(tft);
    for (uint8_t i = 0; i < MAX_SLOTS; i++) { _prevLevel[i] = -1.0f; _prevPerc[i] = 255; }
  }

  // Fixed gauges update every frame (independent of TOTAL/STAGE — they always
  // show vessel totals). Gated internally on integer %, so this is cheap.
  updateGaugeColumn(tft);

  uint16_t bw = barWidth();
  const tFont *font = barFont();

  for (uint8_t i = 0; i < slotCount; i++) {
    float cur = stageMode ? slots[i].stageCurrent : slots[i].current;
    float max = stageMode ? slots[i].stageMax     : slots[i].maxVal;
    float level = (max > 0.0f) ? (cur / max) : 0.0f;
    level = constrain(level, 0.0f, 1.0f);
    uint8_t perc = (uint8_t)(level * 100.0f);

    // Redraw the bar fill on any meaningful level change, but redraw the
    // percentage label ONLY when the integer % actually changes. Decoupling the
    // two stops the number from being cleared-and-reprinted every frame (the
    // source of the flicker) while the bar still animates smoothly.
    bool levelChanged = fabsf(level - _prevLevel[i]) >= BAR_LEVEL_HYSTERESIS;
    bool percChanged  = (perc != _prevPerc[i]);
    if (!levelChanged && !percChanged) continue;

    uint16_t x = barX(i);

    if (levelChanged) {
      _prevLevel[i] = level;
      uint16_t fillH = (uint16_t)((BAR_H - 2) * level);
      // Bar always renders in the resource's designated colour, regardless of
      // fill level. Low-level warning is conveyed by the percentage text colour.
      drawBar(tft, x, bw, fillH, resColor(slots[i].type));
    }

    if (percChanged) {
      _prevPerc[i] = perc;

      char percStr[6];
      snprintf(percStr, sizeof(percStr), "%d%%", perc);

      // Percentage label colour depends on fill level:
      //   0-10%   : red text    (critical)
      //  11-30%   : yellow text  (caution)
      //  31-100%  : white text   (nominal)
      uint16_t percFore, percBack;
      thresholdColor((uint16_t)perc,
                     (uint16_t)10, TFT_RED,    TFT_BLACK,
                     (uint16_t)30, TFT_YELLOW, TFT_BLACK,
                                  TFT_WHITE,  TFT_BLACK,
                     percFore, percBack);

      int16_t fontH = (int16_t)font->cap_height;
      tft.fillRect(x, 0, bw, PERC_H, percBack);
      tft.setFont(*font);
      tft.setTextColor(percFore, percBack);
      int16_t tw = getFontStringWidth(font, percStr);
      tft.setCursor(x + (bw - tw) / 2, (PERC_H - fontH) / 2);
      tft.print(percStr);
    }
  }
}


/***************************************************************************************
   SIDEBAR HIT TEST
****************************************************************************************/
int8_t sidebarHitTest(uint16_t x, uint16_t y) {
  if (x < sbX()) return -1;
  for (uint8_t btn = 0; btn < SB_BTN_COUNT; btn++) {
    uint16_t by = sbBtnY(btn);
    if (y >= by && y < by + sbBtnH()) return (int8_t)btn;
  }
  return -1;
}

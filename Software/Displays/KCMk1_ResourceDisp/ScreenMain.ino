/***************************************************************************************
   ScreenMain.ino -- Main tape-meter screen for Kerbal Controller Mk1 Resource Display

   Layout (1024x600):
     Left  : SIDEBAR_W px button column, 4 keys edge-to-edge, full height
     Then  : AXIS_W px shared 0-100% axis
     Then  : one tape meter per active slot at a FIXED pitch, left-anchored, in
             subsystem order, with a bracketed group label above each run of
             same-group meters and a divider between groups

   Sidebar buttons (top to bottom):
     0. TOTL / STG  -- toggles stageMode (which value the fill and counter show)
     1. DFLT        -- resets slots to the STD default group
     2. SEL         -- navigates to the resource selection screen
     3. DATA        -- navigates to the numerical resource detail screen

   Each meter, top to bottom:
     - group label band (shared by a run of same-group meters)
     - the tape: a narrow thermometer column filled in the resource colour to the
       PRIMARY value (vessel total in TOTL, active stage in STG), with
         . a limit-band column on its left. The resource's alarm and caution
           fractions are painted red and yellow on the scale itself, so the limits
           are visible whether or not the level is in them. This is the Shuttle
           F7/O3 meter convention, and it is what the filled bars lacked: their only
           warning was the percentage label changing colour.
         . for resources with a separate stage channel, the tape is split into two
           columns, the way the Shuttle PRPLT QTY meter carried its three pointers
           side by side: the wide left column is the PRIMARY value in the resource
           colour, the narrow right column is the SECONDARY value (stage in TOTL,
           total in STG) in a half-brightness shade of the same colour, and a white
           line across the whole tape marks the secondary level so it reads against
           the ticks even where the narrow column is only a few pixels wide. The
           mode key therefore chooses which value is wide and counted, not which
           one you can see. Resources without a stage channel get one full-width
           column, which is itself the cue that there is no stage figure.
         . tick marks on the right edge: major every 10%, minor every 5%
       The frame is grey when nominal and takes the caution/alarm colour on breach.
     - resource short label
     - percent counter, coloured by state (white / yellow / white-on-red), with a
       trend arrow in a cell to its right while the value is moving
     - units counter (raw resource units, compacted to fit the pitch)
     A slot whose capacity is zero (resource not aboard) shows "---" in grey with an
     empty tape and no bands lit, rather than a 0% alarm.

   Pitch classes: PITCH_STD for up to DEFAULT_SLOT_COUNT meters, PITCH_CMP above
   that. Within a class every meter keeps its width and pitch, and the row is
   centred in the meter area, so a partial set sits in the middle of the screen
   rather than bunched against the axis. Fixed width is what keeps the meters
   reading as instruments rather than as a chart that restretches per vessel.
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


/***************************************************************************************
   LAYOUT CONSTANTS -- MAIN SCREEN
****************************************************************************************/
static const uint16_t SCREEN_W     = KCM_SCREEN_W;   // #3A from SystemConfig
static const uint16_t SCREEN_H     = KCM_SCREEN_H;   // #3A from SystemConfig
static const uint16_t AXIS_W       = 50;   // px reserved for the Y-axis labels + ticks (fits Black_16)
static const uint16_t SIDEBAR_W    = 84;   // px -- width of the nav button column (matches InfoDisp)
static const tFont   *SB_BTN_FONT  = &Roboto_Black_24;  // nav-button label font (fits 4 chars at 84px)

// The sidebar sits on the LEFT edge. On panel B1 this display is outboard of Info
// Display 2, whose own sidebar is on its right edge -- so putting this one on the left
// brings the two button columns together at the boundary between the two screens,
// where they form a single control cluster near the middle of the panel rather than
// sitting a full screen-width apart with the far one at B1's outboard corner.
//
// Content therefore occupies [CONTENT_X, SCREEN_W): the Y-axis strip first, then the
// meters. Everything downstream derives from METER_X0, so this is the only place the
// offset is stated.
static const uint16_t SIDEBAR_X    = 0;
static const uint16_t CONTENT_X    = SIDEBAR_W;
static const uint16_t METER_X0     = CONTENT_X + AXIS_W;        // 134 -- first meter's left edge
static const uint16_t METER_AREA_W = SCREEN_W - METER_X0;       // 890 -- the row of meters is centred in this

// Fixed pitch classes. Both derive from the area width so a screen-size change moves
// them together; neither depends on the live slot count.
static const uint16_t PITCH_STD    = METER_AREA_W / DEFAULT_SLOT_COUNT;  // 98 -- up to 9 meters
static const uint16_t PITCH_CMP    = METER_AREA_W / MAX_SLOTS;           // 55 -- 10 to 16 meters

// Vertical bands, top to bottom.
static const uint16_t GROUP_H      = 26;   // subsystem label band across the top
static const uint16_t TAPE_TOP     = GROUP_H + 6;               // 32
static const uint16_t LABEL_H      = 26;   // resource short label (Black_20 cap 24)
static const uint16_t PERC_H       = 32;   // percent counter + trend arrow (Black_24 cap 29)
static const uint16_t UNITS_H      = 22;   // units counter (Black_16 cap 19)
static const uint16_t FOOT_H       = LABEL_H + PERC_H + UNITS_H;  // 80
static const uint16_t TAPE_BOTTOM  = SCREEN_H - FOOT_H;          // 520
static const uint16_t TAPE_H       = TAPE_BOTTOM - TAPE_TOP;     // 488
static const uint16_t TAPE_INNER_H = TAPE_H - 2;                 // 486 -- fill area inside the frame
static const uint16_t LABEL_Y      = TAPE_BOTTOM;                // 520
static const uint16_t PERC_Y       = LABEL_Y + LABEL_H;          // 546
static const uint16_t UNITS_Y      = PERC_Y + PERC_H;            // 578

static const uint16_t BAND_W       = 4;    // limit-band column, left of the tape
static const uint16_t MARK_H       = 2;    // secondary-value marker line thickness
static const uint16_t SPLIT_GAP    = 1;    // black gap between the primary and secondary columns

// Sidebar: 4 buttons, no padding, edge-to-edge, full height
static const uint8_t  SB_BTN_COUNT = 4;

// Sidebar geometry computed at runtime via inline functions. sbX() is the sidebar's
// left edge; the 1 px divider rule sits on its inboard (right) edge, against the content.
static inline uint16_t sbX()    { return SIDEBAR_X; }
static inline uint16_t sbDivX() { return SIDEBAR_X + SIDEBAR_W - 1; }
static inline uint16_t sbBtnH() { return SCREEN_H / SB_BTN_COUNT; }
static inline uint16_t sbBtnY(uint8_t btn) { return btn * sbBtnH(); }


/***************************************************************************************
   METER STYLE -- everything that differs between the two pitch classes
   (MeterStyle / MeterGeom / MeterCache are defined in KCMk1_ResourceDisp.h: the
   Arduino build hoists a prototype for every function above the tabs, so a type used
   in a signature must already be visible from the header.)
****************************************************************************************/
static const MeterStyle STYLE_STD = {
  PITCH_STD, 30, 9, 6, 14, 6,
  &Roboto_Black_20, &Roboto_Black_24, &Roboto_Black_16, &Roboto_Black_16
};
static const MeterStyle STYLE_CMP = {
  PITCH_CMP, 16, 5, 5, 11, 5,
  &Roboto_Black_16, &Roboto_Black_16, &Roboto_Black_12, &Roboto_Black_12
};

static inline const MeterStyle &meterStyle() {
  return (slotCount <= DEFAULT_SLOT_COUNT) ? STYLE_STD : STYLE_CMP;
}

// Left edge of meter i's pitch cell. The row of slotCount meters is centred in the
// meter area; slotCount only changes through a chrome redraw, so every caller in
// one frame sees the same origin.
static inline uint16_t pitchX(const MeterStyle &st, uint8_t i) {
  uint16_t rowW = slotCount * st.pitch;
  return METER_X0 + (METER_AREA_W - rowW) / 2 + i * st.pitch;
}


static MeterGeom meterGeom(const MeterStyle &st, uint8_t i) {
  MeterGeom g;
  uint16_t scaleW = BAND_W + 1 + st.tapeW + 1 + st.tickL;
  g.px    = pitchX(st, i);
  g.bandX = g.px + (st.pitch - scaleW) / 2;
  g.tapeX = g.bandX + BAND_W + 1;
  g.tickX = g.tapeX + st.tapeW + 1;
  return g;
}

// Half-brightness of an RGB565 colour: each channel shifted down one bit, with the
// bits that would cross a channel boundary masked off. The library's TFT_DIM_VIOLET
// and TFT_DIM_NEON_GRN are exactly this of their parents.
static inline uint16_t dimColor(uint16_t c) {
  return (c >> 1) & 0x7BEF;
}

// Screen y of a 0..1 level on the tape's inner scale (0 at the bottom inner row).
static inline uint16_t levelY(float level) {
  return (TAPE_BOTTOM - 1) - (uint16_t)(TAPE_INNER_H * level);
}


/***************************************************************************************
   SIDEBAR BUTTON DEFINITIONS
   The sidebar is chrome, not data, so it is achromatic -- white-on-black, grey border --
   and shows an engaged state by reverse video. This matches the InfoDisp sidebar and
   the convention bezel-key flight decks use, and it keeps the panel's colour vocabulary
   (yellow caution / red alarm, and the per-resource meter colours) for the telemetry
   the sidebar frames. See the InfoDisp README for the full rationale.

   The mode key is the one key with state: TOTAL is the default and draws like any other
   key, STAGE reverse-videos, so the key reads as "changed from the default" as well as
   naming the mode. The border stays grey on both, since neither is a selection.
****************************************************************************************/
static const ButtonLabel btnModeTotal = {
  "TOTL",
  TFT_WHITE, TFT_WHITE,
  TFT_BLACK, TFT_BLACK,
  TFT_GREY, TFT_GREY
};
static const ButtonLabel btnModeStage = {
  "STG",
  TFT_BLACK, TFT_BLACK,
  TFT_GREY, TFT_GREY,
  TFT_GREY, TFT_GREY
};
static const ButtonLabel btnReset = {
  "DFLT",
  TFT_WHITE, TFT_WHITE,
  TFT_BLACK, TFT_BLACK,
  TFT_GREY, TFT_GREY
};
static const ButtonLabel btnSelect = {
  "SEL",
  TFT_WHITE, TFT_WHITE,
  TFT_BLACK, TFT_BLACK,
  TFT_GREY, TFT_GREY
};
static const ButtonLabel btnDetail = {
  "DATA",
  TFT_WHITE, TFT_WHITE,
  TFT_BLACK, TFT_BLACK,
  TFT_GREY, TFT_GREY
};


/***************************************************************************************
   DRAW SIDEBAR
****************************************************************************************/
static void drawSidebar(KCM_TFT &tft) {
  tft.drawLine(sbDivX(), 0, sbDivX(), SCREEN_H, TFT_GREY);
  uint16_t bx = sbX();
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
  drawButton(tft, sbX(), sbBtnY(0), SIDEBAR_W - 1, sbBtnH(), modeBtn, SB_BTN_FONT, true);
}


/***************************************************************************************
   DRAW Y-AXIS
   Delegates to the library's drawLabelledAxis(). The shared axis spans the same rows
   as every tape, so its labels read against all of them.
****************************************************************************************/
static void drawAxis(KCM_TFT &tft) {
  drawLabelledAxis(tft,
                   CONTENT_X, AXIS_W,
                   TAPE_TOP, TAPE_BOTTOM,
                   &Roboto_Black_16,
                   TFT_LIGHT_GREY, TFT_BLACK);
}


/***************************************************************************************
   ALERT STATE
   0 nominal, 1 caution, 2 alarm -- from the resource's limit table. Waste-type
   resources alert on filling up rather than running down.
****************************************************************************************/
static uint8_t meterState(ResourceType t, float level) {
  ResLimits lim = resLimits(t);
  if (!lim.enabled) return 0;
  if (lim.highIsBad) {
    if (level > lim.alarm) return 2;
    if (level > lim.warn)  return 1;
  } else {
    if (level < lim.alarm) return 2;
    if (level < lim.warn)  return 1;
  }
  return 0;
}

static inline uint16_t stateColor(uint8_t state) {
  return (state == 2) ? TFT_RED : (state == 1) ? TFT_YELLOW : TFT_WHITE;
}

static inline uint16_t frameColor(uint8_t state) {
  return (state == 0) ? TFT_GREY : stateColor(state);
}


/***************************************************************************************
   UNITS COUNTER FORMATTER
   Raw resource units compacted to fit a pitch cell: 1.23 / 12.3 / 123 / 1234 / 12.3k /
   123k. Kept local rather than using formatSep(): that keeps two decimals up to 999
   and adds thousands separators, both too wide for the compact pitch.
****************************************************************************************/
static void fmtUnits(float v, char *buf, size_t n) {
  if (v < 0.0f) v = 0.0f;
  if (v >= 100000.0f) {
    snprintf(buf, n, "%dk", (int)(v / 1000.0f + 0.5f));
  } else if (v >= 10000.0f) {
    dtostrf(v / 1000.0f, 1, 1, buf);
    strlcat(buf, "k", n);
  } else if (v >= 100.0f) {
    snprintf(buf, n, "%d", (int)(v + 0.5f));
  } else if (v >= 10.0f) {
    dtostrf(v, 1, 1, buf);
  } else {
    dtostrf(v, 1, 2, buf);
  }
}


/***************************************************************************************
   METER CHROME -- drawn once per meter by drawStaticMain()
****************************************************************************************/

// Limit bands: the band column is dark grey over the full tape height, with the
// caution and alarm fractions painted on it. For a low-is-bad resource red runs from
// 0 to the alarm fraction and yellow from there to the caution fraction; for a
// high-is-bad (waste) resource the same two bands sit at the top instead.
static void drawBands(KCM_TFT &tft, const MeterGeom &g, ResourceType t) {
  tft.fillRect(g.bandX, TAPE_TOP + 1, BAND_W, TAPE_INNER_H, TFT_DARK_GREY);
  ResLimits lim = resLimits(t);
  if (!lim.enabled) return;

  uint16_t yWarn  = levelY(lim.warn);
  uint16_t yAlarm = levelY(lim.alarm);
  if (lim.highIsBad) {
    // Alarm band from the top of the scale down to the alarm fraction, caution band
    // from there down to the warn fraction (yAlarm is above yWarn on screen).
    tft.fillRect(g.bandX, TAPE_TOP + 1, BAND_W, yAlarm - (TAPE_TOP + 1) + 1, TFT_RED);
    tft.fillRect(g.bandX, yAlarm + 1,   BAND_W, yWarn - yAlarm,              TFT_YELLOW);
  } else {
    // yAlarm is lower on screen than yWarn
    tft.fillRect(g.bandX, yWarn,  BAND_W, (TAPE_BOTTOM - 1) - yWarn + 1, TFT_YELLOW);
    tft.fillRect(g.bandX, yAlarm, BAND_W, (TAPE_BOTTOM - 1) - yAlarm + 1, TFT_RED);
  }
}

// Tick marks right of the frame: major every 10%, minor every 5%.
static void drawTicks(KCM_TFT &tft, const MeterStyle &st, const MeterGeom &g) {
  for (uint8_t pct = 0; pct <= 100; pct += 5) {
    uint16_t y   = levelY(pct / 100.0f);
    uint16_t len = (pct % 10 == 0) ? st.tickL : (st.tickL / 2);
    tft.drawLine(g.tickX, y, g.tickX + len - 1, y, TFT_GREY);
  }
}

static void drawFrame(KCM_TFT &tft, const MeterStyle &st, const MeterGeom &g, uint8_t state) {
  tft.drawRect(g.tapeX, TAPE_TOP, st.tapeW, TAPE_H, frameColor(state));
}

// Group label band and dividers. Runs of same-group meters (slots are already in
// subsystem order) share one label centred over the run, sitting on a bracket line
// with a short down-tick at each end; consecutive runs are separated by a 1 px
// divider through the tape rows.
static void drawGroupBands(KCM_TFT &tft, const MeterStyle &st) {
  uint8_t runStart = 0;
  while (runStart < slotCount) {
    ResGroup grp = resGroup(slots[runStart].type);
    uint8_t runEnd = runStart;
    while (runEnd + 1 < slotCount && resGroup(slots[runEnd + 1].type) == grp) runEnd++;

    uint16_t x0 = pitchX(st, runStart);
    uint16_t x1 = pitchX(st, runEnd) + st.pitch;   // exclusive
    uint16_t ly = GROUP_H - 3;                     // bracket line row
    tft.drawLine(x0 + 3, ly, x1 - 4, ly, TFT_GREY);
    tft.drawLine(x0 + 3, ly, x0 + 3, ly + 3, TFT_GREY);
    tft.drawLine(x1 - 4, ly, x1 - 4, ly + 3, TFT_GREY);
    // Label drawn last, black-backed, so it breaks the bracket line: -- PROP --
    textCenter(tft, st.groupFont, x0, 0, x1 - x0, GROUP_H,
               String(" ") + resGroupLabel(grp) + " ", TFT_LIGHT_GREY, TFT_BLACK);

    if (runEnd + 1 < slotCount) {
      tft.drawLine(x1 - 1, TAPE_TOP, x1 - 1, TAPE_BOTTOM, TFT_DARK_GREY);
    }
    runStart = runEnd + 1;
  }
}


/***************************************************************************************
   METER DYNAMICS -- redrawn by updateScreenMain() as values change
****************************************************************************************/

// Fill: flicker-free two-fillRect technique per column -- black from the top down to
// the level, colour below it. Full repaint each time, so it is robust to the mode-
// toggle path (which resets the caches WITHOUT clearing the screen); a delta-erase
// keyed on the previous value would leave stale fill above a column that shrank
// across the toggle.
//
// mark < 0: one full-width column at `level`.
// mark >= 0: the interior is split -- primary column on the left at `level` in the
// resource colour, secondary column (secW wide) on the right at `mark` in the dimmed
// colour, SPLIT_GAP of black between -- and a white MARK_H line across the whole
// interior at the secondary level, drawn last since it sits on top of both columns.
static void fillColumn(KCM_TFT &tft, uint16_t x, uint16_t w, float level, uint16_t color) {
  uint16_t fillH  = (uint16_t)(TAPE_INNER_H * level);
  uint16_t emptyH = TAPE_INNER_H - fillH;
  if (emptyH > 0) tft.fillRect(x, TAPE_TOP + 1,          w, emptyH, TFT_BLACK);
  if (fillH  > 0) tft.fillRect(x, TAPE_TOP + 1 + emptyH, w, fillH,  color);
}

static void drawFill(KCM_TFT &tft, const MeterStyle &st, const MeterGeom &g,
                     float level, float mark, uint16_t color) {
  uint16_t ix = g.tapeX + 1;
  uint16_t iw = st.tapeW - 2;
  if (mark < 0.0f) {
    fillColumn(tft, ix, iw, level, color);
    return;
  }
  uint16_t priW = iw - st.secW - SPLIT_GAP;
  fillColumn(tft, ix, priW, level, color);
  tft.fillRect(ix + priW, TAPE_TOP + 1, SPLIT_GAP, TAPE_INNER_H, TFT_BLACK);
  fillColumn(tft, ix + priW + SPLIT_GAP, st.secW, mark, dimColor(color));

  int16_t ly = (int16_t)levelY(mark) - (int16_t)MARK_H / 2;
  if (ly < (int16_t)TAPE_TOP + 1) ly = TAPE_TOP + 1;
  if (ly + (int16_t)MARK_H > (int16_t)TAPE_BOTTOM - 1) ly = TAPE_BOTTOM - 1 - MARK_H;
  tft.fillRect(ix, ly, iw, MARK_H, TFT_WHITE);
}

// Percent counter. Alarm is white-on-red across the whole cell, the InfoDisp /
// Annunciator alarm treatment; caution is yellow text; nominal white. A slot with no
// capacity shows "---" in grey.
static void drawPercent(KCM_TFT &tft, const MeterStyle &st, const MeterGeom &g,
                        bool hasData, uint8_t perc, uint8_t state) {
  uint16_t cellW = st.pitch - st.arrowCellW;
  uint16_t back  = (hasData && state == 2) ? TFT_RED : TFT_BLACK;
  uint16_t fore  = !hasData ? TFT_GREY : (state == 2) ? TFT_WHITE : stateColor(state);
  tft.fillRect(g.px, PERC_Y, cellW, PERC_H, back);
  char s[6];
  if (hasData) snprintf(s, sizeof(s), "%d%%", perc);
  else         strlcpy(s, "---", sizeof(s));
  textCenter(tft, st.percFont, g.px, PERC_Y, cellW, PERC_H, String(s), fore, back);
}

// Trend arrow in the cell right of the counter: up = increasing, down = decreasing,
// none = steady. Coloured with the counter's state.
static void drawTrend(KCM_TFT &tft, const MeterStyle &st, const MeterGeom &g,
                      int8_t trend, uint8_t state) {
  uint16_t cx0 = g.px + st.pitch - st.arrowCellW;
  tft.fillRect(cx0, PERC_Y, st.arrowCellW, PERC_H, TFT_BLACK);
  if (trend == 0) return;
  int16_t cx = cx0 + st.arrowCellW / 2;
  int16_t cy = PERC_Y + PERC_H / 2;
  int16_t hw = (int16_t)(st.arrowCellW - 4) / 2;
  int16_t hh = (int16_t)st.arrowHalfH;
  uint16_t col = stateColor(state);
  if (trend > 0) tft.fillTriangle(cx, cy - hh, cx - hw, cy + hh, cx + hw, cy + hh, col);
  else           tft.fillTriangle(cx, cy + hh, cx - hw, cy - hh, cx + hw, cy - hh, col);
}

static void drawUnits(KCM_TFT &tft, const MeterStyle &st, const MeterGeom &g, const char *s) {
  tft.fillRect(g.px, UNITS_Y, st.pitch, UNITS_H, TFT_BLACK);
  if (s[0] == '\0') return;
  textCenter(tft, st.unitsFont, g.px, UNITS_Y, st.pitch, UNITS_H, String(s), TFT_LIGHT_GREY, TFT_BLACK);
}


/***************************************************************************************
   PER-METER UPDATE CACHE
   One entry per slot. Reset by drawStaticMain() (full repaint) and by the mode toggle
   in updateScreenMain() (repaint of the dynamics only).
****************************************************************************************/
static MeterCache _mc[MAX_SLOTS];
static bool       _prevStageMode = false;

static void resetMeterCaches(uint32_t now) {
  for (uint8_t i = 0; i < MAX_SLOTS; i++) {
    _mc[i].level      = -1.0f;
    _mc[i].mark       = -2.0f;
    _mc[i].perc       = 255;
    _mc[i].state      = 255;
    _mc[i].hasData    = false;
    _mc[i].trend      = 127;
    _mc[i].units[0]   = 1;       // sentinel: matches no real string, forces a redraw
    _mc[i].units[1]   = '\0';
    _mc[i].trendRef   = -1.0f;   // < 0 = window not started
    _mc[i].trendRefMs = now;
    _mc[i].trendNow   = 0;
  }
  _prevStageMode = stageMode;
}

// Trend: compare the primary value against the value at the start of a
// TREND_WINDOW_MS window; a move of more than TREND_MIN_FRAC of capacity across the
// window sets the direction. Windows are per slot and restart on every sample, so
// the arrow lags a real change by at most one window and does not chatter on the
// small per-message fluctuations Simpit delivers.
static void updateTrend(MeterCache &c, bool hasData, float cur, float max, uint32_t now) {
  if (!hasData) { c.trendNow = 0; c.trendRef = -1.0f; return; }
  if (c.trendRef < 0.0f) { c.trendRef = cur; c.trendRefMs = now; return; }
  if (now - c.trendRefMs < TREND_WINDOW_MS) return;
  float frac = (cur - c.trendRef) / max;
  c.trendNow   = (frac > TREND_MIN_FRAC) ? 1 : (frac < -TREND_MIN_FRAC) ? -1 : 0;
  c.trendRef   = cur;
  c.trendRefMs = now;
}


/***************************************************************************************
   DRAW STATIC CHROME -- main screen
****************************************************************************************/
void drawStaticMain(KCM_TFT &tft) {
  // Subsystem order is a property of the display, so it is applied here, at the one
  // point every slot-set change passes through on its way to the screen. Detail and
  // the Select order list read slots[] afterwards and so agree with the meters.
  sortSlotsByGroup();

  tft.fillScreen(TFT_BLACK);
  drawSidebar(tft);
  drawAxis(tft);
  resetMeterCaches(millis());

  const MeterStyle &st = meterStyle();
  drawGroupBands(tft, st);

  for (uint8_t i = 0; i < slotCount; i++) {
    if (slots[i].type == RES_NONE) continue;
    MeterGeom g = meterGeom(st, i);
    drawBands(tft, g, slots[i].type);
    drawTicks(tft, st, g);
    drawFrame(tft, st, g, 0);
    textCenter(tft, st.labelFont, g.px, LABEL_Y, st.pitch, LABEL_H,
               resLabel(slots[i].type), TFT_WHITE, TFT_BLACK);
  }
}


/***************************************************************************************
   UPDATE PASS
****************************************************************************************/
void updateScreenMain(KCM_TFT &tft) {
  uint32_t now = millis();

  if (stageMode != _prevStageMode) {
    // Primary and secondary swap: every dynamic element repaints, chrome stays.
    resetMeterCaches(now);
    redrawStageModeButton(tft);
  }

  const MeterStyle &st = meterStyle();

  for (uint8_t i = 0; i < slotCount; i++) {
    if (slots[i].type == RES_NONE) continue;
    ResourceSlot &s = slots[i];
    MeterCache   &c = _mc[i];

    float cur    = stageMode ? s.stageCurrent : s.current;
    float max    = stageMode ? s.stageMax     : s.maxVal;
    float secCur = stageMode ? s.current      : s.stageCurrent;
    float secMax = stageMode ? s.maxVal       : s.stageMax;

    bool  hasData = (max > 0.0f);
    float level   = hasData ? constrain(cur / max, 0.0f, 1.0f) : 0.0f;
    float mark    = -1.0f;   // no secondary column
    if (hasData && resHasStageData(s.type) && secMax > 0.0f)
      mark = constrain(secCur / secMax, 0.0f, 1.0f);

    uint8_t perc  = (uint8_t)(level * 100.0f);
    uint8_t state = hasData ? meterState(s.type, level) : 0;

    updateTrend(c, hasData, cur, max, now);

    char units[12];
    if (hasData) fmtUnits(cur, units, sizeof(units));
    else         units[0] = '\0';

    // Fill redraws on any meaningful level change; the counters only when their
    // text or colour actually changes. Decoupling them is what stops the number
    // being cleared-and-reprinted every frame while the tape still moves smoothly.
    bool levelChanged = fabsf(level - c.level) >= BAR_LEVEL_HYSTERESIS;
    bool markChanged  = (c.mark < -1.5f) ||                       // forced
                        ((mark < 0.0f) != (c.mark < 0.0f)) ||     // marker appeared / vanished
                        (mark >= 0.0f && fabsf(mark - c.mark) >= BAR_LEVEL_HYSTERESIS);
    bool stateChanged = (state != c.state) || (hasData != c.hasData);
    bool percChanged  = (perc != c.perc) || stateChanged;
    bool trendChanged = (c.trendNow != c.trend) || stateChanged;
    bool unitsChanged = (strcmp(units, c.units) != 0);

    if (!levelChanged && !markChanged && !percChanged && !trendChanged && !unitsChanged) continue;

    MeterGeom g = meterGeom(st, i);

    if (stateChanged) {
      c.state   = state;
      c.hasData = hasData;
      drawFrame(tft, st, g, hasData ? state : 0);
    }
    if (levelChanged || markChanged) {
      c.level = level;
      c.mark  = mark;
      drawFill(tft, st, g, level, mark, resColor(s.type));
    }
    if (percChanged) {
      c.perc = perc;
      drawPercent(tft, st, g, hasData, perc, state);
    }
    if (trendChanged) {
      c.trend = c.trendNow;
      drawTrend(tft, st, g, c.trendNow, state);
    }
    if (unitsChanged) {
      strlcpy(c.units, units, sizeof(c.units));
      drawUnits(tft, st, g, units);
    }
  }
}


/***************************************************************************************
   SIDEBAR HIT TEST
****************************************************************************************/
int8_t sidebarHitTest(uint16_t x, uint16_t y) {
  if (x < SIDEBAR_X || x >= SIDEBAR_X + SIDEBAR_W) return -1;
  for (uint8_t btn = 0; btn < SB_BTN_COUNT; btn++) {
    uint16_t by = sbBtnY(btn);
    if (y >= by && y < by + sbBtnH()) return (int8_t)btn;
  }
  return -1;
}

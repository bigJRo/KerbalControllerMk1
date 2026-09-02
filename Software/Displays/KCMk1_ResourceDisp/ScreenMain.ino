/***************************************************************************************
   ScreenMain.ino -- Main tape-meter screen for Kerbal Controller Mk1 Resource Display

   Layout (1024x600):
     Left  : SIDEBAR_W px button column, 4 keys edge-to-edge, full height
     Top   : ALERT_H px alert strip across the content -- an EICAS-style message line
             listing every meter in caution or alarm, worst first ("SF LOW", "CO2
             HIGH", "MP CAUT", "LF BUG"), "REFRESHING" while a channel refresh is pending, and
             at its right end the PROPELLANT BALANCE indicator (below)
     Then  : AXIS_W px shared 0-100% axis
     Then  : one tape meter per active slot at a FIXED pitch, centred as a row, in
             subsystem order, with a bracketed group label above each run of
             same-group meters and a divider between groups

   Propellant balance (Apollo's OXID UNBAL meter): KSP engines burn LF and LOx at
   9:11, so when the two tanks are not in that ratio one runs dry first and the rest
   is dead weight. When both are on the panel, a centre-zero bar shows which side is
   in surplus and a counter says by how many units. Stage quantities when both have
   them, else vessel totals.

   Sidebar buttons (top to bottom):
     0. DFLT  -- resets slots to the STD default group
     1. SEL   -- navigates to the resource selection screen
     2. DATA  -- navigates to the numerical resource detail screen
     3. TTE   -- toggles the counter row between percent and time-to-empty

   Each meter, top to bottom:
     - group label band (shared by a run of same-group meters)
     - the tape: a narrow thermometer column filled in the resource colour to the
       vessel TOTAL, with
         . a limit-band column on its left. The resource's alarm and caution
           fractions are painted red and yellow on the scale itself, so the limits
           are visible whether or not the level is in them. This is the Shuttle
           F7/O3 meter convention, and it is what the filled bars lacked: their only
           warning was the percentage label changing colour.
         . for resources with a separate stage channel, the tape is split into two
           columns, the way the Shuttle PRPLT QTY meter carried its three pointers
           side by side: the wide left column is the vessel total in the resource
           colour, the narrow right column is the ACTIVE STAGE in a half-brightness
           shade of the same colour, and a white line across the whole tape marks
           the stage level so it reads against the ticks even where the narrow
           column is only a few pixels wide. Resources without a stage channel get
           one full-width column, which is itself the cue that there is no stage
           figure. Both values are always visible; there is no mode to toggle.
         . tick marks on the right edge: major every 10%, minor every 5%
       The frame is grey when nominal and takes the caution/alarm colour on breach.
     - resource short label
     - counter row: the percent of capacity, or with the TTE key engaged the time
       to empty at the current rate (time to full for waste-type resources, "---"
       while steady or filling), with a trend arrow two spaces to its right while
       the value is moving. Counter, gap and arrow slot are centred in the pitch
       cell as one group, the arrow slot reserved whether or not an arrow shows,
       so the number never shifts when the trend changes. Coloured by state
       (white / yellow / white-on-red, the alarm tile hugging the group)
     - units counter (raw resource units, compacted to fit the pitch)
     A slot whose capacity is zero shows an empty tape and, in grey, "..." while a
     channel refresh is still pending for it or "---" once it is not going to answer
     (resource not aboard) -- never a 0% alarm.

   Touch: a tap on a meter's label/counter rows opens the Detail screen on that
   resource. A tap on its tape rows sets a RESERVE BUG at that level -- a cyan index
   in the tick zone with a cyan mark on the band, cyan being this project's colour
   for pilot-entered values -- and the meter goes to caution when the level crosses
   it. A tap close to an existing bug clears it. Bugs travel with the vessel's slot
   memory.

   Spacing: the meters always spread across the full meter area, so the pitch is
   the area divided by the slot count. What stays fixed is the meter itself: tape
   width, stage column and fonts come in two classes, standard for up to
   DEFAULT_SLOT_COUNT meters and compact above that, so a meter reads as the same
   instrument whether it has the screen to itself or shares it with fifteen others.
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

// Pitch: the meters spread across the whole area, so this follows the slot count.
// slotCount only changes through a chrome redraw, so every caller in one frame sees
// the same value. The remainder of the division is split either side of the row.
static inline uint16_t meterPitch() {
  return (slotCount > 0) ? (uint16_t)(METER_AREA_W / slotCount) : METER_AREA_W;
}

// Vertical bands, top to bottom.
static const uint16_t ALERT_H      = 24;   // alert strip (Black_16 cap 19)
static const uint16_t GROUP_Y      = ALERT_H;                   // 24
static const uint16_t GROUP_H      = 26;   // subsystem label band
static const uint16_t TAPE_TOP     = GROUP_Y + GROUP_H + 6;     // 56
static const uint16_t LABEL_H      = 26;   // resource short label (Black_20 cap 24)
static const uint16_t PERC_H       = 32;   // percent counter + trend arrow (Black_24 cap 29)
static const uint16_t UNITS_H      = 22;   // units counter (Black_16 cap 19)
static const uint16_t FOOT_H       = LABEL_H + PERC_H + UNITS_H;  // 80
static const uint16_t TAPE_BOTTOM  = SCREEN_H - FOOT_H;          // 520
static const uint16_t TAPE_H       = TAPE_BOTTOM - TAPE_TOP;     // 464
static const uint16_t TAPE_INNER_H = TAPE_H - 2;                 // 462 -- fill area inside the frame
static const uint16_t LABEL_Y      = TAPE_BOTTOM;                // 520
static const uint16_t PERC_Y       = LABEL_Y + LABEL_H;          // 546
static const uint16_t UNITS_Y      = PERC_Y + PERC_H;            // 578

// Alert strip: messages from the content's left edge; the balance indicator, when
// shown, takes the rightmost BAL_W px.
static const tFont   *STRIP_FONT   = &Roboto_Black_16;
static const uint16_t STRIP_X      = CONTENT_X + 4;
static const uint16_t BAL_W        = 270;  // balance indicator cell at the strip's right end
static const uint16_t BAL_BAR_W    = 100;  // centre-zero bar
static const uint16_t BAL_BAR_H    = 10;
static const float    LF_PER_LOX   = 9.0f / 11.0f;   // KSP LF:LOx burn ratio

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
  40, 12, 6, 10, 6,
  &Roboto_Black_20, &Roboto_Black_24, &Roboto_Black_16, &Roboto_Black_16
};
static const MeterStyle STYLE_CMP = {
  22, 7, 5, 7, 5,
  &Roboto_Black_16, &Roboto_Black_16, &Roboto_Black_12, &Roboto_Black_12
};

static inline const MeterStyle &meterStyle() {
  return (slotCount <= DEFAULT_SLOT_COUNT) ? STYLE_STD : STYLE_CMP;
}

// Left edge of meter i's pitch cell. The few pixels of division remainder are split
// either side of the row so it stays centred in the area.
static inline uint16_t pitchX(const MeterStyle &st, uint8_t i) {
  (void)st;
  uint16_t pitch = meterPitch();
  uint16_t rowW  = slotCount * pitch;
  return METER_X0 + (METER_AREA_W - rowW) / 2 + i * pitch;
}


static MeterGeom meterGeom(const MeterStyle &st, uint8_t i) {
  MeterGeom g;
  uint16_t scaleW = BAND_W + 1 + st.tapeW + 1 + st.tickL;
  g.px    = pitchX(st, i);
  g.bandX = g.px + (meterPitch() - scaleW) / 2;
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

   TTE is the one key with state: percent is the default and the key draws like any
   other, time-to-empty reverse-videos, so the key reads as "changed from the default"
   as well as naming the mode. The border stays grey on both, since neither is a
   selection.
****************************************************************************************/
static const ButtonLabel btnTteOff = {
  "TTE",
  TFT_WHITE, TFT_WHITE,
  TFT_BLACK, TFT_BLACK,
  TFT_GREY, TFT_GREY
};
static const ButtonLabel btnTteOn = {
  "TTE",
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

  // Buttons 0-2: action buttons, drawn "on" so their background colour always shows
  drawButton(tft, bx, sbBtnY(0), bw, sbBtnH(), btnReset,  SB_BTN_FONT, true);
  drawButton(tft, bx, sbBtnY(1), bw, sbBtnH(), btnSelect, SB_BTN_FONT, true);
  drawButton(tft, bx, sbBtnY(2), bw, sbBtnH(), btnDetail, SB_BTN_FONT, true);

  // Button 3: TTE counter-mode toggle
  const ButtonLabel &tteBtn = tteMode ? btnTteOn : btnTteOff;
  drawButton(tft, bx, sbBtnY(3), bw, sbBtnH(), tteBtn, SB_BTN_FONT, true);
}


/***************************************************************************************
   REDRAW TTE BUTTON ONLY
   Called when tteMode toggles to avoid a full screen redraw.
****************************************************************************************/
void redrawTteButton(KCM_TFT &tft) {
  const ButtonLabel &tteBtn = tteMode ? btnTteOn : btnTteOff;
  drawButton(tft, sbX(), sbBtnY(3), SIDEBAR_W - 1, sbBtnH(), tteBtn, SB_BTN_FONT, true);
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
   ALERT COLOURS
   The state itself comes from alertState() in Resources.ino (with hysteresis).
****************************************************************************************/
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

// Reserve bug: a cyan index in the tick zone, tip against the frame, plus a cyan mark
// across the band column so it reads on the scale. Redrawing clears the whole tick
// zone and band and repaints them, since the old bug sat on both; both are cheap.
static void drawBug(KCM_TFT &tft, const MeterStyle &st, const MeterGeom &g,
                    ResourceType t, float bug) {
  tft.fillRect(g.tickX, TAPE_TOP, st.tickL + 1, TAPE_H, TFT_BLACK);
  drawTicks(tft, st, g);
  drawBands(tft, g, t);
  if (bug < 0.0f) return;
  int16_t y  = (int16_t)levelY(bug);
  int16_t hh = (int16_t)st.tickL;
  tft.fillTriangle(g.tickX, y, g.tickX + st.tickL, y - hh, g.tickX + st.tickL, y + hh, TFT_CYAN);
  tft.fillRect(g.bandX, y - 1, BAND_W, 2, TFT_CYAN);
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
    uint16_t x1 = pitchX(st, runEnd) + meterPitch();   // exclusive
    uint16_t ly = GROUP_Y + GROUP_H - 3;           // bracket line row
    tft.drawLine(x0 + 3, ly, x1 - 4, ly, TFT_GREY);
    tft.drawLine(x0 + 3, ly, x0 + 3, ly + 3, TFT_GREY);
    tft.drawLine(x1 - 4, ly, x1 - 4, ly + 3, TFT_GREY);
    // Label drawn last, black-backed, so it breaks the bracket line: -- PROP --
    textCenter(tft, st.groupFont, x0, GROUP_Y, x1 - x0, GROUP_H,
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
// mark < 0: one full-width column at `level` (the vessel total).
// mark >= 0: the interior is split -- total column on the left at `level` in the
// resource colour, stage column (secW wide) on the right at `mark` in the dimmed
// colour, SPLIT_GAP of black between -- and a white MARK_H line across the whole
// interior at the stage level, drawn last since it sits on top of both columns.
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

// Counter row: the percent (or the time-to-empty string in TTE mode), a two-space
// gap, and the trend arrow slot, laid out as one group centred in the pitch cell.
// The arrow slot is reserved even when no arrow shows, so the number stays put as
// the trend comes and goes. Alarm is white on a red tile that hugs the group, the
// InfoDisp / Annunciator alarm treatment; caution is yellow text; nominal white. A
// slot with no capacity shows its "..." or "---" in grey. Up = increasing, down =
// decreasing, no arrow = steady.
static const uint16_t COUNTER_PAD = 4;   // red tile margin beyond the group

static void drawCounterRow(KCM_TFT &tft, const MeterStyle &st, const MeterGeom &g,
                           bool hasData, const char *s, int8_t trend, uint8_t state) {
  uint16_t pitch  = meterPitch();
  bool     alarm  = hasData && state == 2;
  uint16_t back   = alarm ? TFT_RED : TFT_BLACK;
  uint16_t fore   = !hasData ? TFT_GREY : alarm ? TFT_WHITE : stateColor(state);
  int16_t  textW  = getFontStringWidth(st.percFont, s);
  int16_t  gapW   = getFontStringWidth(st.percFont, "  ");
  int16_t  groupW = textW + gapW + st.arrowW;
  int16_t  gx     = g.px + ((int16_t)pitch - groupW) / 2;   // group left edge

  tft.fillRect(g.px, PERC_Y, pitch, PERC_H, TFT_BLACK);
  if (alarm) tft.fillRect(gx - COUNTER_PAD, PERC_Y + 1, groupW + COUNTER_PAD * 2, PERC_H - 2, back);
  textCenter(tft, st.percFont, gx, PERC_Y, textW, PERC_H, String(s), fore, back);

  if (trend == 0) return;
  int16_t cx = gx + textW + gapW + st.arrowW / 2;
  int16_t cy = PERC_Y + PERC_H / 2;
  int16_t hw = (int16_t)st.arrowW / 2;
  int16_t hh = (int16_t)st.arrowHalfH;
  if (trend > 0) tft.fillTriangle(cx, cy - hh, cx - hw, cy + hh, cx + hw, cy + hh, fore);
  else           tft.fillTriangle(cx, cy + hh, cx - hw, cy - hh, cx + hw, cy - hh, fore);
}

static void drawUnits(KCM_TFT &tft, const MeterStyle &st, const MeterGeom &g, const char *s) {
  uint16_t pitch = meterPitch();
  tft.fillRect(g.px, UNITS_Y, pitch, UNITS_H, TFT_BLACK);
  if (s[0] == '\0') return;
  textCenter(tft, st.unitsFont, g.px, UNITS_Y, pitch, UNITS_H, String(s), TFT_LIGHT_GREY, TFT_BLACK);
}


/***************************************************************************************
   ALERT STRIP
   One line, worst first: alarms white-on-red as "<LBL> LOW" (or HIGH for a waste
   product), cautions in yellow as "<LBL> CAUT", a reserve-bug crossing as "<LBL> BUG", and
   "REFRESHING" in white while a channel refresh is pending.
   Messages that do not fit collapse into "+N" in grey. Redrawn only when the
   composed line changes, which the signature buffer detects.
****************************************************************************************/
enum StripCode : uint8_t { STRIP_NONE = 0, STRIP_CAUTION, STRIP_ALARM, STRIP_BUG };
static uint8_t _stripCode[MAX_SLOTS];
static char    _stripSig[160];   // last drawn line, "\x01" = force
static bool    _balShown = false;

static uint16_t stripAvailW() {
  return (uint16_t)(SCREEN_W - STRIP_X - (_balShown ? BAL_W : 0));
}

// Append one message to the line; returns false when it would not fit. A message
// with a non-black background gets a filled tile behind it, STRIP_PAD wider than the
// text each side -- the alarm treatment is white on red, as on the counter cells.
static const uint16_t STRIP_PAD = 4;

static bool stripPut(KCM_TFT &tft, uint16_t &x, uint16_t xEnd, const char *text,
                     uint16_t fore, uint16_t back) {
  int16_t w = getFontStringWidth(STRIP_FONT, text);
  if (x + w + STRIP_PAD > xEnd) return false;
  if (back != TFT_BLACK) tft.fillRect(x - STRIP_PAD, 1, w + STRIP_PAD * 2, ALERT_H - 2, back);
  tft.setFont(*STRIP_FONT);
  tft.setTextColor(fore, back);
  tft.setCursor(x, (ALERT_H - STRIP_FONT->cap_height) / 2);
  tft.print(text);
  x += w + STRIP_PAD + getFontStringWidth(STRIP_FONT, "  ");
  return true;
}

static void updateAlertStrip(KCM_TFT &tft) {
  // Compose the signature: one token per message in display order.
  char sig[sizeof(_stripSig)];
  size_t n = 0;
  if (refreshPending) n += snprintf(sig + n, sizeof(sig) - n, "R|");
  for (uint8_t sev = STRIP_ALARM; sev >= STRIP_CAUTION; sev--) {
    for (uint8_t i = 0; i < slotCount; i++) {
      uint8_t code = _stripCode[i];
      bool pick = (sev == STRIP_ALARM) ? (code == STRIP_ALARM)
                                       : (code == STRIP_CAUTION || code == STRIP_BUG);
      if (!pick || n >= sizeof(sig) - 8) continue;
      n += snprintf(sig + n, sizeof(sig) - n, "%d%d|", code, i);
    }
  }
  if (strcmp(sig, _stripSig) == 0) return;
  strlcpy(_stripSig, sig, sizeof(_stripSig));

  uint16_t xEnd = STRIP_X + stripAvailW();
  tft.fillRect(CONTENT_X, 0, xEnd - CONTENT_X, ALERT_H, TFT_BLACK);
  uint16_t x = STRIP_X + STRIP_PAD;
  uint8_t  dropped = 0;
  if (refreshPending) stripPut(tft, x, xEnd, "REFRESHING", TFT_WHITE, TFT_BLACK);
  for (uint8_t sev = STRIP_ALARM; sev >= STRIP_CAUTION; sev--) {
    for (uint8_t i = 0; i < slotCount; i++) {
      uint8_t code = _stripCode[i];
      bool pick = (sev == STRIP_ALARM) ? (code == STRIP_ALARM)
                                       : (code == STRIP_CAUTION || code == STRIP_BUG);
      if (!pick) continue;
      char msg[16];
      // Red names the condition (LOW, or HIGH for a waste product); yellow names the
      // tier (CAUT), matching the panel's caution/alarm vocabulary; a bug crossing
      // says so.
      const char *what = (code == STRIP_BUG)     ? "BUG"
                       : (code == STRIP_CAUTION) ? "CAUT"
                       : (resLimits(slots[i].type).highIsBad ? "HIGH" : "LOW");
      snprintf(msg, sizeof(msg), "%s %s", resLabel(slots[i].type), what);
      bool alarm = (code == STRIP_ALARM);
      if (dropped || !stripPut(tft, x, xEnd, msg, alarm ? TFT_WHITE : TFT_YELLOW,
                               alarm ? TFT_RED : TFT_BLACK)) dropped++;
    }
  }
  if (dropped) {
    char more[8];
    snprintf(more, sizeof(more), "+%d", dropped);
    int16_t w = getFontStringWidth(STRIP_FONT, more);
    tft.setFont(*STRIP_FONT);
    tft.setTextColor(TFT_GREY, TFT_BLACK);
    tft.setCursor(xEnd - w, (ALERT_H - STRIP_FONT->cap_height) / 2);
    tft.print(more);
  }
}


/***************************************************************************************
   PROPELLANT BALANCE
   Surplus of one propellant over what the other can burn with it, at 9:11. Positive
   = LOx in surplus (pointer right), negative = LF in surplus (pointer left). The bar
   is scaled so a surplus equal to half the propellant aboard is full deflection.
****************************************************************************************/
static int16_t _balPx = -1;        // last drawn pointer offset, px from centre; -32768 = force
static char    _balText[24];       // last drawn counter

static int8_t findSlot(ResourceType t) {
  for (uint8_t i = 0; i < slotCount; i++) if (slots[i].type == t) return (int8_t)i;
  return -1;
}

// Returns true when the indicator applies, filling surplus (units, signed) and the
// pointer fraction (-1..1).
static bool balanceCompute(float &surplus, float &frac) {
  int8_t lf = findSlot(RES_LIQUID_FUEL), lox = findSlot(RES_LIQUID_OX);
  if (lf < 0 || lox < 0) return false;
  const ResourceSlot &a = slots[lf], &b = slots[lox];
  if (a.maxVal <= 0.0f || b.maxVal <= 0.0f) return false;
  bool useStage = (a.stageMax > 0.0f && b.stageMax > 0.0f);
  float fuel = useStage ? a.stageCurrent : a.current;
  float ox   = useStage ? b.stageCurrent : b.current;
  float total = fuel + ox;
  if (total <= 0.0f) return false;
  float loxNeeded = fuel / LF_PER_LOX;          // LOx the LF on hand can burn
  if (ox >= loxNeeded) surplus = ox - loxNeeded;          // LOx left over
  else                 surplus = -(fuel - ox * LF_PER_LOX); // LF left over, negative
  frac = constrain(surplus / (0.5f * total), -1.0f, 1.0f);
  return true;
}

static void drawBalanceChrome(KCM_TFT &tft) {
  uint16_t x0 = SCREEN_W - BAL_W;
  tft.fillRect(x0, 0, BAL_W, ALERT_H, TFT_BLACK);
  tft.setFont(Roboto_Black_12);
  tft.setTextColor(TFT_GREY, TFT_BLACK);
  tft.setCursor(x0 + 4, (ALERT_H - 14) / 2);
  tft.print("BAL");
  uint16_t barX = x0 + 60, barY = (ALERT_H - BAL_BAR_H) / 2;
  tft.setCursor(barX - 18, (ALERT_H - 14) / 2);
  tft.print("LF");
  tft.setCursor(barX + BAL_BAR_W + 4, (ALERT_H - 14) / 2);
  tft.print("LOx");
  tft.drawRect(barX, barY, BAL_BAR_W, BAL_BAR_H, TFT_GREY);
  tft.drawLine(barX + BAL_BAR_W / 2, barY - 2, barX + BAL_BAR_W / 2, barY + BAL_BAR_H + 1, TFT_LIGHT_GREY);
}

static void updateBalance(KCM_TFT &tft) {
  float surplus, frac;
  bool show = balanceCompute(surplus, frac);
  if (show != _balShown) {
    _balShown = show;
    _stripSig[0] = 1; _stripSig[1] = '\0';   // message width changed: relay the strip
    if (show) drawBalanceChrome(tft);
    else      tft.fillRect(SCREEN_W - BAL_W, 0, BAL_W, ALERT_H, TFT_BLACK);
    _balPx = -32768;
    _balText[0] = 1; _balText[1] = '\0';
  }
  if (!show) return;

  uint16_t x0 = SCREEN_W - BAL_W;
  uint16_t barX = x0 + 60, barY = (ALERT_H - BAL_BAR_H) / 2;
  int16_t px = (int16_t)(frac * (BAL_BAR_W / 2 - 3));
  if (px != _balPx) {
    if (_balPx != -32768) {
      tft.fillRect(barX + BAL_BAR_W / 2 + _balPx - 1, barY + 1, 3, BAL_BAR_H - 2, TFT_BLACK);
      tft.drawLine(barX + BAL_BAR_W / 2, barY + 1, barX + BAL_BAR_W / 2, barY + BAL_BAR_H - 2, TFT_LIGHT_GREY);
    }
    _balPx = px;
    tft.fillRect(barX + BAL_BAR_W / 2 + px - 1, barY + 1, 3, BAL_BAR_H - 2, TFT_WHITE);
  }

  char text[24], num[12];
  fmtUnits(fabsf(surplus), num, sizeof(num));
  snprintf(text, sizeof(text), "%s +%s", (surplus >= 0.0f) ? "LOx" : "LF", num);
  if (strcmp(text, _balText) != 0) {
    strlcpy(_balText, text, sizeof(_balText));
    uint16_t tx = barX + BAL_BAR_W + 34;
    tft.fillRect(tx, 0, SCREEN_W - tx, ALERT_H, TFT_BLACK);
    tft.setFont(Roboto_Black_12);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(tx, (ALERT_H - 14) / 2);
    tft.print(text);
  }
}


/***************************************************************************************
   PER-METER DRAWN-STATE CACHE
   One entry per slot. Reset by drawStaticMain() (full repaint) and by the TTE toggle
   in updateScreenMain() (repaint of the dynamics only). Rates and trends come from
   Sampling.ino and are not touched by either.
****************************************************************************************/
static MeterCache _mc[MAX_SLOTS];
static bool       _prevTteMode = false;

static void resetDrawCaches() {
  for (uint8_t i = 0; i < MAX_SLOTS; i++) {
    _mc[i].level      = -1.0f;
    _mc[i].mark       = -2.0f;
    _mc[i].state      = 255;
    _mc[i].hasData    = false;
    _mc[i].awaiting   = false;
    _mc[i].bug        = -2.0f;
    _mc[i].trend      = 127;
    _mc[i].counter[0] = 1;       // sentinel: matches no real string, forces a redraw
    _mc[i].counter[1] = '\0';
    _mc[i].units[0]   = 1;
    _mc[i].units[1]   = '\0';
  }
  _prevTteMode = tteMode;
  for (uint8_t i = 0; i < MAX_SLOTS; i++) _stripCode[i] = STRIP_NONE;
  _stripSig[0] = 1; _stripSig[1] = '\0';
  _balShown = false;
  _balPx = -32768;
  _balText[0] = 1; _balText[1] = '\0';
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
  resetDrawCaches();
  resetAllSampling();   // slots may have been reordered by the sort above

  const MeterStyle &st = meterStyle();
  drawGroupBands(tft, st);

  for (uint8_t i = 0; i < slotCount; i++) {
    if (slots[i].type == RES_NONE) continue;
    MeterGeom g = meterGeom(st, i);
    drawBands(tft, g, slots[i].type);
    drawTicks(tft, st, g);
    drawFrame(tft, st, g, 0);
    textCenter(tft, st.labelFont, g.px, LABEL_Y, meterPitch(), LABEL_H,
               resLabel(slots[i].type), TFT_WHITE, TFT_BLACK);
  }
}


/***************************************************************************************
   UPDATE PASS
****************************************************************************************/
void updateScreenMain(KCM_TFT &tft) {
  if (tteMode != _prevTteMode) {
    // Counter row changes meaning: every dynamic element repaints, chrome and the
    // rate estimate stay.
    resetDrawCaches();
    redrawTteButton(tft);
  }

  const MeterStyle &st = meterStyle();

  for (uint8_t i = 0; i < slotCount; i++) {
    if (slots[i].type == RES_NONE) continue;
    ResourceSlot &s = slots[i];
    MeterCache   &c = _mc[i];

    float cur = s.current;
    float max = s.maxVal;

    bool  hasData = (max > 0.0f);
    float level   = hasData ? constrain(cur / max, 0.0f, 1.0f) : 0.0f;
    float mark    = -1.0f;   // no stage column
    if (hasData && resHasStageData(s.type) && s.stageMax > 0.0f)
      mark = constrain(s.stageCurrent / s.stageMax, 0.0f, 1.0f);

    bool    awaiting = !hasData && slotAwaiting(i);
    uint8_t state    = hasData ? alertState(s.type, level, s.bug, c.state) : 0;
    const SlotSample &smp = sampleTot[i];

    // Message for the alert strip. A caution the fixed limits would not have raised
    // on their own is the reserve bug's.
    if      (!hasData || state == 0) _stripCode[i] = STRIP_NONE;
    else if (state == 2)             _stripCode[i] = STRIP_ALARM;
    else if (alertState(s.type, level, -1.0f, c.state) >= 1) _stripCode[i] = STRIP_CAUTION;
    else                             _stripCode[i] = STRIP_BUG;

    char counter[8];
    if (!hasData)     strlcpy(counter, awaiting ? "..." : "---", sizeof(counter));
    else if (tteMode) fmtTte(sampleTteSeconds(smp, s.type, cur, max), counter, sizeof(counter));
    else              snprintf(counter, sizeof(counter), "%d%%", (int)(level * 100.0f));

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
    bool stateChanged   = (state != c.state) || (hasData != c.hasData) || (awaiting != c.awaiting);
    bool counterChanged = (strcmp(counter, c.counter) != 0) || stateChanged;
    bool trendChanged   = (smp.trend != c.trend) || stateChanged;
    bool unitsChanged   = (strcmp(units, c.units) != 0);
    bool bugChanged     = (c.bug < -1.5f) || ((s.bug < 0.0f) != (c.bug < 0.0f)) ||
                          (s.bug >= 0.0f && fabsf(s.bug - c.bug) > 0.0001f);

    if (!levelChanged && !markChanged && !counterChanged && !trendChanged && !unitsChanged && !bugChanged) continue;

    MeterGeom g = meterGeom(st, i);

    if (bugChanged) {
      c.bug = s.bug;
      drawBug(tft, st, g, s.type, s.bug);
    }
    if (stateChanged) {
      c.state    = state;
      c.hasData  = hasData;
      c.awaiting = awaiting;
      drawFrame(tft, st, g, hasData ? state : 0);
    }
    if (levelChanged || markChanged) {
      c.level = level;
      c.mark  = mark;
      drawFill(tft, st, g, level, mark, resColor(s.type));
    }
    if (counterChanged || trendChanged) {
      strlcpy(c.counter, counter, sizeof(c.counter));
      c.trend = smp.trend;
      drawCounterRow(tft, st, g, hasData, counter, smp.trend, state);
    }
    if (unitsChanged) {
      strlcpy(c.units, units, sizeof(c.units));
      drawUnits(tft, st, g, units);
    }
  }

  updateBalance(tft);
  updateAlertStrip(tft);
}


/***************************************************************************************
   METER HIT TEST
   Which meter, and which part of it, a touch landed on. The tape rows give the
   scale position of the touch so the caller can place a reserve bug; the foot rows
   (label, counter, units) open the Detail screen on that resource.
****************************************************************************************/
int8_t meterHitTest(uint16_t x, uint16_t y, bool &onTape, float &level) {
  const MeterStyle &st = meterStyle();
  if (slotCount == 0) return -1;
  uint16_t x0 = pitchX(st, 0);
  uint16_t pitch = meterPitch();
  uint16_t x1 = pitchX(st, slotCount - 1) + pitch;
  if (x < x0 || x >= x1) return -1;
  int8_t i = (int8_t)((x - x0) / pitch);
  if (i >= (int8_t)slotCount || slots[i].type == RES_NONE) return -1;
  if (y >= TAPE_TOP && y < TAPE_BOTTOM) {
    onTape = true;
    level  = (float)((TAPE_BOTTOM - 1) - y) / (float)TAPE_INNER_H;
    level  = constrain(level, 0.0f, 1.0f);
    return i;
  }
  if (y >= LABEL_Y && y < SCREEN_H) {
    onTape = false;
    level  = 0.0f;
    return i;
  }
  return -1;
}


/***************************************************************************************
   TOGGLE RESERVE BUG
   Sets slot i's bug at the tapped level, snapped to a whole percent, or clears it if
   the tap is within BUG_CLEAR_TOL of the bug already there. The update pass sees the
   change through its cache and redraws the index.
****************************************************************************************/
void toggleMeterBug(uint8_t i, float level) {
  if (i >= slotCount) return;
  ResourceSlot &s = slots[i];
  if (s.bug >= 0.0f && fabsf(level - s.bug) <= BUG_CLEAR_TOL) {
    s.bug = -1.0f;
    if (debugMode) { Serial.print(F("ResourceDisp: bug cleared on ")); Serial.println(resLabel(s.type)); }
    return;
  }
  s.bug = roundf(level * 100.0f) / 100.0f;
  if (debugMode) {
    Serial.print(F("ResourceDisp: bug set on ")); Serial.print(resLabel(s.type));
    Serial.print(F(" at ")); Serial.println(s.bug);
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

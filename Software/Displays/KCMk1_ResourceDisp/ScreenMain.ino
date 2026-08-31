/***************************************************************************************
   ScreenMain.ino -- Main bar graph screen for Kerbal Controller Mk1 Resource Display

   Layout:
     Left region  : vertical resource bars, dynamically sized based on slotCount
     Right region : SIDEBAR_W px button column, 4 buttons edge-to-edge, full height

   Sidebar buttons (top to bottom):
     0. TOTL / STG  -- toggles stageMode. TOTL=dark green, STG=crimson.
     1. DFLT        -- resets slots to the STD default group
     2. SEL         -- navigates to the resource selection screen
     3. DATA        -- navigates to the numerical resource detail screen

   Button labels are horizontal, centred, using SB_BTN_FONT (Roboto_Black_24).
   Bottom three buttons (DFLT, SEL, DATA) use a navy background.

   Each bar:
     - Always drawn in the resource's designated fixed color, regardless of fill
       level. Low-level warning is conveyed solely by the percentage text color.
     - Two-fillRect technique: black from top to empty boundary, then color below
     - Resource label centred below the bar in white (font scales with bar width)
     - Percentage centred above the bar in white (font scales with bar width)
     - Percentage color (thresholdColor uses strict <): >=30% white,
       10-29% yellow (caution), 0-9% red (critical)
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
// Display 2, whose own sidebar is on its right edge — so putting this one on the left
// brings the two button columns together at the boundary between the two screens,
// where they form a single control cluster near the middle of the panel rather than
// sitting a full screen-width apart with the far one at B1's outboard corner.
//
// Content therefore occupies [CONTENT_X, SCREEN_W): the Y-axis strip first, then the
// bars. Everything downstream derives from CONTENT_X via barX() and drawAxis(), so
// this is the only place the offset is stated.
static const uint16_t SIDEBAR_X    = 0;
static const uint16_t CONTENT_X    = SIDEBAR_W;
static inline uint16_t barRegionW() { return SCREEN_W - SIDEBAR_W; }
static inline uint16_t barAreaW()   { return barRegionW() - AXIS_W; }
static const uint16_t LABEL_H      = 44;
static const uint16_t PERC_H       = 36;
static const uint16_t BAR_TOP      = PERC_H;
static const uint16_t BAR_BOTTOM   = SCREEN_H - LABEL_H;
static const uint16_t BAR_H        = BAR_BOTTOM - BAR_TOP;
static const uint16_t BAR_PAD      = 14;   // gap between bars — wider for clearer separation

// Sidebar: 4 buttons, no padding, edge-to-edge, full height
static const uint8_t  SB_BTN_COUNT = 4;

// Sidebar geometry computed at runtime via inline functions so barRegionW()
// always reflects the current SIDEBAR_W value without duplication. sbX() is the
// sidebar's left edge; the 1 px divider rule sits on its inboard (right) edge,
// against the content.
static inline uint16_t sbX()    { return SIDEBAR_X; }
static inline uint16_t sbDivX() { return SIDEBAR_X + SIDEBAR_W - 1; }
static inline uint16_t sbBtnH() { return SCREEN_H / SB_BTN_COUNT; }
static inline uint16_t sbBtnY(uint8_t btn) { return btn * sbBtnH(); }


/***************************************************************************************
   SIDEBAR BUTTON DEFINITIONS
   The sidebar is chrome, not data, so it is achromatic — white-on-black, grey border —
   and shows an engaged state by reverse video. This matches the InfoDisp sidebar and
   the convention bezel-key flight decks use, and it keeps the panel's colour vocabulary
   (green nominal / yellow caution / red alarm, and the per-resource bar colours) for
   the telemetry the sidebar frames. See the InfoDisp README for the full rationale.

   Previously the mode key filled DARK_GREEN for TOTAL and TFT_CORNELL for STAGE, to
   distinguish two equally normal display modes. Both are bar identity colours on this
   very panel — DARK_GREEN is MonoPropellant and TFT_CORNELL is CO2 (see resColor() in
   Resources.ino) — so the key wore two resources' colours while sitting beside their
   bars. TFT_CORNELL is also a red (#B51C19), a strong one to park permanently next to
   percentage labels that turn red below 10%. The three action keys filled navy, which
   signals nothing.

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
   BAR GEOMETRY HELPERS
   Bars occupy barAreaW() (the bar region minus the left axis strip).
   barX() offsets all bars to the right of the axis.
****************************************************************************************/
static uint16_t barWidth() {
  if (slotCount == 0) return 0;
  uint16_t totalPad = BAR_PAD * (slotCount + 1);
  return (barAreaW() - totalPad) / slotCount;
}

// Pass the already-computed bar width so the per-bar loop doesn't recompute
// barWidth() (an integer divide) once per bar per frame.
static uint16_t barX(uint8_t index, uint16_t bw) {
  return CONTENT_X + AXIS_W + BAR_PAD + index * (bw + BAR_PAD);
}


/***************************************************************************************
   DRAW Y-AXIS
   Delegates to the library's drawLabelledAxis(). See KerbalDisplayCommon.h.
****************************************************************************************/
static void drawAxis(KCM_TFT &tft) {
  drawLabelledAxis(tft,
                   CONTENT_X, AXIS_W,
                   BAR_TOP, BAR_BOTTOM,
                   &Roboto_Black_16,
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
   DRAW STATIC CHROME -- main screen
****************************************************************************************/
static float   _prevLevel[MAX_SLOTS];
static uint8_t _prevPerc[MAX_SLOTS];   // last integer % drawn per bar; 255 = force repaint
static bool    _prevStageMode = false;

void drawStaticMain(KCM_TFT &tft) {
  tft.fillScreen(TFT_BLACK);
  drawSidebar(tft);
  drawAxis(tft);

  // Reset update-pass state so all bars and percentage labels repaint on first pass
  for (uint8_t i = 0; i < MAX_SLOTS; i++) { _prevLevel[i] = -1.0f; _prevPerc[i] = 255; }
  _prevStageMode = stageMode;

  const tFont *font = barFont();
  uint16_t bw = barWidth();
  for (uint8_t i = 0; i < slotCount; i++) {
    uint16_t x = barX(i, bw);
    tft.drawRect(x, BAR_TOP, bw, BAR_H, TFT_GREY);
    if (slots[i].type != RES_NONE)
      textCenter(tft, font, x, BAR_BOTTOM, bw, LABEL_H,
                 resLabel(slots[i].type), TFT_WHITE, TFT_BLACK);
  }
}


/***************************************************************************************
   DRAW ONE BAR -- flicker-free two-fillRect technique
   NOTE: intentionally NOT the library's drawVertBarGraph() delta-erase. This full
   empty+fill repaint is robust to the mode-toggle path (updateScreenMain resets
   _prevLevel to -1 WITHOUT clearing the screen); a delta-erase keyed on prevVal would
   leave stale fill above a bar that shrank across the toggle.
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

    uint16_t x = barX(i, bw);

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

      // Percentage label colour depends on fill level (thresholdColor: strict <):
      //   0-9%    : red text    (critical)
      //  10-29%   : yellow text  (caution)
      //  30-100%  : white text   (nominal)
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
  if (x < SIDEBAR_X || x >= SIDEBAR_X + SIDEBAR_W) return -1;
  for (uint8_t btn = 0; btn < SB_BTN_COUNT; btn++) {
    uint16_t by = sbBtnY(btn);
    if (y >= by && y < by + sbBtnH()) return (int8_t)btn;
  }
  return -1;
}

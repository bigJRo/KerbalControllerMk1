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

   Button labels are horizontal, centred, using Roboto_Black_20.
   Bottom three buttons (DFLT, SEL, DATA) use a navy background.

   Each bar:
     - Drawn in the resource's fixed color when above LOW_RES_THRESHOLD; red below it
     - Two-fillRect technique: black from top to empty boundary, then color below
     - Resource label centred below the bar in white (font scales with bar width)
     - Percentage centred above the bar in white (font scales with bar width)
     - Percentage color: >30% white, 11-30% yellow (caution), 0-10% red (critical)
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


/***************************************************************************************
   LAYOUT CONSTANTS -- MAIN SCREEN
****************************************************************************************/
static const uint16_t SCREEN_W     = 800;
static const uint16_t SCREEN_H     = 480;
static const uint16_t AXIS_W       = 44;   // px reserved on left for Y-axis labels + ticks
static const uint16_t SIDEBAR_W    = 80;   // px -- width of right-hand nav button column
static inline uint16_t barRegionW() { return SCREEN_W - SIDEBAR_W; }
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
static void drawSidebar(RA8875 &tft) {
  tft.drawLine(sbX(), 0, sbX(), SCREEN_H, TFT_GREY);
  uint16_t bx = sbX() + 1;
  uint16_t bw = SIDEBAR_W - 1;

  // Button 0: TOTAL / STAGE mode toggle -- always drawn "on" (illuminated)
  const ButtonLabel &modeBtn = stageMode ? btnModeStage : btnModeTotal;
  drawButton(tft, bx, sbBtnY(0), bw, sbBtnH(), modeBtn, &Roboto_Black_20, true);

  // Buttons 1-3: action buttons, drawn "on" so their background color always shows
  drawButton(tft, bx, sbBtnY(1), bw, sbBtnH(), btnReset,  &Roboto_Black_20, true);
  drawButton(tft, bx, sbBtnY(2), bw, sbBtnH(), btnSelect, &Roboto_Black_20, true);
  drawButton(tft, bx, sbBtnY(3), bw, sbBtnH(), btnDetail, &Roboto_Black_20, true);
}


/***************************************************************************************
   REDRAW MODE BUTTON ONLY
   Called when stageMode toggles to avoid a full screen redraw.
****************************************************************************************/
void redrawStageModeButton(RA8875 &tft) {
  const ButtonLabel &modeBtn = stageMode ? btnModeStage : btnModeTotal;
  drawButton(tft, sbX() + 1, sbBtnY(0), SIDEBAR_W - 1, sbBtnH(), modeBtn, &Roboto_Black_20, true);
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
  return AXIS_W + BAR_PAD + index * (barWidth() + BAR_PAD);
}


/***************************************************************************************
   DRAW Y-AXIS
   Delegates to the library's drawLabelledAxis(). See KerbalDisplayCommon.h.
****************************************************************************************/
static void drawAxis(RA8875 &tft) {
  drawLabelledAxis(tft,
                   0, AXIS_W,
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
   DRAW STATIC CHROME -- main screen
****************************************************************************************/
static float _prevLevel[MAX_SLOTS];
static bool  _prevStageMode = false;

void drawStaticMain(RA8875 &tft) {
  tft.fillScreen(TFT_BLACK);
  drawSidebar(tft);
  drawAxis(tft);

  // Reset update-pass state so all bars and percentage labels repaint on first pass
  for (uint8_t i = 0; i < MAX_SLOTS; i++) _prevLevel[i] = -1.0f;
  _prevStageMode = stageMode;

  const tFont *font = barFont();
  uint16_t bw = barWidth();
  for (uint8_t i = 0; i < slotCount; i++) {
    uint16_t x = barX(i);
    tft.drawRect(x, BAR_TOP, bw, BAR_H, TFT_GREY);
    if (slots[i].type != RES_NONE) {
      tft.setFont(font);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      const char *lbl = resLabel(slots[i].type);
      int16_t lblW = getFontStringWidth(font, lbl);
      int16_t lblH = (int16_t)font->font_height;
      tft.setCursor(x + (bw - lblW) / 2, BAR_BOTTOM + (LABEL_H - lblH) / 2);
      tft.print(lbl);
    }
  }
}


/***************************************************************************************
   DRAW ONE BAR -- flicker-free two-fillRect technique
****************************************************************************************/
static void drawBar(RA8875 &tft, uint16_t x, uint16_t bw, uint16_t fillH, uint16_t color) {
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
void updateScreenMain(RA8875 &tft) {
  if (stageMode != _prevStageMode) {
    _prevStageMode = stageMode;
    redrawStageModeButton(tft);
    for (uint8_t i = 0; i < MAX_SLOTS; i++) _prevLevel[i] = -1.0f;
  }

  uint16_t bw = barWidth();

  for (uint8_t i = 0; i < slotCount; i++) {
    float cur = stageMode ? slots[i].stageCurrent : slots[i].current;
    float max = stageMode ? slots[i].stageMax     : slots[i].maxVal;
    float level = (max > 0.0f) ? (cur / max) : 0.0f;
    level = constrain(level, 0.0f, 1.0f);

    if (fabsf(level - _prevLevel[i]) < BAR_LEVEL_HYSTERESIS) continue;
    _prevLevel[i] = level;

    uint16_t x     = barX(i);
    uint16_t fillH = (uint16_t)((BAR_H - 2) * level);
    uint8_t  perc  = (uint8_t)(level * 100.0f);

    drawBar(tft, x, bw, fillH,
            (perc < LOW_RES_THRESHOLD) ? TFT_RED : resColor(slots[i].type));

    char percStr[6];
    snprintf(percStr, sizeof(percStr), "%d%%", perc);

    // Percentage label colour depends on fill level:
    //   0-10%   : red text on black background   (critical)
    //  11-30%   : yellow text on black background (caution)
    //  31-100%  : white text on black background  (nominal)
    // Bar fill shifts to red below LOW_RES_THRESHOLD (AAA_Config, default 20%).
    uint16_t percFore, percBack;
    thresholdColor(perc,
                   10, TFT_RED,    TFT_BLACK,
                   30, TFT_YELLOW, TFT_BLACK,
                       TFT_WHITE,  TFT_BLACK,
                   percFore, percBack);

    const tFont *font = barFont();
    int16_t fontH = (int16_t)font->font_height;
    tft.fillRect(x, 0, bw, PERC_H, percBack);
    tft.setFont(font);
    tft.setTextColor(percFore, percBack);
    int16_t tw = getFontStringWidth(font, percStr);
    tft.setCursor(x + (bw - tw) / 2, (PERC_H - fontH) / 2);
    tft.print(percStr);
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

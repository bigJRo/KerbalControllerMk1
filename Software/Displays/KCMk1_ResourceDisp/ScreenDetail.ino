/***************************************************************************************
   ScreenDetail.ino -- Numerical resource detail screen for KCMk1 Resource Display

   Layout (800×480):
     Left panel (DET_SEL_W=130px): selector buttons, one per active slot
     Right panel:
       Header (DET_HDR_H=66px): resource name in Roboto_Black_48 white + color accent strip + BACK
       Divider
       [DET_SECT_W=26px vertical "CRAFT" label] + 3 Craft rows: Available / Total / Remaining%
       Divider
       [DET_SECT_W=26px vertical "STAGE" label] + 3 Stage rows: Available / Total / Remaining%

   Flicker-free rendering:
     drawDetailChrome() — draws labels, dividers, header once on screen entry or slot switch
     drawDetailValues() — calls printValue() per row, skipped entirely if value unchanged
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


/***************************************************************************************
   LAYOUT CONSTANTS
****************************************************************************************/
static const uint16_t DET_PAD = 6;
static const uint16_t DET_SEL_W = 130;
static const uint16_t DET_PNL_X = DET_SEL_W + 1;
static const uint16_t DET_PNL_W = 800 - DET_PNL_X;
static const uint16_t DET_HDR_H = 66;                     // taller to fit Roboto_Black_48 (58px)
static const uint16_t DET_ROW_H = (480 - DET_HDR_H) / 6;  // ~69px — used for all rows in both layouts

// Vertical section label strip (left of data rows)
static const uint16_t DET_SECT_W = 26;  // wide enough for Roboto_Black_16
static const uint16_t DET_ROW_X = DET_PNL_X + DET_SECT_W;
static const uint16_t DET_ROW_W = 800 - DET_ROW_X;

// Color accent strip in header — offset a few px from the panel edge
static const uint16_t DET_ACCENT_X = DET_PNL_X + DET_PAD;
static const uint16_t DET_ACCENT_W = 4;

static const uint16_t DET_BACK_W = 110;
static const uint16_t DET_BACK_H = DET_HDR_H - DET_PAD * 2;
static const uint16_t DET_BACK_X = 800 - DET_BACK_W - DET_PAD;
static const uint16_t DET_BACK_Y = DET_PAD;

static const ButtonLabel detBtnBack = {
  "BACK", TFT_WHITE, TFT_WHITE, TFT_DARK_GREEN, TFT_GREEN, NO_BORDER, NO_BORDER
};

static uint8_t _detailSlot = 0;
static String _detValCache[6];


/***************************************************************************************
   HELPERS — layout depends on whether the current slot has real stage data
****************************************************************************************/
static bool _detHasStage() {
  if (_detailSlot >= slotCount) return false;
  return resHasStageData(slots[_detailSlot].type);
}

static uint16_t detRowY(uint8_t row) {
  return DET_HDR_H + row * DET_ROW_H;
}

// Number of rows to render: 3 (craft only) or 6 (craft + stage)
static uint8_t _detRowCount() { return _detHasStage() ? 6 : 3; }

/***************************************************************************************
   ROW LABEL
   Short label for the left (chrome) side of each data row.
   Rows share "Available / Total / Remaining" labels — section context comes from the
   vertical CRAFT/STAGE strip, not the row label itself.
****************************************************************************************/
static const char *detRowLabel(uint8_t row) {
  switch (row) {
    case 0: return "Available:";
    case 1: return "Total:";
    case 2: return "Remaining:";
    case 3: return "Available:";
    case 4: return "Total:";
    case 5: return "Remaining:";
    default: return "";
  }
}

/***************************************************************************************
   ROW VALUE
   Computes the formatted value string for a given row from the currently selected slot.
   Returns "--" if no valid slot is selected.
   Called every update pass; the cache in drawDetailValues suppresses redundant redraws.
****************************************************************************************/
static String detRowValue(uint8_t row) {
  if (_detailSlot >= slotCount) return "--";
  ResourceSlot &s = slots[_detailSlot];
  float craftPerc = (s.maxVal > 0.0f) ? (s.current / s.maxVal * 100.0f) : 0.0f;
  float stagePerc = (s.stageMax > 0.0f) ? (s.stageCurrent / s.stageMax * 100.0f) : 0.0f;
  switch (row) {
    case 0: return formatFloat(s.current, 2);
    case 1: return formatFloat(s.maxVal, 2);
    case 2: return formatFloat(craftPerc, 1) + "%";
    case 3: return formatFloat(s.stageCurrent, 2);
    case 4: return formatFloat(s.stageMax, 2);
    case 5: return formatFloat(stagePerc, 1) + "%";
    default: return "--";
  }
}


/***************************************************************************************
   DRAW SELECTOR COLUMN
****************************************************************************************/
static void drawDetailSelector(RA8875 &tft) {
  tft.fillRect(0, 0, DET_SEL_W, 480, TFT_BLACK);
  tft.fillRect(DET_SEL_W, 0, 2, 480, TFT_DARK_GREY);  // 2px divider
  if (slotCount == 0) return;

  uint16_t btnH = 480 / slotCount;
  static const uint16_t BTN_PAD = 3;

  // Scale font to button height so text is as large as possible
  const tFont *btnFont;
  if      (btnH >= 70) btnFont = &Roboto_Black_20;
  else if (btnH >= 48) btnFont = &Roboto_Black_16;
  else                 btnFont = &Roboto_Black_12;

  for (uint8_t i = 0; i < slotCount; i++) {
    if (slots[i].type == RES_NONE) continue;
    ButtonLabel btn;
    btn.text = resFullName(slots[i].type);
    btn.fontColorOff = TFT_DARK_GREY;
    btn.fontColorOn = TFT_BLACK;
    btn.backgroundColorOff = TFT_OFF_BLACK;
    btn.backgroundColorOn = resColor(slots[i].type);
    btn.borderColorOff = TFT_GREY;
    btn.borderColorOn = TFT_WHITE;
    drawButton(tft, BTN_PAD, i * btnH + BTN_PAD,
               DET_SEL_W - BTN_PAD * 2, btnH - BTN_PAD * 2,
               btn, btnFont, i == _detailSlot);
  }
}


/***************************************************************************************
   DRAW CHROME -- drawn once on screen entry or slot switch
   Renders everything that doesn't change frame-to-frame: header, accent strip,
   BACK button, divider lines, vertical CRAFT/STAGE labels, and row label text.
   Does NOT draw values — those are handled by drawDetailValues() each frame.
   Invalidates _detValCache so drawDetailValues() does a full value repaint after.
****************************************************************************************/
static void drawDetailChrome(RA8875 &tft) {
  tft.fillRect(DET_PNL_X, 0, DET_PNL_W, 480, TFT_BLACK);
  for (uint8_t i = 0; i < 6; i++) _detValCache[i] = "";  // invalidate all 6 regardless of row count

  if (slotCount == 0 || _detailSlot >= slotCount) {
    tft.setFont(&Roboto_Black_20);
    tft.setTextColor(TFT_GREY, TFT_BLACK);
    tft.setCursor(DET_PNL_X + DET_PAD, DET_HDR_H + DET_PAD);
    tft.print("No resource selected");
    drawButton(tft, DET_BACK_X, DET_BACK_Y, DET_BACK_W, DET_BACK_H, detBtnBack, &Roboto_Black_20, false);
    return;
  }

  uint16_t resCol = resColor(slots[_detailSlot].type);

  // Header accent strip — offset a few px from panel edge
  tft.fillRect(DET_ACCENT_X, 0, DET_ACCENT_W, DET_HDR_H - 2, resCol);

  // Resource name — Roboto_Black_48 white, clearly larger than data rows
  tft.setFont(&Roboto_Black_48);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(DET_ACCENT_X + DET_ACCENT_W + DET_PAD, (DET_HDR_H - 58) / 2);
  tft.print(resFullName(slots[_detailSlot].type));

  // BACK button
  drawButton(tft, DET_BACK_X, DET_BACK_Y, DET_BACK_W, DET_BACK_H, detBtnBack, &Roboto_Black_20, false);

  // Vertical section labels — delegates to library's drawVerticalText()
  // Always show CRAFT; only show STAGE if this resource has a real stage channel.
  if (_detHasStage()) {
    drawVerticalText(tft, DET_PNL_X, detRowY(0), DET_SECT_W, DET_ROW_H * 3,
                     &Roboto_Black_16, "CRAFT", TFT_GREY, TFT_BLACK);
    drawVerticalText(tft, DET_PNL_X, detRowY(3), DET_SECT_W, DET_ROW_H * 3,
                     &Roboto_Black_16, "STAGE", TFT_GREY, TFT_BLACK);
  } else {
    drawVerticalText(tft, DET_PNL_X, detRowY(0), DET_SECT_W, DET_ROW_H * 3,
                     &Roboto_Black_16, "CRAFT", TFT_GREY, TFT_BLACK);
  }

  // Row labels (chrome only, no values)
  uint8_t rowCount = _detRowCount();
  for (uint8_t i = 0; i < rowCount; i++) {
    printDispChrome(tft, &Roboto_Black_36,
                    DET_ROW_X, detRowY(i), DET_ROW_W, DET_ROW_H,
                    detRowLabel(i),
                    TFT_WHITE, TFT_BLACK, NO_BORDER);
  }

  // 1px dividers — below header, and between Craft/Stage sections (if stage shown)
  tft.drawLine(DET_SEL_W, DET_HDR_H,  800, DET_HDR_H,  TFT_DARK_GREY);
  if (_detHasStage()) {
    tft.drawLine(DET_SEL_W, detRowY(3), 800, detRowY(3), TFT_DARK_GREY);
  }
}


/***************************************************************************************
   DRAW VALUES -- called every update pass
   Uses printValue() which redraws only the right-hand value region of each row,
   leaving the label chrome completely untouched (no flicker on the label side).
   _detValCache suppresses the printValue call entirely when a value hasn't changed,
   so stable rows produce zero draw calls per frame.
****************************************************************************************/
static void drawDetailValues(RA8875 &tft) {
  if (slotCount == 0 || _detailSlot >= slotCount) return;
  uint8_t rowCount = _detRowCount();
  for (uint8_t i = 0; i < rowCount; i++) {
    String val = detRowValue(i);
    if (val == _detValCache[i]) continue;
    _detValCache[i] = val;
    printValue(tft, &Roboto_Black_36,
               DET_ROW_X, detRowY(i), DET_ROW_W, DET_ROW_H,
               detRowLabel(i), val,
               TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK);
  }
}


/***************************************************************************************
   PUBLIC INTERFACE
****************************************************************************************/
void drawStaticDetail(RA8875 &tft) {
  tft.fillScreen(TFT_BLACK);
  if (_detailSlot >= slotCount && slotCount > 0) _detailSlot = 0;
  drawDetailSelector(tft);
  drawDetailChrome(tft);
}

void updateScreenDetail(RA8875 &tft) {
  drawDetailValues(tft);
}

bool handleDetailTouch(uint16_t x, uint16_t y) {
  if (x >= DET_BACK_X && x < DET_BACK_X + DET_BACK_W && y >= DET_BACK_Y && y < DET_BACK_Y + DET_BACK_H) {
    switchToScreen(screen_Main);
    return false;
  }
  if (x < DET_SEL_W && slotCount > 0) {
    uint16_t btnH = 480 / slotCount;
    uint8_t  hit  = (uint8_t)(y / btnH);
    if (hit < slotCount && hit != _detailSlot) {
      _detailSlot = hit;
      clearTouchISR();           // discard touches queued before redraw
      drawDetailSelector(infoDisp);
      drawDetailChrome(infoDisp);
      clearTouchISR();           // discard touches that fired *during* redraw
    }
    return true;
  }
  return false;
}

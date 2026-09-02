/***************************************************************************************
   ScreenDetail.ino -- Numerical resource detail screen for KCMk1 Resource Display

   Layout (1024×600, geometry derives from KCM_SCREEN_W/H):
     Left panel (DET_SEL_W=180px): selector buttons, one per active slot
     Right panel:
       Header (DET_HDR_H=66px): resource name in Roboto_Black_48 white + color accent strip + BACK
       Divider
       [DET_SECT_W=32px vertical "CRAFT" label] + 5 Craft rows:
           Available / Total / Remaining% / Rate (units per game second) / Time (to empty)
       Divider
       [DET_SECT_W=32px vertical "STAGE" label] + the same 5 Stage rows
     Rate and Time come from Sampling.ino, the same estimate the Main screen's trend
     arrows and TTE counters use. Row height and font follow the row count: 5 rows
     (no stage channel) get Roboto_Black_48, 10 rows get Roboto_Black_32.
       Bug bar (DET_BUG_H, bottom of the right panel): the reserve bug's value in
       cyan, keys -10 / -1 / +1 / +10 to set it to a precise percent, and CLR. The
       first step on a resource without a bug starts one at the caution fraction.

   Flicker-free rendering:
     drawDetailChrome() — draws labels, dividers, header once on screen entry or slot switch
     drawDetailValues() — calls printValue() per row, skipped entirely if value unchanged
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"

// PrintState instances for KDC v2 printValue() — one per detail row (max 10).
// Defined here, declared extern in KCMk1_ResourceDisp.h.
PrintState psDetailRows[DET_MAX_ROWS];


/***************************************************************************************
   LAYOUT CONSTANTS
****************************************************************************************/
static const uint16_t DET_PAD = 6;
static const uint16_t DET_SEL_W = 180;   // wider selector column — larger buttons + font
static const uint16_t DET_PNL_X = DET_SEL_W + 1;
static const uint16_t DET_PNL_W = KCM_SCREEN_W - DET_PNL_X;
static const uint16_t DET_HDR_H = 66;                             // taller to fit Roboto_Black_48 (58px)
static const uint8_t  DET_SECT_ROWS = 5;                          // rows per CRAFT / STAGE section

// Bug bar along the bottom of the right panel. Tall enough that the keys clear the
// bottom-edge touch dead zone with a finger-sized target to spare, and the hit test
// takes the whole bar height, not just the drawn key.
static const uint16_t DET_BUG_H     = 80;
static const uint16_t DET_BUG_Y     = KCM_SCREEN_H - DET_BUG_H;   // 520
static const uint16_t DET_BUG_KEY_W = 110;
static const uint16_t DET_BUG_KEY_H = DET_BUG_H - DET_PAD * 2;    // 68
static const uint16_t DET_BUG_RO_W  = 170;                        // "BUG 25%" readout cell
static const uint8_t  DET_BUG_KEYS  = 5;                          // -10 -1 +1 +10 CLR
static const int8_t   DET_BUG_STEP[DET_BUG_KEYS] = { -10, -1, 1, 10, 0 };   // 0 = CLR
static const char    *DET_BUG_TEXT[DET_BUG_KEYS] = { "-10", "-1", "+1", "+10", "CLR" };

// Vertical section label strip (left of data rows)
static const uint16_t DET_SECT_W = 32;  // wide enough for Roboto_Black_20
static const uint16_t DET_ROW_X = DET_PNL_X + DET_SECT_W;
static const uint16_t DET_ROW_W = KCM_SCREEN_W - DET_ROW_X;

// Color accent strip in header — offset a few px from the panel edge
static const uint16_t DET_ACCENT_X = DET_PNL_X + DET_PAD;
static const uint16_t DET_ACCENT_W = 4;

static const uint16_t DET_BACK_W = 110;
static const uint16_t DET_BACK_H = DET_HDR_H - DET_PAD * 2;
static const uint16_t DET_BACK_X = KCM_SCREEN_W - DET_BACK_W - DET_PAD;
static const uint16_t DET_BACK_Y = DET_PAD;

// Achromatic navigation chrome, matching the Select screen's BACK and the Main
// sidebar. The grey border carries the key shape now that the fill is black.
static const ButtonLabel detBtnBack = {
  "BACK", TFT_WHITE, TFT_WHITE, TFT_BLACK, TFT_BLACK, TFT_GREY, TFT_GREY
};

static uint8_t _detailSlot = 0;
static String _detValCache[DET_MAX_ROWS];
static uint16_t _detAbsentSig = 0;   // bit per slot: absent when the selector was drawn

static uint16_t _detAbsentNow() {
  uint16_t sig = 0;
  for (uint8_t i = 0; i < slotCount && i < 16; i++) if (resAbsent(slots[i].type)) sig |= (1u << i);
  return sig;
}

void setDetailSlot(uint8_t i) {
  if (i < slotCount) _detailSlot = i;
}


/***************************************************************************************
   HELPERS — layout depends on whether the current slot has real stage data
****************************************************************************************/
static bool _detHasStage() {
  if (_detailSlot >= slotCount) return false;
  return resHasStageData(slots[_detailSlot].type);
}

// Number of rows to render: 5 (craft only) or 10 (craft + stage)
static uint8_t _detRowCount() { return _detHasStage() ? 2 * DET_SECT_ROWS : DET_SECT_ROWS; }

static uint16_t _detRowH() { return (KCM_SCREEN_H - DET_HDR_H - DET_BUG_H) / _detRowCount(); }

static uint16_t detRowY(uint8_t row) {
  return DET_HDR_H + row * _detRowH();
}

// Row font follows the row height so both layouts fill their rows.
static const tFont *_detRowFont() {
  return (_detRowCount() > DET_SECT_ROWS) ? &Roboto_Black_32 : &Roboto_Black_48;
}


/***************************************************************************************
   BUG BAR
   Readout on the left, step keys and CLR to its right. The keys use the plain
   achromatic chrome; CLR takes the orange guard treatment since it discards a
   pilot-entered value. The readout redraws alone on a key press; the Main screen
   picks the change up through its own cache on return.
****************************************************************************************/
static uint16_t _detBugKeyX(uint8_t k) {
  return DET_PNL_X + DET_PAD + DET_BUG_RO_W + k * (DET_BUG_KEY_W + DET_PAD);
}

static void drawDetailBugReadout(KCM_TFT &tft) {
  char buf[16];
  if (_detailSlot < slotCount && slots[_detailSlot].bug >= 0.0f)
    snprintf(buf, sizeof(buf), "BUG %d%%", (int)roundf(slots[_detailSlot].bug * 100.0f));
  else
    strlcpy(buf, "BUG ---", sizeof(buf));
  tft.fillRect(DET_PNL_X, DET_BUG_Y, DET_BUG_RO_W, DET_BUG_H, TFT_BLACK);
  textCenter(tft, &Roboto_Black_24, DET_PNL_X, DET_BUG_Y, DET_BUG_RO_W, DET_BUG_H,
             String(buf), TFT_CYAN, TFT_BLACK);
}

static void drawDetailBugBar(KCM_TFT &tft) {
  tft.fillRect(DET_PNL_X, DET_BUG_Y, DET_PNL_W, DET_BUG_H, TFT_BLACK);
  tft.drawLine(DET_SEL_W, DET_BUG_Y, KCM_SCREEN_W, DET_BUG_Y, TFT_DARK_GREY);
  drawDetailBugReadout(tft);
  for (uint8_t k = 0; k < DET_BUG_KEYS; k++) {
    bool clr = (DET_BUG_STEP[k] == 0);
    ButtonLabel b;
    b.text = DET_BUG_TEXT[k];
    b.fontColorOff = b.fontColorOn = clr ? TFT_ORANGE : TFT_WHITE;
    b.backgroundColorOff = b.backgroundColorOn = clr ? TFT_OFF_BLACK : TFT_BLACK;
    b.borderColorOff = b.borderColorOn = clr ? TFT_ORANGE : TFT_GREY;
    drawButton(tft, _detBugKeyX(k), DET_BUG_Y + DET_PAD, DET_BUG_KEY_W, DET_BUG_KEY_H,
               b, &Roboto_Black_20, false);
  }
}

// Apply one key. A step on a resource without a bug starts one at the caution
// fraction, so the first press gives a sensible bug rather than 1%.
static void adjustDetailBug(int8_t stepPct) {
  if (_detailSlot >= slotCount) return;
  ResourceSlot &s = slots[_detailSlot];
  if (stepPct == 0) { s.bug = -1.0f; return; }
  if (s.bug < 0.0f) s.bug = RES_WARN_FRAC;
  else              s.bug += stepPct / 100.0f;
  s.bug = constrain(roundf(s.bug * 100.0f) / 100.0f, 0.01f, 0.99f);
}

/***************************************************************************************
   ROW LABEL
   Short label for the left (chrome) side of each data row.
   Rows share "Available / Total / Remaining" labels — section context comes from the
   vertical CRAFT/STAGE strip, not the row label itself.
****************************************************************************************/
static const char *detRowLabel(uint8_t row) {
  switch (row % DET_SECT_ROWS) {
    case 0: return "Available:";
    case 1: return "Total:";
    case 2: return "Remaining:";
    case 3: return "Rate:";
    case 4: return "Time:";
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
  bool  stage = (row >= DET_SECT_ROWS);
  float cur   = stage ? s.stageCurrent : s.current;
  float max   = stage ? s.stageMax     : s.maxVal;
  const SlotSample &smp = stage ? sampleStg[_detailSlot] : sampleTot[_detailSlot];
  char buf[12];
  switch (row % DET_SECT_ROWS) {
    case 0: return formatFloat(cur, 2);
    case 1: return formatFloat(max, 2);
    case 2: return formatFloat((max > 0.0f) ? (cur / max * 100.0f) : 0.0f, 1) + "%";
    case 3: fmtRate(smp, buf, sizeof(buf)); return String(buf);
    case 4: fmtTte((max > 0.0f) ? sampleTteSeconds(smp, s.type, cur, max) : -1.0f, buf, sizeof(buf));
            return String(buf);
    default: return "--";
  }
}


/***************************************************************************************
   DRAW SELECTOR COLUMN
****************************************************************************************/
static void drawDetailSelector(KCM_TFT &tft) {
  tft.fillRect(0, 0, DET_SEL_W, KCM_SCREEN_H, TFT_BLACK);
  tft.fillRect(DET_SEL_W, 0, 2, KCM_SCREEN_H, TFT_DARK_GREY);  // 2px divider
  if (slotCount == 0) return;

  uint16_t btnH = KCM_SCREEN_H / slotCount;
  static const uint16_t BTN_PAD = 3;

  // Scale font to button height so text is as large as possible while still
  // leaving room for a TWO-LINE wrapped name (e.g. "Enriched Uranium") plus its
  // descenders without spilling over the button border. Thresholds are sized for
  // the two-line case: ~2*cap_height + descender must clear (btnH - 2*BTN_PAD).
  const tFont *btnFont;
  if      (btnH >= 100) btnFont = &Roboto_Black_28;
  else if (btnH >= 80)  btnFont = &Roboto_Black_24;
  else if (btnH >= 64)  btnFont = &Roboto_Black_20;
  else if (btnH >= 54)  btnFont = &Roboto_Black_16;
  else                  btnFont = &Roboto_Black_12;

  for (uint8_t i = 0; i < slotCount; i++) {
    if (slots[i].type == RES_NONE) continue;
    // A resource the vessel does not carry keeps its key -- its rows read "---" --
    // but draws dimmed, as it does on the Select screen.
    bool absent = resAbsent(slots[i].type);
    ButtonLabel btn;
    btn.text = resFullName(slots[i].type);
    btn.fontColorOff = TFT_DARK_GREY;
    btn.fontColorOn = absent ? TFT_DARK_GREY : TFT_BLACK;
    btn.backgroundColorOff = TFT_OFF_BLACK;
    btn.backgroundColorOn = absent ? TFT_OFF_BLACK : resColor(slots[i].type);
    btn.borderColorOff = absent ? TFT_DARK_GREY : TFT_GREY;
    btn.borderColorOn = absent ? TFT_DARK_GREY : TFT_WHITE;
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
static void drawDetailChrome(KCM_TFT &tft) {
  tft.fillRect(DET_PNL_X, 0, DET_PNL_W, KCM_SCREEN_H, TFT_BLACK);
  for (uint8_t i = 0; i < DET_MAX_ROWS; i++) {
    _detValCache[i] = "";   // invalidate every row regardless of row count
    psDetailRows[i] = PrintState{};  // reset PrintState sentinel — forces full clear on next draw
  }

  if (slotCount == 0 || _detailSlot >= slotCount) {
    tft.setFont(Roboto_Black_20);
    tft.setTextColor(TFT_GREY, TFT_BLACK);
    tft.setCursor(DET_PNL_X + DET_PAD, DET_HDR_H + DET_PAD);
    tft.print("No resource selected");
    drawButton(tft, DET_BACK_X, DET_BACK_Y, DET_BACK_W, DET_BACK_H, detBtnBack, &Roboto_Black_20, false);
    return;
  }

  uint16_t resCol = resColor(slots[_detailSlot].type);

  // Header accent strip — offset a few px from panel edge
  tft.fillRect(DET_ACCENT_X, 0, DET_ACCENT_W, DET_HDR_H - 2, resCol);

  // Resource name — Roboto_Black_48 white; the colour accent strip and top
  // position distinguish it from the (also 48px) data rows below.
  tft.setFont(Roboto_Black_48);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(DET_ACCENT_X + DET_ACCENT_W + DET_PAD, (DET_HDR_H - 58) / 2);
  tft.print(resFullName(slots[_detailSlot].type));

  // BACK button
  drawButton(tft, DET_BACK_X, DET_BACK_Y, DET_BACK_W, DET_BACK_H, detBtnBack, &Roboto_Black_20, false);

  // Vertical section labels — delegates to library's drawVerticalText()
  // Always show CRAFT; only show STAGE if this resource has a real stage channel.
  uint16_t rowH    = _detRowH();
  uint16_t sectH   = rowH * DET_SECT_ROWS;
  const tFont *rowFont = _detRowFont();
  drawVerticalText(tft, DET_PNL_X, detRowY(0), DET_SECT_W, sectH,
                   &Roboto_Black_20, "CRAFT", TFT_GREY, TFT_BLACK);
  if (_detHasStage()) {
    drawVerticalText(tft, DET_PNL_X, detRowY(DET_SECT_ROWS), DET_SECT_W, sectH,
                     &Roboto_Black_20, "STAGE", TFT_GREY, TFT_BLACK);
  }

  // Row labels (chrome only, no values)
  uint8_t rowCount = _detRowCount();
  for (uint8_t i = 0; i < rowCount; i++) {
    printDispChrome(tft, rowFont,
                    DET_ROW_X, detRowY(i), DET_ROW_W, rowH,
                    detRowLabel(i),
                    TFT_WHITE, TFT_BLACK, NO_BORDER);
  }

  // 1px dividers — below header, and between Craft/Stage sections (if stage shown)
  tft.drawLine(DET_SEL_W, DET_HDR_H,  KCM_SCREEN_W, DET_HDR_H,  TFT_DARK_GREY);
  if (_detHasStage()) {
    tft.drawLine(DET_SEL_W, detRowY(DET_SECT_ROWS), KCM_SCREEN_W, detRowY(DET_SECT_ROWS), TFT_DARK_GREY);
  }

  drawDetailBugBar(tft);
}


/***************************************************************************************
   DRAW VALUES -- called every update pass
   Uses printValue() which redraws only the right-hand value region of each row,
   leaving the label chrome completely untouched (no flicker on the label side).
   _detValCache suppresses the printValue call entirely when a value hasn't changed,
   so stable rows produce zero draw calls per frame.
****************************************************************************************/
static void drawDetailValues(KCM_TFT &tft) {
  if (slotCount == 0 || _detailSlot >= slotCount) return;
  uint8_t rowCount = _detRowCount();
  uint16_t rowH    = _detRowH();
  const tFont *rowFont = _detRowFont();
  for (uint8_t i = 0; i < rowCount; i++) {
    String val = detRowValue(i);
    if (val == _detValCache[i]) continue;
    _detValCache[i] = val;
    printValue(tft, rowFont,
               DET_ROW_X, detRowY(i), DET_ROW_W, rowH,
               detRowLabel(i), val,
               TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, psDetailRows[i]);
  }
}


/***************************************************************************************
   PUBLIC INTERFACE
****************************************************************************************/
void drawStaticDetail(KCM_TFT &tft) {
  tft.fillScreen(TFT_BLACK);
  if (_detailSlot >= slotCount && slotCount > 0) _detailSlot = 0;
  _detAbsentSig = _detAbsentNow();
  drawDetailSelector(tft);
  drawDetailChrome(tft);
}

void updateScreenDetail(KCM_TFT &tft) {
  // Presence can change while this screen is up (a refresh answers); the selector
  // dimming follows it.
  uint16_t sig = _detAbsentNow();
  if (sig != _detAbsentSig) {
    _detAbsentSig = sig;
    drawDetailSelector(tft);
  }
  drawDetailValues(tft);
}

bool handleDetailTouch(uint16_t x, uint16_t y) {
  if (x >= DET_BACK_X && x < DET_BACK_X + DET_BACK_W && y >= DET_BACK_Y && y < DET_BACK_Y + DET_BACK_H) {
    switchToScreen(screen_Main);
    return false;
  }
  // Bug bar keys -- the whole bar height and half the gap either side count
  if (y >= DET_BUG_Y && x >= DET_PNL_X && slotCount > 0 && _detailSlot < slotCount) {
    for (uint8_t k = 0; k < DET_BUG_KEYS; k++) {
      uint16_t kx = _detBugKeyX(k);
      if (x + DET_PAD / 2 >= kx && x < kx + DET_BUG_KEY_W + DET_PAD / 2) {
        adjustDetailBug(DET_BUG_STEP[k]);
        drawDetailBugReadout(infoDisp);
        return true;
      }
    }
    return false;
  }
  if (x < DET_SEL_W && slotCount > 0) {
    uint16_t btnH = KCM_SCREEN_H / slotCount;
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

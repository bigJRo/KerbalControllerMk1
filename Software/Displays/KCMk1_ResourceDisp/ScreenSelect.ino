/***************************************************************************************
   ScreenSelect.ino -- Resource selection screen for Kerbal Controller Mk1 Resource Display

   Layout (1024×600):
     Row 1 (TITLE_H):  "Select Resources" title (large) + slot count + BACK button
     Row 2 (PRESET_H): 6 preset group buttons across grid width, left of BACK
     Left panel (0..GRID_W-1):        5-column resource grid below header rows
     Right panel (GRID_W+pad..W-1):   selection order list + DFLT and CLEAR buttons
     All backgrounds pure black. Geometry derives from KCM_SCREEN_W/H.

   The preset table itself lives in Resources.ino (PRESETS), shared with the default
   layout logic. DFLT makes the current selection, with its reserve bugs, the layout
   every vessel not in memory starts with; it is stored with the vessel memory. The
   key lights when the selection already is the default. CLEAR then DFLT (an empty
   set cannot be a default) drops the stored one, back to the SPCT preset.
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


/***************************************************************************************
   LAYOUT CONSTANTS
****************************************************************************************/
static const uint16_t SEL_PAD    = 6;

// Row 1: title + slot count (same line) + BACK
static const uint16_t TITLE_H    = 48;   // just fits 43px Roboto_Black_36

// Row 2: preset buttons
static const uint16_t PRESET_H   = 44;

// Combined header height
static const uint16_t TOP_H      = TITLE_H + PRESET_H;

// Resource grid (left panel, below header)
static const uint8_t  SEL_COLS   = 5;
static const uint8_t  SEL_ROWS   = (RESOURCE_TYPE_COUNT + SEL_COLS - 1) / SEL_COLS;
static const uint16_t GRID_W     = (KCM_SCREEN_W * 3) / 4;   // 768 @1024 — grid 3/4, order list 1/4
static const uint16_t SEL_BTN_W  = (GRID_W - SEL_PAD * (SEL_COLS + 1)) / SEL_COLS;
static const uint16_t SEL_BTN_H  = (KCM_SCREEN_H - TOP_H - SEL_PAD * (SEL_ROWS + 1)) / SEL_ROWS;
static const uint16_t SEL_START_X = SEL_PAD;
static const uint16_t SEL_START_Y = TOP_H + SEL_PAD;

// Right panel (order list)
static const uint16_t PANEL_X    = GRID_W + SEL_PAD * 2;
static const uint16_t PANEL_W    = KCM_SCREEN_W - PANEL_X - SEL_PAD;

// BACK button — spans both title and preset rows for easy pressing.
// Achromatic navigation chrome, matching the Main sidebar. The grey border is not
// decoration here: with a black fill on a black screen it is the only thing that makes
// the key visible.
static const ButtonLabel btnBack = {
  "BACK", TFT_WHITE, TFT_WHITE, TFT_BLACK, TFT_BLACK, TFT_GREY, TFT_GREY
};
static const uint16_t BACK_W = 110;
static const uint16_t BACK_H = TITLE_H + PRESET_H - SEL_PAD * 2;
static const uint16_t BACK_X = KCM_SCREEN_W - BACK_W - SEL_PAD;
static const uint16_t BACK_Y = SEL_PAD;

// CLEAR button (bottom of right panel). This one deselects every resource, so it keeps
// an affordance the plain keys do not — but as a guard, not an alarm: orange legend and
// border on off-black, the same treatment the InfoDisp Ascent Autopilot ARM button uses
// for a control with consequences. A red fill would have read as a caution condition in
// a panel whose percentage labels turn red below 10%.
static const ButtonLabel btnClear = {
  "CLEAR", TFT_ORANGE, TFT_ORANGE, TFT_OFF_BLACK, TFT_OFF_BLACK, TFT_ORANGE, TFT_ORANGE
};
static const uint16_t CLEAR_H = 48;
static const uint16_t CLEAR_Y = KCM_SCREEN_H - CLEAR_H - SEL_PAD;
static const uint16_t CLEAR_X = PANEL_X;
static const uint16_t CLEAR_W = PANEL_W;

// DFLT button, above CLEAR. Cyan is the pilot-entry colour everywhere on the panel,
// and this key stores a pilot entry. Lit (cyan fill) when the current selection is
// the default already, so it also reads as a state.
static const ButtonLabel btnDflt = {
  "DFLT", TFT_CYAN, TFT_BLACK, TFT_OFF_BLACK, TFT_CYAN, TFT_CYAN, TFT_CYAN
};
static const uint16_t DFLT_H = 48;
static const uint16_t DFLT_Y = CLEAR_Y - DFLT_H - SEL_PAD;
static const uint16_t DFLT_X = PANEL_X;
static const uint16_t DFLT_W = PANEL_W;

// Preset group buttons (row 2, across GRID_W only — BACK occupies right side)
static const uint16_t PRESET_BTN_W = (BACK_X - SEL_PAD * (PRESET_COUNT + 1)) / PRESET_COUNT;
static const uint16_t PRESET_BTN_H = PRESET_H - SEL_PAD * 2;
static const uint16_t PRESET_Y        = TITLE_H + SEL_PAD;
static const uint16_t PRESET_TOUCH_Y2 = TITLE_H + PRESET_H;  // bottom of preset touch zone
// Touch zone starts at y=0 so the full column above each button also registers
static const uint16_t PRESET_TOUCH_Y1 = 0;


/***************************************************************************************
   HELPER -- is a resource type currently in any slot?
****************************************************************************************/
static bool isSelected(ResourceType t) {
  for (uint8_t i = 0; i < slotCount; i++) {
    if (slots[i].type == t) return true;
  }
  return false;
}


/***************************************************************************************
   HELPER -- add a resource to the next empty slot. Returns true if added.
****************************************************************************************/
static bool addResource(ResourceType t) {
  if (t == RES_EVA_PROP && !evaActive) return false;   // EVA fuel only exists on EVA
  if (resAbsent(t)) return false;                       // the vessel does not carry it
  if (slotCount >= MAX_SLOTS) return false;
  slots[slotCount].type = t;
  initSlotValues(slots[slotCount]);   // 0 live / visible demo values
  slotCount++;
  sortSlotsByGroup();   // keep the ORDER list showing what the Main screen will draw
  return true;
}


/***************************************************************************************
   HELPER -- remove a resource from slots[], compact the array.
****************************************************************************************/
static void removeResource(ResourceType t) {
  for (uint8_t i = 0; i < slotCount; i++) {
    if (slots[i].type == t) {
      for (uint8_t j = i; j < slotCount - 1; j++) slots[j] = slots[j + 1];
      slots[slotCount - 1] = ResourceSlot();
      slotCount--;
      return;
    }
  }
}


/***************************************************************************************
   LOAD PRESET GROUP
****************************************************************************************/
static void loadPreset(uint8_t presetIndex) {
  if (presetIndex >= PRESET_COUNT) return;
  for (uint8_t i = 0; i < MAX_SLOTS; i++) slots[i] = ResourceSlot();
  slotCount = 0;
  const PresetGroup &pg = PRESETS[presetIndex];
  for (uint8_t i = 0; i < pg.count && slotCount < MAX_SLOTS; i++) {
    if (resAbsent(pg.types[i])) continue;   // not aboard this vessel
    slots[slotCount].type = pg.types[i];
    initSlotValues(slots[slotCount]);
    slotCount++;
  }
  sortSlotsByGroup();
  requestResourceRefresh();
}


/***************************************************************************************
   DRAW ONE GRID BUTTON
****************************************************************************************/
static void drawSelectButton(KCM_TFT &tft, uint8_t gridIndex, bool isOn) {
  ResourceType t = resTypeByIndex(gridIndex);
  if (t == RES_NONE) return;

  uint8_t  col = gridIndex % SEL_COLS;
  uint8_t  row = gridIndex / SEL_COLS;
  uint16_t x   = SEL_START_X + col * (SEL_BTN_W + SEL_PAD);
  uint16_t y   = SEL_START_Y + row * (SEL_BTN_H + SEL_PAD);

  // EVA Propellant is shown only while a Kerbal is on EVA; otherwise the cell is
  // blank (hidden and not selectable).
  if (t == RES_EVA_PROP && !evaActive) {
    tft.fillRect(x, y, SEL_BTN_W, SEL_BTN_H, TFT_BLACK);
    return;
  }

  ButtonLabel btn;
  btn.text               = resFullName(t);

  // On EVA the selection is locked to the EVA bar set — every other resource is
  // drawn dimmed and inert (tap handler ignores it). A resource the vessel is known
  // not to carry is drawn the same way: there is nothing to show for it.
  if ((evaActive && !isEvaResource(t)) || resAbsent(t)) {
    btn.fontColorOff = btn.fontColorOn = TFT_DARK_GREY;
    btn.backgroundColorOff = btn.backgroundColorOn = TFT_OFF_BLACK;
    btn.borderColorOff = btn.borderColorOn = TFT_DARK_GREY;
    drawButton(tft, x, y, SEL_BTN_W, SEL_BTN_H, btn, &Roboto_Black_20, false);
    return;
  }

  btn.fontColorOff       = TFT_DARK_GREY;
  btn.fontColorOn        = TFT_BLACK;
  btn.backgroundColorOff = TFT_OFF_BLACK;
  btn.backgroundColorOn  = resColor(t);
  btn.borderColorOff     = TFT_GREY;
  btn.borderColorOn      = TFT_WHITE;

  drawButton(tft, x, y, SEL_BTN_W, SEL_BTN_H, btn, &Roboto_Black_20, isOn);
}


/***************************************************************************************
   DRAW SLOT COUNT -- small text in title row, right of centre
   Also the screen's one-line feedback: for SEL_FLASH_MS it carries a note beside the
   count. A tap the limit refuses (adding at MAX_SLOTS) flashes MAX in yellow, so the
   pilot sees why the grid did not respond; DFLT flashes DFLT SET or DFLT CLR in cyan.
****************************************************************************************/
static const uint32_t SEL_FLASH_MS = 700;
static uint32_t _selFlashUntil = 0;   // millis() at which the flash reverts; 0 = idle

static void drawSlotCount(KCM_TFT &tft, const char *note, uint16_t noteColor) {
  char countStr[32];
  if (note) snprintf(countStr, sizeof(countStr), "%d / %d  %s", slotCount, MAX_SLOTS, note);
  else      snprintf(countStr, sizeof(countStr), "%d / %d", slotCount, MAX_SLOTS);
  int16_t cw = getFontStringWidth(&Roboto_Black_16, countStr);
  uint16_t cx = BACK_X - cw - SEL_PAD * 2;
  uint16_t cy = (TITLE_H - 20) / 2;  // vertically centred in title row (font height ~20px)
  // Clear the widest string this cell can hold so a shorter one leaves no ghost.
  int16_t maxW = getFontStringWidth(&Roboto_Black_16, "16 / 16  DFLT SET");
  tft.fillRect(BACK_X - maxW - SEL_PAD * 2 - 2, 0, maxW + 4, TITLE_H, TFT_BLACK);
  tft.setFont(Roboto_Black_16);
  tft.setTextColor(note ? noteColor : TFT_GREY, TFT_BLACK);
  tft.setCursor(cx, cy);
  tft.print(countStr);
}

static void flashNote(KCM_TFT &tft, const char *note, uint16_t color) {
  _selFlashUntil = millis() + SEL_FLASH_MS;
  drawSlotCount(tft, note, color);
}

static void flashSlotLimit(KCM_TFT &tft) { flashNote(tft, "MAX", TFT_YELLOW); }


/***************************************************************************************
   DRAW PRESET BUTTONS ROW
****************************************************************************************/
static void drawPresetButtons(KCM_TFT &tft) {
  tft.fillRect(0, TITLE_H, BACK_X, PRESET_H, TFT_BLACK);
  for (uint8_t i = 0; i < PRESET_COUNT; i++) {
    uint16_t bx = SEL_PAD + i * (PRESET_BTN_W + SEL_PAD);
    ButtonLabel btn;
    btn.text               = PRESETS[i].label;
    // Preset keys are momentary actions — achromatic like the rest of the chrome.
    btn.fontColorOff       = TFT_WHITE;
    btn.fontColorOn        = TFT_WHITE;
    btn.backgroundColorOff = TFT_BLACK;
    btn.backgroundColorOn  = TFT_BLACK;
    btn.borderColorOff     = TFT_GREY;
    btn.borderColorOn      = TFT_WHITE;
    drawButton(tft, bx, PRESET_Y, PRESET_BTN_W, PRESET_BTN_H, btn, &Roboto_Black_16, false);
  }
}


/***************************************************************************************
   DRAW ORDER PANEL -- right-side list showing selection order
****************************************************************************************/
static void drawOrderPanel(KCM_TFT &tft) {
  uint16_t listH = DFLT_Y - TOP_H - SEL_PAD * 2;
  tft.fillRect(PANEL_X, TOP_H, PANEL_W, listH + SEL_PAD, TFT_BLACK);

  tft.setFont(Roboto_Black_12);
  tft.setTextColor(TFT_GREY, TFT_BLACK);
  tft.setCursor(PANEL_X + 4, TOP_H + 4);
  tft.print("ORDER");

  uint16_t labelH = 18;
  uint16_t listY  = TOP_H + labelH + SEL_PAD;
  uint16_t availH = DFLT_Y - listY - SEL_PAD;
  uint16_t rowH   = availH / MAX_SLOTS;

  for (uint8_t i = 0; i < MAX_SLOTS; i++) {
    uint16_t ry     = listY + i * rowH;
    bool     filled = (i < slotCount && slots[i].type != RES_NONE);

    tft.fillRect(PANEL_X, ry, PANEL_W, rowH - 1, TFT_BLACK);
    tft.setFont(Roboto_Black_12);

    char numStr[4];
    snprintf(numStr, sizeof(numStr), "%d.", i + 1);
    tft.setTextColor(TFT_DARK_GREY, TFT_BLACK);
    tft.setCursor(PANEL_X + 2, ry + (rowH - 12) / 2);
    tft.print(numStr);

    if (filled) {
      // A selected resource the vessel does not carry keeps its place in the list
      // but shows dimmed: it draws no meter until the vessel has some.
      uint16_t col = resAbsent(slots[i].type) ? TFT_DARK_GREY : resColor(slots[i].type);
      tft.fillRect(PANEL_X + 22, ry + 1, PANEL_W - 24, rowH - 3, col);
      tft.setTextColor(TFT_BLACK, col);
      tft.setCursor(PANEL_X + 25, ry + (rowH - 12) / 2);
      tft.print(resLabel(slots[i].type));
    }
  }

  drawButton(tft, DFLT_X,  DFLT_Y,  DFLT_W,  DFLT_H,  btnDflt,  &Roboto_Black_16, layoutIsDefault());
  drawButton(tft, CLEAR_X, CLEAR_Y, CLEAR_W, CLEAR_H, btnClear, &Roboto_Black_16, false);
}


/***************************************************************************************
   DRAW STATIC CHROME -- selection screen
****************************************************************************************/
void drawStaticSelect(KCM_TFT &tft) {
  tft.fillScreen(TFT_BLACK);

  // Title — large font, vertically centred in title row
  tft.setFont(Roboto_Black_36);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(SEL_PAD, (TITLE_H - 43) / 2);
  tft.print("Select Resources");

  // Slot count — same line, right-aligned before BACK
  _selFlashUntil = 0;
  drawSlotCount(tft, nullptr, TFT_GREY);

  // BACK button — tall, spans both rows
  drawButton(tft, BACK_X, BACK_Y, BACK_W, BACK_H, btnBack, &Roboto_Black_20, false);

  // Preset buttons row
  drawPresetButtons(tft);

  // Divider between grid and right panel
  tft.fillRect(GRID_W + SEL_PAD, TOP_H, 2, KCM_SCREEN_H - TOP_H, TFT_DARK_GREY);

  // Resource grid
  for (uint8_t i = 0; i < RESOURCE_TYPE_COUNT; i++) {
    drawSelectButton(tft, i, isSelected(resTypeByIndex(i)));
  }

  // Right panel
  drawOrderPanel(tft);
}


/***************************************************************************************
   UPDATE PASS
****************************************************************************************/
void updateScreenSelect(KCM_TFT &tft) {
  // The only per-frame work is reverting a limit flash once it has been seen.
  if (_selFlashUntil != 0 && (int32_t)(millis() - _selFlashUntil) >= 0) {
    _selFlashUntil = 0;
    drawSlotCount(tft, nullptr, TFT_GREY);
  }
}


/***************************************************************************************
   HANDLE SELECTION TOUCH
****************************************************************************************/
bool handleSelectTouch(uint16_t x, uint16_t y) {
  // BACK button — spans both header rows
  if (x >= BACK_X && x < BACK_X + BACK_W && y >= BACK_Y && y < BACK_Y + BACK_H) {
    switchToScreen(screen_Main);
    return false;
  }

  // On EVA the selection is locked to the fixed EVA bar set — only BACK responds.
  // CLEAR, presets and grid taps are all ignored so nothing else can be added.
  if (evaActive) return false;

  // CLEAR button
  if (x >= CLEAR_X && x < CLEAR_X + CLEAR_W && y >= CLEAR_Y && y < CLEAR_Y + CLEAR_H) {
    for (uint8_t i = 0; i < MAX_SLOTS; i++) slots[i] = ResourceSlot();
    slotCount = 0;
    drawStaticSelect(infoDisp);
    return true;
  }

  // DFLT button: the current selection becomes the default for vessels not in
  // memory; an empty selection drops the stored default instead. Written to EEPROM
  // at once, since this is a deliberate act and a hitch here is invisible.
  if (x >= DFLT_X && x < DFLT_X + DFLT_W && y >= DFLT_Y && y < DFLT_Y + DFLT_H) {
    if (slotCount == 0) clearDefaultLayout();
    else                setDefaultLayout();
    persistStoreNow();
    drawButton(infoDisp, DFLT_X, DFLT_Y, DFLT_W, DFLT_H, btnDflt, &Roboto_Black_16, layoutIsDefault());
    flashNote(infoDisp, slotCount == 0 ? "DFLT CLR" : "DFLT SET", TFT_CYAN);
    if (debugMode) Serial.println(slotCount == 0 ? F("ResourceDisp: default layout cleared (SPCT)")
                                                 : F("ResourceDisp: default layout set"));
    return true;
  }

  // Preset buttons — full column height from top of screen to bottom of preset row,
  // left of BACK button. This gives a large easy-to-hit target for each preset.
  if (y >= PRESET_TOUCH_Y1 && y < PRESET_TOUCH_Y2 && x < BACK_X) {
    for (uint8_t i = 0; i < PRESET_COUNT; i++) {
      uint16_t bx = SEL_PAD + i * (PRESET_BTN_W + SEL_PAD);
      if (x >= bx && x < bx + PRESET_BTN_W) {
        loadPreset(i);
        drawStaticSelect(infoDisp);
        return true;
      }
    }
  }

  // Resource grid buttons
  if (y >= SEL_START_Y && x < GRID_W) {
    for (uint8_t i = 0; i < RESOURCE_TYPE_COUNT; i++) {
      uint8_t  col = i % SEL_COLS;
      uint8_t  row = i / SEL_COLS;
      uint16_t bx  = SEL_START_X + col * (SEL_BTN_W + SEL_PAD);
      uint16_t by  = SEL_START_Y + row * (SEL_BTN_H + SEL_PAD);

      if (x >= bx && x < bx + SEL_BTN_W && y >= by && y < by + SEL_BTN_H) {
        ResourceType t = resTypeByIndex(i);
        if (t == RES_NONE) return false;
        if (t == RES_EVA_PROP) return false;   // EVA Propellant only exists on EVA (grid locked then)
        if (resAbsent(t)) return false;        // inert: the vessel does not carry it

        bool wasSelected = isSelected(t);
        if (wasSelected) {
          removeResource(t);
        } else {
          if (!addResource(t)) {
            if (slotCount >= MAX_SLOTS) flashSlotLimit(infoDisp);
            return false;
          }
          requestResourceRefresh();   // so the new slot populates immediately
        }

        // Redraw this button only
        drawSelectButton(infoDisp, i, !wasSelected);

        // Refresh slot count in title row
        _selFlashUntil = 0;
        drawSlotCount(infoDisp, nullptr, TFT_GREY);

        // Refresh order panel (preset buttons untouched)
        drawOrderPanel(infoDisp);

        return true;
      }
    }
  }
  return false;
}




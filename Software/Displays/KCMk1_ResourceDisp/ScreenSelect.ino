/***************************************************************************************
   ScreenSelect.ino -- Resource selection screen for Kerbal Controller Mk1 Resource Display

   Layout (1024×600):
     Row 1 (TITLE_H):  "Select Resources" title (large) + slot count + BACK button
     Row 2 (PRESET_H): 6 preset group buttons across grid width, left of BACK
     Left panel (0..GRID_W-1):        5-column resource grid below header rows
     Right panel (GRID_W+pad..W-1):   selection order list + CLEAR button
     All backgrounds pure black. Geometry derives from KCM_SCREEN_W/H.
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

// Preset group buttons (row 2, across GRID_W only — BACK occupies right side)
static const uint8_t  PRESET_COUNT = 6;
static const uint16_t PRESET_BTN_W = (BACK_X - SEL_PAD * (PRESET_COUNT + 1)) / PRESET_COUNT;
static const uint16_t PRESET_BTN_H = PRESET_H - SEL_PAD * 2;
static const uint16_t PRESET_Y        = TITLE_H + SEL_PAD;
static const uint16_t PRESET_TOUCH_Y2 = TITLE_H + PRESET_H;  // bottom of preset touch zone
// Touch zone starts at y=0 so the full column above each button also registers
static const uint16_t PRESET_TOUCH_Y1 = 0;


/***************************************************************************************
   PRESET GROUP DEFINITIONS
****************************************************************************************/
struct PresetGroup {
  const char*  label;
  ResourceType types[MAX_SLOTS];
  uint8_t      count;
};

// Everything in a preset should be aboard the craft type it names; a resource the
// vessel turns out not to carry is skipped at load time (see loadPreset) and would
// draw no meter anyway. Nine or fewer keeps the standard meter class.
static const PresetGroup PRESETS[PRESET_COUNT] = {
  { "SPCT", {   // Spacecraft: stock launch-to-orbit set
      RES_ELEC_CHARGE, RES_LIQUID_FUEL, RES_LIQUID_OX, RES_MONO_PROP, RES_SOLID_FUEL,
      RES_LS_OXYGEN, RES_LS_FOOD, RES_LS_WATER, RES_ABLATOR
    }, 9 },
  { "XPD", {    // Expedition: nuclear / hydrogen deep-space craft
      RES_ELEC_CHARGE, RES_LIQUID_FUEL, RES_LIQUID_OX, RES_MONO_PROP,
      RES_LIQUID_H2, RES_ENRICHED_URANIUM, RES_LS_OXYGEN, RES_LS_FOOD, RES_LS_WATER
    }, 9 },
  { "SRF", {    // Surface: rover, lander, ISRU base
      RES_ELEC_CHARGE, RES_STORED_CHARGE, RES_ORE, RES_LIQUID_FUEL, RES_LIQUID_OX,
      RES_MONO_PROP, RES_LS_OXYGEN, RES_LS_FOOD, RES_LS_WATER
    }, 9 },
  { "LSP", {    // Life support: everything TAC-LS
      RES_ELEC_CHARGE, RES_LS_OXYGEN, RES_LS_CO2, RES_LS_FOOD,
      RES_LS_WASTE, RES_LS_WATER, RES_LS_LIQUID_WASTE, RES_FERTILIZER
    }, 8 },
  { "ACFT", {   // Aircraft: jets breathe air, no oxidizer. A spaceplane is SPCT plus Intake Air.
      RES_ELEC_CHARGE, RES_LIQUID_FUEL, RES_INTAKE_AIR,
      RES_MONO_PROP, RES_LS_OXYGEN, RES_LS_FOOD, RES_LS_WATER
    }, 7 },
  { "ADV", {   // Advanced Resource Group
      RES_ELEC_CHARGE, RES_STORED_CHARGE, RES_XENON, RES_ORE,
      RES_LIQUID_H2, RES_LIQUID_METHANE, RES_LITHIUM,
      RES_ENRICHED_URANIUM, RES_DEPLETED_URANIUM,
      RES_LS_OXYGEN, RES_LS_FOOD, RES_LS_WATER,
      RES_LS_CO2, RES_LS_WASTE, RES_LS_LIQUID_WASTE, RES_FERTILIZER
    }, 16 },
};


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
// Returns false when the MIN_SLOTS floor blocked the removal.
static bool removeResource(ResourceType t) {
  // Enforce MIN_SLOTS floor — don't remove if already at the minimum.
  // (CLEAR bypasses this intentionally by zeroing slotCount directly.)
  if (slotCount <= MIN_SLOTS) return false;
  for (uint8_t i = 0; i < slotCount; i++) {
    if (slots[i].type == t) {
      for (uint8_t j = i; j < slotCount - 1; j++) slots[j] = slots[j + 1];
      slots[slotCount - 1] = ResourceSlot();
      slotCount--;
      return true;
    }
  }
  return true;
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
   A tap the limits refuse (adding at MAX_SLOTS, removing at MIN_SLOTS) used to do
   nothing at all. It now flashes this count yellow for SEL_FLASH_MS with the limit
   named, so the pilot sees why the grid did not respond.
****************************************************************************************/
static const uint32_t SEL_FLASH_MS = 700;
static uint32_t _selFlashUntil = 0;   // millis() at which the flash reverts; 0 = idle
static bool     _selFlashMin   = false;

static void drawSlotCount(KCM_TFT &tft, bool flash) {
  char countStr[24];
  if (flash) snprintf(countStr, sizeof(countStr), "%d / %d  %s", slotCount, MAX_SLOTS, _selFlashMin ? "MIN" : "MAX");
  else       snprintf(countStr, sizeof(countStr), "%d / %d", slotCount, MAX_SLOTS);
  int16_t cw = getFontStringWidth(&Roboto_Black_16, countStr);
  uint16_t cx = BACK_X - cw - SEL_PAD * 2;
  uint16_t cy = (TITLE_H - 20) / 2;  // vertically centred in title row (font height ~20px)
  // Clear the widest string this cell can hold so a shorter one leaves no ghost.
  int16_t maxW = getFontStringWidth(&Roboto_Black_16, "16 / 16  MAX");
  tft.fillRect(BACK_X - maxW - SEL_PAD * 2 - 2, 0, maxW + 4, TITLE_H, TFT_BLACK);
  tft.setFont(Roboto_Black_16);
  tft.setTextColor(flash ? TFT_YELLOW : TFT_GREY, TFT_BLACK);
  tft.setCursor(cx, cy);
  tft.print(countStr);
}

static void flashSlotLimit(KCM_TFT &tft, bool atMin) {
  _selFlashMin   = atMin;
  _selFlashUntil = millis() + SEL_FLASH_MS;
  drawSlotCount(tft, true);
}


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
  uint16_t listH = CLEAR_Y - TOP_H - SEL_PAD * 2;
  tft.fillRect(PANEL_X, TOP_H, PANEL_W, listH + SEL_PAD, TFT_BLACK);

  tft.setFont(Roboto_Black_12);
  tft.setTextColor(TFT_GREY, TFT_BLACK);
  tft.setCursor(PANEL_X + 4, TOP_H + 4);
  tft.print("ORDER");

  uint16_t labelH = 18;
  uint16_t listY  = TOP_H + labelH + SEL_PAD;
  uint16_t availH = CLEAR_Y - listY - SEL_PAD;
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
  drawSlotCount(tft, false);

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
    drawSlotCount(tft, false);
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
          if (!removeResource(t)) { flashSlotLimit(infoDisp, true); return false; }
        } else {
          if (!addResource(t)) {
            if (slotCount >= MAX_SLOTS) flashSlotLimit(infoDisp, false);
            return false;
          }
          requestResourceRefresh();   // so the new slot populates immediately
        }

        // Redraw this button only
        drawSelectButton(infoDisp, i, !wasSelected);

        // Refresh slot count in title row
        _selFlashUntil = 0;
        drawSlotCount(infoDisp, false);

        // Refresh order panel (preset buttons untouched)
        drawOrderPanel(infoDisp);

        return true;
      }
    }
  }
  return false;
}




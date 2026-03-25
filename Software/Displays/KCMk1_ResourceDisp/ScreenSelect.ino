/***************************************************************************************
   ScreenSelect.ino -- Resource selection screen for Kerbal Controller Mk1 Resource Display

   Layout (800×480):
     Row 1 (TITLE_H):  "Select Resources" title (large) + slot count + BACK button
     Row 2 (PRESET_H): 6 preset group buttons across grid width, left of BACK
     Left panel (0..GRID_W-1):    5-column resource grid below header rows
     Right panel (GRID_W+pad..799): selection order list + CLEAR button
     All backgrounds pure black.
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
static const uint16_t GRID_W     = 600;
static const uint16_t SEL_BTN_W  = (GRID_W - SEL_PAD * (SEL_COLS + 1)) / SEL_COLS;
static const uint16_t SEL_BTN_H  = (480 - TOP_H - SEL_PAD * (SEL_ROWS + 1)) / SEL_ROWS;
static const uint16_t SEL_START_X = SEL_PAD;
static const uint16_t SEL_START_Y = TOP_H + SEL_PAD;

// Right panel (order list)
static const uint16_t PANEL_X    = GRID_W + SEL_PAD * 2;
static const uint16_t PANEL_W    = 800 - PANEL_X - SEL_PAD;

// BACK button — spans both title and preset rows for easy pressing
static const ButtonLabel btnBack = {
  "BACK", TFT_WHITE, TFT_WHITE, TFT_DARK_GREEN, TFT_GREEN, NO_BORDER, NO_BORDER
};
static const uint16_t BACK_W = 110;
static const uint16_t BACK_H = TITLE_H + PRESET_H - SEL_PAD * 2;
static const uint16_t BACK_X = 800 - BACK_W - SEL_PAD;
static const uint16_t BACK_Y = SEL_PAD;

// CLEAR button (bottom of right panel)
static const ButtonLabel btnClear = {
  "CLEAR", TFT_WHITE, TFT_WHITE, TFT_MAROON, TFT_RED, NO_BORDER, NO_BORDER
};
static const uint16_t CLEAR_H = 48;
static const uint16_t CLEAR_Y = 480 - CLEAR_H - SEL_PAD;
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

static const PresetGroup PRESETS[PRESET_COUNT] = {
  { "STD", {   // Standard Resource Group
      RES_ELEC_CHARGE, RES_LIQUID_FUEL, RES_LIQUID_OX, RES_MONO_PROP,
      RES_SOLID_FUEL, RES_LS_OXYGEN, RES_LS_FOOD, RES_LS_WATER
    }, 8 },
  { "XPD", {   // Expedition Craft Resource Group
      RES_ELEC_CHARGE, RES_LIQUID_FUEL, RES_LIQUID_OX, RES_MONO_PROP,
      RES_LIQUID_H2, RES_ENRICHED_URANIUM, RES_LS_OXYGEN, RES_LS_FOOD, RES_LS_WATER
    }, 9 },
  { "VEH", {   // Vehicle Resource Group
      RES_ELEC_CHARGE, RES_STORED_CHARGE, RES_LIQUID_FUEL, RES_LIQUID_OX,
      RES_MONO_PROP, RES_SOLID_FUEL, RES_XENON, RES_ORE, RES_ABLATOR,
      RES_LIQUID_H2, RES_LIQUID_METHANE, RES_LITHIUM,
      RES_ENRICHED_URANIUM, RES_DEPLETED_URANIUM, RES_FERTILIZER
    }, 15 },
  { "LSP", {   // Life Support Resource Group
      RES_ELEC_CHARGE, RES_LS_OXYGEN, RES_LS_CO2, RES_LS_FOOD,
      RES_LS_WASTE, RES_LS_WATER, RES_LS_LIQUID_WASTE, RES_FERTILIZER
    }, 8 },
  { "AIR", {   // Aircraft Resource Group
      RES_ELEC_CHARGE, RES_LIQUID_FUEL, RES_LIQUID_OX, RES_INTAKE_AIR,
      RES_MONO_PROP, RES_LS_OXYGEN, RES_LS_FOOD, RES_LS_WATER
    }, 8 },
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
  if (slotCount >= MAX_SLOTS) return false;
  slots[slotCount].type         = t;
  // In live mode start at zero — Simpit will populate on next message.
  // In demo mode start at 1.0 so bars are visible immediately.
  slots[slotCount].current      = demoMode ? 1.0f : 0.0f;
  slots[slotCount].maxVal       = demoMode ? 1.0f : 0.0f;
  slots[slotCount].stageCurrent = demoMode ? 0.4f : 0.0f;
  slots[slotCount].stageMax     = demoMode ? 0.4f : 0.0f;
  slotCount++;
  return true;
}


/***************************************************************************************
   HELPER -- remove a resource from slots[], compact the array.
****************************************************************************************/
static void removeResource(ResourceType t) {
  // Enforce MIN_SLOTS floor — don't remove if already at the minimum.
  // (CLEAR bypasses this intentionally by zeroing slotCount directly.)
  if (slotCount <= MIN_SLOTS) return;
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
    slots[slotCount].type         = pg.types[i];
    slots[slotCount].current      = demoMode ? 1.0f : 0.0f;
    slots[slotCount].maxVal       = demoMode ? 1.0f : 0.0f;
    slots[slotCount].stageCurrent = demoMode ? 0.4f : 0.0f;
    slots[slotCount].stageMax     = demoMode ? 0.4f : 0.0f;
    slotCount++;
  }
  // In live mode, request a Simpit refresh so the new slots populate immediately
  if (!demoMode) simpit.requestMessageOnChannel(0);
}


/***************************************************************************************
   DRAW ONE GRID BUTTON
****************************************************************************************/
static void drawSelectButton(RA8875 &tft, uint8_t gridIndex, bool isOn) {
  ResourceType t = resTypeByIndex(gridIndex);
  if (t == RES_NONE) return;

  uint8_t  col = gridIndex % SEL_COLS;
  uint8_t  row = gridIndex / SEL_COLS;
  uint16_t x   = SEL_START_X + col * (SEL_BTN_W + SEL_PAD);
  uint16_t y   = SEL_START_Y + row * (SEL_BTN_H + SEL_PAD);

  ButtonLabel btn;
  btn.text               = resFullName(t);
  btn.fontColorOff       = TFT_DARK_GREY;
  btn.fontColorOn        = TFT_BLACK;
  btn.backgroundColorOff = TFT_OFF_BLACK;
  btn.backgroundColorOn  = resColor(t);
  btn.borderColorOff     = TFT_GREY;
  btn.borderColorOn      = TFT_WHITE;

  drawButton(tft, x, y, SEL_BTN_W, SEL_BTN_H, btn, &Roboto_Black_20, isOn);
}


/***************************************************************************************
   DRAW SLOT COUNT -- small text in title row, left side
****************************************************************************************/
static void drawSlotCount(RA8875 &tft) {
  char countStr[20];
  snprintf(countStr, sizeof(countStr), "%d / %d", slotCount, MAX_SLOTS);
  int16_t cw = getFontStringWidth(&Roboto_Black_16, countStr);
  uint16_t cx = BACK_X - cw - SEL_PAD * 2;
  uint16_t cy = (TITLE_H - 20) / 2;  // vertically centred in title row (font height ~20px)
  tft.fillRect(cx - 2, 0, cw + 4, TITLE_H, TFT_BLACK);
  tft.setFont(&Roboto_Black_16);
  tft.setTextColor(TFT_GREY, TFT_BLACK);
  tft.setCursor(cx, cy);
  tft.print(countStr);
}


/***************************************************************************************
   DRAW PRESET BUTTONS ROW
****************************************************************************************/
static void drawPresetButtons(RA8875 &tft) {
  tft.fillRect(0, TITLE_H, BACK_X, PRESET_H, TFT_BLACK);
  for (uint8_t i = 0; i < PRESET_COUNT; i++) {
    uint16_t bx = SEL_PAD + i * (PRESET_BTN_W + SEL_PAD);
    ButtonLabel btn;
    btn.text               = PRESETS[i].label;
    btn.fontColorOff       = TFT_WHITE;
    btn.fontColorOn        = TFT_WHITE;
    btn.backgroundColorOff = TFT_NAVY;
    btn.backgroundColorOn  = TFT_ROYAL;
    btn.borderColorOff     = TFT_GREY;
    btn.borderColorOn      = TFT_WHITE;
    drawButton(tft, bx, PRESET_Y, PRESET_BTN_W, PRESET_BTN_H, btn, &Roboto_Black_16, false);
  }
}


/***************************************************************************************
   DRAW ORDER PANEL -- right-side list showing selection order
****************************************************************************************/
static void drawOrderPanel(RA8875 &tft) {
  uint16_t listH = CLEAR_Y - TOP_H - SEL_PAD * 2;
  tft.fillRect(PANEL_X, TOP_H, PANEL_W, listH + SEL_PAD, TFT_BLACK);

  tft.setFont(&Roboto_Black_12);
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
    tft.setFont(&Roboto_Black_12);

    char numStr[4];
    snprintf(numStr, sizeof(numStr), "%d.", i + 1);
    tft.setTextColor(TFT_DARK_GREY, TFT_BLACK);
    tft.setCursor(PANEL_X + 2, ry + (rowH - 12) / 2);
    tft.print(numStr);

    if (filled) {
      uint16_t col = resColor(slots[i].type);
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
void drawStaticSelect(RA8875 &tft) {
  tft.fillScreen(TFT_BLACK);

  // Title — large font, vertically centred in title row
  tft.setFont(&Roboto_Black_36);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(SEL_PAD, (TITLE_H - 43) / 2);
  tft.print("Select Resources");

  // Slot count — same line, right-aligned before BACK
  drawSlotCount(tft);

  // BACK button — tall, spans both rows
  drawButton(tft, BACK_X, BACK_Y, BACK_W, BACK_H, btnBack, &Roboto_Black_20, false);

  // Preset buttons row
  drawPresetButtons(tft);

  // Divider between grid and right panel
  tft.fillRect(GRID_W + SEL_PAD, TOP_H, 2, 480 - TOP_H, TFT_DARK_GREY);

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
void updateScreenSelect(RA8875 &tft) {
  // No per-frame updates on select screen — all changes are touch-driven redraws.
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

        bool wasSelected = isSelected(t);
        if (wasSelected) {
          removeResource(t);
        } else {
          if (!addResource(t)) return false;
          // In live mode, request refresh so the new slot populates immediately
          if (!demoMode) simpit.requestMessageOnChannel(0);
        }

        // Redraw this button only
        drawSelectButton(infoDisp, i, !wasSelected);

        // Refresh slot count in title row
        drawSlotCount(infoDisp);

        // Refresh order panel (preset buttons untouched)
        drawOrderPanel(infoDisp);

        return true;
      }
    }
  }
  return false;
}




/***************************************************************************************
   ScreenSOI.ino -- SOI screen for Kerbal Controller Mk1 Annunciator
   Shows celestial body data with left KASA meatball and right body BMP.
****************************************************************************************/
#include "KCMk1_Annunciator.h"

// PrintState instances for KDC v2 printDisp() — one per row in drawSOIBody()
static const uint8_t SOI_MAX_ROWS = 8;  // max rows: 8 for atmospheric bodies
PrintState psSOIRows[SOI_MAX_ROWS];



/***************************************************************************************
   LAYOUT CONSTANTS -- SOI SCREEN
****************************************************************************************/
const uint16_t SOI_IMG_W  = 240;
const uint16_t SOI_IMG_H  = 168;
const uint16_t SOI_NAME_X = SOI_IMG_W;
const uint16_t SOI_NAME_W = 800 - 2 * SOI_IMG_W;  // 320px centre panel
const uint16_t SOI_ROWS_Y = SOI_IMG_H;
const uint16_t SOI_ROWS_H = 480 - SOI_IMG_H;       // 312px for data rows
const uint16_t SOI_ROW_H  = 36;                    // 36px per row -- fits 8-9 rows


/***************************************************************************************
   STATIC CHROME -- draw once on screen entry
   Draws background and left KASA meatball BMP.
   drawSOIBody() is called immediately after to fill body-specific content.
****************************************************************************************/
void drawStaticSOI(RA8875 &tft) {
  tft.setXY(0, 0);
  tft.fillScreen(TFT_BLACK);
  drawBMP(tft, "/KASA_Meatball_240x168.bmp", 2, 2);
}


/***************************************************************************************
   SOI BODY CONTENT UPDATE
   Redraws all body-specific content: name, data rows, right BMP.
   Left meatball is NOT touched -- drawn once in drawStaticSOI.
   Called whenever currentBody changes while screen_SOI is active.
****************************************************************************************/
void drawSOIBody(RA8875 &tft) {
  struct SOIRow { const char *label; String value; };
  SOIRow rows[8];
  uint8_t rowCount = 0;

  rows[rowCount++] = { "Min Safe Alt:",   formatAlt(currentBody.minSafe)                   };
  rows[rowCount++] = { "SOI Radius:",     formatAlt(currentBody.soiAlt)                    };
  if (currentBody.hasAtmo) {
    rows[rowCount++] = { "Reentry Alt:",  formatAlt(currentBody.reentryAlt)                };
    rows[rowCount++] = { "High Atmo Alt:", formatAlt(currentBody.flyHigh)                  };
    rows[rowCount++] = { "Low Space Alt:", formatAlt(currentBody.lowSpace)                 };
  }
  rows[rowCount++] = { "High Space Alt:", formatAlt(currentBody.highSpace)                 };
  rows[rowCount++] = { "Condition:",      String(currentBody.cond)                         };
  rows[rowCount++] = { "Surf. Gravity:",  String(currentBody.gravity, 2) + " m/s\xb2"     };

  tft.fillRect(SOI_NAME_X, 0, SOI_NAME_W, SOI_IMG_H, TFT_BLACK);
  printTitle(tft, &Roboto_Black_48, SOI_NAME_X, 0, SOI_NAME_W, SOI_IMG_H,
             currentBody.dispName, TFT_WHITE, TFT_BLACK, NO_BORDER);

  tft.fillRect(0, SOI_ROWS_Y, SOI_NAME_X + SOI_NAME_W + SOI_IMG_W, SOI_ROWS_H, TFT_BLACK);
  for (uint8_t i = 0; i < rowCount; i++) {
    uint16_t y = SOI_ROWS_Y + i * SOI_ROW_H;
    printDisp(tft, &Roboto_Black_28, 0, y, SOI_NAME_X + SOI_NAME_W + SOI_IMG_W, SOI_ROW_H,
              rows[i].label, rows[i].value,
              TFT_WHITE, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, NO_BORDER,
              psSOIRows[i]);
  }

  // Right BMP drawn last -- sits on top of any overlapping content
  drawBMP(tft, currentBody.image, SOI_NAME_X + SOI_NAME_W + 2, 2);
}


/***************************************************************************************
   UPDATE PASS -- redraws body content only when gameSOI has changed.
   prev.gameSOI is synced at screen entry to suppress a redundant redraw.
****************************************************************************************/
void updateScreenSOI(RA8875 &tft) {
  if (state.gameSOI != prev.gameSOI) {
    drawSOIBody(tft);
    prev.gameSOI = state.gameSOI;
  }
}

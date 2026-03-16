/***************************************************************************************
   ScreenStandby.ino -- Standby screen for Kerbal Controller Mk1 Annunciator
   Full-screen splash BMP -- no dynamic content.
****************************************************************************************/
#include "KCMk1_Annunciator.h"



/***************************************************************************************
   STATIC CHROME -- draw once on screen entry
****************************************************************************************/
void drawStaticStandby(RA8875 &tft) {
  tft.setXY(0, 0);
  tft.fillScreen(TFT_BLACK);
  drawBMP(tft, "/StandbySplash_800x480.bmp", 0, 0);
}


/***************************************************************************************
   UPDATE PASS -- nothing to update on standby
****************************************************************************************/
void updateScreenStandby(RA8875 &tft) {
  (void)tft;
}

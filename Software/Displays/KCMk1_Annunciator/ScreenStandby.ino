/***************************************************************************************
   ScreenStandby.ino -- Standby screen for Kerbal Controller Mk1 Annunciator
   Full-screen splash BMP -- no dynamic content.
****************************************************************************************/
#include "KCMk1_Annunciator.h"



/***************************************************************************************
   STATIC CHROME -- draw once on screen entry
****************************************************************************************/
void drawStaticStandby(KCM_TFT &tft) {
  drawStandbySplash(tft);   // #5A delegates to KDC library
}


/***************************************************************************************
   UPDATE PASS -- nothing to update on standby
****************************************************************************************/
void updateScreenStandby(KCM_TFT &tft) {
  (void)tft;
}

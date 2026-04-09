/***************************************************************************************
   ScreenStandby.ino -- Standby screen for Kerbal Controller Mk1 Resource Display
   Full-screen BMP splash — identical to the Annunciator standby screen.
   Shown when not in a KSP flight scene, or on first power-on before Simpit connects.
   In demoMode, the main screen is shown immediately and standby is bypassed.
   Triggered by SCENE_CHANGE_MESSAGE (leaving flight) in SimpitHandler.
   Exited by SCENE_CHANGE_MESSAGE (entering flight) → switchToScreen(screen_Main).
   Requires /StandbySplash_800x480.bmp on the SD card.
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


void drawStaticStandby(RA8875 &tft) {
  drawStandbySplash(tft);   // #5A delegates to KDC library
}


void updateScreenStandby(RA8875 &tft) {
  (void)tft;  // nothing to update — standby is purely static
}

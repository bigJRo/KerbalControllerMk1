/***************************************************************************************
   AAA_Globals.ino -- Variable definitions for Kerbal Controller Mk1 Information Display
   All global variable instances are owned here.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


/***************************************************************************************
   DISPLAY AND TOUCH
****************************************************************************************/
RA8875      infoDisp  = RA8875(RA8875_CS, RA8875_RESET);
TouchResult lastTouch;


/***************************************************************************************
   SCREEN STATE
****************************************************************************************/
ScreenType activeScreen = screen_LNCH;
ScreenType prevScreen   = screen_COUNT;  // sentinel -- forces chrome on first loop


/***************************************************************************************
   CELESTIAL BODY (derived from gameSOI — not stored in AppState)
   Updated whenever state.gameSOI changes. Access radius via currentBody.radius.
   Initialised to Kerbin so screens work before first Simpit or demo SOI update.
****************************************************************************************/
BodyParams currentBody;


/***************************************************************************************
   APPLICATION STATE
****************************************************************************************/
AppState state;


/***************************************************************************************
   SWITCH TO SCREEN
   Sets activeScreen and forces a full chrome redraw on the next loop pass by
   resetting prevScreen to the sentinel value screen_COUNT.
   Always use this function — never set activeScreen directly.
****************************************************************************************/
void switchToScreen(ScreenType s) {
  activeScreen = s;
  prevScreen   = screen_COUNT;
}

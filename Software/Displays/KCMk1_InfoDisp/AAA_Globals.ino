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
   SIMPIT OBJECT
   Moved here from SimpitHandler.ino (#9) — owned in globals alongside other
   shared objects. SerialUSB1 is the second USB COM port, dedicated to KSP telemetry.
****************************************************************************************/
KerbalSimpit simpit(SerialUSB1);


/***************************************************************************************
   SCREEN STATE
****************************************************************************************/
ScreenType activeScreen = screen_LNCH;
ScreenType prevScreen   = screen_COUNT;  // sentinel -- forces chrome on first loop


/***************************************************************************************
   SCREEN TRANSITION TIMESTAMP  (#8)
****************************************************************************************/
uint32_t lastScreenSwitch = 0;


/***************************************************************************************
   FLIGHT STATE
****************************************************************************************/
bool simpitConnected = false;  // true after Simpit handshake succeeds
bool _pendingContextSwitch = false;  // set on vessel change; cleared when FLIGHT_STATUS arrives
bool _pendingDockCheck     = false;  // set after context switch; cleared when TARGETINFO arrives
bool flightScene     = false;  // true when KSP is in a flight scene
bool idleState       = false;  // true when master wants standby when not in flight


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
  // Reset ORB advanced mode when navigating away — always return to Apsides default
  if (s != screen_ORB && _orbAdvancedMode) {
    _orbAdvancedMode = false;
  }
  activeScreen     = s;
  prevScreen       = screen_COUNT;
  lastScreenSwitch = millis();   // #8 record timestamp for touch debounce and diagnostics
}


/***************************************************************************************
   CONTEXT SCREEN SELECTION
   Returns the most operationally relevant screen for the current vessel state.
   Called on VESSEL_CHANGE_MESSAGE and on entering a flight scene.

   Priority (highest to lowest):
     1. Plane type                             → ACFT
     2. Pre-launch or Landed situation         → LNCH  (any vessel type on the ground)
     3. Lander type (airborne or sub-orbital)  → LNDG (powered descent)
     4. Anything else (orbit, flight…)         → APSI
****************************************************************************************/
ScreenType contextScreen() {
  // 1. Plane always goes to aircraft screen regardless of situation
  if (state.vesselType == type_Plane)
    return screen_ACFT;

  // 1b. Rover always goes to rover screen regardless of situation
  if (state.vesselType == type_Rover)
    return screen_MISC;

  // 2. Any vessel on the ground → launch screen
  if ((state.situation & sit_PreLaunch) || (state.situation & sit_Landed))
    return screen_LNCH;

  // 3. Lander type in flight → powered descent
  if (state.vesselType == type_Lander) {
    _lndgReentryMode = false;
    return screen_LNDG;
  }

  // 4. Target within docking range → docking screen
  // 4. Target within docking range → docking screen
  // Use tgtDistance alone — KSP may report targetAvailable=false even while
  // actively sending TARGETINFO with a valid distance (observed in KSP1).
  if (state.tgtDistance > 0.0f && state.tgtDistance <= DOCK_DIST_WARN_M)
    return screen_DOCK;

  // 5. Recoverable vessel (debris, probe, etc. that can be recovered) → Vehicle Info
  if (state.isRecoverable)
    return screen_VEH;

  // 6. Everything else (orbit, sub-orbital flight, splashed, unknown)
  return screen_ORB;
}


/***************************************************************************************
   STANDBY SCREEN
   Shown when not in a flight scene (menus, tracking station, etc.).
   Displays the shared splash BMP used by all KCMk1 panels.
****************************************************************************************/
void drawStandbyScreen(RA8875 &tft) {
  drawStandbySplash(tft);   // #5A delegates to KDC library (subsumes #11 setXY fix)
}

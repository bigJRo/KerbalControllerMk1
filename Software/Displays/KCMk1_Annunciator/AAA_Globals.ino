/***************************************************************************************
   AAA_Globals.ino -- Variable definitions for Kerbal Controller Mk1 Annunciator
   All global variable instances are owned here. Types and structs are declared
   in KCMk1_Annunciator.h; extern declarations there make them visible to all tabs.
****************************************************************************************/

#include "KCMk1_Annunciator.h"


/***************************************************************************************
   DISPLAY AND TOUCH
****************************************************************************************/
RA8875       infoDisp  = RA8875(RA8875_CS, RA8875_RESET);
TouchResult  lastTouch;


/***************************************************************************************
   SIMPIT OBJECT
   Uses SerialUSB1 for Simpit traffic, leaving Serial free for debug output.
****************************************************************************************/
KerbalSimpit simpit(SerialUSB1);


/***************************************************************************************
   APPLICATION STATE -- current and previous (dirty detection)
****************************************************************************************/
AppState state;
AppState prev;


/***************************************************************************************
   TELEMETRY LOGIC FLAGS
   Derived from Simpit messages. Drive C&W logic and display behaviour.
   Not part of AppState -- not dirty-tracked, not drawn directly.
****************************************************************************************/
bool    inFlight      = false;
bool    inEVA         = false;
bool    hasTarget     = false;
bool    flightScene   = false;
bool    docked        = false;
bool    isRecoverable = false;
bool    hasO2         = false;
bool    inAtmo        = false;
bool    physTW        = false;   // true when time warp is physics warp
bool    simpitConnected = false; // true after Simpit handshake succeeds
bool    idleState       = false; // true when master wants standby screen when not in flight
uint8_t rawSituation  = 0;       // raw Simpit vesselSituation bitmask -- preserves sit_Landed


/***************************************************************************************
   CELESTIAL BODY (derived from gameSOI -- not in AppState)
****************************************************************************************/
BodyParams currentBody;


/***************************************************************************************
   SCREEN STATE
****************************************************************************************/
ScreenType activeScreen     = screen_Standby;
ScreenType prevScreen       = screen_COUNT;
uint32_t   lastScreenSwitch = 0;
bool       firstPassOnMain  = false;


/***************************************************************************************
   INVALIDATE ALL STATE
   Resets prev to a sentinel that differs from state on every field,
   forcing a full redraw on the next update pass.
****************************************************************************************/
void invalidateAllState() {
  prev.masterAlarmOn        = !state.masterAlarmOn;
  prev.cautionWarningState  = ~state.cautionWarningState;
  prev.vesselSituationState = ~state.vesselSituationState;
  prev.vehCtrlMode          = (CtrlMode)0xFF;
  prev.vesselType           = (VesselType)0xFF;
  prev.vesselName           = "\x01";
  prev.maxTemp              = state.maxTemp   + 1;
  prev.crewCount            = state.crewCount + 1;
  prev.twIndex              = state.twIndex   + 1;
  prev.commNet              = state.commNet   + 1;
  prev.stage                = state.stage     + 1;
  prev.skinTemp             = state.skinTemp  + 1;
  prev.ctrlGrp              = state.ctrlGrp   + 1;
  prev.gameSOI              = "\x01";
  prev.gear_on              = !state.gear_on;
  prev.brakes_on            = !state.brakes_on;
  prev.lights_on            = !state.lights_on;
  prev.RCS_on               = !state.RCS_on;
  prev.SAS_on               = !state.SAS_on;
  prev.abort_on             = !state.abort_on;
  prev.alt_sl               = -1.0f;
  prev.alt_surf             = -1.0f;
  prev.vel_surf             = -1.0f;
  prev.vel_vert             = -1.0f;
  prev.apoapsis             = -1.0f;
  prev.stageDV              = -1.0f;
  prev.stageBurnTime        = -1.0f;
  prev.gForces              = -1.0f;
  prev.EC                   = -1.0f;
  prev.EC_total             = -1.0f;
  prev.throttleCmd          = state.throttleCmd + 1;
}


/***************************************************************************************
   SWITCH TO SCREEN
   Central helper -- always use this instead of setting activeScreen directly.
   Ensures invalidateAllState(), prevScreen reset, and timestamp are never forgotten.
   Does NOT silence audio or call resetDisplays() -- callers handle those if needed.
****************************************************************************************/
void switchToScreen(ScreenType s) {
  activeScreen     = s;
  prevScreen       = screen_COUNT;
  lastScreenSwitch = millis();
  invalidateAllState();
}


/***************************************************************************************
   RESET DISPLAYS
   Called on scene exit or vessel switch. Resets per-flight boolean flags to false,
   calls invalidateAllState() to force a full redraw on the next update pass, and
   sets prevScreen = screen_COUNT to trigger the chrome transition block.

   state (AppState) and currentBody are intentionally NOT reset. Most Simpit channels
   are event-driven and only send when values change -- after a vessel switch, fields
   that haven't changed (same SOI, same crew count, etc.) will not resend. Preserving
   the last known values means the display shows something sensible immediately; fields
   update normally as Simpit delivers data for the new vessel.

   flightScene is also NOT reset here. The caller (SCENE_CHANGE_MESSAGE handler) sets
   flightScene before calling resetDisplays(), so the value is already correct.
   Resetting it here would undo the caller's work.
****************************************************************************************/
void resetDisplays() {
  rawSituation  = 0;
  inFlight      = false;
  inEVA         = false;
  hasTarget     = false;
  docked        = false;
  isRecoverable = false;
  hasO2         = false;
  inAtmo        = false;

  invalidateAllState();
  prevScreen    = screen_COUNT;
}
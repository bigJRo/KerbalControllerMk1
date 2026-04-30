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
bool    inFlight        = false;
bool    inEVA           = false;
bool    hasTarget       = false;
bool    flightScene     = false;
bool    docked          = false;
bool    isRecoverable   = false;
bool    hasO2           = false;
bool    inAtmo          = false;
bool    physTW          = false;   // true when time warp is physics warp
bool    simpitConnected = false;   // true after Simpit handshake succeeds
bool    idleState       = false;   // true when master wants standby screen when not in flight
uint8_t rawSituation    = 0;       // raw Simpit vesselSituation bitmask -- preserves sit_Landed


/***************************************************************************************
   CHUTE ENVELOPE STATE
   Evaluated in CautionWarning.ino, read by ScreenMain.ino to choose button colour.
   Dirty-tracked separately from cautionWarningState because CW_CHUTE_ENV bit alone
   cannot encode four colour states -- prevChuteEnvState carries the previous colour
   so ScreenMain can detect changes even when the bit itself does not toggle.
****************************************************************************************/
ChuteEnvState chuteEnvState     = chute_Off;
ChuteEnvState prevChuteEnvState = chute_Off;


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
bool       alarmSilenced    = false; // true when crew has silenced active master alarm


/***************************************************************************************
   INVALIDATE ALL STATE
   Resets prev to a sentinel that differs from state on every field,
   forcing a full redraw on the next update pass.
   Also resets prevChuteEnvState to a sentinel so CHUTE_ENV redraws on first pass.
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
  prev.periapsis            = -1.0f;
  prev.stageDV              = -1.0f;
  prev.stageBurnTime        = -1.0f;
  prev.gForces              = -1.0f;
  prev.EC                   = -1.0f;
  prev.EC_total             = -1.0f;
  prev.LF_stage             = -1.0f;
  prev.LF_stage_tot         = -1.0f;
  prev.OX_stage             = -1.0f;
  prev.OX_stage_tot         = -1.0f;
  prev.SF_stage             = -1.0f;
  prev.SF_stage_tot         = -1.0f;
  prev.mono                 = -1.0f;
  prev.mono_tot             = -1.0f;
  prev.tacFood              = -1.0f;
  prev.tacFood_tot          = -1.0f;
  prev.tacWater             = -1.0f;
  prev.tacWater_tot         = -1.0f;
  prev.tacOxygen            = -1.0f;
  prev.tacOxygen_tot        = -1.0f;
  prev.tacCO2               = -1.0f;
  prev.tacCO2_tot           = -1.0f;
  prev.tacWaste             = -1.0f;
  prev.tacWaste_tot         = -1.0f;
  prev.tacWW                = -1.0f;
  prev.tacWW_tot            = -1.0f;
  prev.atmoPressure         = -1.0f;
  prev.atmoTemp             = -1.0f;
  prev.throttleCmd          = state.throttleCmd + 1;

  // Force CHUTE_ENV redraw on next update pass
  prevChuteEnvState = (chuteEnvState == chute_Off) ? chute_Red : chute_Off;
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

   chuteEnvState is reset to chute_Off on scene/vessel change since atmospheric
   state is not guaranteed to be valid until ATMO_CONDITIONS_MESSAGE arrives.
****************************************************************************************/
void resetDisplays() {
  rawSituation    = 0;
  inFlight        = false;
  inEVA           = false;
  hasTarget       = false;
  docked          = false;
  isRecoverable   = false;
  hasO2           = false;
  inAtmo          = false;
  chuteEnvState   = chute_Off;

  invalidateAllState();
  prevScreen = screen_COUNT;
}

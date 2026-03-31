/***************************************************************************************
   CautionWarning.ino -- C&W state logic for Kerbal Controller Mk1 Annunciator
   Contains updateCautionWarningState(), which recomputes state.cautionWarningState
   and state.masterAlarmOn from current telemetry on every relevant Simpit message
   and every demo frame.
   CW_* bit index constants are defined in KCMk1_Annunciator.h so that masterAlarmMask
   in AAA_Config.ino can reference them without a forward dependency.
   C&W panel drawing (colours, dirty-tracking) is in ScreenMain.ino.
****************************************************************************************/
#include "KCMk1_Annunciator.h"



/***************************************************************************************
   UPDATE CAUTION & WARNING STATE
   Recomputes state.cautionWarningState from current telemetry.
   Called after each Simpit message that affects C&W inputs, and from stepDemoState().
   Also updates state.masterAlarmOn from the result.
****************************************************************************************/
void updateCautionWarningState() {
  uint16_t cw = 0;

  // vesselSituationState display bitmask -- see FLIGHT_STATUS_MESSAGE in SimpitHandler.ino.
  // isAloft: true when flying, sub-orbital, orbital, or escaping. All altitude- and
  // atmosphere-based C&W conditions gate on this before applying further checks.
  bool isPreLaunch = state.vesselSituationState & (1 << VSIT_PRELAUNCH);
  bool isAloft     = state.vesselSituationState & ((1<<VSIT_FLIGHT)|(1<<VSIT_SUBORBIT)|(1<<VSIT_ORBIT)|(1<<VSIT_ESCAPE));

  // Bit 0/1 -- HIGH/LOW SPACE: aloft and above the atmosphere, mutually exclusive.
  bool aboveHighSpace = (isAloft && !inAtmo &&
                         currentBody.highSpace > 0 && state.alt_sl > currentBody.highSpace);
  if (aboveHighSpace)
    bitSet(cw, CW_HIGH_SPACE);
  else if (isAloft && !inAtmo)
    bitSet(cw, CW_LOW_SPACE);

  // Bit 2/3 -- FLYING HIGH/LOW: aloft and inside the atmosphere, mutually exclusive.
  bool aboveFlyHigh = (isAloft && inAtmo &&
                       currentBody.flyHigh > 0 && state.alt_sl > currentBody.flyHigh);
  if (aboveFlyHigh)
    bitSet(cw, CW_FLYING_HIGH);
  else if (isAloft && inAtmo)
    bitSet(cw, CW_FLYING_LOW);

  // Bit 4 -- ALT: low surface altitude while aloft
  if (isAloft && state.alt_surf < CW_ALT_THRESHOLD_M)
    bitSet(cw, CW_ALT);

  // Bit 5 -- DESCENT: descending while aloft
  if (isAloft && state.vel_vert < 0.0f)
    bitSet(cw, CW_DESCENT);

  // Bit 6 -- GROUND PROX: descending, gear up, <10s to impact
  if (isAloft && state.vel_vert < 0.0f && !state.gear_on &&
      state.alt_surf > 0.0f && (state.alt_surf / fabsf(state.vel_vert)) < CW_GROUND_PROX_S)
    bitSet(cw, CW_GROUND_PROX);

  // Bit 7 -- MECO: throttle at 0% -- only meaningful once airborne or in space,
  // and suppressed during pre-launch (throttle is 0 on the pad by definition).
  bool mecoNow = inFlight && !isPreLaunch && state.throttleCmd == 0;
  if (mecoNow) bitSet(cw, CW_MECO);

  // Track when MECO last cleared so LOW_DV can apply a hold-off window.
  static bool     _prevMeco      = false;
  static uint32_t _mecoClearedAt = 0;
  if (_prevMeco && !mecoNow) _mecoClearedAt = millis();
  _prevMeco = mecoNow;
  bool inMecoHoldOff = !mecoNow && (millis() - _mecoClearedAt) < LOW_DV_MECO_HOLDOFF_MS;

  // Bit 8 -- HIGH G
  if (state.gForces > CW_HIGH_G_ALARM || state.gForces < CW_HIGH_G_WARN)
    bitSet(cw, CW_HIGH_G);

  // Bit 9 -- BUS VOLTAGE: EC below 10%
  if (state.EC_total > 0.0f && (state.EC / state.EC_total) < CW_EC_LOW_FRAC)
    bitSet(cw, CW_BUS_VOLTAGE);

  // Bit 10 -- HIGH TEMP
  if (state.maxTemp > tempAlarm || state.skinTemp > tempAlarm)
    bitSet(cw, CW_HIGH_TEMP);

  // Bit 11 -- LOW DV (in flight only, suppressed during pre-launch, MECO, and
  // the brief hold-off window after MECO clears).
  // The hold-off prevents a false flash on throttle-up: stageBurnTime is a
  // KSP estimate derived from instantaneous thrust, which is unreliable for
  // ~1 second after ignition. stageDV = 0 is a legitimate warning condition
  // (empty stage) so there is no zero-guard here.
  if (inFlight && !isPreLaunch && !mecoNow && !inMecoHoldOff &&
      (state.stageDV < CW_LOW_DV_MS || state.stageBurnTime < CW_LOW_BURN_S))
    bitSet(cw, CW_LOW_DV);

  // Bit 12 -- WARP
  if (state.twIndex > 0)
    bitSet(cw, CW_WARP);

  // Bit 13 -- ATMO
  if (inAtmo)
    bitSet(cw, CW_ATMO);

  // Bit 14 -- O2 PRESENT: only meaningful when in atmosphere.
  // hasO2 may retain a stale true value after leaving atmosphere depending on
  // Simpit message timing, so gate on inAtmo to keep it accurate.
  if (inAtmo && hasO2)
    bitSet(cw, CW_O2_PRESENT);

  // Bit 15 -- CONTACT: landed or splashed per raw Simpit bits (not pre-launch)
  if ((rawSituation & sit_Landed) || (rawSituation & sit_Splashed))
    bitSet(cw, CW_CONTACT);

  state.cautionWarningState = cw;
  state.masterAlarmOn = (cw & masterAlarmMask) != 0;

  if (debugMode && cw != prev.cautionWarningState) {
    Serial.print(F("Annunciator: C&W 0x"));
    Serial.println(cw, HEX);
  }
}

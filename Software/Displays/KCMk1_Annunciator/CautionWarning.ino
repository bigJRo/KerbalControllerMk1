/***************************************************************************************
   CautionWarning.ino -- C&W state logic for Kerbal Controller Mk1 Annunciator
   Contains updateCautionWarningState(), which recomputes state.cautionWarningState,
   state.masterAlarmOn, and chuteEnvState from current telemetry on every relevant
   Simpit message and every demo frame.
   CW_* bit index constants are defined in KCMk1_Annunciator.h so that masterAlarmMask
   in AAA_Config.ino can reference them without a forward dependency.
   C&W panel drawing (colours, dirty-tracking) is in ScreenMain.ino.

   TWO-TIER INDICATORS
   CW_PE_LOW, CW_PROP_LOW, and CW_LIFE_SUPPORT each have yellow and red severity tiers
   sharing a single C&W bit. The convention is:
     - The C&W bit is set ONLY when the red (alarm) threshold is breached.
     - The yellow (caution) condition is tracked via a separate companion bool
       (peLowYellow, propLowYellow, lifeSupportYellow) also written into state here.
     - ScreenMain.ino reads the companion bool to choose yellow vs red button colour.
     - masterAlarmMask fires only when the C&W bit is set (red condition).
   This keeps the single-bit convention intact while supporting multi-colour buttons.

   CHUTE ENVELOPE
   CW_CHUTE_ENV uses four states (off/red/yellow/green) rather than standard on/off.
   The state is written to the global chuteEnvState and read by ScreenMain.ino.
   The CW bit itself is set whenever the chute env is not off (any active state),
   giving ScreenMain a single dirty-detect signal while chuteEnvState carries the colour.

   SRB ACTIVE
   Tracks solid fuel depletion using a static previous-reading variable.
   The indicator is ON when SF_stage is below 99% of total AND decreasing.
   It goes off when SF_stage drops below 0.5% (exhausted) or returns to >99% (new stage).

   ELEC GEN
   Tracks EC delta between successive calls using a static previous-reading variable.
   Positive delta = EC increasing = charging. Initialized to current EC on first call.
****************************************************************************************/
#include "KCMk1_Annunciator.h"
#include <cfloat>   // DBL_MAX -- used in CW_Ap_LOW and CW_ORBIT_STABLE soiAlt comparisons


/***************************************************************************************
   TWO-TIER COMPANION STATE
   Written here, read by ScreenMain.ino to choose button colour for two-tier indicators.
   Declared as globals so ScreenMain can extern them without additional header changes.
   peLowYellow    : true when Pe is in atmosphere but above reentry threshold (yellow)
   propLowYellow  : true when stage prop is below warn% but above alarm% (yellow)
   lsYellow       : true when any TAC-LS resource is in caution range but not alarm
****************************************************************************************/
bool peLowYellow     = false;
bool propLowYellow   = false;
bool lsYellow        = false;


/***************************************************************************************
   UPDATE CAUTION & WARNING STATE
   Recomputes state.cautionWarningState, chuteEnvState, and state.masterAlarmOn
   from current telemetry. Called after each Simpit message that affects C&W inputs
   and from stepDemoState().
****************************************************************************************/
void updateCautionWarningState() {
  uint32_t cw = 0;

  // Situation convenience flags
  // isAloft: true when flying, sub-orbital, orbital, or escaping.
  // All altitude- and atmosphere-based C&W conditions gate on isAloft.
  bool isPreLaunch = bitRead(state.vesselSituationState, VSIT_PRELAUNCH);
  bool isAloft     = bitRead(state.vesselSituationState, VSIT_FLIGHT)   ||
                     bitRead(state.vesselSituationState, VSIT_SUBORBIT)  ||
                     bitRead(state.vesselSituationState, VSIT_ORBIT)     ||
                     bitRead(state.vesselSituationState, VSIT_ESCAPE);
  bool isOrbit     = bitRead(state.vesselSituationState, VSIT_ORBIT);
  bool isSubOrbit  = bitRead(state.vesselSituationState, VSIT_SUBORBIT);

  // Throttle holdoff tracking -- suppresses LOW_DV immediately after throttle-up.
  // KSP stageBurnTime is computed from instantaneous thrust and is unreliable for
  // ~1 second after ignition. The holdoff prevents a false flash on throttle-up.
  bool throttleZero = inFlight && !isPreLaunch && state.throttleCmd == 0;
  static bool     _prevThrottleZero = false;
  static uint32_t _throttleUpAt     = 0;
  if (_prevThrottleZero && !throttleZero) _throttleUpAt = millis();
  _prevThrottleZero = throttleZero;
  bool inThrottleHoldOff = !throttleZero &&
                           (millis() - _throttleUpAt) < LOW_DV_MECO_HOLDOFF_MS;

  // ---------------------------------------------------------------------------------
  // ROW 0 -- WARNING tier (red, all trigger master alarm)
  // ---------------------------------------------------------------------------------

  // CW_LOW_DV: stage delta-v or burn time below threshold.
  // Suppressed during pre-launch, throttle-zero coast, and holdoff window.
  if (inFlight && !isPreLaunch && !throttleZero && !inThrottleHoldOff &&
      (state.stageDV < CW_LOW_DV_MS || state.stageBurnTime < CW_LOW_BURN_S))
    bitSet(cw, CW_LOW_DV);

  // CW_HIGH_G: g-forces outside safe envelope.
  // Thresholds from KCMk1_SystemConfig: CW_HIGH_G_ALARM (positive) and CW_HIGH_G_WARN (negative).
  if (state.gForces > CW_HIGH_G_ALARM || state.gForces < CW_HIGH_G_WARN)
    bitSet(cw, CW_HIGH_G);

  // CW_HIGH_TEMP: any part temperature exceeds alarm threshold.
  if (state.maxTemp > tempAlarm || state.skinTemp > tempAlarm)
    bitSet(cw, CW_HIGH_TEMP);

  // CW_BUS_VOLTAGE: electric charge below 5% of total capacity.
  // EC_total guard prevents false trigger when ARP mod is absent (EC_total stays 0).
  if (state.EC_total > 0.0f && (state.EC / state.EC_total) < CW_EC_LOW_FRAC)
    bitSet(cw, CW_BUS_VOLTAGE);

  // CW_ABORT: abort action group is active.
  if (state.abort_on)
    bitSet(cw, CW_ABORT);

  // ---------------------------------------------------------------------------------
  // ROW 1 -- Mixed: WARNING (red) and two-tier indicators + informational
  // ---------------------------------------------------------------------------------

  // CW_GROUND_PROX: airborne, descending, less than 10s to terrain impact.
  // Gear state deliberately NOT gated -- fires regardless of gear position.
  // A powered rocket landing with gear down at <10s to impact still warrants alarm.
  if (isAloft && state.vel_vert < 0.0f &&
      state.alt_surf > 0.0f &&
      (state.alt_surf / fabsf(state.vel_vert)) < CW_GROUND_PROX_S)
    bitSet(cw, CW_GROUND_PROX);

  // CW_PE_LOW: two-tier periapsis warning.
  // Logic depends on whether the body has an atmosphere:
  //   No atmosphere: single red tier when Pe < minSafe. No yellow tier.
  //   Has atmosphere: red when Pe < reentryAlt (committed reentry).
  //                   yellow when Pe >= reentryAlt AND Pe < max(minSafe, lowSpace).
  // currentBody.lowSpace is the atmosphere top in metres (0 on airless bodies).
  // currentBody.reentryAlt is the committed reentry threshold (0 on airless bodies).
  {
    peLowYellow = false;
    if (!currentBody.hasAtmo) {
      // Airless body -- single red tier only
      if (state.periapsis < currentBody.minSafe)
        bitSet(cw, CW_PE_LOW);
    } else {
      // Atmospheric body -- two tier
      float atmoUpperBound = max(currentBody.minSafe, currentBody.lowSpace);
      bool peCommitted = (state.periapsis < currentBody.reentryAlt);
      bool peInAtmo    = (state.periapsis < atmoUpperBound);
      if (peCommitted) {
        bitSet(cw, CW_PE_LOW);   // red tier
      } else if (peInAtmo) {
        peLowYellow = true;      // yellow tier -- aerobrake zone
      }
    }
  }

  // CW_PROP_LOW: two-tier stage propellant warning.
  // Monitors the lower of LF% and OX% on the current stage.
  // Red (alarm) : below CW_PROP_LOW_ALARM_FRAC (5%)
  // Yellow (caution): below CW_PROP_LOW_WARN_FRAC (20%) but above alarm
  // Suppressed when both stage totals are zero (no engines / no tanks on stage).
  {
    propLowYellow = false;
    bool hasProp = (state.LF_stage_tot > 0.0f || state.OX_stage_tot > 0.0f);
    if (hasProp) {
      float lfFrac = (state.LF_stage_tot > 0.0f)
                     ? (state.LF_stage / state.LF_stage_tot) : 1.0f;
      float oxFrac = (state.OX_stage_tot > 0.0f)
                     ? (state.OX_stage / state.OX_stage_tot) : 1.0f;
      float minFrac = min(lfFrac, oxFrac);
      if (minFrac < CW_PROP_LOW_ALARM_FRAC) {
        bitSet(cw, CW_PROP_LOW);   // red tier
      } else if (minFrac < CW_PROP_LOW_WARN_FRAC) {
        propLowYellow = true;      // yellow tier
      }
    }
  }

  // CW_LIFE_SUPPORT: two-tier TAC-LS resource warning.
  // Computes time remaining for each consumable and time to full for each waste
  // using fixed TAC-LS consumption rates multiplied by crew count.
  // Fires on worst-case condition across all six resources.
  // Suppressed when crewCount is 0 (uncrewed vessel, no TAC-LS consumption).
  // Note: uses double precision for rate * crew multiplication to avoid float
  //       precision loss on very small rates (e.g. FOOD_RATE = 1.69e-5).
  {
    lsYellow = false;
    bool lsAlarm = false;
    if (state.crewCount > 0) {
      double crew = (double)state.crewCount;

      // Consumables: time remaining = available / (rate * crew)
      double foodRate  = TACLS_FOOD_RATE  * crew;
      double waterRate = TACLS_WATER_RATE * crew;
      double o2Rate    = TACLS_OXYGEN_RATE * crew;

      float foodRemS  = (foodRate  > 0.0) ? (float)(state.tacFood  / foodRate)  : 1e9f;
      float waterRemS = (waterRate > 0.0) ? (float)(state.tacWater / waterRate) : 1e9f;
      float o2RemS    = (o2Rate    > 0.0) ? (float)(state.tacOxygen / o2Rate)   : 1e9f;

      // Waste resources: time to full = space remaining / (rate * crew)
      double co2Rate = TACLS_CO2_RATE       * crew;
      double wRate   = TACLS_WASTE_RATE     * crew;
      double wwRate  = TACLS_WASTEWATER_RATE * crew;

      float co2Space = (state.tacCO2_tot > 0.0f)
                       ? (state.tacCO2_tot - state.tacCO2) : 1e9f;
      float wSpace   = (state.tacWaste_tot > 0.0f)
                       ? (state.tacWaste_tot - state.tacWaste) : 1e9f;
      float wwSpace  = (state.tacWW_tot > 0.0f)
                       ? (state.tacWW_tot - state.tacWW) : 1e9f;

      float co2RemS  = (co2Rate > 0.0) ? (float)(co2Space / co2Rate) : 1e9f;
      float wRemS    = (wRate   > 0.0) ? (float)(wSpace   / wRate)   : 1e9f;
      float wwRemS   = (wwRate  > 0.0) ? (float)(wwSpace  / wwRate)  : 1e9f;

      // Check alarm thresholds (red tier)
      if (foodRemS  < TACLS_FOOD_ALARM_S)   lsAlarm = true;
      if (waterRemS < TACLS_WATER_ALARM_S)  lsAlarm = true;
      if (o2RemS    < TACLS_OXYGEN_ALARM_S) lsAlarm = true;
      if (co2RemS   < TACLS_OXYGEN_ALARM_S) lsAlarm = true;  // CO2 mirrors O2 window
      if (wRemS     < TACLS_OXYGEN_ALARM_S) lsAlarm = true;  // Waste mirrors O2 window
      if (wwRemS    < TACLS_OXYGEN_ALARM_S) lsAlarm = true;  // WasteWater mirrors O2 window

      // Check waste fill fraction thresholds
      if (state.tacCO2_tot > 0.0f &&
          (state.tacCO2 / state.tacCO2_tot) > TACLS_WASTE_ALARM_FRAC) lsAlarm = true;
      if (state.tacWaste_tot > 0.0f &&
          (state.tacWaste / state.tacWaste_tot) > TACLS_WASTE_ALARM_FRAC) lsAlarm = true;
      if (state.tacWW_tot > 0.0f &&
          (state.tacWW / state.tacWW_tot) > TACLS_WASTE_ALARM_FRAC) lsAlarm = true;

      if (lsAlarm) {
        bitSet(cw, CW_LIFE_SUPPORT);  // red tier
      } else {
        // Check caution thresholds (yellow tier)
        if (foodRemS  < TACLS_FOOD_WARN_S)   lsYellow = true;
        if (waterRemS < TACLS_WATER_WARN_S)  lsYellow = true;
        if (o2RemS    < TACLS_OXYGEN_WARN_S) lsYellow = true;
        if (co2RemS   < TACLS_OXYGEN_WARN_S) lsYellow = true;
        if (wRemS     < TACLS_OXYGEN_WARN_S) lsYellow = true;
        if (wwRemS    < TACLS_OXYGEN_WARN_S) lsYellow = true;
        if (state.tacCO2_tot > 0.0f &&
            (state.tacCO2 / state.tacCO2_tot) > TACLS_WASTE_WARN_FRAC) lsYellow = true;
        if (state.tacWaste_tot > 0.0f &&
            (state.tacWaste / state.tacWaste_tot) > TACLS_WASTE_WARN_FRAC) lsYellow = true;
        if (state.tacWW_tot > 0.0f &&
            (state.tacWW / state.tacWW_tot) > TACLS_WASTE_WARN_FRAC) lsYellow = true;
      }
    }
  }

  // CW_O2_PRESENT: atmosphere contains breathable oxygen.
  // Gated on inAtmo -- hasO2 may retain stale true after leaving atmosphere.
  if (inAtmo && hasO2)
    bitSet(cw, CW_O2_PRESENT);

  // ---------------------------------------------------------------------------------
  // ROW 2 -- CAUTION tier (yellow)
  // ---------------------------------------------------------------------------------

  // CW_IMPACT_IMM: airborne, descending, less than 60s to terrain impact.
  // No lower bound -- stays lit even when GROUND_PROX is also active (<10s).
  // The two indicators cascade and overlap intentionally, giving the crew
  // a persistent caution alongside the master alarm in the final seconds.
  if (isAloft && state.vel_vert < 0.0f && state.alt_surf > 0.0f) {
    float tImpact = state.alt_surf / fabsf(state.vel_vert);
    if (tImpact < CW_IMPACT_IMM_S)
      bitSet(cw, CW_IMPACT_IMM);
  }

  // CW_ALT: surface altitude below threshold while airborne.
  if (isAloft && state.alt_surf < CW_ALT_THRESHOLD_M)
    bitSet(cw, CW_ALT);

  // CW_DESCENT: vessel is airborne and descending.
  if (isAloft && state.vel_vert < 0.0f)
    bitSet(cw, CW_DESCENT);

  // CW_GEAR_UP: gear retracted below altitude threshold while airborne and descending.
  // Separate threshold from CW_ALT (both are 200m by default but independently tunable).
  if (isAloft && state.vel_vert < 0.0f &&
      state.alt_surf < CW_GEAR_UP_THRESHOLD_M && !state.gear_on)
    bitSet(cw, CW_GEAR_UP);

  // CW_ATMO: vessel is inside the atmosphere.
  if (inAtmo)
    bitSet(cw, CW_ATMO);

  // ---------------------------------------------------------------------------------
  // ROW 3 -- CAUTION tier (yellow)
  // ---------------------------------------------------------------------------------

  // CW_RCS_LOW: total vessel MonoPropellant below 15%.
  // Suppressed when mono_tot is zero (no RCS tanks / RCS not present on vessel).
  if (state.mono_tot > 0.0f &&
      (state.mono / state.mono_tot) < CW_RCS_LOW_FRAC)
    bitSet(cw, CW_RCS_LOW);

  // CW_PROP_IMBAL: stage LF/OX ratio deviates from nominal by more than tolerance.
  // Only fires when both LF and OX are present on the stage (mixed-propellant stages).
  // Ratio = LF_available / OX_available. Nominal = 0.8182 (9:11 by mass).
  // Suppressed when either tank total is zero (solid-only or mono-only stage).
  if (state.LF_stage_tot > 0.0f && state.OX_stage_tot > 0.0f &&
      state.OX_stage > 0.0f) {
    float ratio = state.LF_stage / state.OX_stage;
    float dev   = fabsf(ratio - CW_PROP_NOMINAL_RATIO) / CW_PROP_NOMINAL_RATIO;
    if (dev > CW_PROP_IMBAL_TOL)
      bitSet(cw, CW_PROP_IMBAL);
  }

  // CW_COMM_LOST: CommNet signal lost while in flight.
  // commNet is 0-100 signal strength percentage from flightStatusMessage.
  // Suppressed during pre-launch (pad has ground contact by definition).
  // Fires for both crewed and uncrewed vessels -- crew should always know
  // when comms are lost even if they can still fly manually.
  if (inFlight && !isPreLaunch && state.commNet == 0)
    bitSet(cw, CW_COMM_LOST);

  // CW_Ap_LOW: apoapsis inside atmosphere or below minSafe while in SUB-ORB or ORBIT.
  // threshold = lowSpace (atmosphere top) if body has atmosphere, else minSafe.
  // Suppressed on Kerbol (soiAlt == DBL_MAX) -- low Ap in solar orbit not actionable.
  {
    float apThreshold = currentBody.hasAtmo ? currentBody.lowSpace : currentBody.minSafe;
    if ((isSubOrbit || isOrbit) &&
        currentBody.soiAlt < DBL_MAX &&
        apThreshold > 0.0f &&
        state.apoapsis < apThreshold)
      bitSet(cw, CW_Ap_LOW);
  }

  // CW_HIGH_Q: dynamic pressure exceeds body-specific threshold.
  // Uses currentBody.highQThreshold (Pa). 0 means suppressed for this body.
  // Proxy: 0.5 * atmoPressure_Pa * vel_surf² approximates dynamic pressure (Pa).
  // atmoPressure from Simpit is in kPa so multiply by 1000 to convert to Pa.
  // Threshold is calibrated empirically per body in the BodyParams table.
  if (currentBody.highQThreshold > 0.0f && inAtmo && state.atmoPressure > 0.0f) {
    float dynPressProxy = 0.5f * (state.atmoPressure * 1000.0f) *
                          (state.vel_surf * state.vel_surf);
    if (dynPressProxy > currentBody.highQThreshold)
      bitSet(cw, CW_HIGH_Q);
  }

  // ---------------------------------------------------------------------------------
  // ROW 4 -- POSITIVE and STATE indicators
  // ---------------------------------------------------------------------------------

  // CW_ORBIT_STABLE: positive confirmation of a stable orbit.
  // Both Pe and Ap must be between the lower safe bound and soiAlt.
  // Lower bound = lowSpace (atmosphere top) for atmospheric bodies, else minSafe.
  // Upper bound = soiAlt (SOI radius). Always passes for Kerbol (soiAlt == DBL_MAX).
  {
    float lowerBound = currentBody.hasAtmo ? currentBody.lowSpace : currentBody.minSafe;
    if (isOrbit &&
        state.periapsis >= lowerBound &&
        state.apoapsis  >= lowerBound &&
        (double)state.apoapsis < currentBody.soiAlt)
      bitSet(cw, CW_ORBIT_STABLE);
  }

  // CW_ELEC_GEN: electric charge is actively increasing.
  // Compares current EC to the previous reading. A positive delta indicates generation
  // exceeds consumption. Uses a static to persist the previous value between calls.
  // Initialized to current EC on first call (delta = 0, indicator starts off).
  {
    static float _prevEC       = -1.0f;
    static bool  _initialized  = false;
    if (!_initialized) {
      _prevEC      = state.EC;
      _initialized = true;
    }
    if (state.EC > _prevEC + 0.01f)   // 0.01 unit hysteresis to suppress jitter
      bitSet(cw, CW_ELEC_GEN);
    _prevEC = state.EC;
  }

  // CW_CHUTE_ENV: chute deployment envelope state.
  // OFF    = not in atmosphere (indicator dark).
  // RED    = in atmosphere, airspeed above safe drogue deployment speed.
  // YELLOW = airspeed within drogue safe envelope, above main chute safe speed.
  // GREEN  = airspeed within main chute safe deployment envelope.
  // vel_surf is used as the airspeed proxy (surface velocity).
  // Thresholds in AAA_Config.ino are Kerbin defaults; tune per body as needed.
  {
    ChuteEnvState newChuteState = chute_Off;
    if (inAtmo) {
      if (state.vel_surf > CW_CHUTE_DROGUE_MAX_SPEED)
        newChuteState = chute_Red;
      else if (state.vel_surf > CW_CHUTE_MAIN_MAX_SPEED)
        newChuteState = chute_Yellow;
      else
        newChuteState = chute_Green;
    }
    chuteEnvState = newChuteState;
    if (newChuteState != chute_Off)
      bitSet(cw, CW_CHUTE_ENV);
  }

  // CW_SRB_ACTIVE: solid rocket booster currently burning.
  // ON  when SF_stage is below 99% of total AND decreasing from previous reading.
  // OFF when SF_stage drops below 0.5% (exhausted) or is above 99% (full/new stage).
  // Uses a static to persist the previous SF_stage value between calls.
  {
    static float _prevSF = -1.0f;
    bool srbBurning = false;
    if (state.SF_stage_tot > 0.0f) {
      float sfFrac = state.SF_stage / state.SF_stage_tot;
      if (sfFrac > 0.005f && sfFrac < 0.99f && state.SF_stage < _prevSF - 0.0001f)
        srbBurning = true;
    }
    if (srbBurning)
      bitSet(cw, CW_SRB_ACTIVE);
    _prevSF = state.SF_stage;
  }

  // CW_EVA_ACTIVE: a Kerbal is currently on EVA.
  // inEVA is set by FLIGHT_STATUS_MESSAGE in SimpitHandler.ino.
  if (inEVA)
    bitSet(cw, CW_EVA_ACTIVE);

  // ---------------------------------------------------------------------------------
  // COMMIT
  // ---------------------------------------------------------------------------------
  state.cautionWarningState = cw;
  state.masterAlarmOn = (cw & masterAlarmMask) != 0;

  if (debugMode && cw != prev.cautionWarningState) {
    Serial.print(F("Annunciator: C&W 0x"));
    Serial.println(cw, HEX);
    if (peLowYellow)   Serial.println(F("Annunciator: Pe LOW yellow"));
    if (propLowYellow) Serial.println(F("Annunciator: PROP LOW yellow"));
    if (lsYellow)      Serial.println(F("Annunciator: LIFE SUPPORT yellow"));
  }
}

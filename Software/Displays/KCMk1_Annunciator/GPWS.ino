/***************************************************************************************
   GPWS.ino -- Ground Proximity Warning System function for the KCMk1 Annunciator

   WHAT THIS IS
   An aviation-faithful GPWS + callout suite that runs entirely on the Annunciator's
   Teensy 4.1. It watches the Simpit telemetry in `state` (surface altitude ~ radio
   altitude, vertical speed, surface speed, gear, situation, roll, pitch, surface-
   velocity pitch, and target range) and speaks callouts through the DFPlayer Mini on
   the 7" board (Serial2). It is INDEPENDENT of the tone() master-alarm state machine.

   CONTROL MODEL (mode/flags relayed from the GPWS Input panel over I2C)
     OFF                      -> silent.
     GREEN (ACTIVE)           -> EVERYTHING: all warnings + altitude callouts + the
                                 minimums/approaching-minimums + RETARD + bug tone.
     AMBER + proxAlarm        -> callouts only: altitude callouts + minimums +
                                 approaching-minimums + RETARD + bug tone (no warnings).
     AMBER + rdvRadar         -> target-DISTANCE callouts + bug tone only.
   proxAlarm/rdvRadar are mutually-exclusive amber submodes. Three audio profiles:
     warningsOn   = ACTIVE
     altCallouts  = ACTIVE || (PROX && !rdvRadar)
     distCallouts =           (PROX &&  rdvRadar)

   WARNINGS (GREEN only) -- real-GPWS mode coverage
     Mode 1  SINK RATE (outer) + PULL UP (inner), floored descent-rate-vs-altitude
             envelopes (< 2450 ft).
     Mode 2  TERRAIN,TERRAIN -> PULL UP from smoothed terrain closure (d alt_surf/dt),
             then a post-warning TERRAIN tail while clearance keeps decreasing.
     Mode 3  DON'T SINK -- altitude loss after takeoff, proportional to height;
             spoken twice, then only as the loss deepens.
     Mode 4  TOO LOW GEAR (< 500 ft) + speed-expanded TOO LOW TERRAIN (4A, < 1000 ft)
             + minimum-terrain-clearance floor (4C, 75% of post-takeoff peak, gear up).
     Mode 6  altitude callouts (feet) + MINIMUMS + altitude-ramped BANK ANGLE.
   Plus non-GPWS extras (also GREEN, fixed-threshold approximations -- no per-craft
   Simpit source): STALL (AoA = pitch - surfaceVelocityPitch), and the takeoff-roll
   V1 / ROTATE speed callouts.
   Modes 5 (glideslope) and 7 (windshear) and forward-looking terrain are omitted --
   no ILS / wind / terrain-database data in KSP.

   CALLOUTS (altitude profiles) -- radio-altitude FEET values, spoken once per crossing:
     2500/1000/500/400/300/200/100/90/80/70/60/50/40/30/20/10/5 ft (Airbus-dense
     schedule near the ground). APPROACHING MINIMUMS at threshold+margin, MINIMUMS at
     the threshold. RETARD repeats in the flare (gear down, in atmo, very low).
   CALLOUTS (distance profile) -- target range in METRES: 500/200/100/50/40/30/20/10/5.
   Number clips are shared between the feet and metre ladders.

   THRESHOLD "BUG" (encoder value, 0-9999 m) -- crossing it plays the bug TONE clip:
   descending through it (altitude, where it is also the MINIMUMS decision height) or
   closing through it (range).

   CADENCE -- callouts/minimums/tone fire ONCE per crossing. PULL UP repeats near-
   gaplessly (HARD_GAP_MS, BUSY-gated). SINK RATE / DON'T SINK / TOO LOW * / BANK ANGLE
   / STALL / RETARD re-annunciate every ~1.5 s while in the envelope. Doublet phrasing
   (TERRAIN,TERRAIN / DON'T SINK,DON'T SINK / BANK ANGLE,BANK ANGLE / WHOOP WHOOP PULL
   UP) is baked into the clip; the firmware provides the between-annunciation repeat.

   PRIORITY (high -> low, one DFPlayer clip per frame -- EGPWS aural-priority order,
   per the Honeywell MK VI/VIII Pilot Guide aural-priority table):
     STALL > PULL UP / TERRAIN (hard) > post-warning TERRAIN > bug TONE > MINIMUMS >
     TOO LOW TERRAIN (4A/4C) > RETARD > altitude/distance callouts > TOO LOW GEAR >
     SINK RATE > DON'T SINK > BANK ANGLE > V1 > ROTATE.

   CROSSING-TRACKER DISCIPLINE -- `_prevAlt`/`_prevDist` advance only when the ladder is
   serviced; frozen while a higher-priority clip owns the audio or the player is busy
   (so a deferred callout announces the CURRENT altitude/range, not a backlog); bumped
   up while climbing / range opening. The bug tone uses separate trackers.

   Clip numbers = GPWS_CLIP_* below (documented in KCMk1_Annunciator/README.md, DFPlayer
   folder /01). All tunables are in the TUNABLES block.

   Licensed under the GNU General Public License v3.0 (GPL-3.0).
   Final code written by Jason Rostoker for Jeb's Controller Works.
****************************************************************************************/
#include "KCMk1_Annunciator.h"


/***************************************************************************************
   DFPLAYER INSTANCE
****************************************************************************************/
static KCM_DFPlayer gpwsDfp(KCM_DFPLAYER_SERIAL);


/***************************************************************************************
   CLIP NUMBER MAP (DFPlayer folder /01). Spoken text documented in the README.
   Number words (15-31) are shared between the feet altitude ladder and the metre
   distance ladder.
****************************************************************************************/
#define GPWS_FOLDER            1

#define GPWS_CLIP_PULL_UP       1    // "WHOOP WHOOP PULL UP"
#define GPWS_CLIP_TERRAIN       2    // "TERRAIN, TERRAIN"
#define GPWS_CLIP_SINK_RATE     3    // "SINK RATE"
#define GPWS_CLIP_TOO_LOW_GEAR  4    // "TOO LOW, GEAR"
#define GPWS_CLIP_TOO_LOW_TERR  5    // "TOO LOW, TERRAIN"
#define GPWS_CLIP_DONT_SINK     6    // "DON'T SINK, DON'T SINK"
#define GPWS_CLIP_BANK_ANGLE    7    // "BANK ANGLE, BANK ANGLE"
#define GPWS_CLIP_STALL         8    // "STALL" (or "STALL, STALL")
#define GPWS_CLIP_MINIMUMS      9    // "MINIMUMS"
#define GPWS_CLIP_APPR_MIN     10    // "APPROACHING MINIMUMS"
#define GPWS_CLIP_V1           11    // "V ONE"
#define GPWS_CLIP_ROTATE       12    // "ROTATE"
#define GPWS_CLIP_RETARD       13    // "RETARD"
#define GPWS_CLIP_TONE         14    // bug tone / beep
#define GPWS_CLIP_2500         15    // "TWO THOUSAND FIVE HUNDRED"
#define GPWS_CLIP_1000         16    // "ONE THOUSAND"
#define GPWS_CLIP_500          17    // "FIVE HUNDRED"
#define GPWS_CLIP_400          18    // "FOUR HUNDRED"
#define GPWS_CLIP_300          19    // "THREE HUNDRED"
#define GPWS_CLIP_200          20    // "TWO HUNDRED"
#define GPWS_CLIP_100          21    // "ONE HUNDRED"
#define GPWS_CLIP_90           22    // "NINETY"
#define GPWS_CLIP_80           23    // "EIGHTY"
#define GPWS_CLIP_70           24    // "SEVENTY"
#define GPWS_CLIP_60           25    // "SIXTY"
#define GPWS_CLIP_50           26    // "FIFTY"
#define GPWS_CLIP_40           27    // "FORTY"
#define GPWS_CLIP_30           28    // "THIRTY"
#define GPWS_CLIP_20           29    // "TWENTY"
#define GPWS_CLIP_10           30    // "TEN"
#define GPWS_CLIP_5            31    // "FIVE"


/***************************************************************************************
   CALLOUT LADDERS
****************************************************************************************/
struct GpwsRung { float val; uint8_t clip; };

static const GpwsRung ALT_LADDER[] = {   // radio-altitude FEET -> metres AGL
  { 762.0f, GPWS_CLIP_2500 },  // 2500 ft
  { 304.8f, GPWS_CLIP_1000 },  // 1000 ft
  { 152.4f, GPWS_CLIP_500  },  //  500 ft
  { 121.9f, GPWS_CLIP_400  },  //  400 ft
  {  91.4f, GPWS_CLIP_300  },  //  300 ft
  {  61.0f, GPWS_CLIP_200  },  //  200 ft
  {  30.5f, GPWS_CLIP_100  },  //  100 ft
  {  27.4f, GPWS_CLIP_90   },  //   90 ft
  {  24.4f, GPWS_CLIP_80   },  //   80 ft
  {  21.3f, GPWS_CLIP_70   },  //   70 ft
  {  18.3f, GPWS_CLIP_60   },  //   60 ft
  {  15.2f, GPWS_CLIP_50   },  //   50 ft
  {  12.2f, GPWS_CLIP_40   },  //   40 ft
  {   9.1f, GPWS_CLIP_30   },  //   30 ft
  {   6.1f, GPWS_CLIP_20   },  //   20 ft
  {   3.0f, GPWS_CLIP_10   },  //   10 ft
  {   1.5f, GPWS_CLIP_5    },  //    5 ft
};
static const uint8_t ALT_LADDER_COUNT = sizeof(ALT_LADDER) / sizeof(ALT_LADDER[0]);

static const GpwsRung DIST_LADDER[] = {  // target range, metres
  { 500.0f, GPWS_CLIP_500 }, { 200.0f, GPWS_CLIP_200 }, { 100.0f, GPWS_CLIP_100 },
  {  50.0f, GPWS_CLIP_50  }, {  40.0f, GPWS_CLIP_40  }, {  30.0f, GPWS_CLIP_30  },
  {  20.0f, GPWS_CLIP_20  }, {  10.0f, GPWS_CLIP_10  }, {   5.0f, GPWS_CLIP_5   },
};
static const uint8_t DIST_LADDER_COUNT = sizeof(DIST_LADDER) / sizeof(DIST_LADDER[0]);


/***************************************************************************************
   TUNABLES  (all GPWS flight-tuning lives here)
****************************************************************************************/
static const uint8_t GPWS_VOLUME   = 24;      // DFPlayer volume 0..30

// Mode 1 -- descent-rate envelopes (m/s vs m AGL), floored, active below the ceiling.
static const float M1_CEIL_M       = 746.8f;  // 2450 ft
static const float SINK_FLOOR_MS   = 5.1f;    // ~1000 fpm floor
static const float SINK_SLOPE      = 0.0272f;
static const float PULLUP_FLOOR_MS = 7.62f;   // ~1500 fpm floor
static const float PULLUP_SLOPE    = 0.0374f;

// Mode 2 -- terrain closure (m/s) from smoothed d(alt_surf)/dt.
static const float M2_CEIL_M       = 746.8f;
static const float TERR_FLOOR_MS   = 10.0f;
static const float TERR_SLOPE      = 0.040f;
static const float CLOSURE_ALPHA   = 0.30f;
static const float CLOSURE_MIN_DT_S= 0.05f;
// Post-warning "TERRAIN": after PULL UP exits, keep saying TERRAIN while terrain
// clearance is still decreasing (closure above this floor) -- manual Mode 2 behaviour.
static const float POST_TERR_FLOOR_MS = 2.0f;

// Mode 3 -- DON'T SINK.
static const float M3_CEIL_M       = 457.0f;  // ~1500 ft climbout window
static const float M3_LOSS_FRAC    = 0.10f;   // ~10% of height AGL
static const float M3_LOSS_MIN_M   = 3.0f;
// Manual: DON'T SINK is spoken only TWICE unless the altitude loss keeps growing.
// After the first two, it repeats only when the loss deepens by this factor (20%).
static const float M3_WORSEN_FRAC  = 1.20f;

// Mode 4A/4C -- terrain clearance.
static const float GEAR_ALT_M      = 152.4f;  // 500 ft  -> TOO LOW GEAR
static const float TERR4_ALT_M     = 304.8f;  // 1000 ft -> TOO LOW TERRAIN (high speed)
static const float TOOLOW_SPEED_MS = 98.0f;   // ~190 kt expansion speed
// Mode 4C takeoff Minimum Terrain Clearance floor = MTC_FRAC of the post-takeoff peak
// radio altitude (non-decreasing). Sinking below it (gear up) -> TOO LOW TERRAIN.
static const float MTC_FRAC        = 0.75f;   // manual: 75% of (15 s avg) radio altitude

// Mode 6 -- BANK ANGLE altitude-dependent limit (deg), turbofan envelope:
// +/-10 deg at 5-30 ft, ramp to +/-40 deg by 150 ft, ramp to +/-55 deg by 2450 ft.
static const float BANK_LO_DEG     = 10.0f;
static const float BANK_MID_DEG    = 40.0f;
static const float BANK_HI_DEG     = 55.0f;
static const float BANK_RAMP_LO_M  = 9.1f;    // 30 ft
static const float BANK_RAMP_MID_M = 45.7f;   // 150 ft
static const float BANK_RAMP_HI_M  = 746.8f;  // 2450 ft

// Approach markers.
static const float APPR_MIN_MARGIN_M = 30.5f; // "APPROACHING MINIMUMS" this far above the bug (100 ft)
static const float RETARD_ALT_M      = 6.1f;  // RETARD flare below this AGL (20 ft)

// STALL -- AoA = pitch - surfaceVelocityPitch (degrees). Cross-panel aligned with the
// InfoDisp aircraft EADI AoA arc via KCMk1_SystemConfig (KCM_AOA_STALL_DEG = "beyond
// stall AoA" red tier), so the display and the aural stall point are identical.
static const float STALL_AOA_DEG     = KCM_AOA_STALL_DEG;
static const float STALL_MIN_SPEED_MS= 10.0f; // ignore near-zero airspeed noise

// Takeoff-roll speed callouts (m/s). KSP planes vary widely: the common rule of thumb
// is to rotate (nose up) around ~50 m/s, and typical liftoff runs ~40-110 m/s
// depending on the design (light STOL low, heavy jets high). Defaults sit mid-band --
// SET THESE to the plane you fly. V1 is just below rotation.
static const float V1_SPEED_MS       = 55.0f;   // ~107 kt
static const float VR_SPEED_MS       = 65.0f;   // ~126 kt (rotate)

// General.
static const float DESCENT_DEADBAND_MS = 0.1f;
static const float ALT_JUMP_M          = 2000.0f;
static const float DIST_JUMP_M         = 2000.0f;
static const float MIN_DEDUP_M         = 8.0f;
static const float THR_MIN_M           = 1.0f;
static const float THR_MAX_M           = 9999.0f;
static const float TGT_MIN_M           = 0.5f;

// Repeat cadences (ms).
static const uint16_t HARD_GAP_MS   = 1400;
static const uint16_t SINK_GAP_MS   = 1500;
static const uint16_t MODE3_GAP_MS  = 1500;
static const uint16_t TERR4_GAP_MS  = 1500;
static const uint16_t GEAR_GAP_MS   = 1500;
static const uint16_t BANK_GAP_MS   = 1500;
static const uint16_t STALL_GAP_MS  = 1200;
static const uint16_t RETARD_GAP_MS = 1000;
static const uint16_t POSTTERR_GAP_MS = 1500;   // Mode 2 post-warning TERRAIN repeat


/***************************************************************************************
   LIVE CONFIG (relayed by the master over I2C via gpwsSetConfig())
****************************************************************************************/
#define GPWS_MODE_OFF     0
#define GPWS_MODE_ACTIVE  1
#define GPWS_MODE_PROX    2

static uint8_t  _gpwsMode      = GPWS_MODE_OFF;
static bool     _gpwsProxAlarm = false;
static bool     _gpwsRdvRadar  = false;
static int16_t  _gpwsThreshold = 200;


/***************************************************************************************
   RUNTIME STATE
****************************************************************************************/
static bool     _gpwsEnabled  = false;
static bool     _busyAttached = false;

static float    _prevAlt      = -1.0f;
static float    _prevDist     = -1.0f;
static float    _tonePrevAlt  = -1.0f;
static float    _tonePrevDist = -1.0f;
static bool     _tonePending  = false;
static bool     _minPending   = false;   // MINIMUMS crossed (gear down), awaiting the player

static float    _closPrevAlt  = -1.0f;
static uint32_t _closPrevMs   = 0;
static float    _closRate     = 0.0f;

static bool     _hardActive   = false;
static bool     _terrainDone  = false;
static uint32_t _lastHardMs   = 0;
static uint32_t _lastSinkMs   = 0;
static bool     _terr2Recent  = false;   // in/after a Mode-2 episode (post-warning TERRAIN)
static uint32_t _lastPostTerrMs = 0;

static bool     _wasOnGround  = false;
static bool     _m3Armed      = false;
static float    _m3MaxAlt     = 0.0f;
static bool     _m3Active     = false;
static uint32_t _lastM3Ms     = 0;
static uint8_t  _m3Count      = 0;       // DON'T SINK annunciations this episode
static float    _m3LastLoss   = 0.0f;    // altitude loss at the last DON'T SINK

static bool     _terr4Active  = false;
static uint32_t _lastTerr4Ms  = 0;
static bool     _gearActive   = false;
static uint32_t _lastGearMs   = 0;
static bool     _bankActive   = false;
static uint32_t _lastBankMs   = 0;
static bool     _stallActive  = false;
static uint32_t _lastStallMs  = 0;
static bool     _retardActive = false;
static uint32_t _lastRetardMs = 0;

static bool     _v1Done       = false;   // V1 spoken this takeoff roll
static bool     _rotateDone   = false;   // ROTATE spoken this takeoff roll


/***************************************************************************************
   HELPERS
****************************************************************************************/
static inline void gpwsPlay(uint8_t clip) {
  gpwsDfp.playFolderTrack(GPWS_FOLDER, clip);
  if (debugMode) { Serial.print(F("Annunciator: GPWS clip ")); Serial.println(clip); }
}

static inline bool gpwsThresholdValid() {
  float t = (float)_gpwsThreshold;
  return (t >= THR_MIN_M && t <= THR_MAX_M);
}

static float gpwsBankLimit(float alt) {
  if (alt <= BANK_RAMP_LO_M) return BANK_LO_DEG;
  if (alt <= BANK_RAMP_MID_M)
    return BANK_LO_DEG + (BANK_MID_DEG - BANK_LO_DEG) *
           (alt - BANK_RAMP_LO_M) / (BANK_RAMP_MID_M - BANK_RAMP_LO_M);
  if (alt <= BANK_RAMP_HI_M)
    return BANK_MID_DEG + (BANK_HI_DEG - BANK_MID_DEG) *
           (alt - BANK_RAMP_MID_M) / (BANK_RAMP_HI_M - BANK_RAMP_MID_M);
  return BANK_HI_DEG;
}

// Lowest rung in [prev, cur] the value descended/closed THROUGH. `spec` holds special
// rungs that mask numeric rungs within `dedup`; a special with clip 0 masks only (used
// so MINIMUMS -- fired from its own higher-priority slot -- suppresses the number at
// the decision height without re-firing here).
static uint8_t gpwsCrossed(const GpwsRung *tbl, uint8_t n, float prev, float cur,
                           const GpwsRung *spec, uint8_t ns, float dedup) {
  uint8_t best = 0; float bestVal = 1e9f;
  for (uint8_t i = 0; i < ns; i++) {
    float r = spec[i].val;
    if (spec[i].clip != 0 && prev > r && cur <= r && r < bestVal) { best = spec[i].clip; bestVal = r; }
  }
  for (uint8_t i = 0; i < n; i++) {
    float r = tbl[i].val;
    bool masked = false;
    for (uint8_t j = 0; j < ns; j++) if (fabsf(r - spec[j].val) < dedup) { masked = true; break; }
    if (masked) continue;
    if (prev > r && cur <= r && r < bestVal) { best = tbl[i].clip; bestVal = r; }
  }
  return best;
}

static void gpwsClearLatches() {
  _prevAlt = _prevDist = -1.0f;
  _tonePrevAlt = _tonePrevDist = -1.0f;
  _tonePending = false; _minPending = false;
  _closPrevAlt = -1.0f; _closPrevMs = 0; _closRate = 0.0f;
  _hardActive = false; _terrainDone = false; _terr2Recent = false;
  _wasOnGround = false; _m3Armed = false; _m3MaxAlt = 0.0f; _m3Active = false;
  _m3Count = 0; _m3LastLoss = 0.0f;
  _terr4Active = false; _gearActive = false; _bankActive = false;
  _stallActive = false; _retardActive = false;
  _v1Done = false; _rotateDone = false;
}


/***************************************************************************************
   SET CONFIG / RESET / SETUP
****************************************************************************************/
void gpwsSetConfig(uint8_t cfgByte, int16_t thresholdM) {
  uint8_t newMode = cfgByte & 0x03;
  bool    newProx = (cfgByte >> 2) & 0x01;
  bool    newRdv  = (cfgByte >> 3) & 0x01;
  bool changed = (newMode != _gpwsMode) || (newProx != _gpwsProxAlarm) ||
                 (newRdv != _gpwsRdvRadar) || (thresholdM != _gpwsThreshold);
  _gpwsMode = newMode; _gpwsProxAlarm = newProx; _gpwsRdvRadar = newRdv;
  _gpwsThreshold = thresholdM;
  if (debugMode && changed) {
    Serial.print(F("Annunciator: GPWS cfg mode=")); Serial.print(_gpwsMode);
    Serial.print(F(" prox=")); Serial.print(_gpwsProxAlarm);
    Serial.print(F(" rdv="));  Serial.print(_gpwsRdvRadar);
    Serial.print(F(" thr="));  Serial.println(_gpwsThreshold);
  }
}

void gpwsReset() {
  gpwsDfp.stop();
  gpwsClearLatches();
  _gpwsEnabled = false;
}

void gpwsSetup() {
  gpwsDfp.begin(GPWS_VOLUME, KCM_DFPLAYER_BAUD);
  gpwsDfp.attachBusyPin(KCM_AUDIO_BUSY_PIN);
  _busyAttached = gpwsDfp.busyPinAttached();
  gpwsClearLatches();
  if (debugMode) Serial.println(F("Annunciator: GPWS ready (DFPlayer on Serial2)"));
}


/***************************************************************************************
   UPDATE  -- call every loop() pass.
****************************************************************************************/
void gpwsUpdate() {
  bool enabled = flightScene && audioEnabled && (_gpwsMode != GPWS_MODE_OFF);
  if (!enabled) {
    if (_gpwsEnabled) { gpwsDfp.stop(); gpwsClearLatches(); }
    _gpwsEnabled = false;
    return;
  }
  _gpwsEnabled = true;

  bool warningsOn   = (_gpwsMode == GPWS_MODE_ACTIVE);
  bool altCallouts  = (_gpwsMode == GPWS_MODE_ACTIVE) ||
                      (_gpwsMode == GPWS_MODE_PROX && !_gpwsRdvRadar);
  bool distCallouts = (_gpwsMode == GPWS_MODE_PROX && _gpwsRdvRadar);

  // --- Geometry ----------------------------------------------------------------
  uint8_t sit  = state.vesselSituationState;
  bool isAloft = bitRead(sit, VSIT_FLIGHT)  || bitRead(sit, VSIT_SUBORBIT) ||
                 bitRead(sit, VSIT_ORBIT)   || bitRead(sit, VSIT_ESCAPE);
  bool onGround = bitRead(sit, VSIT_LANDED) || bitRead(sit, VSIT_SPLASH) ||
                  bitRead(sit, VSIT_PRELAUNCH);
  float alt   = state.alt_surf;
  float vs    = fabsf(state.vel_vert);
  float spd   = state.vel_surf;
  bool descending = (state.vel_vert < -DESCENT_DEADBAND_MS);
  bool airDesc = isAloft && descending && alt > 0.0f;
  float thr   = (float)_gpwsThreshold;
  bool thrOK  = gpwsThresholdValid();
  float aoa   = state.pitch - state.srfVelPitch;   // angle of attack proxy (deg)

  uint32_t now = millis();
  bool busy = _busyAttached && gpwsDfp.isPlaying();

  // --- Mode 2 closure rate -----------------------------------------------------
  if (_closPrevMs == 0 || alt < 0.0f) {
    _closPrevAlt = alt; _closPrevMs = now; _closRate = 0.0f;
  } else {
    float dt = (now - _closPrevMs) / 1000.0f;
    if (dt >= CLOSURE_MIN_DT_S) {
      float inst = -(alt - _closPrevAlt) / dt;
      if (fabsf(alt - _closPrevAlt) > ALT_JUMP_M) inst = 0.0f;
      _closRate += CLOSURE_ALPHA * (inst - _closRate);
      _closPrevAlt = alt; _closPrevMs = now;
    }
  }

  // --- Mode 3 arm/track --------------------------------------------------------
  if (onGround) { _m3Armed = false; _m3MaxAlt = 0.0f; }
  else if (isAloft) {
    if (_wasOnGround) { _m3Armed = true; _m3MaxAlt = alt; }
    if (alt > M3_CEIL_M) _m3Armed = false;
    if (alt > _m3MaxAlt) _m3MaxAlt = alt;
  }
  _wasOnGround = onGround;

  // --- Ladder trackers (re-seed on jump; bump up while climbing / range opening)
  if (_prevAlt < 0.0f || fabsf(alt - _prevAlt) > ALT_JUMP_M) _prevAlt = alt;
  if (alt > _prevAlt) _prevAlt = alt;

  float dist = state.tgtDistance;
  bool tgtValid = (dist > TGT_MIN_M);
  if (!tgtValid) { _prevDist = -1.0f; _tonePrevDist = -1.0f; }
  else {
    if (_prevDist < 0.0f || fabsf(dist - _prevDist) > DIST_JUMP_M) _prevDist = dist;
    if (dist > _prevDist) _prevDist = dist;
  }

  // --- Bug-tone + MINIMUMS crossing (independent trackers) ----------------------
  // The bug tone fires on any threshold crossing; MINIMUMS is gear-gated (manual:
  // decision-height callout requires gear down) and fires from its own high slot.
  if (altCallouts && thrOK && airDesc) {
    if (_tonePrevAlt < 0.0f || fabsf(alt - _tonePrevAlt) > ALT_JUMP_M) _tonePrevAlt = alt;
    if (_tonePrevAlt > thr && alt <= thr) {
      _tonePending = true;
      if (state.gear_on) _minPending = true;
    }
    _tonePrevAlt = alt;
  } else if (!distCallouts) {
    _tonePrevAlt = -1.0f; _minPending = false;
  }
  if (distCallouts && thrOK && tgtValid) {
    if (_tonePrevDist < 0.0f || fabsf(dist - _tonePrevDist) > DIST_JUMP_M) _tonePrevDist = dist;
    if (_tonePrevDist > thr && dist <= thr) _tonePending = true;
    _tonePrevDist = dist;
  }

  // --- Conditions (evaluated every frame so latch/held state stays correct) -----
  bool inM1   = airDesc && alt < M1_CEIL_M;
  bool sink1  = warningsOn && inM1 && vs > (SINK_FLOOR_MS   + SINK_SLOPE   * alt);
  bool pull1  = warningsOn && inM1 && vs > (PULLUP_FLOOR_MS + PULLUP_SLOPE * alt);
  bool terr2  = warningsOn && isAloft && alt > 0.0f && alt < M2_CEIL_M &&
                _closRate > (TERR_FLOOR_MS + TERR_SLOPE * alt);
  // Mode 2 post-warning: keep saying TERRAIN after PULL UP exits while still closing.
  if (_closRate <= 0.0f || !warningsOn) _terr2Recent = false;
  bool postTerr = warningsOn && _terr2Recent && isAloft && alt > 0.0f &&
                  alt < M2_CEIL_M && _closRate > POST_TERR_FLOOR_MS;

  bool m3Sink = warningsOn && _m3Armed && descending &&
                (_m3MaxAlt - alt) > max(M3_LOSS_MIN_M, M3_LOSS_FRAC * _m3MaxAlt);
  if (!m3Sink) { _m3Active = false; _m3Count = 0; }

  bool gearUp   = warningsOn && !state.gear_on && isAloft && alt > 0.0f;
  bool gearCond = gearUp && alt < GEAR_ALT_M;
  // TOO LOW TERRAIN = Mode 4A (high-speed gear-up) OR Mode 4C (takeoff MTC floor).
  bool terr4A   = gearUp && alt < TERR4_ALT_M && spd > TOOLOW_SPEED_MS;
  bool mtcBreach= warningsOn && _m3Armed && !state.gear_on && alt > 0.0f &&
                  alt < MTC_FRAC * _m3MaxAlt;
  bool tlTerr   = terr4A || mtcBreach;
  if (!gearCond) _gearActive  = false;
  if (!tlTerr)   _terr4Active = false;

  bool bankCond = warningsOn && isAloft && inAtmo && fabsf(state.roll) > gpwsBankLimit(alt);
  if (!bankCond) _bankActive = false;

  bool stallCond = warningsOn && isAloft && inAtmo && spd > STALL_MIN_SPEED_MS &&
                   aoa > STALL_AOA_DEG;
  if (!stallCond) _stallActive = false;

  bool retardCond = altCallouts && airDesc && state.gear_on && inAtmo && alt < RETARD_ALT_M;
  if (!retardCond) _retardActive = false;

  // V1 / ROTATE: takeoff roll only; re-arm when stationary on the ground.
  if (onGround && spd < 5.0f) { _v1Done = false; _rotateDone = false; }

  // === Priority ladder (EGPWS aural priority order) ============================

  // 1: STALL -- a separate stall-warning system that outranks all of GPWS.
  if (stallCond) {
    if (!busy && (!_stallActive || (now - _lastStallMs) >= STALL_GAP_MS)) {
      gpwsPlay(GPWS_CLIP_STALL); _lastStallMs = now; _stallActive = true;
    }
    return;
  }

  // 2: HARD -- PULL UP (Mode 1 inner or Mode 2) with a TERRAIN entry on Mode 2.
  if (pull1 || terr2) {
    if (terr2) _terr2Recent = true;   // arm the post-warning TERRAIN
    if (!_hardActive) { _hardActive = true; _terrainDone = !terr2; }
    if (terr2 && !_terrainDone) { gpwsPlay(GPWS_CLIP_TERRAIN); _terrainDone = true; _lastHardMs = now; }
    else if (!busy && (now - _lastHardMs) >= HARD_GAP_MS) { gpwsPlay(GPWS_CLIP_PULL_UP); _lastHardMs = now; }
    return;
  }
  _hardActive = false; _terrainDone = false;

  // 3: Mode 2 post-warning TERRAIN (still closing after PULL UP exits).
  if (postTerr) {
    if (!busy && (now - _lastPostTerrMs) >= POSTTERR_GAP_MS) {
      gpwsPlay(GPWS_CLIP_TERRAIN); _lastPostTerrMs = now;
    }
    return;
  }

  // 4: bug TONE (threshold marker -- leads MINIMUMS).
  if (_tonePending) {
    if (!busy) { gpwsPlay(GPWS_CLIP_TONE); _tonePending = false; }
    return;
  }

  // 5: MINIMUMS (decision height, gear down) -- ranks above TOO LOW TERRAIN/callouts.
  if (_minPending) {
    if (!busy) { gpwsPlay(GPWS_CLIP_MINIMUMS); _minPending = false; }
    return;
  }

  // 6: TOO LOW TERRAIN (Mode 4A high-speed / Mode 4C takeoff MTC).
  if (tlTerr) {
    if (!busy && (!_terr4Active || (now - _lastTerr4Ms) >= TERR4_GAP_MS)) {
      gpwsPlay(GPWS_CLIP_TOO_LOW_TERR); _lastTerr4Ms = now; _terr4Active = true;
    }
    return;
  }

  // 7: RETARD (flare) -- above the low altitude numbers so it isn't buried.
  if (retardCond) {
    if (!busy && (!_retardActive || (now - _lastRetardMs) >= RETARD_GAP_MS)) {
      gpwsPlay(GPWS_CLIP_RETARD); _lastRetardMs = now; _retardActive = true;
    }
    return;
  }

  // 8: altitude callouts (numbers + APPROACHING MINIMUMS) / distance callouts.
  // Above TOO LOW GEAR / SINK RATE / DON'T SINK per the EGPWS priority table. On a
  // crossing this owns the frame (fires or defers); with no crossing it falls through.
  if (altCallouts && airDesc && _prevAlt > 0.0f) {
    GpwsRung spec[2]; uint8_t ns = 0;
    if (thrOK && state.gear_on) {          // minimums family only with gear down
      spec[ns++] = { thr, 0 };             // mask the number at the decision height
      float appr = thr + APPR_MIN_MARGIN_M;
      if (appr <= THR_MAX_M) spec[ns++] = { appr, GPWS_CLIP_APPR_MIN };
    }
    uint8_t clip = gpwsCrossed(ALT_LADDER, ALT_LADDER_COUNT, _prevAlt, alt, spec, ns, MIN_DEDUP_M);
    if (clip != 0) {
      if (!busy) { gpwsPlay(clip); _prevAlt = alt; }
      return;
    }
    _prevAlt = alt;
  } else if (distCallouts && tgtValid && _prevDist > 0.0f && dist < _prevDist) {
    uint8_t clip = gpwsCrossed(DIST_LADDER, DIST_LADDER_COUNT, _prevDist, dist, nullptr, 0, MIN_DEDUP_M);
    if (clip != 0) {
      if (!busy) { gpwsPlay(clip); _prevDist = dist; }
      return;
    }
    _prevDist = dist;
  } else {
    _prevAlt = alt;
  }

  // 9: TOO LOW GEAR.
  if (gearCond) {
    if (!busy && (!_gearActive || (now - _lastGearMs) >= GEAR_GAP_MS)) {
      gpwsPlay(GPWS_CLIP_TOO_LOW_GEAR); _lastGearMs = now; _gearActive = true;
    }
    return;
  }

  // 10: SINK RATE (Mode 1 outer).
  if (sink1) {
    if (!busy && (now - _lastSinkMs) >= SINK_GAP_MS) { gpwsPlay(GPWS_CLIP_SINK_RATE); _lastSinkMs = now; }
    return;
  }

  // 11: DON'T SINK (Mode 3) -- spoken twice, then only as the loss deepens.
  if (m3Sink) {
    float loss = _m3MaxAlt - alt;
    bool speak = false;
    if (!_m3Active)                                                     speak = true;   // 1st
    else if (_m3Count < 2 && (now - _lastM3Ms) >= MODE3_GAP_MS)         speak = true;   // 2nd
    else if (_m3Count >= 2 && loss > _m3LastLoss * M3_WORSEN_FRAC &&
             (now - _lastM3Ms) >= MODE3_GAP_MS)                          speak = true;   // worsening
    if (speak && !busy) {
      gpwsPlay(GPWS_CLIP_DONT_SINK);
      _lastM3Ms = now; _m3LastLoss = loss; _m3Active = true;
      if (_m3Count < 2) _m3Count++;
    }
    return;
  }

  // 12: BANK ANGLE.
  if (bankCond) {
    if (!busy && (!_bankActive || (now - _lastBankMs) >= BANK_GAP_MS)) {
      gpwsPlay(GPWS_CLIP_BANK_ANGLE); _lastBankMs = now; _bankActive = true;
    }
    return;
  }

  // 13: V1 (takeoff roll).
  if (warningsOn && onGround && !_v1Done && spd >= V1_SPEED_MS) {
    if (!busy) { gpwsPlay(GPWS_CLIP_V1); _v1Done = true; }
    return;
  }

  // 14: ROTATE (takeoff roll).
  if (warningsOn && onGround && !_rotateDone && spd >= VR_SPEED_MS) {
    if (!busy) { gpwsPlay(GPWS_CLIP_ROTATE); _rotateDone = true; }
    return;
  }
}

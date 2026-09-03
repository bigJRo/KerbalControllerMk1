/***************************************************************************************
   Audio.ino -- Audio wiring and master alarm condition tracking for KCMk1 Annunciator

   ALARM CONDITION TRACKING
   Condition bits and bitmask live here rather than in the KerbalDisplayAudio library
   because they are application-specific -- they map directly to C&W warning bits that
   only the sketch knows about. The library is told only to start or stop the tone;
   all condition logic is managed here.

   Each ALARM_* bit corresponds to a WARNING-level C&W bit. updateAlarmMask() is called
   from ScreenMain.ino whenever a WARNING bit transitions. The library alarm starts when
   the mask goes from 0 to non-zero and stops when it returns to 0.

   alarmSilenced (bool, defined in AAA_Globals.ino): set when the crew presses the
   master alarm button while conditions are active. Prevents the alarm restarting for
   existing conditions. Reset when all conditions clear. A new condition activating
   while silenced restarts the alarm so the crew cannot miss a fresh warning.
   Defined as a global (not a local static) so TouchEvents.ino can set it on button press.

   AUDIO TRIGGER LOCATIONS
   C&W bit change triggers (alarm, caution tone, chirps) -- serviceAlarmAudio() below,
     called from loop() every pass so the audio does not depend on which screen is
     up. (It used to live in the Main screen's update pass, which left the SOI screen
     mute: a new warning there made no sound, and the alarm was silenced on the way.)
   Scene change and vessel change silencing               -- SimpitHandler.ino

   The audio state machine is in the KerbalDisplayAudio library.
   All audio is gated by the audioEnabled flag in AAA_Config.ino.
   audioEnabled can also be set at runtime via I2C from the master.

   TWO-TIER INDICATORS (CW_PE_LOW, CW_PROP_LOW, CW_LIFE_SUPPORT)
   These indicators have yellow and red tiers driven by the same C&W bit.
   CautionWarning.ino only sets the bit when the red threshold is breached, so the
   alarm only fires on the red condition. The yellow condition is handled separately
   in ScreenMain.ino via the button color -- the bit is not set in the yellow tier.
****************************************************************************************/
#include "KCMk1_Annunciator.h"


/***************************************************************************************
   MASTER ALARM CONDITION BITS
   Each bit maps one WARNING-level C&W condition to the alarm bitmask.
   Expanded to uint16_t to accommodate 9 alarm conditions (was uint8_t with 5).
   Defined here so the mapping is visible alongside the tracking logic.
   Order matches the C&W layout rows for easy cross-reference.
****************************************************************************************/
const uint16_t ALARM_LOW_DV      = (1u << 0);  // CW_LOW_DV       Row 0
const uint16_t ALARM_HIGH_G      = (1u << 1);  // CW_HIGH_G       Row 0
const uint16_t ALARM_HIGH_TEMP   = (1u << 2);  // CW_HIGH_TEMP    Row 0
const uint16_t ALARM_BUS_VOLTAGE = (1u << 3);  // CW_BUS_VOLTAGE  Row 0
const uint16_t ALARM_ABORT       = (1u << 4);  // CW_ABORT        Row 0
const uint16_t ALARM_GROUND_PROX = (1u << 5);  // CW_GROUND_PROX  Row 1
const uint16_t ALARM_PE_LOW      = (1u << 6);  // CW_PE_LOW       Row 1 (red tier only)
const uint16_t ALARM_PROP_LOW    = (1u << 7);  // CW_PROP_LOW     Row 1 (red tier only)
const uint16_t ALARM_LIFE_SUPPORT= (1u << 8);  // CW_LIFE_SUPPORT Row 1 (red tier only)


/***************************************************************************************
   ALARM ACTIVE MASK
   Tracks which alarm conditions are currently active. The library alarm runs while
   this mask is non-zero. Expanded to uint16_t to match ALARM_* constants above.
****************************************************************************************/
uint16_t alarmActiveMask = 0;


/***************************************************************************************
   CW → ALARM MAPPING TABLE
   Single source of truth mapping each WARNING-level C&W bit to its ALARM_* mask bit.
   Used by both the C&W transition handler (ScreenMain, via applyAlarmTransitions())
   and syncMasterAlarmAudio() so the two can never drift out of sync.
****************************************************************************************/
struct CwAlarmMap { uint8_t cwBit; uint16_t alarmBit; };
static const CwAlarmMap CW_ALARM_MAP[] = {
  { CW_LOW_DV,       ALARM_LOW_DV       },
  { CW_HIGH_G,       ALARM_HIGH_G       },
  { CW_HIGH_TEMP,    ALARM_HIGH_TEMP    },
  { CW_BUS_VOLTAGE,  ALARM_BUS_VOLTAGE  },
  { CW_ABORT,        ALARM_ABORT        },
  { CW_GROUND_PROX,  ALARM_GROUND_PROX  },
  { CW_PE_LOW,       ALARM_PE_LOW       },
  { CW_PROP_LOW,     ALARM_PROP_LOW     },
  { CW_LIFE_SUPPORT, ALARM_LIFE_SUPPORT },
};
static const uint8_t CW_ALARM_MAP_COUNT = sizeof(CW_ALARM_MAP) / sizeof(CW_ALARM_MAP[0]);

// Route a C&W newBits/clrBits transition pair through the alarm mask via the table.
// Called from the C&W update pass in ScreenMain.ino (replaces the per-bit if ladder).
void applyAlarmTransitions(uint32_t newBits, uint32_t clrBits) {
  for (uint8_t i = 0; i < CW_ALARM_MAP_COUNT; i++) {
    if (newBits & (1ul << CW_ALARM_MAP[i].cwBit)) updateAlarmMask(CW_ALARM_MAP[i].alarmBit, true);
    if (clrBits & (1ul << CW_ALARM_MAP[i].cwBit)) updateAlarmMask(CW_ALARM_MAP[i].alarmBit, false);
  }
}


/***************************************************************************************
   UPDATE ALARM MASK
   Call when a WARNING-level C&W bit transitions on or off.
   condBit: one of the ALARM_* constants above.
   on=true:  condition became active.
   on=false: condition cleared.

   Silence latch behaviour:
   - Crew silences alarm while conditions are active -> alarmSilenced = true.
   - Existing conditions changing state do NOT restart alarm while silenced.
   - A NEW condition appearing while silenced DOES restart alarm (crew must know).
   - When all conditions clear naturally, silence latch resets automatically.
****************************************************************************************/
void updateAlarmMask(uint16_t condBit, bool on) {
  if (on) {
    bool wasActive = (alarmActiveMask != 0);
    alarmActiveMask |= condBit;
    if (!wasActive && !alarmSilenced) {
      // First active condition -- start alarm
      audioStartAlarm();
    } else if (wasActive && alarmSilenced) {
      // New condition while silenced -- restart alarm for new condition
      alarmSilenced = false;
      audioStartAlarm();
    } else if (!alarmSilenced && audioGetState() != AUDIO_MASTER_ALARM) {
      // Re-triggered after all conditions previously cleared naturally
      audioStartAlarm();
    }
  } else {
    alarmActiveMask &= ~condBit;
    if (alarmActiveMask == 0) {
      // All conditions cleared -- stop alarm and reset silence latch
      alarmSilenced = false;
      audioStopAlarm();
    }
  }
}


/***************************************************************************************
   SYNC MASTER ALARM AUDIO WITH CURRENT C&W STATE
   Reconciles the alarm-condition mask against the C&W bits that are active RIGHT NOW,
   rather than waiting for a bit to transition. Call on entry to the main screen so an
   alarm condition that is already active (e.g. after a screen change, or the forced
   lamp-test state) sounds immediately instead of staying silent until the next
   transition.

   Reconciles the mask DIRECTLY to the current C&W state rather than pushing each bit
   through updateAlarmMask(). Routing a re-entry through updateAlarmMask() treated the
   already-active conditions as "new while silenced" and un-silenced the alarm, so a
   crew-silenced master alarm would re-blare after a mere SOI->Main round-trip. Setting
   the mask directly preserves the silence latch: a silenced alarm stays silent until
   the conditions actually clear (mask -> 0, which resets the latch) or a genuinely new
   condition arrives via the live updateAlarmMask() path in the C&W update pass.
****************************************************************************************/
void syncMasterAlarmAudio() {
  uint32_t cw = state.cautionWarningState;
  uint16_t m  = 0;
  for (uint8_t i = 0; i < CW_ALARM_MAP_COUNT; i++)
    if (bitRead(cw, CW_ALARM_MAP[i].cwBit)) m |= CW_ALARM_MAP[i].alarmBit;

  alarmActiveMask = m;
  if (m == 0) {
    alarmSilenced = false;
    audioStopAlarm();
  } else if (!alarmSilenced && audioGetState() != AUDIO_MASTER_ALARM) {
    audioStartAlarm();
  }
}


/***************************************************************************************
   AUDIO SERVICE -- every loop pass, whatever screen is up
   Keeps its own copy of the C&W word and of the crossing references, so the display
   caches can be invalidated freely without replaying a cue. On the first pass of a
   flight scene (and again after a vessel switch, via audioResetService) it seeds the
   references from the live state and reconciles the master alarm against the bits
   active right now, so a condition already up on entry sounds, and no crossing chirp
   fires for a reference that was never seen.
****************************************************************************************/
static bool     _audSeeded  = false;
static uint32_t _audPrevCW  = 0;
static uint8_t  _audPrevSit = 0;
static float    _audPrevAlt = 0.0f, _audPrevVel = 0.0f, _audPrevAp = 0.0f;

void audioResetService() { _audSeeded = false; }

void serviceAlarmAudio() {
  if (!flightScene || !audioEnabled) { _audSeeded = false; return; }
  uint32_t cw = state.cautionWarningState;

  if (!_audSeeded) {
    _audSeeded  = true;
    _audPrevCW  = cw;
    _audPrevSit = state.vesselSituationState;
    _audPrevAlt = state.alt_sl; _audPrevVel = state.vel_surf; _audPrevAp = state.apoapsis;
    syncMasterAlarmAudio();
    return;
  }

  // C&W transitions: master alarm through the shared table, then the caution cues.
  uint32_t newBits = cw & ~_audPrevCW;
  uint32_t clrBits = _audPrevCW & ~cw;
  if (newBits | clrBits) {
    applyAlarmTransitions(newBits, clrBits);
    if (newBits & ((1ul << CW_ALT) | (1ul << CW_IMPACT_IMM)))                          audioCautionTone();
    if (newBits & ((1ul << CW_DESCENT) | (1ul << CW_ATMO) | (1ul << CW_GEAR_UP)))      audioCautionChirp();
  }
  _audPrevCW = cw;

  // Upward threshold crossings: alert chirps.
  if (state.alt_sl   >= ALERT_ALT_THRESHOLD && _audPrevAlt < ALERT_ALT_THRESHOLD) audioAlertChirp();
  if (state.vel_surf >= ALERT_VEL_THRESHOLD && _audPrevVel < ALERT_VEL_THRESHOLD) audioAlertChirp();
  if (currentBody.minSafe > 0 &&
      state.apoapsis >= currentBody.minSafe && _audPrevAp < currentBody.minSafe)   audioAlertChirp();
  if (bitRead(state.vesselSituationState, VSIT_ORBIT) && !bitRead(_audPrevSit, VSIT_ORBIT)) audioAlertChirp();
  _audPrevAlt = state.alt_sl; _audPrevVel = state.vel_surf; _audPrevAp = state.apoapsis;
  _audPrevSit = state.vesselSituationState;
}

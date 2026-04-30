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
   C&W bit change triggers (alarm, caution tone, chirps) -- ScreenMain.ino
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

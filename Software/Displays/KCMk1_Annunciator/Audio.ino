/***************************************************************************************
   Audio.ino -- Audio wiring and master alarm condition tracking for KCMk1 Annunciator

   ALARM CONDITION TRACKING
   Condition bits and bitmask live here rather than in the KerbalDisplayAudio library
   because they are application-specific — they map directly to C&W warning bits that
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
   C&W bit change triggers (alarm, caution tone, chirps) — ScreenMain.ino
   Scene change and vessel change silencing                — SimpitHandler.ino

   The audio state machine is in the KerbalDisplayAudio library.
   All audio is gated by the audioEnabled flag in AAA_Config.ino.
   audioEnabled can also be set at runtime via I2C from the master.
****************************************************************************************/
#include "KCMk1_Annunciator.h"


/***************************************************************************************
   MASTER ALARM CONDITION BITS
   Each bit maps one WARNING-level C&W condition to the alarm bitmask.
   Defined here so the mapping is visible alongside the tracking logic.
****************************************************************************************/
const uint8_t ALARM_GROUND_PROX = (1 << 0);  // CW_GROUND_PROX
const uint8_t ALARM_HIGH_G      = (1 << 1);  // CW_HIGH_G
const uint8_t ALARM_BUS_VOLTAGE = (1 << 2);  // CW_BUS_VOLTAGE
const uint8_t ALARM_HIGH_TEMP   = (1 << 3);  // CW_HIGH_TEMP
const uint8_t ALARM_LOW_DV      = (1 << 4);  // CW_LOW_DV


/***************************************************************************************
   ALARM ACTIVE MASK
   Tracks which alarm conditions are currently set. The library alarm runs while
   this mask is non-zero.
****************************************************************************************/
uint8_t alarmActiveMask = 0;


/***************************************************************************************
   UPDATE ALARM MASK
   Call when a WARNING-level C&W bit transitions.
   condBit: one of the ALARM_* constants above.
   on=true: condition became active; on=false: condition cleared.
****************************************************************************************/
void updateAlarmMask(uint8_t condBit, bool on) {
  if (on) {
    bool wasActive = (alarmActiveMask != 0);
    alarmActiveMask |= condBit;
    if (!wasActive && !alarmSilenced) {
      // First active condition — start alarm
      audioStartAlarm();
    } else if (wasActive && alarmSilenced) {
      // New condition while silenced — restart alarm
      alarmSilenced = false;
      audioStartAlarm();
    } else if (!alarmSilenced && audioGetState() != AUDIO_MASTER_ALARM) {
      // Re-triggered after all conditions previously cleared naturally
      audioStartAlarm();
    }
  } else {
    alarmActiveMask &= ~condBit;
    if (alarmActiveMask == 0) {
      // All conditions cleared — stop alarm and reset silence latch
      alarmSilenced = false;
      audioStopAlarm();
    }
  }
}

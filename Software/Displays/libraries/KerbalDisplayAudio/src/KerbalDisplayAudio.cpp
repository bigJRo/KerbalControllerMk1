#include "KerbalDisplayAudio.h"

/***************************************************************************************
   KerbalDisplayAudio Library — Implementation
   Non-blocking audio state machine for KCMk1 display panels.
   All public API and configuration defines are in KerbalDisplayAudio.h.
****************************************************************************************/

// Bitmask of which master alarm conditions are currently active
static uint8_t    _alarmActiveMask = 0;
static AudioState _audioState      = AUDIO_IDLE;
static uint32_t   _audioPhaseStart = 0;
static bool       _chirpSecondNote = false;
static bool       _chirpAscending  = true;
static bool       _alarmHiPhase    = true;
static bool       _alarmSilenced   = false;

// Internal: start the master alarm tone
static void _startAlarm() {
  _alarmHiPhase    = true;
  _audioPhaseStart = millis();
  _audioState      = AUDIO_MASTER_ALARM;
  tone(AUDIO_PIN, AUDIO_ALARM_FREQ_HI);
}

// Internal: start a chirp sequence
static void _startChirp(bool ascending) {
  _chirpAscending  = ascending;
  _chirpSecondNote = false;
  _audioPhaseStart = millis();
  _audioState      = AUDIO_CHIRP;
  tone(AUDIO_PIN, ascending ? AUDIO_CHIRP_ALERT_LO : AUDIO_CHIRP_CAUTION_HI);
}

void setupAudio() {
  pinMode(AUDIO_PIN, OUTPUT);
  noTone(AUDIO_PIN);
}

void updateAudio() {
  if (_audioState == AUDIO_IDLE) return;
  uint32_t now = millis();

  switch (_audioState) {

    case AUDIO_CHIRP:
      if (!_chirpSecondNote) {
        if (now - _audioPhaseStart >= AUDIO_CHIRP_NOTE_MS) {
          _chirpSecondNote = true;
          _audioPhaseStart = now;
          tone(AUDIO_PIN, _chirpAscending ? AUDIO_CHIRP_ALERT_HI : AUDIO_CHIRP_CAUTION_LO);
        }
      } else {
        if (now - _audioPhaseStart >= AUDIO_CHIRP_NOTE_MS) {
          noTone(AUDIO_PIN);
          _audioState = AUDIO_IDLE;
        }
      }
      break;

    case AUDIO_CAUTION_TONE:
      if (now - _audioPhaseStart >= AUDIO_CAUTION_TONE_MS) {
        noTone(AUDIO_PIN);
        _audioState = AUDIO_IDLE;
      }
      break;

    case AUDIO_MASTER_ALARM:
      if (now - _audioPhaseStart >= AUDIO_ALARM_PHASE_MS) {
        _audioPhaseStart = now;
        _alarmHiPhase    = !_alarmHiPhase;
        tone(AUDIO_PIN, _alarmHiPhase ? AUDIO_ALARM_FREQ_HI : AUDIO_ALARM_FREQ_LO);
      }
      break;
  }
}

void audioAlertChirp() {
  if (_audioState == AUDIO_MASTER_ALARM) return;
  if (_alarmSilenced) return;
  _startChirp(true);
}

void audioCautionChirp() {
  if (_audioState == AUDIO_MASTER_ALARM) return;
  if (_alarmSilenced) return;
  _startChirp(false);
}

void audioCautionTone() {
  if (_audioState == AUDIO_MASTER_ALARM) return;
  if (_alarmSilenced) return;
  noTone(AUDIO_PIN);
  _audioPhaseStart = millis();
  _audioState      = AUDIO_CAUTION_TONE;
  tone(AUDIO_PIN, AUDIO_CAUTION_TONE_FREQ);
}

void audioMasterAlarm(uint8_t condBit, bool on) {
  if (on) {
    bool wasActive = (_alarmActiveMask != 0);
    _alarmActiveMask |= condBit;
    if (!wasActive && !_alarmSilenced) {
      // First active condition — start alarm
      _startAlarm();
    } else if (wasActive && _alarmSilenced) {
      // A new condition arrived while the alarm was silenced — restart.
      // Silencing only covers the condition(s) active at the time; a new
      // condition re-asserts the alarm so the crew cannot inadvertently
      // miss a fresh warning.
      _alarmSilenced = false;
      _startAlarm();
    } else if (!_alarmSilenced && _audioState != AUDIO_MASTER_ALARM) {
      // Condition re-triggered after all conditions previously cleared
      // (wasActive was false but _alarmSilenced is also false, meaning
      // the alarm had already stopped naturally). Restart it.
      _startAlarm();
    }
  } else {
    _alarmActiveMask &= ~condBit;
    if (_alarmActiveMask == 0) {
      // All conditions cleared — stop alarm and reset silence latch so
      // the next condition that fires will start the alarm normally.
      _alarmSilenced = false;
      if (_audioState == AUDIO_MASTER_ALARM) {
        noTone(AUDIO_PIN);
        _audioState = AUDIO_IDLE;
      }
    }
  }
}

void audioSilence() {
  if (_audioState == AUDIO_MASTER_ALARM) {
    noTone(AUDIO_PIN);
    _audioState    = AUDIO_IDLE;
    _alarmSilenced = true;
  }
}

AudioState audioGetState() {
  return _audioState;
}

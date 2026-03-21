#include "KerbalDisplayAudio.h"

/***************************************************************************************
   KerbalDisplayAudio Library — Implementation
   Non-blocking audio state machine for KCMk1 display panels.
   All public API and configuration defines are in KerbalDisplayAudio.h.
****************************************************************************************/

static AudioState _audioState      = AUDIO_IDLE;
static uint32_t   _audioPhaseStart = 0;
static bool       _chirpSecondNote = false;
static bool       _chirpAscending  = true;
static bool       _alarmHiPhase    = true;

// Internal: start a chirp sequence (used by audioAlertChirp and audioCautionChirp)
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
  _startChirp(true);
}

void audioCautionChirp() {
  if (_audioState == AUDIO_MASTER_ALARM) return;
  _startChirp(false);
}

void audioCautionTone() {
  if (_audioState == AUDIO_MASTER_ALARM) return;
  noTone(AUDIO_PIN);
  _audioPhaseStart = millis();
  _audioState      = AUDIO_CAUTION_TONE;
  tone(AUDIO_PIN, AUDIO_CAUTION_TONE_FREQ);
}

void audioStartAlarm() {
  if (_audioState == AUDIO_MASTER_ALARM) return;
  _alarmHiPhase    = true;
  _audioPhaseStart = millis();
  _audioState      = AUDIO_MASTER_ALARM;
  tone(AUDIO_PIN, AUDIO_ALARM_FREQ_HI);
}

void audioStopAlarm() {
  if (_audioState == AUDIO_MASTER_ALARM) {
    noTone(AUDIO_PIN);
    _audioState = AUDIO_IDLE;
  }
}

void audioSilence() {
  if (_audioState == AUDIO_MASTER_ALARM) {
    noTone(AUDIO_PIN);
    _audioState = AUDIO_IDLE;
  }
}

AudioState audioGetState() {
  return _audioState;
}

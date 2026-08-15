# KerbalDisplayAudio

**Kerbal Controller Mk1 — Audio Feedback Library** · v1.3.0
Non-blocking audio state machine for KCMk1 rev-2 KSP controller display panels (Teensy 4.1).
Part of the KCMk1 controller system.

---

## Overview

KerbalDisplayAudio provides audio feedback for KCMk1 display panels via the Arduino `tone()` API. It manages a priority-ordered state machine of four audio modes — master alarm, caution tone, caution chirp, and alert chirp — with all timing driven by `millis()` so that `loop()` never blocks.

The master alarm tone is modelled on the Space Shuttle Caution & Warning specification: 375 Hz / 1000 Hz alternating at 2.5 Hz. The library drives the tone on command via `audioStartAlarm()` and `audioStopAlarm()`. Condition tracking — which warnings are active, the silence latch, and re-trigger logic — is the responsibility of the calling sketch, not the library.

---

## Hardware

| Pin | Define | Default | Function |
|-----|--------|---------|----------|
| 29 | `AUDIO_PIN` | `KCM_AUDIO_TONE_PIN` (29) | `tone()` output — KCMk1 TONE net (pin 29 → PAM8302A amp input → external speaker) |
| 30 | `AUDIO_EN_PIN` | `KCM_AUDIO_EN_PIN` (30) | Amp enable — TONE_EN net (pin 30) → PAM8302A `/SD`. Driven HIGH while sounding, LOW when idle |

**Pins come from `KCMk1_SystemConfig.h` automatically.** The header does a guarded `#include <KCMk1_SystemConfig.h>` (via `__has_include`) and defaults `AUDIO_PIN` / `AUDIO_EN_PIN` to `KCM_AUDIO_TONE_PIN` / `KCM_AUDIO_EN_PIN`. This resolves in **every** translation unit — including the library's own `.cpp` — which matters because a sketch-level `#define` does *not* reach a separately-compiled library `.cpp`. So sketches just include the header; no pin `#define` is needed:

```cpp
#include <KerbalDisplayAudio.h>   // AUDIO_PIN = 29, AUDIO_EN_PIN = 30 from SystemConfig
```

The `tone()` output has moved with the hardware: rev-1 pin 9 → rev-2 pin 2 → **KC-01-1911 V2.1 pin 29** (pin 9 is now the display backlight). On V2.1 the buzzer stage (S8050) was replaced by a **PAM8302A Class-D amplifier** driving an external 8 Ω speaker with an input volume trim; the amp's active-low `/SD` (net **TONE_EN**, pin 30) lets the library power it down between cues to kill Class-D idle hiss. `/SD` is pulled low on the board, so the amp is OFF until firmware enables it.

For a stand-alone build (no `KCMk1_SystemConfig` on the include path), `AUDIO_PIN` falls back to `29` and `AUDIO_EN_PIN` to `AUDIO_EN_NONE` (enable logic compiles out). Define either macro before the `#include` to force a non-board value.

Sampled audio (voice callouts, stingers) is handled separately by the bundled `KCM_DFPlayer` driver — a DFPlayer Mini on `Serial2` (RX2 = 7 / TX2 = 8, 9600 baud). Its BUSY line is wired to **pin 11** (`KCM_AUDIO_BUSY_PIN`, net AUDIO_BUSY): call `attachBusyPin(KCM_AUDIO_BUSY_PIN)` then `isPlaying()` to poll playback state (BUSY low = clip playing). It is independent of the `tone()` state machine documented here; see `KCM_DFPlayer.h`.

---

## Dependencies

| Dependency | Notes |
|------------|-------|
| `Arduino.h` | `tone()` / `noTone()` — available on all Arduino-compatible targets |
| `KCMk1_SystemConfig.h` | *Optional* — pulled in via `__has_include` for the `AUDIO_PIN` / `AUDIO_EN_PIN` defaults. Absent → literal fallbacks. |

No hard external-library dependency; `KCMk1_SystemConfig` is used only if present.

---

## Configuration

All frequency and timing constants are overrideable. Define them before the `#include`.

| Constant | Default | Description |
|----------|---------|-------------|
| `AUDIO_PIN` | `KCM_AUDIO_TONE_PIN` → `29` | Output pin for `tone()` (from SystemConfig; literal `29` if absent) |
| `AUDIO_EN_PIN` | `KCM_AUDIO_EN_PIN` → `30` | PAM8302A amp-enable pin (active-high; from SystemConfig). `AUDIO_EN_NONE` disables the feature |
| `AUDIO_CHIRP_ALERT_LO` | `880` Hz | Alert chirp first note (A5) |
| `AUDIO_CHIRP_ALERT_HI` | `1109` Hz | Alert chirp second note (C#6) |
| `AUDIO_CHIRP_CAUTION_HI` | `1200` Hz | Caution chirp first note (tritone) |
| `AUDIO_CHIRP_CAUTION_LO` | `849` Hz | Caution chirp second note (tritone) |
| `AUDIO_CHIRP_NOTE_MS` | `120` ms | Duration of each chirp note (240 ms total per chirp) |
| `AUDIO_CAUTION_TONE_FREQ` | `1000` Hz | Caution constant tone frequency |
| `AUDIO_CAUTION_TONE_MS` | `1200` ms | Caution constant tone duration |
| `AUDIO_ALARM_FREQ_LO` | `375` Hz | Master alarm low tone (Space Shuttle C/W spec) |
| `AUDIO_ALARM_FREQ_HI` | `1000` Hz | Master alarm high tone (Space Shuttle C/W spec) |
| `AUDIO_ALARM_PHASE_MS` | `200` ms | Duration of each alarm phase (2.5 Hz alternation) |

---

## Features

### State Machine

The audio state machine has five states, serviced in priority order (high to low):

| Priority | State | Description |
|----------|-------|-------------|
| 1 | `AUDIO_MASTER_ALARM` | Two-tone alternating loop — runs until all conditions clear or silenced |
| 1 | `AUDIO_MASTER_ALARM_SILENCED` | Alarm latched but muted — `noTone()` called, latch held (crew acknowledged) |
| 2 | `AUDIO_CAUTION_TONE` | Constant tone for a fixed duration |
| 3 | `AUDIO_CHIRP` | Two-note sequence (ascending or descending), plays once |
| 4 | `AUDIO_IDLE` | Silent |

Lower-priority sounds are suppressed while a higher-priority state is active. Chirps and caution tones are suppressed whenever the state machine is in `AUDIO_MASTER_ALARM` or `AUDIO_MASTER_ALARM_SILENCED`. A silenced alarm sits in the distinct `AUDIO_MASTER_ALARM_SILENCED` state: the tone is off but the latch is held, so it has not returned to `AUDIO_IDLE`.

### Master Alarm

The library drives the master alarm tone but has no knowledge of which conditions are active or why. Condition tracking, the silence latch, and re-trigger logic all live in the calling sketch (`Audio.ino` in the KCMk1 Annunciator). The sketch calls `audioStartAlarm()` when the first condition becomes active and `audioStopAlarm()` when the last condition clears.

The sketch is responsible for:
- Maintaining a bitmask of which alarm conditions are currently active
- Calling `audioStartAlarm()` when the mask transitions from 0 to non-zero
- Calling `audioStopAlarm()` when the mask transitions from non-zero to 0
- Managing a silence latch that prevents restart for existing conditions after `audioSilence()`
- Restarting the alarm (and clearing the latch) if a new condition fires while silenced

### API

`setupAudio()` — configures `AUDIO_PIN` as output and silences it, and (when `AUDIO_EN_PIN` is set) configures the amp-enable pin as output and shuts the amp down. Call once from `setup()`.

`updateAudio()` — services all audio timing. Call every `loop()` iteration. Returns immediately if audio is idle.

`audioAlertChirp()` — plays a two-note ascending sequence (A5 → C#6). Signals a positive event such as an altitude or velocity threshold crossing, or orbital insertion. Suppressed if master alarm is active.

`audioCautionChirp()` — plays a two-note descending sequence (tritone interval). Signals a newly-set caution condition such as entering atmosphere or beginning descent. Suppressed if master alarm is active.

`audioCautionTone()` — plays a constant 1000 Hz tone for 1200 ms. Used for the ALT caution condition. Suppressed if master alarm is active.

`audioStartAlarm()` — starts the two-tone master alarm loop. Has no effect if already running. Call when the sketch's alarm condition mask transitions from 0 to non-zero.

`audioStopAlarm()` — ends the master-alarm latch entirely, returning to `AUDIO_IDLE` from either `AUDIO_MASTER_ALARM` or `AUDIO_MASTER_ALARM_SILENCED`. Has no effect if no alarm is latched. Call when the sketch's alarm condition mask transitions from non-zero to 0. Distinct from `audioSilence()`: stop ends the latch; silence only mutes the tone while holding it.

`audioSilence()` — mutes a sounding master alarm by transitioning `AUDIO_MASTER_ALARM` → `AUDIO_MASTER_ALARM_SILENCED` (calls `noTone()` but keeps the alarm latched). It is a no-op unless the master alarm is currently sounding. Use when the crew presses the master-alarm acknowledge button. To end the alarm entirely, use `audioStopAlarm()`.

`audioGetState()` — returns the current `AudioState` enum value (`AUDIO_IDLE`, `AUDIO_CHIRP`, `AUDIO_CAUTION_TONE`, `AUDIO_MASTER_ALARM`, or `AUDIO_MASTER_ALARM_SILENCED`).

---

## Usage

```cpp
#include <KerbalDisplayAudio.h>

// Sketch-side alarm condition tracking (application-specific)
static const uint8_t ALARM_HIGH_TEMP = (1 << 0);
static const uint8_t ALARM_LOW_DV    = (1 << 1);
static uint8_t alarmMask     = 0;
static bool    alarmSilenced = false;

void setAlarmCondition(uint8_t bit, bool on) {
  if (on) {
    bool wasActive = (alarmMask != 0);
    alarmMask |= bit;
    if (!wasActive && !alarmSilenced) {
      audioStartAlarm();
    } else if (wasActive && alarmSilenced) {
      alarmSilenced = false;   // new condition overrides silence latch
      audioStartAlarm();
    } else if (!alarmSilenced && audioGetState() != AUDIO_MASTER_ALARM) {
      audioStartAlarm();       // re-trigger after natural stop
    }
  } else {
    alarmMask &= ~bit;
    if (alarmMask == 0) {
      alarmSilenced = false;
      audioStopAlarm();
    }
  }
}

void setup() {
  setupAudio();
}

void loop() {
  updateAudio();   // must be called every loop pass

  // Drive conditions from telemetry, e.g.:
  // setAlarmCondition(ALARM_HIGH_TEMP, state.maxTemp > tempAlarm);

  // Silence on button press:
  // if (masterAlarmButtonPressed) { alarmSilenced = true; audioSilence(); }
}
```

---

## Version History

| Version | Notes |
|---------|-------|
| **1.3.0** | KC-01-1911 V2.1: the DFPlayer Mini BUSY line is now wired to the Teensy (net AUDIO_BUSY, pin 11 / `KCM_AUDIO_BUSY_PIN`) instead of only driving the front-panel LED. `KCM_DFPlayer` gains `attachBusyPin(pin)` and `isPlaying()` so sketches can poll when a clip finishes (BUSY low = playing). Optional and backward compatible — without a busy pin the driver stays open-loop as before. |
| **1.2.0** | Hardware rev **KC-01-1911 V2.1**: buzzer stage (S8050) replaced by a PAM8302A Class-D amplifier + external speaker with input volume trim. `AUDIO_PIN` (TONE) default moved 2 → 29. New optional `AUDIO_EN_PIN` (net TONE_EN, pin 30 → amp `/SD`): the library drives it HIGH while any cue is sounding and LOW when idle or silenced, powering the amp down to mute Class-D idle hiss between cues. `AUDIO_PIN`/`AUDIO_EN_PIN` now derive from `KCMk1_SystemConfig` automatically (guarded `__has_include`), so they resolve in the library's own `.cpp` — not just the sketch. Backward compatible — no SystemConfig → literals (`29` / `AUDIO_EN_NONE`, enable logic compiles out). |
| **1.1.0** | Hardware rev 2: `AUDIO_PIN` default moved 9 → 2 (TONE buzzer; pin 9 is now the display backlight); sampled audio added via the bundled `KCM_DFPlayer` (DFPlayer Mini on Serial2). New `AUDIO_MASTER_ALARM_SILENCED` state — `audioSilence()` now mutes a sounding alarm by latching into this state (tone off, latch held) instead of returning to `AUDIO_IDLE`, and `audioStopAlarm()` ends the latch from either alarm state. |
| **1.0.1** | `audioSilence()` stops all audio unconditionally (previously only stopped when in `AUDIO_MASTER_ALARM` state). Clarified documentation distinguishing `audioSilence()` from `audioStopAlarm()`. *(Superseded by 1.1.0.)* |
| **1.0.0** | Initial release. Four-state priority machine: `AUDIO_IDLE`, `AUDIO_CHIRP`, `AUDIO_CAUTION_TONE`, `AUDIO_MASTER_ALARM`. Space Shuttle C/W spec alarm (375 Hz / 1000 Hz at 2.5 Hz). `millis()`-based timing throughout; no `delay()`. |

---

## Notes

- **Condition tracking is the sketch's responsibility** — the library only drives the tone. The sketch must maintain the active condition bitmask, the silence latch, and all re-trigger logic. See `Audio.ino` in the KCMk1 Annunciator for the reference implementation.
- **`audioEnabled` gating** — the library has no internal enable/disable flag. The calling sketch is responsible for gating calls behind its own `audioEnabled` flag. In KCMk1 sketches this is done in `ScreenMain.ino`; `audioSilence()` is called unconditionally on scene change and vessel switch regardless of the flag.
- **Single output pin** — all audio is multiplexed through one `tone()` pin. Only one sound plays at a time; the state machine priority order determines which.
- **No blocking** — `updateAudio()` never calls `delay()`. All timing is `millis()`-based. The function is a no-op when idle.
- **`tone()` on Teensy 4.1** — `tone()` uses a hardware timer. `AUDIO_PIN` must be a PWM-capable pin.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.

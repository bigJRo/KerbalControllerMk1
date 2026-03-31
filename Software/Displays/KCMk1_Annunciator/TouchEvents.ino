/***************************************************************************************
   TouchEvents.ino -- Touch input handling for Kerbal Controller Mk1 Annunciator

   Defence layers (ported from KCMk1_InfoDisp):
   1. Count filter   — reject count != 1. No multi-touch gestures on this panel.
   2. Y dead zone    — reject y >= SCREEN_H - TOUCH_DEAD_ZONE (bottom 12px).
                       GSL1680F ghost touches accumulate at y≈479 (screen boundary).
   3. X bounds check — reject x >= SCREEN_W.
   4. Double-read with coordinate stability — re-read after 8ms; discard if count
                       dropped to 0 OR coordinates moved more than TOUCH_JITTER_MAX px.
                       Phantom noise jumps between reads; real touches are stable.
   5. Debounce       — TOUCH_DEBOUNCE_MS suppresses rapid re-fires within a burst.
   6. Require-release — set on any confirmed touch, suppressing burst tail until
                        the INT pin goes low.

   Gestures:
     1-finger on Main -> x < MASTER_W, y < MASTER_H  : silence master alarm audio
                      -> x < MASTER_W, y >= MASTER_H : switch to SOI screen
     1-finger on SOI  -> anywhere: return to Main screen
     Any touch outside flightScene is ignored.
****************************************************************************************/
#include "KCMk1_Annunciator.h"


static const uint32_t TOUCH_DEBOUNCE_MS  = 500;   // ms — main debounce window
static const uint16_t TOUCH_DEAD_ZONE    = 12;    // px — reject y >= SCREEN_H - this
static const uint16_t TOUCH_JITTER_MAX   = 20;    // px — max movement across reads
static const uint16_t SCREEN_W           = 800;
static const uint16_t SCREEN_H           = 480;

static uint32_t _lastTouchTime   = 0;
static bool     _waitForRelease  = false;


void processTouchEvents() {
  if (!isTouched()) {
    _waitForRelease = false;
    return;
  }

  // First read
  lastTouch = readTouch();
  if (lastTouch.count == 0) return;

  // Count filter — reject anything that isn't a single-finger touch
  if (lastTouch.count != 1) {
    if (debugMode) {
      Serial.print(F("Annunciator: Touch discarded (count="));
      Serial.print(lastTouch.count); Serial.println(F(")"));
    }
    return;
  }

  if (_waitForRelease) return;

  uint32_t now = millis();
  if (now - _lastTouchTime < TOUCH_DEBOUNCE_MS) return;

  uint16_t x1 = lastTouch.points[0].x;
  uint16_t y1 = lastTouch.points[0].y;

  // Bounds check
  if (x1 >= SCREEN_W || y1 >= SCREEN_H) return;

  // Y dead zone — bottom-edge phantom rejection
  if (y1 >= SCREEN_H - TOUCH_DEAD_ZONE) {
    if (debugMode) {
      Serial.print(F("Annunciator: Touch discarded (y dead zone y="));
      Serial.print(y1); Serial.println(F(")"));
    }
    return;
  }

  // Double-read after 8ms — confirm touch is real and stable
  delay(8);
  TouchResult confirm = readTouch();
  if (confirm.count == 0) {
    if (debugMode) Serial.println(F("Annunciator: Touch discarded (phantom — count=0 on reread)"));
    return;
  }

  uint16_t x2 = confirm.points[0].x;
  uint16_t y2 = confirm.points[0].y;
  uint16_t dx = (x2 > x1) ? x2 - x1 : x1 - x2;
  uint16_t dy = (y2 > y1) ? y2 - y1 : y1 - y2;
  if (dx > TOUCH_JITTER_MAX || dy > TOUCH_JITTER_MAX) {
    if (debugMode) {
      Serial.print(F("Annunciator: Touch discarded (jitter dx="));
      Serial.print(dx); Serial.print(F(" dy=")); Serial.print(dy); Serial.println(F(")"));
    }
    return;
  }

  // Confirmed — use the more recent coordinate sample
  lastTouch = confirm;
  _lastTouchTime   = now;
  _waitForRelease  = true;

  if (debugMode) {
    Serial.print(F("Annunciator: Touch x="));
    Serial.print(x2); Serial.print(F(" y=")); Serial.println(y2);
  }

  if (!flightScene) return;

  switch (activeScreen) {

    case screen_Main:
      if (x2 < MASTER_W && y2 < MASTER_H) {
        // Master alarm button — silence alarm audio
        if (audioEnabled) {
          alarmSilenced = true;
          audioSilence();
        }
      } else if (x2 < MASTER_W && y2 >= MASTER_H) {
        // SOI area — switch to SOI screen
        switchToScreen(screen_SOI);
        clearTouchISR();
      }
      break;

    case screen_SOI:
      switchToScreen(screen_Main);
      clearTouchISR();
      break;

    default:
      break;
  }
}

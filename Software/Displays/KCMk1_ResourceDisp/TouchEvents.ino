/***************************************************************************************
   TouchEvents.ino -- Touch input handling for Kerbal Controller Mk1 Resource Display

   Defence layers (ported from KCMk1_InfoDisp):
   1. Count filter  — reject count > 1 (multi-finger noise; no multi-touch gestures
                       on this panel).
   2. Y dead zone   — reject y >= SCREEN_H - TOUCH_DEAD_ZONE (bottom 12px).
                       GSL1680F ghost touches accumulate at y≈479 (screen boundary).
   3. X bounds check — reject x >= SCREEN_W.
   4. Double-read with coordinate stability — re-read after 8ms; discard if count
                       dropped to 0 OR coordinates moved more than TOUCH_JITTER_MAX px.
                       Phantom noise jumps between reads; real touches are stable.
   5. Debounce       — TOUCH_DEBOUNCE_MS suppresses rapid re-fires within a burst.
   6. Require-release — set on any confirmed touch, suppressing burst tail until
                        the INT pin goes low.

   Gestures:
     screen_Standby -> no touch response in live mode; any touch advances to Main in demo.
     screen_Main    -> sidebar btn 0 (TOTAL/STAGE) : toggle stageMode
                    -> sidebar btn 1 (DFLT)        : reset slots to default (STD preset)
                    -> sidebar btn 2 (SELECT)      : switch to screen_Select
                    -> sidebar btn 3 (DETAIL)      : switch to screen_Detail
     screen_Select  -> resource grid / presets / BACK / CLEAR : handled by handleSelectTouch()
     screen_Detail  -> selector column / BACK      : handled by handleDetailTouch()
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


static const uint32_t TOUCH_DEBOUNCE_MS  = KCM_TOUCH_DEBOUNCE_MS;     // #3B from SystemConfig
static const uint16_t TOUCH_DEAD_ZONE    = KCM_TOUCH_DEAD_ZONE_PX;    // #3B px — reject y >= SCREEN_H - this
static const uint16_t TOUCH_JITTER_MAX   = KCM_TOUCH_JITTER_MAX_PX;   // #3B px — max movement across reads
// SCREEN_W and SCREEN_H are defined in ScreenMain.ino (shared across tabs)

static uint32_t _lastTouchTime  = 0;
static bool     _waitForRelease = false;


void processTouchEvents() {
  if (!isTouched()) {
    _waitForRelease = false;
    return;
  }

  // First read — quick checks before the 8ms delay
  lastTouch = readTouch();
  if (lastTouch.count == 0) return;

  // Count filter — reject multi-finger (this panel has no multi-touch gestures)
  if (lastTouch.count != 1) {
    if (debugMode) {
      Serial.print(F("ResourceDisp: Touch discarded (count="));
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
      Serial.print(F("ResourceDisp: Touch discarded (y dead zone y="));
      Serial.print(y1); Serial.println(F(")"));
    }
    return;
  }

  // Double-read after 8ms — confirm touch is real and stable
  delay(8);
  TouchResult confirm = readTouch();
  if (confirm.count == 0) {
    if (debugMode) Serial.println(F("ResourceDisp: Touch discarded (phantom — count=0 on reread)"));
    return;
  }

  uint16_t x2 = confirm.points[0].x;
  uint16_t y2 = confirm.points[0].y;
  uint16_t dx = (x2 > x1) ? x2 - x1 : x1 - x2;
  uint16_t dy = (y2 > y1) ? y2 - y1 : y1 - y2;
  if (dx > TOUCH_JITTER_MAX || dy > TOUCH_JITTER_MAX) {
    if (debugMode) {
      Serial.print(F("ResourceDisp: Touch discarded (jitter dx="));
      Serial.print(dx); Serial.print(F(" dy=")); Serial.print(dy); Serial.println(F(")"));
    }
    return;
  }

  // Confirmed — use the more recent coordinate sample
  lastTouch = confirm;
  _lastTouchTime  = now;
  _waitForRelease = true;

  if (debugMode) {
    Serial.print(F("ResourceDisp: Touch x="));
    Serial.print(x2); Serial.print(F(" y=")); Serial.println(y2);
  }

  switch (activeScreen) {

    case screen_Standby:
      // In live mode: no touch response — transitions driven by Simpit SCENE_CHANGE.
      // In demo mode: any touch advances to the main screen.
      if (demoMode) switchToScreen(screen_Main);
      break;

    case screen_Main: {
      int8_t btn = sidebarHitTest(x2, y2);
      switch (btn) {
        case 0:
          stageMode = !stageMode;
          if (debugMode) Serial.println(stageMode
            ? F("ResourceDisp: stageMode STAGE")
            : F("ResourceDisp: stageMode TOTAL"));
          break;
        case 1:
          if (debugMode) Serial.println(F("ResourceDisp: reset slots"));
          initDefaultSlots();
          switchToScreen(screen_Main);
          clearTouchISR();
          break;
        case 2:
          switchToScreen(screen_Select);
          clearTouchISR();
          break;
        case 3:
          switchToScreen(screen_Detail);
          clearTouchISR();
          break;
        default:
          break;
      }
      break;
    }

    case screen_Select:
      handleSelectTouch(x2, y2);
      clearTouchISR();
      break;

    case screen_Detail:
      handleDetailTouch(x2, y2);
      clearTouchISR();
      break;

    default:
      break;
  }
}

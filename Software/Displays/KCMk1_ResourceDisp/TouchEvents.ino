/***************************************************************************************
   TouchEvents.ino -- Touch input handling for Kerbal Controller Mk1 Resource Display

   Touch is the rev-2 FT5316 via KCM_Touch (polled over software I2C; no ISR).

   Defence layers (shared with KCMk1_InfoDisp, the FT5316 reference port):
   1. Count filter  — reject count > 1 (multi-finger noise; no multi-touch gestures
                       on this panel).
   2. Y dead zone   — reject y >= SCREEN_H - TOUCH_DEAD_ZONE (bottom edge band), where
                       boundary ghost touches tend to land.
   3. X bounds check — reject x >= SCREEN_W.
   4. Double-read with coordinate stability — re-read after 8ms; discard if count
                       dropped to 0 OR coordinates moved more than TOUCH_JITTER_MAX px.
                       Phantom noise jumps between reads; real touches are stable.
   5. Debounce       — TOUCH_DEBOUNCE_MS suppresses rapid re-fires within a burst.
   6. Require-release — set on any confirmed touch, suppressing the burst tail until
                        a polled isTouched() reads clear.

   Gestures:
     screen_Standby -> no touch response in live mode; any touch advances to Main in demo.
     screen_Main    -> tap anywhere on a meter            : open Detail on that resource
                    -> touch HELD still BUG_HOLD_MS on a tape : set a reserve bug there,
                       or clear the bug if the touch landed on one
                    -> touch on a bug, then DRAG            : move the bug with the finger
                       (a meter touch is deferred until release, the hold matures, or
                        a drag begins; see the hold state below -- the sidebar keys
                        still fire on touch-down)
                    -> sidebar btn 0 (DFLT)        : reset slots to default (STD preset)
                    -> sidebar btn 1 (SELECT)      : switch to screen_Select
                    -> sidebar btn 2 (DETAIL)      : switch to screen_Detail
                    -> sidebar btn 3 (TTE)         : toggle tteMode (counter row % / time)
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

// Meter hold state. A confirmed touch on a meter does not act at once: it arms this
// record and the outcome is decided later -- on release before BUG_HOLD_MS it is a
// tap (Detail); once the hold matures on a tape it sets the bug at the level FIRST
// touched, or clears the bug the touch landed on, and the release then does nothing.
// A touch that landed on a bug and then travels more than BUG_DRAG_MIN_PX becomes a
// drag: the bug follows the finger until release, and neither hold nor tap fires.
// That travel threshold is the hysteresis; a twitch during a hold stays a hold.
//
// Release is a polled register read over bit-banged I2C, and one read can come back
// empty (or fail outright) while the finger is still down. A release therefore only
// counts once it has persisted for TOUCH_RELEASE_MS; a shorter gap is ignored and the
// hold keeps timing from its original start.
static const uint32_t TOUCH_RELEASE_MS = 80;
static uint32_t _releaseSeenMs = 0;    // first untouched read of the current gap; 0 = none
static bool     _holdActive   = false;
static bool     _holdFired    = false;
static bool     _holdOnTape   = false;
static bool     _holdOnBug    = false;   // touch landed within BUG_GRAB_TOL of a bug
static bool     _holdDragging = false;   // travel exceeded BUG_DRAG_MIN_PX: bug follows finger
static uint32_t _holdStartMs  = 0;
static uint8_t  _holdSlot     = 0;
static float    _holdLevel    = 0.0f;
static uint16_t _holdY0       = 0;       // touch-down row, for the drag threshold


void processTouchEvents() {
  if (!isTouched()) {
    if (_holdActive) {
      uint32_t t = millis();
      if (_releaseSeenMs == 0) { _releaseSeenMs = t; return; }        // gap starts
      if (t - _releaseSeenMs < TOUCH_RELEASE_MS) return;              // may be a glitch
      // Release confirmed. A hold that never matured and never dragged is a tap.
      if (!_holdFired && !_holdDragging) {
        if (debugMode) Serial.println(F("ResourceDisp: meter tap -> Detail"));
        setDetailSlot(_holdSlot);
        switchToScreen(screen_Detail);
        clearTouchISR();
      }
      _holdActive    = false;
      _releaseSeenMs = 0;
    }
    _waitForRelease = false;
    return;
  }

  // Finger still down on an armed meter. A gap that ended before TOUCH_RELEASE_MS is
  // forgotten here. A touch that landed on a bug is watched for travel; past the
  // threshold it becomes a drag and the bug follows every subsequent sample. Only a
  // touch that has not dragged can mature into a hold.
  if (_holdActive) {
    _releaseSeenMs = 0;
    if (_holdOnTape && _holdOnBug) {
      TouchResult tr = readTouch();
      if (tr.count >= 1) {
        uint16_t y  = tr.points[0].y;
        uint16_t dy = (y > _holdY0) ? y - _holdY0 : _holdY0 - y;
        if (!_holdDragging && dy >= BUG_DRAG_MIN_PX) {
          _holdDragging = true;
          if (debugMode) Serial.println(F("ResourceDisp: bug drag started"));
        }
        if (_holdDragging) setMeterBug(_holdSlot, meterLevelAtY(y));
      }
    }
    if (!_holdFired && !_holdDragging && _holdOnTape && millis() - _holdStartMs >= BUG_HOLD_MS) {
      if (_holdOnBug) {
        if (debugMode) Serial.println(F("ResourceDisp: meter hold matured -> bug cleared"));
        clearMeterBug(_holdSlot);
      } else {
        if (debugMode) Serial.println(F("ResourceDisp: meter hold matured -> bug set"));
        setMeterBug(_holdSlot, _holdLevel);
      }
      _holdFired = true;
    }
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
          // DFLT resets to the standard vessel meter set. Disabled on EVA — the EVA
          // set is fixed while a Kerbal is on EVA.
          if (evaActive) {
            if (debugMode) Serial.println(F("ResourceDisp: DFLT ignored (EVA active)"));
          } else {
            if (debugMode) Serial.println(F("ResourceDisp: reset slots"));
            initDefaultSlots();
            switchToScreen(screen_Main);
          }
          clearTouchISR();
          break;
        case 1:
          switchToScreen(screen_Select);
          clearTouchISR();
          break;
        case 2:
          switchToScreen(screen_Detail);
          clearTouchISR();
          break;
        case 3:
          tteMode = !tteMode;
          if (debugMode) Serial.println(tteMode
            ? F("ResourceDisp: counter row TTE")
            : F("ResourceDisp: counter row PERCENT"));
          break;
        default: {
          // Not a sidebar key: arm the meter hold. Tap or bug is decided later.
          bool  onTape = false;
          float level  = 0.0f;
          int8_t m = meterHitTest(x2, y2, onTape, level);
          if (m >= 0) {
            _holdActive    = true;
            _holdFired     = false;
            _holdDragging  = false;
            _holdOnTape    = onTape;
            _holdOnBug     = onTape && meterBugNear((uint8_t)m, level);
            _holdStartMs   = millis();
            _holdSlot      = (uint8_t)m;
            _holdLevel     = level;
            _holdY0        = y2;
            _releaseSeenMs = 0;
            if (debugMode) {
              Serial.print(F("ResourceDisp: meter touch armed slot=")); Serial.print(m);
              Serial.print(F(" tape=")); Serial.println(onTape);
            }
          }
          break;
        }
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

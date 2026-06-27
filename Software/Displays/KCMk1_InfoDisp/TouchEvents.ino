/***************************************************************************************
   TouchEvents.ino -- Touch input for Kerbal Controller Mk1 Information Display

   Defence layers:
   1. ISR flag — touchISR() attached to CTP_INT_PIN RISING captures touches that land
      and lift during long draw calls (SCFT, ACFT). _touchPending persists until
      processTouchEvents() runs, preventing missed touches (Problem A).
   2. Count filter — reject count > MAX_TOUCH_COUNT (1). Multi-finger events on a
      single-button sidebar are never intentional; count>1 is a strong phantom signal.
   3. Y dead zone — reject y >= SCREEN_H - TOUCH_DEAD_ZONE (bottom 12px). GSL1680
      ghost touches from edge noise accumulate at y≈479 (screen boundary). Real
      sidebar touches are always well above this band.
   4. X bounds check — reject x >= SCREEN_W.
   5. Double-read with coordinate stability — re-read after 8ms; reject if count
      dropped to 0 OR if coordinates moved more than TOUCH_JITTER_MAX pixels.
      Skipped for ISR-latched touches (finger already lifted, coordinates static).
      Phantom noise jumps around between reads; real touches are stable.
   6. Debounce 500ms — prevents rapid re-fires within a burst.
   7. Require-release — set on ANY confirmed touch, suppressing the rest of a burst
      until INT goes low.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


static const uint32_t TOUCH_DEBOUNCE_MS  = KCM_TOUCH_DEBOUNCE_MS;     // #3B from SystemConfig
static const uint16_t TOUCH_DEAD_ZONE    = KCM_TOUCH_DEAD_ZONE_PX;    // #3B px — reject y >= SCREEN_H - this
static const uint8_t  MAX_TOUCH_COUNT    = 1;                          // reject multi-finger events
static const uint16_t TOUCH_JITTER_MAX   = KCM_TOUCH_JITTER_MAX_PX;   // #3B px — max coordinate movement across reads

static uint32_t lastTouchTime      = 0;
static uint32_t lastTitleTouchTime = 0;
static bool     _waitForRelease    = false;

// ── Touch pending flag ────────────────────────────────────────────────────────────────
// Set by ISR on CTP_INT_PIN rising edge. Persists across long draw calls so touches
// that land and lift during a heavy redraw (SCFT, ACFT) are not missed.
// Cleared in processTouchEvents() once the event has been handled or rejected.
volatile bool _touchPending = false;

void touchISR() {
    _touchPending = true;
}

// Title bar uses a shorter debounce — toggles are intentional quick taps
static const uint32_t TITLE_DEBOUNCE_MS = KCM_TOUCH_TITLE_DEBOUNCE_MS;  // #3B from SystemConfig


void processTouchEvents() {
  // Accept touch if GPIO is currently high OR if the ISR caught a rising edge
  // that may have already ended (finger lifted during a long draw call).
  bool gpioHigh = isTouched();
  if (!gpioHigh && !_touchPending) {
    _waitForRelease = false;
    return;
  }

  // Clear the pending flag now — if we reject below, the touch is consumed anyway.
  _touchPending = false;

  // First read — use readTouch() directly. When acting on a latched _touchPending
  // the GPIO may already be low, but the GSL1680 holds the last coordinates in its
  // register until overwritten by the next touch, so the read is still valid.
  lastTouch = readTouch();
  if (lastTouch.count == 0) return;

  // Count filter — real single-button presses are always count=1
  if (lastTouch.count != 1) {
    if (debugMode) {
      Serial.print(F("InfoDisp: Touch discarded (count="));
      Serial.print(lastTouch.count);
      Serial.println(F(")"));
    }
    return;
  }

  if (_waitForRelease) return;

  uint32_t now = millis();
  if (now - lastTouchTime < TOUCH_DEBOUNCE_MS) return;

  uint16_t x1 = lastTouch.points[0].x;
  uint16_t y1 = lastTouch.points[0].y;

  // x2/y2 default to first-read coords — updated to confirmed coords if double-read runs
  uint16_t x2 = x1;
  uint16_t y2 = y1;

  // Double-read after 8ms — confirm touch is real and stable.
  // Skip if GPIO is already low (touch was latched by ISR — finger already lifted,
  // coordinates are static in the GSL1680 register, no jitter possible).
  if (gpioHigh) {
    delay(8);
    TouchResult confirm = readTouch();

    if (confirm.count == 0) {
      if (debugMode) Serial.println(F("InfoDisp: Touch discarded (phantom — count=0 on reread)"));
      return;
    }

    // Coordinate stability check — real touches don't jump
    x2 = confirm.points[0].x;
    y2 = confirm.points[0].y;
    uint16_t dx = (x2 > x1) ? x2 - x1 : x1 - x2;
    uint16_t dy = (y2 > y1) ? y2 - y1 : y1 - y2;
    if (dx > TOUCH_JITTER_MAX || dy > TOUCH_JITTER_MAX) {
      if (debugMode) {
        Serial.print(F("InfoDisp: Touch discarded (jitter dx="));
        Serial.print(dx);
        Serial.print(F(" dy="));
        Serial.print(dy);
        Serial.println(F(")"));
      }
      return;
    }

    // Confirmed — use the more recent coordinate sample
    lastTouch = confirm;
  }

  // Bounds and dead zone checks — applied to final coordinates
  if (x2 >= SCREEN_W || y2 >= SCREEN_H) return;
  if (y2 >= SCREEN_H - TOUCH_DEAD_ZONE) {
    if (debugMode) {
      Serial.print(F("InfoDisp: Touch discarded (y dead zone y="));
      Serial.print(y2);
      Serial.println(F(")"));
    }
    return;
  }

  // Title bar hit — checked BEFORE main debounce with its own shorter timer
  // Title bar = y < TITLE_TOP (62px), x < CONTENT_W
  if (y2 < TITLE_TOP && x2 < CONTENT_W) {
    if (now - lastTitleTouchTime >= TITLE_DEBOUNCE_MS) {
      lastTitleTouchTime = now;
      _waitForRelease = true;

      if (debugMode) {
        Serial.print(F("InfoDisp: Title touch x="));
        Serial.print(x2);
        Serial.print(F(" y="));
        Serial.println(y2);
      }

      if (activeScreen == screen_ORB) {
        _orbAdvancedMode = !_orbAdvancedMode;
        // Full chrome redraw — basic and advanced have different layouts.
        // switchToScreen() clears the screen and calls chromeScreen_ORB dispatch
        // which branches on _orbAdvancedMode to the correct layout.
        switchToScreen(screen_ORB);
        clearTouchISR();
        if (debugMode) {
          Serial.print(F("InfoDisp: ORB mode -> "));
          Serial.println(_orbAdvancedMode ? F("ADVANCED") : F("APSIDES"));
        }
      } else if (activeScreen == screen_LNDG) {
        _lndgReentryMode = !_lndgReentryMode;
        for (uint8_t r = 0; r < ROW_COUNT; r++) rowCache[3][r].value = "\x01";
        switchToScreen(screen_LNDG);
        clearTouchISR();
        if (debugMode) {
          Serial.print(F("InfoDisp: LNDG mode -> "));
          Serial.println(_lndgReentryMode ? F("RE-ENTRY") : F("POWERED DESCENT"));
        }
      } else if (activeScreen == screen_LNCH) {
        if (_lnchManualOverride) {
          _lnchManualOverride = false;
        } else {
          _lnchManualOverride = true;
          _lnchOrbitalMode    = !_lnchOrbitalMode;
        }
        for (uint8_t r = 0; r < ROW_COUNT; r++) rowCache[0][r].value = "\x01";
        switchToScreen(screen_LNCH);
        clearTouchISR();
        if (debugMode) {
          Serial.print(F("InfoDisp: LNCH phase -> "));
          Serial.print(_lnchOrbitalMode ? F("CIRCULARIZATION") : F("ASCENT"));
          Serial.println(_lnchManualOverride ? F(" [MANUAL]") : F(" [AUTO]"));
        }
      }
    }
    return;
  }

  // Stamp debounce and require-release immediately — suppresses burst tail
  lastTouchTime = now;
  _waitForRelease = true;

  if (debugMode) {
    Serial.print(F("InfoDisp: Touch count="));
    Serial.print(lastTouch.count);
    Serial.print(F(" x="));
    Serial.print(x2);
    Serial.print(F(" y="));
    Serial.println(y2);
  }

  // Pre-launch board: tap anywhere in content area to advance to ascent mode
  if (activeScreen == screen_LNCH && _lnchPrelaunchMode &&
      x2 < SCREEN_W - SIDEBAR_W && y2 >= TITLE_TOP) {
    _lnchPrelaunchMode      = false;
    _lnchPrelaunchDismissed = true;   // prevent FLIGHT_STATUS from re-entering
    _lnchOrbitalMode        = false;
    _lnchManualOverride     = false;
    for (uint8_t r = 0; r < ROW_COUNT; r++) rowCache[0][r].value = "\x01";
    switchToScreen(screen_LNCH);
    clearTouchISR();
    if (debugMode) Serial.println(F("InfoDisp: Pre-launch board dismissed by tap"));
    return;
  }

  // Sidebar hit test — right-hand SIDEBAR_W column
  if (x2 >= SCREEN_W - SIDEBAR_W) {
    uint8_t btn = (uint8_t)(y2 / (SCREEN_H / SCREEN_COUNT));
    if (btn < SCREEN_COUNT) {
      ScreenType target = (ScreenType)btn;
      if (target != activeScreen) {
        switchToScreen(target);
        clearTouchISR();
      }
    }
  }

}

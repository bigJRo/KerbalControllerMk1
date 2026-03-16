/***************************************************************************************
   TouchEvents.ino -- Touch input handling for Kerbal Controller Mk1 Annunciator
****************************************************************************************/
#include "KCMk1_Annunciator.h"



/***************************************************************************************
   TOUCH DEBOUNCE
****************************************************************************************/
static const uint32_t TOUCH_DEBOUNCE_MS = 200;
static uint32_t       lastTouchTime     = 0;


/***************************************************************************************
   PROCESS TOUCH EVENTS
   Called every loop iteration. Reads touch events and dispatches actions per screen.
   Debounced to TOUCH_DEBOUNCE_MS to prevent rapid repeat triggers.

   Gestures:
     3-finger anywhere    -> if on Standby and flightScene: switch to Main
                            otherwise: ignored (standby commanded by master via idle_state)
     1-finger on Main     -> top-left quadrant (x < MASTER_W, y < MASTER_H):
                            silence master alarm audio
                          -> bottom-left quadrant (x < MASTER_W, y >= MASTER_H):
                            switch to SOI screen
     1-finger on SOI      -> anywhere: return to Main screen
     Any touch outside flightScene is ignored (except 3-finger standby->main).
****************************************************************************************/
void processTouchEvents() {
  if (!isTouched()) return;

  uint32_t now = millis();
  if (now - lastTouchTime < TOUCH_DEBOUNCE_MS) return;

  lastTouch = readTouch();
  if (lastTouch.count == 0) return;

  lastTouchTime = now;

  uint16_t x = lastTouch.points[0].x;
  uint16_t y = lastTouch.points[0].y;

  if (debugMode) {
    Serial.print(F("Annunciator: Touch: "));
    Serial.print(lastTouch.count);
    Serial.print(F(" point(s) -- x0="));
    Serial.print(x);
    Serial.print(F(" y0="));
    Serial.println(y);
  }

  // 3-finger touch: if on standby and in a flight scene, advance to main screen.
  // Note: switching TO standby via touch has been removed -- idle_state (set by
  // the master via I2C) is the sole mechanism for commanding the standby screen.
  if (lastTouch.count >= 3) {
    if (activeScreen == screen_Standby && flightScene) {
      switchToScreen(screen_Main);
    }
    return;
  }

  if (!flightScene) return;

  switch (activeScreen) {

    case screen_Main:
      // Master alarm button area -- silence alarm
      if (x < MASTER_W && y < MASTER_H) {
        if (audioEnabled) audioSilence();
      }
      // SOI area -- switch to SOI screen
      if (x < MASTER_W && y >= MASTER_H) {
        switchToScreen(screen_SOI);
      }
      break;

    case screen_SOI:
      switchToScreen(screen_Main);
      break;

    default:
      break;
  }
}

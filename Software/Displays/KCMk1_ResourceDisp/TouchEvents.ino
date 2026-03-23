/***************************************************************************************
   TouchEvents.ino -- Touch input handling for Kerbal Controller Mk1 Resource Display

   Gestures:
     screen_Standby -> (no touch response — waiting for KSP flight scene)
     screen_Main   -> sidebar btn 0 (TOTAL/STAGE) : toggle stageMode
     screen_Main   -> sidebar btn 1 (DFLT)        : reset slots to default (STD preset)
     screen_Main   -> sidebar btn 2 (SELECT)      : switch to screen_Select
     screen_Main   -> sidebar btn 3 (DETAIL)      : switch to screen_Detail
     screen_Select -> resource grid / presets / BACK / CLEAR : handled by handleSelectTouch()
     screen_Detail -> selector column / BACK      : handled by handleDetailTouch()
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


/***************************************************************************************
   TOUCH DEBOUNCE
****************************************************************************************/
static const uint32_t TOUCH_DEBOUNCE_MS = 350;
static uint32_t       lastTouchTime     = 0;


/***************************************************************************************
   PROCESS TOUCH EVENTS
   Called every loop() iteration.
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
    Serial.print(F("ResourceDisp: Touch "));
    Serial.print(lastTouch.count);
    Serial.print(F("pt x="));
    Serial.print(x);
    Serial.print(F(" y="));
    Serial.println(y);
  }

  switch (activeScreen) {

    case screen_Standby:
      // In live mode: no touch response — transitions driven by Simpit SCENE_CHANGE.
      // In demo mode: any touch advances to the main screen.
      if (demoMode) switchToScreen(screen_Main);
      break;

    case screen_Main: {
      int8_t btn = sidebarHitTest(x, y);
      switch (btn) {
        case 0:
          // TOTAL / STAGE toggle
          stageMode = !stageMode;
          if (debugMode) Serial.println(stageMode
            ? F("ResourceDisp: stageMode STAGE")
            : F("ResourceDisp: stageMode TOTAL"));
          break;

        case 1:
          // DFLT -- restore default slot configuration
          if (debugMode) Serial.println(F("ResourceDisp: reset slots"));
          initDefaultSlots();
          switchToScreen(screen_Main);
          // Clear any ISR flags queued during redraw
          clearTouchISR();
          break;

        case 2:
          // SELECT
          switchToScreen(screen_Select);
          clearTouchISR();
          break;

        case 3:
          // DETAIL
          switchToScreen(screen_Detail);
          clearTouchISR();
          break;

        default:
          break;
      }
      break;
    }

    case screen_Select:
      handleSelectTouch(x, y);
      // Clear ISR flags that may have queued during any full redraws
      clearTouchISR();
      break;

    case screen_Detail:
      handleDetailTouch(x, y);
      clearTouchISR();
      break;

    default:
      break;
  }
}

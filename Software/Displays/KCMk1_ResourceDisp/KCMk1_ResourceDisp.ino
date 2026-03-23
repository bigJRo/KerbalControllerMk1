/********************************************************************************************************************************
  KCMk1_ResourceDisp.ino -- Kerbal Controller Mk1 Resource Display
  Main sketch file. Contains only setup() and loop().
  All application logic is in the companion .ino tabs:
    AAA_Config.ino       -- tunable constants (thresholds, modes, slot config)
    AAA_Globals.ino      -- ResourceSlot struct, display objects, Simpit object, screen state, globals
    Resources.ino        -- resource type definitions, color map, slot initialisation
    ScreenMain.ino       -- main bar graph screen with 4-button sidebar
    ScreenSelect.ino     -- resource selection screen (grid + presets + order panel)
    ScreenDetail.ino     -- numerical resource detail screen (craft/stage values per resource)
    TouchEvents.ino      -- touch debounce and gesture dispatch
    SimpitHandler.ino    -- KerbalSimpit message handler and channel registration (Phase 2)
    Demo.ino             -- demo mode animation (sine-wave resource values, no KSP connection)

  Libraries:
    KerbalDisplayCommon  -- display primitives, BMP loader, touch driver, fonts, system utils
    KerbalDisplayAudio   -- non-blocking audio state machine (not used yet, reserved)
    KerbalSimpit         -- KSP telemetry communication via KerbalSimpit KSP plugin

  Hardware:
    Teensy 4.0, RA8875 800x480 TFT, GSL1680F capacitive touch
    SerialUSB1 → KSP (Simpit), Serial → debug output
    I2C slave at 0x11 (not yet implemented — deferred to integration phase)

  Phase 1: Display framework with demo values and touch-based resource selection. ✓
  Phase 2: Simpit integration for live resource telemetry. ← current
  Phase 3 (future): I2C slave interface to KCMk1 master at address 0x11.

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by Jason Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include "KCMk1_ResourceDisp.h"



void setup() {
  Serial.begin(115200);
  SerialUSB1.begin(115200);
  setKDCDebugMode(debugMode);
  if (debugMode) Serial.println(F("ResourceDisp: startup"));

  setupDisplay(infoDisp, TFT_BLACK);
  if (DISPLAY_ROTATION != 0) infoDisp.setRotation(DISPLAY_ROTATION);
  setupSD();
  setupTouch();

  if (demoMode) {
    if (debugMode) Serial.println(F("ResourceDisp: Demo mode -- Simpit disabled."));
    initDemoMode();
  } else {
    // Live mode: initDefaultSlots() sets the initial slot TYPE configuration
    // (which resources appear on the main screen) without implying any resource
    // is actually present. Values are immediately zeroed so bars start empty.
    // Simpit will populate values once a flight scene is entered.
    // The user's slot selection persists across scene changes — only values are
    // zeroed on SCENE_CHANGE, not the slot type configuration.
    initDefaultSlots();
    for (uint8_t i = 0; i < slotCount; i++) {
      slots[i].current = slots[i].maxVal = slots[i].stageCurrent = slots[i].stageMax = 0.0f;
    }
    initSimpit();
  }

  // Always show standby on boot — BMP splash from SD card.
  // In demo mode, a touch on the standby screen transitions to the main screen.
  // In live mode, SCENE_CHANGE_MESSAGE drives the transition.
  switchToScreen(screen_Standby);
}


void loop() {

  // --- Touch input ---
  processTouchEvents();

  // --- Screen chrome on transition ---
  // Matches Annunciator pattern: all transition logic lives here, not inside updateScreen*.
  // prevScreen == screen_COUNT is the sentinel set by switchToScreen().
  if (prevScreen == screen_COUNT) {
    if (debugMode) {
      const char *names[] = { "Standby", "Main", "Select", "Detail" };
      Serial.print(F("ResourceDisp: screen -> "));
      Serial.println(activeScreen < screen_COUNT ? names[activeScreen] : "?");
    }

    switch (activeScreen) {
      case screen_Standby:
        drawStaticStandby(infoDisp);
        break;
      case screen_Main:
        drawStaticMain(infoDisp);   // also resets _prevLevel / _prevStageMode
        break;
      case screen_Select:
        drawStaticSelect(infoDisp);
        break;
      case screen_Detail:
        drawStaticDetail(infoDisp);
        break;
      default:
        break;
    }
    prevScreen = activeScreen;
  }

  // --- Update resource values ---
  if (demoMode) {
    stepDemoState();
  } else {
    simpit.update();
  }

  // --- Update display ---
  switch (activeScreen) {
    case screen_Standby: updateScreenStandby(infoDisp); break;
    case screen_Main:    updateScreenMain(infoDisp);    break;
    case screen_Select:  updateScreenSelect(infoDisp);  break;
    case screen_Detail:  updateScreenDetail(infoDisp);  break;
    default: break;
  }
}

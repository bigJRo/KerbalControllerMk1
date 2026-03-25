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
    ScreenStandby.ino    -- standby BMP splash screen
    TouchEvents.ino      -- touch debounce and gesture dispatch
    BootScreen.ino       -- terminal-aesthetic BIOS POST boot simulation sequence
    SimpitHandler.ino    -- KerbalSimpit message handler and channel registration
    I2CSlave.ino         -- I2C slave interface to KCMk1 master (Teensy 4.1) at address 0x11
    Demo.ino             -- demo mode animation (sine-wave resource values, no KSP connection)

  Libraries:
    KerbalDisplayCommon  -- display primitives, BMP loader, touch driver, fonts, system utils
    KerbalDisplayAudio   -- audio library (included as dependency; audio not used on this panel)
    KerbalSimpit         -- KSP telemetry communication via KerbalSimpit KSP plugin

  Hardware:
    Teensy 4.0, RA8875 800x480 TFT, GSL1680F capacitive touch
    SerialUSB1 -> KSP (Simpit), Serial -> debug output
    Wire (pins 18/19) -> I2C slave at 0x11 (master Teensy 4.1)

  Phase 1: Display framework with demo values and touch-based resource selection. ✓
  Phase 2: Simpit integration for live resource telemetry. ✓
  Phase 3: I2C slave interface + boot handshake with KCMk1 master. <- current

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
  setupI2CSlave();

  bootSimText(infoDisp);

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

  // Notify the master that initialisation is complete. Build a fresh status
  // packet (simpitConnected / demoMode state is now valid) and assert INT so
  // the master can read it. Then spin until the master sends I2C_REQ_PROCEED.
  // While waiting, keep servicing the I2C receive handler via updateI2CState()
  // so the PROCEED command is actually processed.
  buildI2CPacketAndAssert();
  if (debugMode) Serial.println(F("ResourceDisp: waiting for master PROCEED..."));
  while (!i2cProceedReceived) {
    updateI2CState();
  }
  if (debugMode) Serial.println(F("ResourceDisp: PROCEED received, entering loop."));

  // Always show standby on boot — BMP splash from SD card.
  // In demo mode, a touch on the standby screen transitions to the main screen.
  // In live mode, SCENE_CHANGE_MESSAGE drives the transition.
  switchToScreen(screen_Standby);
}


void loop() {

  // --- Touch input ---
  processTouchEvents();

  // --- I2C slave state update ---
  updateI2CState();

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
    // Handle deferred main screen redraws requested by SimpitHandler.
    // Checked here, after simpit.update() has fully processed all pending messages,
    // so the screen is cleared with the final slot state — not mid-message-batch.
    if (needsMainRedraw && activeScreen == screen_Main) {
      drawStaticMain(infoDisp);
      prevScreen      = screen_Main;
      needsMainRedraw = false;
    }
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

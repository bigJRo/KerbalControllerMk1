/********************************************************************************************************************************
  KCMk1_ResourceDisp.ino -- Kerbal Controller Mk1 Resource Display
  Main sketch file. Contains only setup() and loop().
  All application logic is in the companion .ino tabs:
    AAA_Config.ino       -- tunable constants (thresholds, modes, slot config)
    AAA_Globals.ino      -- ResourceSlot struct, display objects, Simpit object, screen state, globals
    Resources.ino        -- resource type definitions, color map, slot initialisation
    ScreenMain.ino       -- main tape-meter screen with 4-button sidebar
    ScreenMainStrip.ino  -- the Main screen's alert strip and propellant balance indicator
    ScreenEVA.ino        -- the Main screen's EVA layout: 270-degree ring gauges
    ScreenSelect.ino     -- resource selection screen (grid + presets + order panel)
    ScreenDetail.ino     -- numerical resource detail screen (craft/stage values per resource)
    ScreenStandby.ino    -- standby BMP splash screen
    TouchEvents.ino      -- touch debounce and gesture dispatch
    BootScreen.ino       -- terminal-aesthetic BIOS POST boot simulation sequence
    SimpitHandler.ino    -- KerbalSimpit message handler and channel registration
    I2CSlave.ino         -- I2C slave interface to KCMk1 master (Teensy 4.1) at address 0x11
    Demo.ino             -- demo mode animation (sine-wave resource values, no KSP connection)
    Sampling.ino         -- per-slot rate/trend sampling shared by the Main and Detail screens

  Libraries:
    KerbalDisplayCommon  -- display primitives, BMP loader, fonts, system utils (RA8876/KCM_TFT)
    KCM_Touch            -- FT5316 capacitive touch driver
    KerbalDisplayAudio   -- audio library (included as dependency; audio not used on this panel)
    KerbalSimpit         -- KSP telemetry communication via KerbalSimpit KSP plugin

  Hardware (rev 2):
    Teensy 4.1, LT7683 (RA8876-compatible) 1024x600 TFT on a 16-bit 8080 parallel bus (FlexIO3),
    FT5316 capacitive touch (software I2C, pins 4/5)
    SerialUSB1 -> KSP (Simpit), Serial -> debug output
    Wire2 (pins 24/25) -> I2C slave at 0x11 (master Teensy 4.1)

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
  if (debugMode) {
    uint32_t t = millis();
    while (!Serial && (millis() - t < 2000)) {}
    Serial.print(F("ResourceDisp: v"));
    Serial.print(SKETCH_VERSION_MAJOR); Serial.print('.');
    Serial.print(SKETCH_VERSION_MINOR); Serial.print('.');
    Serial.println(SKETCH_VERSION_PATCH);
  }

  setupDisplay(infoDisp, TFT_BLACK);
  if (DISPLAY_ROTATION != 0) infoDisp.setRotation(DISPLAY_ROTATION);
  setupSD();
  setupTouch();
  setupI2CSlave();
  persistLoad();   // vessel memory and the TTE toggle from EEPROM (before any screen reads them)

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
    initDefaultSlots();   // live mode: sets slot TYPES; values already zeroed within
    initSimpit();
  }

  // Notify the master that initialisation is complete. Build a fresh status
  // packet (simpitConnected / demoMode state is now valid) and assert INT so
  // the master can read it. Then spin until the master sends I2C_REQ_PROCEED.
  // While waiting, keep servicing the I2C receive handler via updateI2CState()
  // so the PROCEED command is actually processed.
  // Skip entirely in standalone test mode — no master is present to send PROCEED,
  // so proceed straight past the boot screen into loop().
  if (!STANDALONE_TEST) {
    buildI2CPacketAndAssert();
    if (debugMode) Serial.println(F("ResourceDisp: waiting for master PROCEED..."));
    while (!i2cProceedReceived) {
      updateI2CState();
    }
    if (debugMode) Serial.println(F("ResourceDisp: PROCEED received, entering loop."));
  } else if (debugMode) {
    Serial.println(F("ResourceDisp: STANDALONE_TEST — skipping master PROCEED handshake."));
  }

  // Demo mode: skip the standby splash and go straight to the main bar screen so
  // the panel self-runs on the bench without needing a touch to advance (mirrors
  // the InfoDisp). Live mode shows the standby splash (BMP from SD) until a
  // SCENE_CHANGE_MESSAGE — or a touch in demo — drives the transition.
  if (demoMode) {
    switchToScreen(screen_Main);
  } else {
    switchToScreen(screen_Standby);
  }
}


// Backup of the vessel bar set captured while NOT on EVA, so the vessel layout can
// be restored when EVA ends. Refreshed every non-EVA frame in loop() below.
static ResourceSlot _evaBackup[MAX_SLOTS];
static uint8_t      _evaBackupCount = 0;

void loop() {

  // --- Touch input ---
  processTouchEvents();

  // --- I2C slave state update ---
  updateI2CState();

  // Snapshot the current (non-EVA) slot config each frame so it can be restored
  // when EVA ends. Taken before simpit.update() so it reflects the settled vessel
  // layout rather than a transient mid-transition state.
  if (!evaActive) {
    memcpy(_evaBackup, slots, sizeof(_evaBackup));
    _evaBackupCount = slotCount;
  }

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

  // --- EVA mode reconcile (after all messages for this frame are processed) ---
  // evaFlag is latched by FLIGHT_STATUS_MESSAGE. On a change, swap the bar set:
  // entering EVA loads the fixed EC/EVA/O2/Food/Water set; leaving restores the
  // vessel layout snapshotted in _evaBackup. switchToScreen(Main) shows the result
  // and forces a clean chrome redraw on the next pass.
  if (evaFlag != evaActive) {
    evaActive = evaFlag;
    if (evaActive) {
      loadEvaSlots();
    } else {
      memcpy(slots, _evaBackup, sizeof(_evaBackup));
      slotCount = _evaBackupCount;
      requestResourceRefresh();
    }
    if (debugMode) {
      Serial.print(F("ResourceDisp: EVA mode -> "));
      Serial.println(evaActive ? F("ON (EVA bar set)") : F("OFF (vessel bars)"));
    }
    switchToScreen(screen_Main);
  }

  // --- Rate and trend sampling (both modes; every screen reads from it) ---
  updateAllSampling();

  // --- Persistence: write a settled layout / bug / toggle change to EEPROM ---
  persistService();

  // --- Update display ---
  switch (activeScreen) {
    case screen_Standby: updateScreenStandby(infoDisp); break;
    case screen_Main:    updateScreenMain(infoDisp);    break;
    case screen_Select:  updateScreenSelect(infoDisp);  break;
    case screen_Detail:  updateScreenDetail(infoDisp);  break;
    default: break;
  }
}

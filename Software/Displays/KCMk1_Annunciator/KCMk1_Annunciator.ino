/********************************************************************************************************************************
  KCMk1_Annunciator.ino -- Kerbal Controller Mk1 Annunciator Panel
  Main sketch file. Contains only setup() and loop().
  All application logic is in the companion .ino tabs:
    KCMk1_Annunciator.h -- types, enums, AppState, C&W constants, extern declarations
    AAA_Config.ino     -- tunable constants (thresholds, modes, C&W numeric values)
    AAA_Globals.ino    -- AppState struct, telemetry flags, display objects, screen state, switchToScreen()
    BootScreen.ino     -- Terminal-aesthetic boot simulation sequence (IBM CP437 font, graphics mode only)
    CautionWarning.ino -- updateCautionWarningState() -- recomputes C&W bits from telemetry each frame
    SimpitHandler.ino  -- Simpit message handler (onSimpitMessage) and channel registration (initSimpit)
    ScreenMain.ino     -- main screen layout constants, static chrome, C&W panel, update pass
    ScreenSOI.ino      -- SOI screen static chrome and update pass
    ScreenStandby.ino  -- standby screen static chrome and update pass
    TouchEvents.ino    -- touch debounce and gesture dispatch (processTouchEvents)
    Audio.ino          -- master alarm condition tracking (ALARM_* bits, updateAlarmMask) and audio wiring notes
    Demo.ino           -- demo mode animation (stepDemoState, initDemoMode)
    I2CSlave.ino       -- I2C slave interface to KCMk1 master (Teensy 4.1) at address 0x10

  Libraries:
    KerbalDisplayCommon  -- display primitives, BMP loader, touch driver, fonts, system utils
    KerbalDisplayAudio   -- non-blocking audio state machine (chirps, caution tone, master alarm)
    KerbalSimpit         -- KSP telemetry communication via KerbalSimpit KSP plugin

  Hardware:
    Teensy 4.0, RA8875 800x480 TFT, GSL1680F capacitive touch, SerialUSB1 -> KSP

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by Jason Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include "KCMk1_Annunciator.h"



void setup() {
  Serial.begin(115200);
  SerialUSB1.begin(115200);
  setKDCDebugMode(debugMode);
  if (debugMode) {
    uint32_t t = millis();
    while (!Serial && (millis() - t < 2000)) {}
    Serial.print(F("Annunciator: v"));
    Serial.print(SKETCH_VERSION_MAJOR); Serial.print('.');
    Serial.print(SKETCH_VERSION_MINOR); Serial.print('.');
    Serial.println(SKETCH_VERSION_PATCH);
  }

  setupDisplay(infoDisp, TFT_BLACK);
  if (DISPLAY_ROTATION != 0) infoDisp.setRotation(DISPLAY_ROTATION);
  setupSD();
  setupTouch();
  setupAudio();
  setupI2CSlave();

  bootSimText(infoDisp);

  if (demoMode) {
    if (debugMode) Serial.println(F("Annunciator: Demo mode -- Simpit disabled."));
    initDemoMode();
  } else {
    initSimpit();
  }

  // Notify the master that initialisation is complete. Build a fresh status
  // packet (simpitConnected / demoMode state is now valid) and assert INT so
  // the master can read it. Then spin until the master sends I2C_REQ_PROCEED.
  // While waiting, keep servicing the I2C receive handler via updateI2CState()
  // so the PROCEED command is actually processed.
  buildI2CPacketAndAssert();
  if (debugMode) Serial.println(F("Annunciator: waiting for master PROCEED..."));
  while (!i2cProceedReceived) {
    updateI2CState();
  }
  if (debugMode) Serial.println(F("Annunciator: PROCEED received, entering loop."));

  infoDisp.fillScreen(TFT_BLACK);
}


void loop() {

  // --- Touch input ---
  processTouchEvents();

  // --- Audio timing ---
  updateAudio();

  // --- I2C slave state update ---
  updateI2CState();

  // --- Screen chrome on transition ---
  if (activeScreen != prevScreen) {
    if (audioEnabled && prevScreen == screen_Main) audioSilence();

    if (debugMode) {
      const char *names[] = { "Standby", "Main", "SOI" };
      Serial.print(F("Annunciator: screen -> "));
      Serial.println(activeScreen < screen_COUNT ? names[activeScreen] : "?");
    }

    switch (activeScreen) {
      case screen_Standby:
        drawStaticStandby(infoDisp);
        break;
      case screen_Main:
        drawStaticMain(infoDisp);
        firstPassOnMain = true;
        // drawStaticMain() syncs prev.gameSOI = state.gameSOI to suppress a
        // redundant SOI label redraw, but the SOI body image and lower data
        // fields still need their first draw. Invalidate prev values for
        // everything drawStaticMain() did NOT draw so the update pass fills them.
        prev.gameSOI    = "\x01";  // force SOI body + image draw on first update
        prev.vesselName = "\x01";  // force vessel name draw
        prev.maxTemp    = state.maxTemp  + 1;
        prev.skinTemp   = state.skinTemp + 1;
        prev.crewCount  = state.crewCount + 1;
        prev.twIndex    = state.twIndex  + 1;
        prev.commNet    = state.commNet  + 1;
        prev.stage      = state.stage    + 1;
        prev.ctrlGrp    = state.ctrlGrp  + 1;
        break;
      case screen_SOI:
        drawStaticSOI(infoDisp);
        drawSOIBody(infoDisp);
        prev.gameSOI = state.gameSOI;  // suppress redundant SOI redraw on first update pass
        break;
      default:
        break;
    }
    prevScreen = activeScreen;
  }

  // --- Update state ---
  if (demoMode) {
    stepDemoState();
  } else {
    simpit.update();
  }

  // --- Update display ---
  switch (activeScreen) {
    case screen_Standby: updateScreenStandby(infoDisp); break;
    case screen_Main:    updateScreenMain(infoDisp);    break;
    case screen_SOI:     updateScreenSOI(infoDisp);     break;
    default:             break;
  }
}

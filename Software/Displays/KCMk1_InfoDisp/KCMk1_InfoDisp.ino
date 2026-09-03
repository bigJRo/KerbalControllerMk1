/********************************************************************************************************************************
  KCMk1_InfoDisp.ino -- Kerbal Controller Mk1 Information Display
  Main sketch file. Contains only setup() and loop().
  All application logic is in the companion .ino tabs:
    AAA_Config.ino       -- tunable constants and operating mode flags
    AAA_Globals.ino      -- global state, display object, AppState, switchToScreen(),
                            contextScreen(), drawStandbyScreen()
    AAA_Screens.ino      -- shared screen infrastructure (layout, chrome, value helpers)
    Screen_*.ino         -- per-screen chrome and update functions (one file per screen)
    TouchEvents.ino      -- touch debounce and sidebar navigation dispatch
    Demo.ino             -- demo mode animation (simulated telemetry values)
    SimpitHandler.ino    -- KerbalSimpit message handler and channel registration

  Libraries:
    KerbalDisplayCommon  -- display primitives, BMP loader, touch driver, fonts, system utils
    KerbalDisplayAudio   -- audio library (included as dependency; audio not used on this panel)
    KerbalSimpit         -- KSP telemetry communication via KerbalSimpit KSP plugin

  Hardware:
    Teensy 4.1, LT7683 (RA8876-compatible) 1024x600 TFT, FT5316 capacitive touch
    SerialUSB1 (USB COM port 2) → KSP via KerbalSimpit plugin

  Phase 1: Display framework with 10 screen types, sidebar navigation, demo values. ✓
  Phase 2: Simpit integration for live KSP telemetry. ✓
  Phase 3: I2C slave interface to KCMk1 master. ✓

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by Jason Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include "KCMk1_InfoDisp.h"


void setup() {
  Serial.begin(115200);
  SerialUSB1.begin(115200);
  setKDCDebugMode(debugMode);   // #2 call immediately after serial init, matches Annunciator/ResourceDisp
  // Wait up to 2 seconds for Serial monitor to connect before printing version.
  // Falls through immediately if not connected (production use without monitor).
  if (debugMode) {
    uint32_t t = millis();
    while (!Serial && (millis() - t < 2000)) {}
    Serial.print(F("InfoDisp: v"));
    Serial.print(SKETCH_VERSION_MAJOR);
    Serial.print('.');
    Serial.print(SKETCH_VERSION_MINOR);
    Serial.print('.');
    Serial.println(SKETCH_VERSION_PATCH);
  }

  setupDisplay(infoDisp, TFT_BLACK);
  if (DISPLAY_ROTATION != 0) infoDisp.setRotation(DISPLAY_ROTATION);
  infoDB.begin(infoDisp);   // page 0 = visible, canvas = page 0 (boot screen lands here)
  setupSD();
  setupTouch();   // FT5316 polling driver (KCM_Touch) — no ISR attach in rev-2
  setupI2CSlave();

  bootSimText(infoDisp);

  if (demoMode) {
    // Demo mode: no KSP connection, show live screens immediately
    if (debugMode) Serial.println(F("InfoDisp: Demo mode — Simpit disabled."));
    initDemoMode();
    switchToScreen(SCREEN_HOME);
  } else {
    // Live mode: show standby splash while waiting for Simpit to connect.
    // SCENE_CHANGE_MESSAGE will replace it with the standby or flight screen.
    initSimpit();
    drawStandbyScreen(infoDisp);
    // Request an immediate telemetry refresh on all channels.
    simpit.requestMessageOnChannel(0);
  }

  // Notify master that initialisation is complete, then wait for PROCEED.
  // Skip in standalone test mode — no master present to send PROCEED.
  if (!STANDALONE_TEST) {
    buildI2CPacketAndAssert();
    if (debugMode) Serial.println(F("InfoDisp: waiting for master PROCEED..."));
    while (!i2cProceedReceived) {
      updateI2CState();
    }
    if (debugMode) Serial.println(F("InfoDisp: PROCEED received, entering loop."));
  }
}


void loop() {
  static bool _wasDemo = false;  // tracks previous demoMode to detect runtime switch

  // --- Touch input (active in both modes, ignored when on standby) ---
  // #6 process touch before Simpit so a tap on a new screen lands on the correct
  // screen rather than the previous one (matches Annunciator/ResourceDisp order)
  if (flightScene || demoMode) processTouchEvents();

  // --- I2C slave state update ---
  updateI2CState();

  // --- Simpit telemetry (live mode only) ---
  if (!demoMode) simpit.update();

  // --- Runtime demo→live transition: draw standby splash if not in flight scene ---
  if (_wasDemo && !demoMode && !flightScene) {
    drawStandbyScreen(infoDisp);
    simpit.requestMessageOnChannel(0);
  }
  _wasDemo = demoMode;

  // --- Standby state: splash already presented; nothing to redraw ---
  if (!flightScene && !demoMode) return;

  if (demoMode) stepDemoState();

  // Screen-independent sampling: the ROVER endurance window accumulates whatever
  // screen is up, so it has an answer the moment ROVER is shown.
  rovrEnduranceService();

  // --- Context routing: both ladders run every frame, so the panels follow the
  //     mission rather than only re-pairing at vessel and scene boundaries. Guarded
  //     by the manual latch, a release band per rule and a minimum dwell. ---
  updateContextScreen();

  // --- Screen transition: a screen change forces a one-time full paint (chrome +
  //     values) of the new screen and a telemetry refresh so its values populate
  //     immediately. Steady-state frames redraw only what changed.
  //
  //     This is detected AFTER updateContextScreen(), which is the last thing in the
  //     frame that can change activeScreen. It used to be detected before, so a switch
  //     the context ladder made on this pass was invisible to the test: the frame took
  //     the incremental branch and drew the NEW screen's change-detected values onto a
  //     BTE copy of the OLD screen's chrome, presenting one frame that was half of each
  //     before the next pass repainted it properly. Touch and Simpit switches never hit
  //     it because both run above this point; only the ladder's own switches did, which
  //     is why it showed up as a flash of two screens whenever a panel followed the
  //     mission on its own. ---
  bool screenChanged = (prevScreen != activeScreen);
  if (screenChanged) {
    if (debugMode) {
      Serial.print(F("InfoDisp: screen -> "));
      Serial.println(activeScreen);
    }
    prevScreen = activeScreen;
    if (!demoMode) simpit.requestMessageOnChannel(0);
  }

  // --- Incremental double buffering ---
  //  Screen entry (rare): paint full chrome + all values onto the back page, then
  //  flip. This is the expensive frame (fillScreen + every label re-rasterized).
  //  Steady state (every other frame): BTE-copy the live front page into the back
  //  page so it exactly mirrors what's on screen, then run only the change-detected
  //  updateScreen (a handful of moved gauges / changed values + their erase/repair)
  //  and flip. Text that didn't change is never re-rasterized — it rides along in
  //  the hardware copy. Tear-free (page flip) and artifact-free from tearing (all
  //  drawing happens on the hidden page).
  uint32_t _copyUs = 0, _updateUs = 0, _flipUs = 0, _t = 0;
  // beginFrame/beginFrameCopy install the full-panel canvas; canvasContentRegion()
  // then offsets the origin so screens keep drawing in content space regardless of
  // which side this unit's sidebar is on. drawSidebar() steps out to panel space for
  // its own strip and restores the content region itself. No-op on unit 2.
  if (screenChanged) {
    infoDB.beginFrame(infoDisp);            // canvas -> back page (no copy; we fully paint it)
    canvasContentRegion(infoDisp);
    drawStaticScreen(infoDisp, activeScreen);
    updateScreen(infoDisp, activeScreen);
    updateModeChip(infoDisp);
    infoDB.flip(infoDisp);
  } else {
    infoDB.beginFrameCopy(infoDisp);        // BTE copy front -> back, canvas -> back
    canvasContentRegion(infoDisp);
    if (fpsDiag) { _copyUs = infoDB.lastCopyUs; _t = micros(); }
    updateScreen(infoDisp, activeScreen);
    updateModeChip(infoDisp);  // AUTO / MAN — is this screen the ladder's choice or the pilot's?
    updateSidebar(infoDisp);   // repaint the strip if a key's state colour changed
    if (fpsDiag) { _updateUs = micros() - _t; _t = micros(); }
    infoDB.flip(infoDisp);
    if (fpsDiag) { _flipUs = micros() - _t; } // async geometry completes here (wait-for-GPU)
  }

  // --- Frame-rate / render-time diagnostic (~1 Hz on Serial). Steady-state frames
  //     only (screen-entry full paints are excluded so the average reflects the
  //     incremental cost). copy = BTE front→back; update = change-detected redraw;
  //     flipwait = block until queued geometry finishes. ---
  if (fpsDiag && !screenChanged) {
    static uint32_t _fpsWinStart  = 0;
    static uint16_t _fpsFrames    = 0;
    static uint32_t _fpsCopySum   = 0;
    static uint32_t _fpsUpdateSum = 0;
    static uint32_t _fpsFlipSum   = 0;
    _fpsFrames++;
    _fpsCopySum   += _copyUs;
    _fpsUpdateSum += _updateUs;
    _fpsFlipSum   += _flipUs;

    uint32_t now = millis();
    if (_fpsWinStart == 0) _fpsWinStart = now;
    if (now - _fpsWinStart >= 1000) {
      float fps = _fpsFrames * 1000.0f / (float)(now - _fpsWinStart);
      Serial.print(F("InfoDisp FPS "));      Serial.print(fps, 1);
      Serial.print(F(" | work ms "));
      Serial.print((_fpsCopySum + _fpsUpdateSum + _fpsFlipSum) / _fpsFrames / 1000.0f, 1);
      Serial.print(F(" (copy "));            Serial.print(_fpsCopySum / _fpsFrames / 1000.0f, 1);
      Serial.print(F(" + update "));         Serial.print(_fpsUpdateSum / _fpsFrames / 1000.0f, 1);
      Serial.print(F(" + flipwait "));       Serial.print(_fpsFlipSum / _fpsFrames / 1000.0f, 1);
      Serial.print(F(") | screen "));        Serial.println(activeScreen);
      _fpsWinStart = now; _fpsFrames = 0;
      _fpsCopySum = 0; _fpsUpdateSum = 0; _fpsFlipSum = 0;
    }
  }
}

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
    Teensy 4.0, RA8875 800x480 TFT, GSL1680F capacitive touch
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
    switchToScreen(screen_LNCH);
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

  // --- Screen transition: request a telemetry refresh so the new screen's
  //     values populate immediately. Chrome itself is now redrawn every frame
  //     (Model A), so this no longer gates the chrome draw. ---
  if (prevScreen != activeScreen) {
    if (debugMode) {
      Serial.print(F("InfoDisp: screen -> "));
      Serial.println(activeScreen);
    }
    prevScreen = activeScreen;
    if (!demoMode) simpit.requestMessageOnChannel(0);
  }

  if (demoMode) stepDemoState();

  // --- Model A double buffering: redraw the ENTIRE active screen to the hidden
  //     back page, then flip. drawStaticScreen fillScreens + redraws all chrome
  //     and invalidates the value caches, so updateScreen then repaints every
  //     value. A complete frame each flip → tear-free and free of the
  //     single-buffer overdraw artifacts (ticks through labels, etc.). ---
  uint32_t _renderStart = 0;
  infoDB.beginFrame(infoDisp);          // includes the frame-period spin-wait (idle time)
  if (fpsDiag) _renderStart = micros(); // time only the real work, not the spin
  drawStaticScreen(infoDisp, activeScreen);
  updateScreen(infoDisp, activeScreen);
  infoDB.flip(infoDisp);                // blocks until the GPU finishes, then presents

  // --- Frame-rate / render-time diagnostic (~1 Hz on Serial). ---
  //   FPS         = end-to-end frames/sec (all loop work included). Capped near
  //                 1e6/KCM_FRAME_PERIOD_US (≈50) unless the render itself is slower.
  //   render ms   = drawStatic + updateScreen + flip's wait-for-GPU, i.e. the true
  //                 per-frame cost. Compare against the ~20 ms frame period: if avg
  //                 < 20 we're period-capped (headroom); if > 20 we're render-bound
  //                 and FPS ≈ 1000/render.
  if (fpsDiag) {
    static uint32_t _fpsWinStart  = 0;
    static uint16_t _fpsFrames    = 0;
    static uint32_t _fpsRenderSum = 0;
    static uint32_t _fpsRenderMax = 0;
    static uint32_t _fpsRenderMin = 0xFFFFFFFF;
    uint32_t r = micros() - _renderStart;
    _fpsFrames++;
    _fpsRenderSum += r;
    if (r > _fpsRenderMax) _fpsRenderMax = r;
    if (r < _fpsRenderMin) _fpsRenderMin = r;

    uint32_t now = millis();
    if (_fpsWinStart == 0) _fpsWinStart = now;
    if (now - _fpsWinStart >= 1000) {
      float fps = _fpsFrames * 1000.0f / (float)(now - _fpsWinStart);
      Serial.print(F("InfoDisp FPS "));      Serial.print(fps, 1);
      Serial.print(F(" | render ms avg/min/max "));
      Serial.print(_fpsRenderSum / _fpsFrames / 1000.0f, 2); Serial.print('/');
      Serial.print(_fpsRenderMin / 1000.0f, 2);              Serial.print('/');
      Serial.print(_fpsRenderMax / 1000.0f, 2);
      Serial.print(F(" | screen "));         Serial.println(activeScreen);
      _fpsWinStart = now; _fpsFrames = 0; _fpsRenderSum = 0;
      _fpsRenderMax = 0; _fpsRenderMin = 0xFFFFFFFF;
    }
  }
}

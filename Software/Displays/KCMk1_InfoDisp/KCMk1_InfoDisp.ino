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
  Phase 2: Simpit integration for live KSP telemetry. <- current
  Phase 3: I2C slave interface to KCMk1 master.

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by Jason Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include "KCMk1_InfoDisp.h"


void setup() {
  Serial.begin(115200);
  SerialUSB1.begin(115200);
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
  setKDCDebugMode(debugMode);

  setupDisplay(infoDisp, TFT_BLACK);
  if (DISPLAY_ROTATION != 0) infoDisp.setRotation(DISPLAY_ROTATION);
  setupSD();
  setupTouch();
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
  buildI2CPacketAndAssert();
  if (debugMode) Serial.println(F("InfoDisp: waiting for master PROCEED..."));
  while (!i2cProceedReceived) {
    updateI2CState();
  }
  if (debugMode) Serial.println(F("InfoDisp: PROCEED received, entering loop."));
}


void loop() {
  static bool _wasDemo = false;  // tracks previous demoMode to detect runtime switch

  // --- Simpit telemetry (live mode only) ---
  if (!demoMode) simpit.update();

  // --- I2C slave state update ---
  updateI2CState();

  // --- Touch input (active in both modes, ignored when on standby) ---
  if (flightScene || demoMode) processTouchEvents();

  // --- Runtime demo→live transition: draw standby splash if not in flight scene ---
  if (_wasDemo && !demoMode && !flightScene) {
    drawStandbyScreen(infoDisp);
    simpit.requestMessageOnChannel(0);
  }
  _wasDemo = demoMode;

  // --- Standby state: no screen chrome or value updates needed ---
  if (!flightScene && !demoMode) return;

  // --- Screen chrome on transition ---
  if (prevScreen == screen_COUNT) {
    if (debugMode) {
      Serial.print(F("InfoDisp: screen -> "));
      Serial.println(activeScreen);
    }
    drawStaticScreen(infoDisp, activeScreen);
    prevScreen = activeScreen;
    // Request a full telemetry refresh so the new screen's values populate
    // immediately rather than waiting for the next natural Simpit update.
    if (!demoMode) simpit.requestMessageOnChannel(0);
  }

  // --- Update display values ---
  if (demoMode) stepDemoState();

  updateScreen(infoDisp, activeScreen);
}

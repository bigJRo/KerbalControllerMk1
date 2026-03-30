/********************************************************************************************************************************
  KCMk1_InfoDisp.ino -- Kerbal Controller Mk1 Information Display
  Main sketch file. Contains only setup() and loop().
  All application logic is in the companion .ino tabs:
    AAA_Config.ino       -- tunable constants and operating mode flags
    AAA_Globals.ino      -- global state, display object, AppState, switchToScreen()
    Screens.ino          -- chrome and update functions for all 10 screen types
    TouchEvents.ino      -- touch debounce and sidebar navigation dispatch
    Demo.ino             -- demo mode animation (simulated telemetry values)

  Libraries:
    KerbalDisplayCommon  -- display primitives, BMP loader, touch driver, fonts, system utils
    KerbalDisplayAudio   -- audio library (included as dependency; audio not used on this panel)

  Hardware:
    Teensy 4.0, RA8875 800x480 TFT, GSL1680F capacitive touch

  Phase 1: Display framework with 10 screen types, sidebar navigation, demo values. <- current
  Phase 2: Simpit integration for live KSP telemetry.
  Phase 3: I2C slave interface to KCMk1 master.

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by Jason Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include "KCMk1_InfoDisp.h"


void setup() {
  Serial.begin(115200);
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
  setupTouch();

  initDemoMode();

  // Drain any stale touch events that survived the GSL1680 firmware load
  // (can happen after a warm Teensy reset where the chip retains prior state)
  uint32_t drainStart = millis();
  while (millis() - drainStart < 300) {   // rollover-safe subtraction
    if (isTouched()) readTouch();
  }

  switchToScreen(screen_LNCH);
}


void loop() {

  // --- Touch input ---
  processTouchEvents();

  // --- Screen chrome on transition ---
  if (prevScreen == screen_COUNT) {
    if (debugMode) {
      Serial.print(F("InfoDisp: screen -> "));
      Serial.println(activeScreen);
    }
    drawStaticScreen(infoDisp, activeScreen);
    prevScreen = activeScreen;
  }

  // --- Update display values ---
  if (demoMode) stepDemoState();

  updateScreen(infoDisp, activeScreen);
}

/***************************************************************************************
   Backlight.ino -- the panel's BLK input, PWM on D0 (design §6, D3)

   As the 7" displays (#47, KCM_Display.h kcmBacklight*): flight and standby levels from
   the master's BACKLIGHT frame [0xB8, flight %, standby %, 0], clamped to the
   KCM_BL_MIN_PCT floor; changes ramp over KCM_BL_RAMP_MS for the full range.

   Level:
     flight   in a flight scene (CONTROL bit 2), and always on the boot screen, SWITCH
              ERR, MASTER SILENT, or with any red reading -- an alert is never dimmed
     standby  the status screen outside a flight scene
   Never off: there is no screen saver (D9).
****************************************************************************************/
static uint8_t  g_blFlight  = KCM_BL_BRIGHTNESS_PCT;
static uint8_t  g_blStandby = KCM_BL_STANDBY_PCT;
static uint16_t g_blCur10;          // as driven, tenths of a percent
static uint32_t g_blLastMs;

static uint8_t blClamp(uint8_t pct) {
  return pct < KCM_BL_MIN_PCT ? (uint8_t)KCM_BL_MIN_PCT : (pct > 100 ? (uint8_t)100 : pct);
}

void backlightSetLevels(uint8_t flightPct, uint8_t standbyPct) {
  g_blFlight = blClamp(flightPct);
  g_blStandby = blClamp(standbyPct);
}

void backlightBegin() {
  analogWriteFreq(BL_PWM_HZ);
  analogWriteRange(1000);
  g_blCur10 = (uint16_t)g_blFlight * 10;
  analogWrite(PIN_BL, g_blCur10);
  g_blLastMs = millis();
}

bool backlightWantsFlight(uint32_t now) {
  if (splashShowing(now) || !g_m.bootDone) return true;
  if (masterSilent(now) || g_m.swN > 0) return true;
  if (g_m.ctrl & CTRL_FLIGHT) return true;
  return statusAnyRed();
}

uint8_t backlightTarget(uint32_t now) { return backlightWantsFlight(now) ? g_blFlight : g_blStandby; }
uint8_t backlightPct() { return (uint8_t)((g_blCur10 + 5) / 10); }

void backlightService(uint32_t now) {
  uint32_t dt = now - g_blLastMs;
  g_blLastMs = now;
  uint16_t target10 = (uint16_t)backlightTarget(now) * 10;
  uint32_t step = dt * 1000UL / KCM_BL_RAMP_MS;     // 1000 tenths over the ramp time
  if (step < 1) step = 1;
  uint16_t was = g_blCur10;
  if (g_blCur10 < target10) g_blCur10 = (uint16_t)min((uint32_t)target10, g_blCur10 + step);
  else if (g_blCur10 > target10) g_blCur10 = ((uint32_t)(g_blCur10 - target10) > step) ? (uint16_t)(g_blCur10 - step) : target10;
  if (g_blCur10 != was) analogWrite(PIN_BL, g_blCur10);
}

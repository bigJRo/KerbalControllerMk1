static void name(const char *s) { feed(VESSEL_NAME_MESSAGE, s, (byte)strlen(s)); }
static void ctl(uint8_t cb) { uint8_t c[2] = { cb, 0 }; processI2CCommand(c); }
static bool layoutIs(const ResourceType *t, uint8_t n) {
  if (slotCount != n) return false;
  for (uint8_t i = 0; i < n; i++) {        // same set of bars (Main sorts them)
    bool found = false;
    for (uint8_t j = 0; j < slotCount; j++) if (slots[j].type == t[i]) found = true;
    if (!found) return false;
  }
  return true;
}
static void blStandby_() { scene(1); }
static void blFlight_() { scene(0); }
int main(int, char **) {
  simpitConnected = true;
  scene(0); frames(2);
  name("Kerbal X"); frames(2);
  static const ResourceType mine[] = { RES_LIQUID_FUEL, RES_LIQUID_OX, RES_ELEC_CHARGE };
  slotCount = 0;
  for (ResourceType t : mine) { slots[slotCount] = ResourceSlot(); slots[slotCount].type = t; slotCount++; }
  saveVesselSlots(currentVesselName); frames(2);
  CHECK(layoutIs(mine, 3) && flightScene, "live: the vessel's own 3-bar layout, in flight");
  ctl(0x20 | 0x02); frames(5);                          // PROCEED + demo
  CHECK(demoMode && !layoutIs(mine, 3), "demo: demo's bars");
  scene(1); frames(3);
  CHECK(demoMode && !flightScene && activeScreen == screen_Main, "demo: leaving flight tracked, screen kept");
  name("Kerbal X"); frames(2);
  CHECK(!layoutIs(mine, 3), "demo: telemetry ignored");
  scene(0); frames(2);
  CHECK(demoMode && flightScene, "demo: re-entering flight tracked");
  ctl(0x20); frames(2);                                 // demo off
  CHECK(!demoMode && activeScreen == screen_Main, "demo off in flight: Main");
  name("Kerbal X"); frames(2);
  CHECK(layoutIs(mine, 3), "demo off: the vessel's own layout recalled, not demo's");
  ctl(0x20 | 0x02); frames(3); scene(1); frames(2); ctl(0x20); frames(2);
  CHECK(!demoMode && !flightScene && activeScreen == screen_Standby, "demo off out of flight: Standby");
  backlightChecks(blStandby_, blFlight_);
  return stDone();
}

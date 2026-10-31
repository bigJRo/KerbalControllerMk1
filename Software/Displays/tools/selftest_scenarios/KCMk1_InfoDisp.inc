static void ctl(uint8_t cb) { uint8_t c[2] = { cb, 0 }; processI2CCommand(c); }
int main(int, char **) {
  simpitConnected = true;
  scene(0); frames(2);
  CHECK(flightScene && !demoMode, "live: in flight");
  ctl(0x20 | 0x02); frames(3);
  uint8_t sc = activeScreen;
  scene(1); frames(3);
  CHECK(demoMode, "demo: a scene change does not clear demo");
  CHECK(!flightScene && activeScreen == sc, "demo: leaving flight tracked, screen kept");
  scene(0); frames(2);
  uint8_t vs = 1; feed(VESSEL_CHANGE_MESSAGE, &vs, 1); frames(2);
  CHECK(demoMode && flightScene, "demo: back in flight tracked, still demo");
  ctl(0x20); frames(2);
  CHECK(!demoMode && flightScene && _pendingContextSwitch, "demo off in flight: fresh start, waits for the real vessel");
  ctl(0x20 | 0x02); frames(2); scene(1); frames(2); ctl(0x20); frames(2);
  CHECK(!demoMode && !flightScene && activeScreen == SCREEN_HOME, "demo off out of flight: standby on the home screen");
  return stDone();
}

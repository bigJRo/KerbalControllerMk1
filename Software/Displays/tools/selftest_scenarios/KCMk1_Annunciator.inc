static void ctl(uint8_t cb) { uint8_t c[3] = { cb, 0, 0 }; processI2CCommand(c, 3); }
int main(int, char **) {
  SECTION("demo follows the master's flag");
  simpitConnected = true;
  scene(0); frames(2);
  CHECK(flightScene && activeScreen == screen_Main, "live: Main in flight");
  ctl(0x20 | 0x02); frames(3);
  scene(1); frames(3);
  CHECK(demoMode && !flightScene && activeScreen == screen_Main, "demo: leaving flight tracked, still demo, screen kept");
  scene(0); frames(2);
  uint8_t d = 2; feed(VESSEL_CHANGE_MESSAGE, &d, 1); frames(2);
  ctl(0x20); frames(2);
  CHECK(!demoMode && flightScene && activeScreen == screen_Main, "demo off in flight: Main");
  CHECK(bitRead(state.vesselSituationState, VSIT_DOCKED), "a dock during demo is kept");
  ctl(0x20 | 0x02); frames(2); scene(1); frames(2); ctl(0x20); frames(2);
  CHECK(!demoMode && !flightScene && activeScreen == screen_Standby, "demo off out of flight: Standby");
  SECTION("DOCK tile touch");
  ctl(ST_PROCEED); frames(2); scene(0); frames(4);
  bitClear(state.vesselSituationState, VSIT_DOCKED);
  tap(DOCK_X + DOCK_W / 2, DOCK_Y + DOCK_H / 2);
  CHECK(bitRead(state.vesselSituationState, VSIT_DOCKED), "a tap on DOCK sets it");
  tap(DOCK_X + DOCK_W / 2, DOCK_Y + DOCK_H / 2);
  CHECK(!bitRead(state.vesselSituationState, VSIT_DOCKED), "a second tap clears it");
  tap(DOCK_X + DOCK_W / 2, DOCK_Y + DOCK_H / 2);
  vesselChange(3); frames(2);
  CHECK(!bitRead(state.vesselSituationState, VSIT_DOCKED), "a real undock overrides the tap");
  tap(DOCK_X - 20, DOCK_Y + DOCK_H / 2);
  CHECK(!bitRead(state.vesselSituationState, VSIT_DOCKED) && activeScreen == screen_Main, "a tap beside the tile does nothing");
  scene(1); frames(3);
  tap(DOCK_X + DOCK_W / 2, DOCK_Y + DOCK_H / 2);
  CHECK(!bitRead(state.vesselSituationState, VSIT_DOCKED), "no effect out of flight");
  return stDone();
}

static void ctl(uint8_t cb) { uint8_t c[2] = { cb, 0 }; processI2CCommand(c); }
static void blStandby_() { scene(1); }
static void blFlight_() { scene(0); }
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
  backlightChecks(blStandby_, blFlight_);

  SECTION("input test screen");
  ctl(ST_PROCEED); scene(0); frames(4);
  uint8_t pk[10];
  auto pull = [](uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, int16_t d) {
    touch2(x0, y0, x1, y1); frames(2);
    touch2(x0 - d / 2, y0 + d / 2, x1 - d / 2, y1 + d / 2); frames(2);
    touch2(x0 - d, y0 + d, x1 - d, y1 + d); frames(2);
    touchAt(x0 - d, y0 + d); frames(1); untouch(); frames(3);   // fingers lift one at a time
  };
  uint8_t before = activeScreen;
  pull(900, 60, 980, 110, 200);
  fillI2CPacketBuffer(pk);
  CHECK(inputTestIsOpen() && pk[2] == 18, "two-finger pull from top-right: open, status byte 2 = 18");
  CHECK(activeScreen == before, "the screen underneath is kept");
  uint8_t f0[40] = { 0xAB, 0 }; f0[2] = 0xFF; f0[3] = 0x1F; f0[4] = 0xFF; f0[5] = 0x1F; f0[6] = 0xFF; f0[7] = 0x1F;
  f0[13] = 0x08; f0[25] = 50; f0[34] = 42; f0[38] = 2; f0[39] = 2;
  inputTestFrame(f0, 40); frames(2);
  CHECK(_itHaveIn && _itIn[13] == 0x08 && _itIn[34] == 42, "a page 0 frame is taken");
  ctl(ST_PROCEED | ST_DEMO); frames(2);
  f0[34] = 77;
  memcpy((void *)pushBuf, f0, 40); pushLen = 40; pushReady = true; frames(2);   // the I2C receive path
  CHECK(_itIn[34] == 77, "and in demo too (through the I2C receive buffer)");
  ctl(ST_PROCEED); frames(2);
  uint8_t bad[40] = { 0xAB, 0 }; bad[34] = 99; inputTestFrame(bad, 39); frames(1);
  CHECK(_itIn[34] == 77, "a frame of the wrong length is dropped");
  uint8_t f1[24] = { 0xAB, 1 }; f1[19] = 0x08;   // module 1 (FUNCTION) DISABLED
  inputTestFrame(f1, 24); frames(1);
  CHECK(_itHaveMod && _itMod[19] == 0x08, "page 1 (24 bytes, with the lifecycles) is taken");
  uint8_t f1old[20] = { 0xAB, 1 }; f1old[2] = 0x33;
  inputTestFrame(f1old, 20); frames(1);
  CHECK(_itMod[2] != 0x33, "the old 20-byte page 1 is dropped");
  uint8_t f2[24] = { 0xAB, 2, 0x0B }; f2[12] = 0x09; f2[13] = 7;
  inputTestFrame(f2, 24); frames(1);
  CHECK(_itHaveDsp && _itDsp[2] == 0x0B && _itDsp[13] == 7, "page 2 (the four displays) is taken");
  frames(40);
  CHECK(_itHaveDsp && stubMillis() - _itDspMs >= 1500, "no page 2 for 1.5 s: the DISPLAYS rows go to \"--\"");
  tap(60, 100);
  CHECK(inputTestIsOpen() && activeScreen == before, "a tap while open does nothing");
  pull(700, 60, 780, 110, 200);
  CHECK(inputTestIsOpen(), "two fingers outside the corner zone: no gesture");
  pull(900, 60, 980, 110, 60);
  CHECK(inputTestIsOpen(), "too short a pull: no gesture");
  uint32_t lt = lastTouchTime;
  pull(900, 60, 980, 110, 200);
  fillI2CPacketBuffer(pk);
  CHECK(!inputTestIsOpen() && pk[2] == activeScreen, "the same pull closes it");
  CHECK(lastTouchTime == lt, "the fingers lifting one at a time are not taken as a tap");
  CHECK(prevScreen == activeScreen, "and the screen underneath is repainted in full");
  scene(1); frames(3);
  pull(900, 60, 980, 110, 200);
  CHECK(inputTestIsOpen(), "available on standby too");
  frames(40);
  CHECK(!(_itHaveIn && stubMillis() - _itInMs < 1500), "no frames for 1.5 s: NO DATA");
  pull(900, 60, 980, 110, 200);
  CHECK(!inputTestIsOpen(), "closed again on standby");
  return stDone();
}

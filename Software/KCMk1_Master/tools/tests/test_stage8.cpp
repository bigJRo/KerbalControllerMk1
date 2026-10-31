// Stage 8 selftest: board services — the INA228 set up and read (12 V, current, power,
// the low-supply warning), the EMC2101 set up for direct fan control, the fan curve from
// the board temperature with its hysteresis, rpm from the tach, the Sys Info STATUS frame,
// the `board` console command.
#include "harness.h"
#include "carriers.h"

static void ina(RegDevice &d, float v, float a, float w) {
  const float lsb = 10.0f / 524288.0f;
  uint32_t rv = (uint32_t)(v / 195.3125e-6f + 0.5f) << 4;
  int32_t  ri = (int32_t)(a / lsb + (a < 0 ? -0.5f : 0.5f)) * 16;
  uint32_t rp = (uint32_t)(w / (3.2f * lsb) + 0.5f);
  d.regs[0x05] = { (uint8_t)(rv >> 16), (uint8_t)(rv >> 8), (uint8_t)rv };
  d.regs[0x07] = { (uint8_t)((uint32_t)ri >> 16), (uint8_t)((uint32_t)ri >> 8), (uint8_t)ri };
  d.regs[0x08] = { (uint8_t)(rp >> 16), (uint8_t)(rp >> 8), (uint8_t)rp };
}
static void emc(RegDevice &d, int8_t tempC, uint16_t rpm) {
  d.regs[0x00] = { (uint8_t)tempC };
  uint16_t count = rpm ? (uint16_t)(5400000UL / rpm) : 0xFFFF;
  d.regs[0x46] = { (uint8_t)count }; d.regs[0x47] = { (uint8_t)(count >> 8) };
}
static int fanSetting(RegDevice &d) { return d.regs.count(0x4C) ? d.regs[0x4C][0] : -1; }
static uint16_t le16(const std::vector<uint8_t> &f, int o) { return (uint16_t)(f[o] | f[o + 1] << 8); }

void runStage8() {
  printf("board: power monitor and fan set up at boot\n");
  Rig rig;
  rig.attachAll();
  ina(rig.ina, 12.0f, 1.5f, 18.0f);
  emc(rig.emc, 22, 1800);
  stubSimpit().kspUp = true;
  boot(1);
  bool cal = false;
  for (auto &w : rig.ina.writes) if (w.size() == 3 && w[0] == 0x02 && (w[1] << 8 | w[2]) == 3750) cal = true;
  CHECK(cal, "INA228 SHUNT_CAL = 3750 (15 mOhm, 10 A full scale)");
  CHECK(rig.emc.regs[0x03][0] & 0x04, "EMC2101 TACH input on");
  CHECK((rig.emc.regs[0x4A][0] & 0x20) && rig.emc.regs[0x4D][0] == 0x1F, "EMC2101 direct fan setting, PWM_F 0x1F");

  printf("board: readings and the Sys Info STATUS frame\n");
  runFor(1100);
  const BoardStatus &bs = boardStatus();
  CHECK(bs.powerValid && bs.busV > 11.99f && bs.busV < 12.01f, "12.00 V (%s)", fstr(bs.busV, 3));
  CHECK(bs.currentA > 1.49f && bs.currentA < 1.51f && bs.powerW > 17.9f && bs.powerW < 18.1f, "1.5 A, 18 W");
  CHECK(bs.tempValid && bs.tempC == 22 && fanSetting(rig.emc) == 30 * 63 / 100, "22 C: fan 30 %%");
  CHECK(bs.fanValid && bs.fanRpm >= 1799 && bs.fanRpm <= 1801, "1800 rpm from the tach");
  runFor(600);
  const auto *f = rig.si.last(SI_FRAME_STATUS);
  CHECK(f && f->size() == SI_STATUS_LEN && (*f)[1] == 0x07 && le16(*f, 2) == 1200 && le16(*f, 4) == 1500 &&
        le16(*f, 6) == 180 && (*f)[8] == 22 && le16(*f, 9) >= 1799, "STATUS frame: 12.00 V, 1500 mA, 18.0 W, 22 C, rpm");

  printf("board: the fan curve and its hysteresis\n");
  emc(rig.emc, 35, 2400); runFor(1100);
  CHECK(fanSetting(rig.emc) == 50 * 63 / 100, "35 C: 50 %%");
  emc(rig.emc, 52, 3000); runFor(1100);
  CHECK(fanSetting(rig.emc) == 63, "52 C: 100 %%");
  emc(rig.emc, 49, 3000); runFor(1100);
  CHECK(fanSetting(rig.emc) == 63, "49 C: still 100 %% (2 C hysteresis)");
  emc(rig.emc, 47, 3000); runFor(1100);
  CHECK(fanSetting(rig.emc) == 70 * 63 / 100, "47 C: 70 %%");
  emc(rig.emc, 10, 0); runFor(1100);
  CHECK(fanSetting(rig.emc) == 30 * 63 / 100 && boardStatus().fanRpm == 0, "cold: 30 %%; no tach: 0 rpm");

  printf("board: low supply\n");
  Serial.out.clear();
  ina(rig.ina, 10.6f, 1.0f, 10.6f); runFor(300);
  CHECK(serialHas("supply low: 10.60 V"), "warning below 11 V (always printed)");
  Serial.out.clear(); runFor(600);
  CHECK(!serialHas("supply low"), "once, not every reading");
  ina(rig.ina, 12.0f, 1.0f, 12.0f); runFor(300);

  printf("board: console\n");
  Serial.out.clear(); Serial.stubType("board\n"); runFor(10);
  CHECK(serialHas("12.00 V") && serialHas("rpm"), "board command");

  printf("displays: backlight levels (GitHub #47)\n");
  {
    CarrierSim *cs[4] = { &rig.ann, &rig.id1, &rig.id2, &rig.res };
    bool all = true;
    for (CarrierSim *c : cs) { const auto *f = c->lastFrame(SYNC_BACKLIGHT);
      if (!f || f->size() != BACKLIGHT_LEN || (*f)[1] != BL_FLIGHT_PCT || (*f)[2] != BL_STANDBY_PCT) all = false; }
    CHECK(all, "every 7\" display gets [0xB8, %u, %u, 0]", BL_FLIGHT_PCT, BL_STANDBY_PCT);
    auto n = [&](CarrierSim &c) { int k = 0; for (auto &f : c.cmds) if (f.size() == BACKLIGHT_LEN && f[0] == SYNC_BACKLIGHT) k++; return k; };
    int k0 = n(rig.res);
    runFor(CARRIER_BACKLIGHT_MS + 100);
    CHECK(n(rig.res) == k0 + 1, "repeated every %u ms", (unsigned)CARRIER_BACKLIGHT_MS);
    Serial.out.clear(); Serial.stubType("backlight 35 8\n"); runFor(150);
    const auto *f = rig.id2.lastFrame(SYNC_BACKLIGHT);
    CHECK(f && (*f)[1] == 35 && (*f)[2] == 8 && n(rig.res) == k0 + 2, "console: new levels sent at once");
    Serial.out.clear(); Serial.stubType("backlight 140\n"); runFor(50);
    CHECK(serialHas("0..100") && carrierBacklightFlight() == 35, "an out-of-range level is refused");
    carrierSetBacklight(BL_FLIGHT_PCT, BL_STANDBY_PCT); runFor(150);
  }

  printf("board: devices missing\n");
  for (int a = 0; a < 128; a++) Wire.detach(a);
  rig.attachAll(); Wire.detach(ADDR_INA228); Wire.detach(ADDR_EMC2101);
  rig.si.frames.clear();
  boot(1);
  runFor(1100);
  f = rig.si.last(SI_FRAME_STATUS);
  CHECK(!boardStatus().powerValid && !boardStatus().fanValid && (!f || (*f)[1] == 0), "no readings claimed");
  for (int a = 0; a < 128; a++) Wire.detach(a);
}

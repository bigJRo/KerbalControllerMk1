// Shared selftest harness: simulated devices (Sys Info Display, display carriers,
// register devices, input modules), the CHECK macro, and helpers that boot the master
// and run its loop against them (tools/host/Arduino.h simulates time, pins and the bus).
#pragma once
#include <Arduino.h>
#include <KerbalSimpit.h>
#include <Watchdog_t4.h>
#include <EEPROM.h>
#include <map>
#include <string>
#include "config.h"
#include "state.h"
#include "sysinfo.h"
#include "carriers.h"
#include "ksp_out.h"
#include "board.h"
#include "debug.h"
#include "boot.h"
#include "module_bus.h"
#include "panels.h"
#include <KerbalModuleCommon.h>

void setup();
void loop();

inline int &g_failsRef() { static int f = 0; return f; }
inline int &g_checksRef() { static int c = 0; return c; }
#define CHECK(c, ...) do { g_checksRef()++; if (!(c)) { g_failsRef()++; printf("  FAIL: "); printf(__VA_ARGS__); printf("  (%s:%d)\n", __FILE__, __LINE__); } } while (0)

/*------------------------------------------------------------------------------------
  Simulated devices
------------------------------------------------------------------------------------*/
struct SysInfoSim : SimDevice {
  std::vector<std::vector<uint8_t>> frames;
  bool onWrite(const uint8_t *b, int n, bool) override { frames.push_back(std::vector<uint8_t>(b, b + n)); return true; }
  int onRead(uint8_t *b, int n) override {
    const uint8_t p[4] = { SYNC_SYSINFO, 0x01, 0, 1 };
    int k = n < 4 ? n : 4; memcpy(b, p, k); return k;
  }
  // Boot screen as the display would show it (REPLACE overwrites the previous line).
  std::vector<std::pair<int, std::string>> screen() const {
    std::vector<std::pair<int, std::string>> s;
    for (auto &f : frames) {
      if (f.empty() || f[0] != SI_FRAME_BOOT_LINE || f.size() < 4) continue;
      std::string t((const char *)f.data() + 4, f[3]);
      if ((f[2] & 0x80) && !s.empty()) s.back() = { f[2] & 3, t };
      else s.push_back({ f[2] & 3, t });
    }
    return s;
  }
  int count(uint8_t id) const { int c = 0; for (auto &f : frames) if (!f.empty() && f[0] == id) c++; return c; }
  const std::vector<uint8_t> *last(uint8_t id) const {
    for (auto it = frames.rbegin(); it != frames.rend(); ++it) if (!it->empty() && (*it)[0] == id) return &*it;
    return nullptr;
  }
  bool shows(const char *text) const { for (auto &l : screen()) if (l.second.find(text) != std::string::npos) return true; return false; }
  int sevOf(const char *text) const { for (auto &l : screen()) if (l.second.find(text) != std::string::npos) return l.first; return -1; }
};

struct CarrierSim : SimDevice {
  uint8_t sync, len, flags = 0;
  int line = -1;                           // INT line index, when the test wires one
  std::vector<std::vector<uint8_t>> cmds;
  CarrierSim(uint8_t s, uint8_t l) : sync(s), len(l) {}
  bool onWrite(const uint8_t *b, int n, bool) override {
    cmds.push_back(std::vector<uint8_t>(b, b + n));
    if (line >= 0 && n >= 1 && (b[0] >> 4) == 0x1) stubPin(PIN_INT_LINE[line]) = LOW;   // STATUS: assert INT
    return true;
  }
  int onRead(uint8_t *b, int n) override {
    if (line >= 0) stubPin(PIN_INT_LINE[line]) = HIGH;
    uint8_t p[10] = { sync, flags };
    int k = n < len ? n : len; memcpy(b, p, k); return k;
  }
};

struct RegDevice : SimDevice {
  std::map<uint8_t, std::vector<uint8_t>> regs; uint8_t ptr = 0;
  bool onWrite(const uint8_t *b, int n, bool) override { if (n >= 1) ptr = b[0]; return true; }
  int onRead(uint8_t *b, int n) override {
    auto it = regs.find(ptr); if (it == regs.end()) return 0;
    int k = n < (int)it->second.size() ? n : (int)it->second.size(); memcpy(b, it->second.data(), k); return k;
  }
};

struct Rig {
  SysInfoSim si;
  CarrierSim ann{ SYNC_ANNUNCIATOR, 6 }, id1{ SYNC_INFODISP, 10 }, id2{ SYNC_INFODISP, 10 }, res{ SYNC_RESDISP, 4 };
  RegDevice ina, emc;
  Rig() {
    ina.regs[0x3E] = { 0x54, 0x49 }; ina.regs[0x3F] = { 0x22, 0x81 };
    emc.regs[0xFE] = { 0x5D }; emc.regs[0xFD] = { 0x16 };
  }
  void attachAll() {
    for (int a = 0; a < 128; a++) Wire.detach(a);
    Wire.attach(ADDR_SYSINFO, &si);
    Wire.attach(ADDR_ANNUNCIATOR, &ann); Wire.attach(ADDR_INFODISP_1, &id1);
    Wire.attach(ADDR_INFODISP_2, &id2); Wire.attach(ADDR_RESDISP, &res);
    Wire.attach(ADDR_INA228, &ina); Wire.attach(ADDR_EMC2101, &emc);
  }
};

inline void resetStubs() {
  Serial.out.clear(); Serial.in.clear(); Serial.inPos = 0;
  Wire.log.clear();
  StubSimpit &s = stubSimpit();
  s.initCalls = s.updateCalls = 0; s.registered.clear(); s.sent.clear(); s.handler = nullptr; s.onInit = nullptr;
  stubWdt() = StubWdt();
  for (int p = 0; p < 64; p++) stubPin(p) = HIGH;     // idle INT lines, free bus
  stubPin(PIN_SAFE_ABORT) = stubPin(PIN_SAFE_STAGE) = LOW;
}

inline void boot(uint32_t srsr) {
  resetStubs();
  stubSrsr() = srsr;
  setup();
}

inline void runFor(uint32_t ms, uint32_t step = 5) {
  for (uint32_t t = 0; t < ms; t += step) { stubMillis() += step; loop(); }
}

inline void inbound(byte type, const void *p, byte n) {
  if (stubSimpit().handler) stubSimpit().handler(type, (byte *)p, n);
}

inline bool serialHas(const char *s) { return Serial.out.find(s) != std::string::npos; }


/*------------------------------------------------------------------------------------
  Input module simulator (ATtiny816 modules, I2C spec §4–§11). Holds INT low from
  power-on (BOOT_READY) until its first data read; in ACTIVE asserts INT on a change;
  answers CMD_GET_IDENTITY with the next read; logs every command it receives.
------------------------------------------------------------------------------------*/
struct ModuleSim : SimDevice {
  uint8_t addr, type, len, line;
  ModKind kind;
  uint8_t life = KMC_STATUS_BOOT_READY, cnt = 0;
  bool identPending = false, fault = false, intLow = false;
  uint32_t state = 0, change = 0;          // buttons / persistent joystick state
  uint8_t events = 0;                      // rising-edge kinds (display, throttle, encoder)
  int16_t axes[3] = { 0, 0, 0 };
  uint16_t thrPos = 0;                     // throttle: lever position (0..INT16_MAX)
  bool thrPrec = false, touched = false;   // throttle: precision mode, pilot touching
  int16_t value = -1;                      // display modules: last CMD_SET_VALUE
  int8_t enc[2] = { 0, 0 };                // dual encoder: deltas since the last read
  uint8_t led[8] = { 0 };
  int ledWrites = 0;
  std::vector<std::vector<uint8_t>> cmds;
  ModuleSim(uint8_t a, uint8_t t, uint8_t l, uint8_t ln, ModKind k) : addr(a), type(t), len(l), line(ln), kind(k) {}
  int pin() const { return PIN_INT_LINE[line]; }
  void setInt(bool low) { intLow = low; stubPin(pin()) = low ? LOW : HIGH; }
  void powerOn() { life = KMC_STATUS_BOOT_READY; cnt = 0; state = change = 0; events = 0; thrPos = 0; thrPrec = touched = false; setInt(true); }
  void moveTo(uint8_t newLine) { stubPin(pin()) = HIGH; line = newLine; stubPin(pin()) = intLow ? LOW : HIGH; }
  int countCmd(uint8_t c) const { int n = 0; for (auto &v : cmds) if (!v.empty() && v[0] == c) n++; return n; }
  uint8_t lastCmd() const { return cmds.empty() ? 0 : cmds.back()[0]; }
  bool onWrite(const uint8_t *b, int n, bool) override {
    if (n < 1) return true;
    cmds.push_back(std::vector<uint8_t>(b, b + n));
    switch (b[0]) {
      case KMC_CMD_GET_IDENTITY: identPending = true; break;
      case KMC_CMD_SET_LED_STATE: if (n >= 9) { memcpy(led, b + 1, 8); ledWrites++; } break;
      case KMC_CMD_ENABLE:  life = KMC_STATUS_ACTIVE; break;
      case KMC_CMD_DISABLE: life = KMC_STATUS_DISABLED; setInt(false); if (kind == KIND_THROTTLE) thrPos = 0; break;
      case KMC_CMD_SET_THROTTLE:      // refused while disabled or touched (I2C spec §9.4)
        if (n >= 3 && life == KMC_STATUS_ACTIVE && !touched) { thrPos = (uint16_t)(b[1] << 8 | b[2]); signal(); }
        break;
      case KMC_CMD_SET_PRECISION: if (n >= 2) { thrPrec = b[1] != 0; signal(); } break;
      case KMC_CMD_SET_VALUE: if (n >= 3) value = (int16_t)(b[1] << 8 | b[2]); break;
      case KMC_CMD_ACK_FAULT: fault = false; break;
      case KMC_CMD_RESET: life = KMC_STATUS_ACTIVE; setInt(false); break;
    }
    return true;
  }
  int onRead(uint8_t *b, int n) override {
    if (identPending) {
      identPending = false;
      uint8_t id[4] = { type, 1, 2, 0 };
      int k = n < 4 ? n : 4; memcpy(b, id, k); return k;
    }
    uint8_t p[16] = { 0 };
    p[0] = (uint8_t)(life | (fault ? KMC_STATUS_FAULT : 0) | KMC_STATUS_DATA_CHANGED);
    p[1] = type; p[2] = cnt;
    switch (kind) {
      case KIND_BUTTONS: {
        int planes = (len == KMC_BUTTON24_PACKET_SIZE) ? 3 : 2;
        for (int i = 0; i < planes; i++) { p[3 + i] = (uint8_t)(state >> (8 * i)); p[3 + planes + i] = (uint8_t)(change >> (8 * i)); }
        break;
      }
      case KIND_JOYSTICK:
        p[3] = (uint8_t)(state & change); p[4] = (uint8_t)change; p[5] = (uint8_t)state;
        for (int i = 0; i < 3; i++) { p[6 + 2 * i] = (uint8_t)((uint16_t)axes[i] >> 8); p[7 + 2 * i] = (uint8_t)axes[i]; }
        break;
      case KIND_DISPLAY:  p[3] = events; p[4] = events; break;
      case KIND_THROTTLE:
        p[3] = (uint8_t)((life == KMC_STATUS_ACTIVE ? 1 : 0) | (thrPrec ? 2 : 0) | (touched ? 4 : 0));
        p[4] = events; p[5] = (uint8_t)(thrPos >> 8); p[6] = (uint8_t)thrPos;
        break;
      case KIND_ENCODER:  p[3] = events; p[4] = events; p[5] = (uint8_t)enc[0]; p[6] = (uint8_t)enc[1]; enc[0] = enc[1] = 0; break;
    }
    change = 0; events = 0;
    setInt(false);
    int k = n < len ? n : len; memcpy(b, p, k); return k;
  }
  void signal() { cnt++; if (life == KMC_STATUS_ACTIVE) setInt(true); }
  void press(uint32_t bits)   { state |= bits;  change |= bits; signal(); }
  void release(uint32_t bits) { state &= ~bits; change |= bits; signal(); }
  void edge(uint8_t bits)     { events |= bits; signal(); }
  void turn(int i, int8_t d)  { enc[i] = (int8_t)(enc[i] + d); signal(); }
  uint8_t ledOf(uint8_t b) const { return kmcLedPackGet(led, b); }
};

/*------------------------------------------------------------------------------------
  The whole controller: the stage 1 rig plus the 13 input modules on their default
  slots (Inventory §7), and helpers to boot it and take KSP into and out of a flight.
------------------------------------------------------------------------------------*/
#define L_INT(n)  ((uint8_t)((n) - 1))
#define L_BINT(n) ((uint8_t)(12 + (n) - 1))

struct Bench {
  Rig rig;
  ModuleSim mods[MOD_COUNT] = {
    { 0x20, KMC_TYPE_UI_CONTROL,        KMC_BUTTON_PACKET_SIZE,   L_INT(4),   KIND_BUTTONS },
    { 0x21, KMC_TYPE_FUNCTION_CONTROL,  KMC_BUTTON24_PACKET_SIZE, L_INT(8),   KIND_BUTTONS },
    { 0x22, KMC_TYPE_ACTION_CONTROL,    KMC_BUTTON_PACKET_SIZE,   L_INT(1),   KIND_BUTTONS },
    { 0x23, KMC_TYPE_STABILITY_CONTROL, KMC_BUTTON_PACKET_SIZE,   L_BINT(6),  KIND_BUTTONS },
    { 0x24, KMC_TYPE_VEHICLE_CONTROL,   KMC_BUTTON24_PACKET_SIZE, L_BINT(4),  KIND_BUTTONS },
    { 0x25, KMC_TYPE_TIME_CONTROL,      KMC_BUTTON_PACKET_SIZE,   L_BINT(3),  KIND_BUTTONS },
    { 0x26, KMC_TYPE_AUX_CTRL,          KMC_BUTTON_PACKET_SIZE,   L_BINT(7),  KIND_BUTTONS },
    { 0x28, KMC_TYPE_JOYSTICK_ROTATION, KMC_JOYSTICK_PACKET_SIZE, L_BINT(8),  KIND_JOYSTICK },
    { 0x29, KMC_TYPE_JOYSTICK_TRANS,    KMC_JOYSTICK_PACKET_SIZE, L_INT(7),   KIND_JOYSTICK },
    { 0x2A, KMC_TYPE_GPWS_INPUT,        KMC_DISPLAY_PACKET_SIZE,  L_INT(6),   KIND_DISPLAY },
    { 0x2B, KMC_TYPE_PRE_WARP_TIME,     KMC_DISPLAY_PACKET_SIZE,  L_BINT(2),  KIND_DISPLAY },
    { 0x2C, KMC_TYPE_THROTTLE,          KMC_THROTTLE_PACKET_SIZE, L_INT(3),   KIND_THROTTLE },
    { 0x2D, KMC_TYPE_DUAL_ENCODER,      KMC_BUTTON_PACKET_SIZE,   L_BINT(11), KIND_ENCODER },
  };
  void attachAll() {
    rig.attachAll();
    for (auto &m : mods) Wire.attach(m.addr, &m);
    rig.ann.line = L_INT(9); rig.id1.line = L_INT(12); rig.id2.line = L_BINT(9); rig.res.line = L_BINT(12);
  }
  void powerOnAll() { for (auto &m : mods) { m.cmds.clear(); m.powerOn(); } }
  ModuleSim &m(ModId id) { return mods[id]; }
};

// A full power-on: modules in BOOT_READY, master boots, KSP not running. `preset` runs
// after the modules power on and before setup() (switch positions, pins, …).
inline void powerOnBoot(Bench &b, uint32_t srsr = 1, void (*preset)(Bench &) = nullptr) {
  b.attachAll();
  b.rig.si.frames.clear();
  stubSimpit().kspUp = false;
  resetStubs();
  b.powerOnAll();
  if (preset) preset(b);
  stubSrsr() = srsr;
  setup();
}

inline void enterFlight() {
  stubSimpit().kspUp = true;
  runFor(2100);
  byte scene = 0;
  inbound(SCENE_CHANGE_MESSAGE, &scene, 1);
  runFor(20);
}

inline void leaveFlight() {
  byte scene = 1;
  inbound(SCENE_CHANGE_MESSAGE, &scene, 1);
  runFor(20);
}

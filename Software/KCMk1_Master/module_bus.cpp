/***************************************************************************************
   module_bus.cpp — module table, INT line learning, start-up, reads, events, LEDs,
   lifecycle.
****************************************************************************************/
#include "module_bus.h"
#include "config.h"
#include "state.h"
#include "debug.h"
#include "i2c_bus.h"
#include "board.h"
#include "boot.h"
#include "carriers.h"
#include <KerbalModuleCommon.h>
#include <EEPROM.h>
#include <strings.h>

// Line index helpers: INT_n is n-1, B_INT_n is 12 + n-1.
#define L_INT(n)  ((uint8_t)((n) - 1))
#define L_BINT(n) ((uint8_t)(12 + (n) - 1))

struct ModuleDef {
  uint8_t addr, type, len, defLine;
  ModKind kind;
  bool    leds;              // takes CMD_SET_LED_STATE
  const char *name;          // boot line / console (≤ 11 characters)
};

// Inventory §7: the default slot map (A2: INT_1–8, B2: B_INT_1–8, …).
static const ModuleDef kMod[MOD_COUNT] = {
  { 0x20, KMC_TYPE_UI_CONTROL,        KMC_BUTTON_PACKET_SIZE,   L_INT(4),   KIND_BUTTONS,  true,  "UI" },
  { 0x21, KMC_TYPE_FUNCTION_CONTROL,  KMC_BUTTON24_PACKET_SIZE, L_INT(8),   KIND_BUTTONS,  true,  "FUNCTION" },
  { 0x22, KMC_TYPE_ACTION_CONTROL,    KMC_BUTTON_PACKET_SIZE,   L_INT(1),   KIND_BUTTONS,  true,  "ACTION" },
  { 0x23, KMC_TYPE_STABILITY_CONTROL, KMC_BUTTON_PACKET_SIZE,   L_BINT(6),  KIND_BUTTONS,  true,  "STABILITY" },
  { 0x24, KMC_TYPE_VEHICLE_CONTROL,   KMC_BUTTON24_PACKET_SIZE, L_BINT(4),  KIND_BUTTONS,  true,  "VEHICLE" },
  { 0x25, KMC_TYPE_TIME_CONTROL,      KMC_BUTTON_PACKET_SIZE,   L_BINT(3),  KIND_BUTTONS,  true,  "TIME" },
  { 0x26, KMC_TYPE_AUX_CTRL,          KMC_BUTTON_PACKET_SIZE,   L_BINT(7),  KIND_BUTTONS,  true,  "AUX" },
  { 0x28, KMC_TYPE_JOYSTICK_ROTATION, KMC_JOYSTICK_PACKET_SIZE, L_BINT(8),  KIND_JOYSTICK, true,  "ROTATION" },
  { 0x29, KMC_TYPE_JOYSTICK_TRANS,    KMC_JOYSTICK_PACKET_SIZE, L_INT(7),   KIND_JOYSTICK, true,  "TRANSLATION" },
  { 0x2A, KMC_TYPE_GPWS_INPUT,        KMC_DISPLAY_PACKET_SIZE,  L_INT(6),   KIND_DISPLAY,  false, "GPWS" },
  { 0x2B, KMC_TYPE_PRE_WARP_TIME,     KMC_DISPLAY_PACKET_SIZE,  L_BINT(2),  KIND_DISPLAY,  false, "PRE-WARP" },
  { 0x2C, KMC_TYPE_THROTTLE,          KMC_THROTTLE_PACKET_SIZE, L_INT(3),   KIND_THROTTLE, false, "THROTTLE" },
  { 0x2D, KMC_TYPE_DUAL_ENCODER,      KMC_BUTTON_PACKET_SIZE,   L_BINT(11), KIND_ENCODER,  false, "DUAL ENC" },
};

struct CarrierLine { uint8_t owner, defLine; CarrierId id; };
static const CarrierLine kCarrierLine[] = {
  { OWNER_ANNUNCIATOR, L_INT(9),   CAR_ANNUNCIATOR },
  { OWNER_INFODISP_1,  L_INT(12),  CAR_INFODISP_1 },
  { OWNER_INFODISP_2,  L_BINT(9),  CAR_INFODISP_2 },
  { OWNER_RESDISP,     L_BINT(12), CAR_RESDISP },
};
static const uint8_t SYSINFO_DEF_LINE = L_INT(2);

static const uint8_t MAX_PKT = 12;
static const uint8_t MAX_BUS_FAILS = 5;    // consecutive failures before a module counts as gone
static const uint8_t MAX_INPUTS = 24;

struct ModRt {
  bool     present, wrongType;
  uint8_t  fwMaj, fwMin, caps;
  uint8_t  life;             // last reported lifecycle
  uint8_t  lifeSent;         // last lifecycle command sent (KMC_STATUS_*), 0xFF none
  uint8_t  lastCnt;
  bool     haveCnt;
  uint32_t down, heldDone;
  uint32_t downSince[MAX_INPUTS];
  uint8_t  led[KMC_LED_PAYLOAD_SIZE], ledSent[KMC_LED_PAYLOAD_SIZE];
  bool     ledValid;         // ledSent reflects the module
  uint32_t lastLedMs, lastPollMs;
  uint32_t reads, missed, faults, badType;
  uint8_t  busFails;         // consecutive failed transactions
  bool     faultLogged;
  ModHandler handler;
};

static ModRt     g_rt[MOD_COUNT];
static uint8_t   g_owner[INT_LINE_COUNT];
static MapSource g_src[INT_LINE_COUNT];
static uint32_t  g_lastReprobeMs;
static bool      g_bulb;

/***************************************************************************************
   INT map: defaults, EEPROM copy, owner lookups
   EEPROM: [0] 'K' [1] 'I' [2] version 1 [3..26] owner per line [27] checksum
****************************************************************************************/
static const int     EE_BASE = 0;
static const uint8_t EE_VERSION = 1;

static uint8_t eeChecksum(const uint8_t *o) {
  uint8_t s = 0x5A;
  for (uint8_t i = 0; i < INT_LINE_COUNT; i++) s = (uint8_t)((s << 1 | s >> 7) ^ o[i]);
  return s;
}

static void mapDefaults() {
  for (uint8_t i = 0; i < INT_LINE_COUNT; i++) { g_owner[i] = OWNER_NONE; g_src[i] = SRC_DEFAULT; }
  for (uint8_t m = 0; m < MOD_COUNT; m++) g_owner[kMod[m].defLine] = m;
  for (auto &c : kCarrierLine) g_owner[c.defLine] = c.owner;
  g_owner[SYSINFO_DEF_LINE] = OWNER_SYSINFO;
}

static bool mapLoad() {
  if (EEPROM.read(EE_BASE) != 'K' || EEPROM.read(EE_BASE + 1) != 'I' || EEPROM.read(EE_BASE + 2) != EE_VERSION)
    return false;
  uint8_t o[INT_LINE_COUNT];
  for (uint8_t i = 0; i < INT_LINE_COUNT; i++) o[i] = EEPROM.read(EE_BASE + 3 + i);
  if (EEPROM.read(EE_BASE + 3 + INT_LINE_COUNT) != eeChecksum(o)) return false;
  for (uint8_t i = 0; i < INT_LINE_COUNT; i++) { g_owner[i] = o[i]; g_src[i] = SRC_STORED; }
  return true;
}

static void mapSave() {
  // EEPROM.update writes only bytes that differ, so saving an unchanged map costs nothing.
  EEPROM.update(EE_BASE, 'K');
  EEPROM.update(EE_BASE + 1, 'I');
  EEPROM.update(EE_BASE + 2, EE_VERSION);
  for (uint8_t i = 0; i < INT_LINE_COUNT; i++) EEPROM.update(EE_BASE + 3 + i, g_owner[i]);
  EEPROM.update(EE_BASE + 3 + INT_LINE_COUNT, eeChecksum(g_owner));
}

void intMapClearStored() { EEPROM.update(EE_BASE, 0xFF); }

uint8_t intLineOwner(uint8_t line) { return line < INT_LINE_COUNT ? g_owner[line] : OWNER_NONE; }
MapSource intLineSource(uint8_t line) { return line < INT_LINE_COUNT ? g_src[line] : SRC_DEFAULT; }
uint8_t intLineOf(uint8_t owner) {
  for (uint8_t i = 0; i < INT_LINE_COUNT; i++) if (g_owner[i] == owner) return i;
  return 0xFF;
}

static const char *ownerName(uint8_t o) {
  if (o < MOD_COUNT) return kMod[o].name;
  switch (o) {
    case OWNER_ANNUNCIATOR: return "ANNUNCIATOR";
    case OWNER_INFODISP_1:  return "INFO DISP 1";
    case OWNER_INFODISP_2:  return "INFO DISP 2";
    case OWNER_RESDISP:     return "RESOURCE";
    case OWNER_SYSINFO:     return "SYS INFO";
  }
  return "-";
}

// Give `line` to `owner`, taking it from whoever had it and moving the owner off its
// old line. Returns the owner's previous line (0xFF if none).
static uint8_t assignLine(uint8_t owner, uint8_t line) {
  uint8_t old = intLineOf(owner);
  if (old != 0xFF && old != line) { g_owner[old] = OWNER_NONE; g_src[old] = SRC_LEARNED; }
  g_owner[line] = owner;
  g_src[line] = SRC_LEARNED;
  return old;
}

static uint32_t sampleLines() {
  uint32_t lows = 0;
  for (uint8_t i = 0; i < INT_LINE_COUNT; i++)
    if (digitalReadFast(PIN_INT_LINE[i]) == LOW) lows |= 1UL << i;
  return lows;
}

static int8_t onlyBit(uint32_t m) {
  if (m == 0 || (m & (m - 1))) return -1;
  int8_t b = 0;
  while (!(m & 1)) { m >>= 1; b++; }
  return b;
}

static void cmdModules(const char *);
static void cmdIntmap(const char *);

/***************************************************************************************
   Accessors
****************************************************************************************/
bool        modulePresent(ModId id)   { return id < MOD_COUNT && g_rt[id].present; }
bool        moduleHasLeds(ModId id)   { return id < MOD_COUNT && kMod[id].leds; }
uint8_t     moduleAddr(ModId id)      { return id < MOD_COUNT ? kMod[id].addr : 0; }
const char *moduleName(ModId id)      { return id < MOD_COUNT ? kMod[id].name : "?"; }
uint8_t     moduleLifecycle(ModId id) { return id < MOD_COUNT ? g_rt[id].life : 0; }
void        moduleBusSetHandler(ModId id, ModHandler h) { if (id < MOD_COUNT) g_rt[id].handler = h; }

bool moduleWantActive() { return g_master.demo || (g_master.kspConnected && g_vessel.flightScene); }

// A present module that fails MAX_BUS_FAILS transactions in a row is treated as gone;
// the re-probe brings it back when it answers again.
static bool noteBus(ModId id, bool ok) {
  ModRt &r = g_rt[id];
  if (ok) { r.busFails = 0; return true; }
  if (r.present && ++r.busFails >= MAX_BUS_FAILS) {
    r.present = false;
    r.busFails = 0;
    dbgErr(DBG_MODULES, "0x%02X %s stopped answering", kMod[id].addr, kMod[id].name);
  }
  return false;
}

bool moduleCommand(ModId id, uint8_t cmd, const uint8_t *payload, uint8_t n) {
  uint8_t b[1 + KMC_LED_PAYLOAD_SIZE];
  if (n > KMC_LED_PAYLOAD_SIZE) n = KMC_LED_PAYLOAD_SIZE;
  b[0] = cmd;
  if (n) memcpy(b + 1, payload, n);
  wdtCrumb(LS_MODULES, kMod[id].addr);
  return noteBus(id, busWrite(kMod[id].addr, b, (uint8_t)(1 + n)) == BUS_OK);
}

void moduleSetLed(ModId id, uint8_t button, uint8_t state) {
  if (id >= MOD_COUNT || button >= 16) return;
  kmcLedPackSet(g_rt[id].led, button, state);
}
uint8_t moduleLed(ModId id, uint8_t button) {
  return (id < MOD_COUNT && button < 16) ? kmcLedPackGet(g_rt[id].led, button) : 0;
}
void moduleSetAllLeds(ModId id, uint8_t state) {
  for (uint8_t b = 0; b < 16; b++) moduleSetLed(id, b, state);
}

void moduleBulbTest(bool on) {
  if (on == g_bulb) return;
  g_bulb = on;
  uint8_t p = on ? 1 : 0;
  for (uint8_t m = 0; m < MOD_COUNT; m++) if (g_rt[m].present) moduleCommand((ModId)m, KMC_CMD_BULB_TEST, &p, 1);
  dbg(DBG_LIFE, "bulb test %s", on ? "on" : "off");
}

/***************************************************************************************
   Identity and start-up
****************************************************************************************/
static bool readIdentity(ModId id, uint8_t *ident) {
  if (!moduleCommand(id, KMC_CMD_GET_IDENTITY)) return false;
  return busRead(kMod[id].addr, ident, KMC_IDENTITY_SIZE) == BUS_OK;
}

static bool readPacket(ModId id, uint8_t *pkt) {
  wdtCrumb(LS_MODULES, kMod[id].addr);
  g_rt[id].reads++;
  return noteBus(id, busRead(kMod[id].addr, pkt, kMod[id].len) == BUS_OK);
}

static void sendLifecycle(ModId id, uint8_t life) {
  uint8_t cmd = (life == KMC_STATUS_ACTIVE) ? KMC_CMD_ENABLE : KMC_CMD_DISABLE;
  if (!moduleCommand(id, cmd)) return;
  ModRt &r = g_rt[id];
  r.lifeSent = life;
  r.down = r.heldDone = 0;
  r.ledValid = false;                         // an ENABLE re-lights; resend the wanted LEDs
  if (life != KMC_STATUS_ACTIVE) memset(r.ledSent, 0, sizeof r.ledSent);
  dbg(DBG_LIFE, "%s %s", kMod[id].name, life == KMC_STATUS_ACTIVE ? "ENABLE" : "DISABLE");
}

// Brings a module from whatever it reports to the master's lifecycle: a BOOT_READY
// module gets CMD_DISABLE first (spec §11), then the wanted state.
static void startModule(ModId id, uint8_t reported, bool keepActive) {
  ModRt &r = g_rt[id];
  r.life = reported;
  if (kMod[id].leds) {
    uint8_t off[KMC_LED_PAYLOAD_SIZE] = { 0 };
    if (!(keepActive && reported == KMC_STATUS_ACTIVE)) moduleCommand(id, KMC_CMD_SET_LED_STATE, off, KMC_LED_PAYLOAD_SIZE);
  }
  if (keepActive && reported == KMC_STATUS_ACTIVE) {
    r.lifeSent = KMC_STATUS_ACTIVE;           // after a watchdog restart: do not reset it
    r.ledValid = false;
  } else {
    sendLifecycle(id, KMC_STATUS_DISABLED);
  }
}

// The wanted LEDs start backlit (ENABLED): what a module shows by itself on ENABLE.
// The panel tabs set their own states from stage 3.
static void resetRt(ModId id) {
  ModHandler h = g_rt[id].handler;
  memset(&g_rt[id], 0, sizeof g_rt[id]);
  g_rt[id].lifeSent = 0xFF;
  g_rt[id].handler = h;
  if (kMod[id].leds) for (uint8_t b = 0; b < 16; b++) kmcLedPackSet(g_rt[id].led, b, KMC_LED_ENABLED);
}

// Probe one module. With `learn`, also find its INT line. Returns true if present.
static bool probe(ModId id, bool learn, bool keepActive, int8_t *learnedLine) {
  uint8_t ident[KMC_IDENTITY_SIZE];
  ModRt &r = g_rt[id];
  if (learnedLine) *learnedLine = -1;
  if (!readIdentity(id, ident)) { r.present = false; r.busFails = 0; return false; }
  if (ident[0] != kMod[id].type) {
    r.present = false;
    r.wrongType = true;
    dbgErr(DBG_MODULES, "0x%02X %s: wrong type 0x%02X (expected 0x%02X)", kMod[id].addr, kMod[id].name,
           ident[0], kMod[id].type);
    return false;
  }
  r.wrongType = false;
  r.fwMaj = ident[1]; r.fwMin = ident[2]; r.caps = ident[3];

  uint32_t before = sampleLines();
  uint8_t pkt[MAX_PKT];
  bool got = readPacket(id, pkt);
  delay(2);                                   // let the module release INT
  uint32_t rose = before & ~sampleLines();
  if (learn && learnedLine) *learnedLine = onlyBit(rose);
  if (learn && onlyBit(rose) < 0 && rose) dbgErr(DBG_INTMAP, "%s: %d lines rose at once, not learned", kMod[id].name,
                                                 __builtin_popcount(rose));
  r.present = true;
  r.haveCnt = got;
  r.lastCnt = got ? pkt[2] : 0;
  startModule(id, got ? (pkt[0] & KMC_STATUS_LIFECYCLE_MASK) : KMC_STATUS_DISABLED, keepActive);
  return true;
}

void moduleBusInit() {
  for (uint8_t m = 0; m < MOD_COUNT; m++) { g_rt[m].handler = nullptr; resetRt((ModId)m); }
  mapDefaults();
  g_bulb = false;
  g_lastReprobeMs = millis();
  consoleRegister("modules", "each module: present, firmware, line, lifecycle, counters", cmdModules);
  consoleRegister("intmap", "intmap [clear] — INT line owners", cmdIntmap);
}

void moduleBusBoot() {
  bool stored = mapLoad();
  bootLine(SEV_INFO, "INT MAP: %s", stored ? "STORED" : "DEFAULT");
  bool keepActive = boardResetInfo().watchdog;
  uint8_t present = 0;
  for (uint8_t m = 0; m < MOD_COUNT; m++) {
    ModId id = (ModId)m;
    int8_t learned;
    uint8_t mapped = intLineOf(m);
    if (!probe(id, true, keepActive, &learned)) {
      if (g_rt[id].wrongType) bootLine(SEV_ERROR, "%02X %-11s WRONG TYPE", kMod[m].addr, kMod[m].name);
      else                    bootLine(SEV_ERROR, "%02X %-11s MISSING", kMod[m].addr, kMod[m].name);
      continue;
    }
    present++;
    if (learned >= 0 && (uint8_t)learned != mapped) {
      assignLine(m, (uint8_t)learned);
      bootLine(SEV_WARN, "%02X %-11s %-8s LEARNED", kMod[m].addr, kMod[m].name, INT_LINE_NAME[learned]);
      dbg(DBG_INTMAP, "%s on %s (map had %s)", kMod[m].name, INT_LINE_NAME[learned],
          mapped < INT_LINE_COUNT ? INT_LINE_NAME[mapped] : "none");
    } else {
      if (learned >= 0) g_src[learned] = SRC_LEARNED;
      uint8_t l = intLineOf(m);
      bootLine(SEV_OK, "%02X %-11s %-8s OK", kMod[m].addr, kMod[m].name, l < INT_LINE_COUNT ? INT_LINE_NAME[l] : "POLLED");
    }
  }
  mapSave();
  bootLine(present == MOD_COUNT ? SEV_OK : SEV_WARN, "MODULES  %u OF %u", present, (unsigned)MOD_COUNT);
}

void moduleBusLearnCarriers() {
  uint8_t learnedN = 0;
  for (auto &c : kCarrierLine) {
    uint32_t before = sampleLines();
    if (!carrierRequestStatus(c.id)) continue;
    uint32_t fell = 0;
    for (uint8_t t = 0; t < 25 && !fell; t++) { delay(2); fell = sampleLines() & ~before; }
    carrierReadStatus(c.id);                   // releases its INT
    int8_t line = onlyBit(fell);
    if (line < 0) continue;
    learnedN++;
    if (g_owner[line] != c.owner) {
      assignLine(c.owner, (uint8_t)line);
      dbg(DBG_INTMAP, "%s on %s", ownerName(c.owner), INT_LINE_NAME[line]);
    } else {
      g_src[line] = SRC_LEARNED;
    }
  }
  mapSave();
  bootLine(learnedN == 4 ? SEV_OK : SEV_INFO, "DISPLAY INT LINES  %u OF 4 SEEN", learnedN);
}

/***************************************************************************************
   Packets → events
****************************************************************************************/
static void defaultHandler(const ModEvent &e) {
  if (e.held) dbg(DBG_INT, "%s held %06lX", kMod[e.id].name, (unsigned long)e.held);
  else if (e.pressed | e.released | e.tapped)
    dbg(DBG_INT, "%s down %06lX pressed %06lX released %06lX tapped %06lX", kMod[e.id].name,
        (unsigned long)e.down, (unsigned long)e.pressed, (unsigned long)e.released, (unsigned long)e.tapped);
}

static void deliver(const ModEvent &e) {
  ModHandler h = g_rt[e.id].handler ? g_rt[e.id].handler : defaultHandler;
  if (g_rt[e.id].handler && (e.pressed | e.released | e.tapped | e.held)) defaultHandler(e);
  h(e);
}

static uint32_t plane(const uint8_t *p, uint8_t n) {
  uint32_t v = 0;
  for (uint8_t i = 0; i < n; i++) v |= (uint32_t)p[i] << (8 * i);
  return v;
}

static void handlePacket(ModId id, const uint8_t *pkt) {
  ModRt &r = g_rt[id];
  const ModuleDef &d = kMod[id];
  if (pkt[1] != d.type) {
    r.badType++;
    dbgErr(DBG_HEALTH, "%s: packet type 0x%02X, expected 0x%02X", d.name, pkt[1], d.type);
    return;
  }
  uint8_t status = pkt[0], life = status & KMC_STATUS_LIFECYCLE_MASK, cnt = pkt[2];
  r.life = life;
  if (life == KMC_STATUS_BOOT_READY) {          // the module rebooted
    dbgErr(DBG_HEALTH, "%s rebooted (BOOT_READY)", d.name);
    r.haveCnt = true; r.lastCnt = cnt;
    startModule(id, life, false);
    return;
  }
  if (status & KMC_STATUS_FAULT) {
    r.faults++;
    if (!r.faultLogged) dbgErr(DBG_HEALTH, "%s reports a fault", d.name);
    r.faultLogged = true;
    moduleCommand(id, KMC_CMD_ACK_FAULT);
  } else {
    r.faultLogged = false;
  }
  if (r.haveCnt && cnt != r.lastCnt) {
    uint8_t gap = (uint8_t)(cnt - r.lastCnt - 1);
    if (gap) { r.missed += gap; dbg(DBG_HEALTH, "%s: %u packet(s) missed", d.name, gap); }
  }
  r.haveCnt = true; r.lastCnt = cnt;
  if (life != KMC_STATUS_ACTIVE) return;        // DISABLED / SLEEPING: inputs suppressed

  ModEvent e;
  memset(&e, 0, sizeof e);
  e.id = id; e.pkt = pkt; e.len = d.len;
  uint32_t now = 0, chg = 0;
  bool persistent = true;                       // the packet carries the current state
  switch (d.kind) {
    case KIND_BUTTONS: {
      uint8_t n = (d.len == KMC_BUTTON24_PACKET_SIZE) ? 3 : 2;
      now = plane(pkt + 3, n); chg = plane(pkt + 3 + n, n);
      break;
    }
    case KIND_JOYSTICK: now = pkt[5]; chg = pkt[4]; break;   // byte 5 = persistent button state
    case KIND_DISPLAY:  e.pressed = pkt[3] & pkt[4]; persistent = false; break;   // rising edges
    case KIND_THROTTLE: e.pressed = pkt[4]; persistent = false; break;
    case KIND_ENCODER:  e.pressed = pkt[3]; persistent = false; break;
  }
  if (persistent) {
    uint32_t prev = r.down;
    chg |= (now ^ prev);                        // a transition the change plane missed
    e.pressed  = chg & now;                     // includes a release and re-press between reads
    e.released = chg & ~now & prev;
    e.tapped   = chg & ~now & ~prev;
    uint32_t ms = millis();
    for (uint8_t b = 0; b < MAX_INPUTS; b++) {
      uint32_t bit = 1UL << b;
      if (e.pressed & bit) { r.downSince[b] = ms; r.heldDone &= ~bit; }
      if ((e.released | e.tapped) & bit) r.heldDone &= ~bit;
    }
    r.down = now;
    e.down = now;
  }
  deliver(e);
}

static void checkHolds(ModId id) {
  ModRt &r = g_rt[id];
  if (!r.down) return;
  uint32_t ms = millis(), held = 0;
  for (uint8_t b = 0; b < MAX_INPUTS; b++) {
    uint32_t bit = 1UL << b;
    if ((r.down & bit) && !(r.heldDone & bit) && ms - r.downSince[b] >= MODBUS_HOLD_MS) held |= bit;
  }
  if (!held) return;
  r.heldDone |= held;
  ModEvent e;
  memset(&e, 0, sizeof e);
  e.id = id; e.down = r.down; e.held = held;
  deliver(e);
}

/***************************************************************************************
   Service
****************************************************************************************/
void moduleBusService() {
  uint32_t ms = millis();
  uint8_t want = moduleWantActive() ? KMC_STATUS_ACTIVE : KMC_STATUS_DISABLED;
  for (uint8_t m = 0; m < MOD_COUNT; m++) {
    ModId id = (ModId)m;
    ModRt &r = g_rt[m];
    if (!r.present) continue;
    if (r.lifeSent != want) sendLifecycle(id, want);
    uint8_t line = intLineOf(m);
    bool read;
    if (line < INT_LINE_COUNT) read = digitalReadFast(PIN_INT_LINE[line]) == LOW;
    else { read = ms - r.lastPollMs >= MODBUS_POLL_MS; if (read) r.lastPollMs = ms; }
    if (read) {
      uint8_t pkt[MAX_PKT];
      if (readPacket(id, pkt)) {
        if (dbgEnabled(DBG_INT)) dbg(DBG_INT, "%s INT: %02X %02X %02X ...", kMod[m].name, pkt[0], pkt[1], pkt[2]);
        handlePacket(id, pkt);
      }
    }
    if (r.lifeSent == KMC_STATUS_ACTIVE) checkHolds(id);
  }
  if (ms - g_lastReprobeMs >= MODBUS_REPROBE_MS) {
    g_lastReprobeMs = ms;
    for (uint8_t m = 0; m < MOD_COUNT; m++) {
      if (g_rt[m].present) continue;
      if (probe((ModId)m, false, false, nullptr)) {
        g_rt[m].ledValid = false;
        dbgErr(DBG_MODULES, "%s now present", kMod[m].name);
      }
    }
  }
}

void moduleBusFlushLeds() {
  uint32_t ms = millis();
  for (uint8_t m = 0; m < MOD_COUNT; m++) {
    ModRt &r = g_rt[m];
    if (!r.present || !kMod[m].leds || r.lifeSent != KMC_STATUS_ACTIVE) continue;
    if (r.ledValid && memcmp(r.led, r.ledSent, KMC_LED_PAYLOAD_SIZE) == 0) continue;
    if (ms - r.lastLedMs < MODBUS_LED_MIN_MS) continue;
    if (moduleCommand((ModId)m, KMC_CMD_SET_LED_STATE, r.led, KMC_LED_PAYLOAD_SIZE)) {
      memcpy(r.ledSent, r.led, KMC_LED_PAYLOAD_SIZE);
      r.ledValid = true;
      r.lastLedMs = ms;
    }
  }
}

/***************************************************************************************
   Console
****************************************************************************************/
static void cmdModules(const char *) {
  static const char *const lifeName[] = { "ACTIVE", "SLEEP", "DISABLED", "BOOT" };
  conPrintf("Modules (want %s):", moduleWantActive() ? "ACTIVE" : "DISABLED");
  for (uint8_t m = 0; m < MOD_COUNT; m++) {
    const ModRt &r = g_rt[m];
    uint8_t l = intLineOf(m);
    if (!r.present) { conPrintf("  %02X %-11s %s", kMod[m].addr, kMod[m].name, r.wrongType ? "WRONG TYPE" : "missing"); continue; }
    conPrintf("  %02X %-11s fw %u.%u  %-8s %-8s reads %lu missed %lu faults %lu", kMod[m].addr, kMod[m].name,
              r.fwMaj, r.fwMin, l < INT_LINE_COUNT ? INT_LINE_NAME[l] : "polled", lifeName[r.life & 3],
              (unsigned long)r.reads, (unsigned long)r.missed, (unsigned long)r.faults);
  }
}

static void cmdIntmap(const char *args) {
  while (*args == ' ') args++;
  if (strncasecmp(args, "clear", 5) == 0) { intMapClearStored(); conPrintf("stored map erased: defaults at next boot"); return; }
  static const char *const src[] = { "default", "stored", "learned" };
  conPrintf("INT lines (level now, owner, source):");
  for (uint8_t i = 0; i < INT_LINE_COUNT; i++)
    conPrintf("  %-8s %s  %-11s %s", INT_LINE_NAME[i], digitalReadFast(PIN_INT_LINE[i]) == LOW ? "LOW " : "high",
              ownerName(g_owner[i]), g_owner[i] == OWNER_NONE ? "" : src[g_src[i]]);
}


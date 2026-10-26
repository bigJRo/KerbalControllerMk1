/***************************************************************************************
   debug.h — the master's debug log and serial command console (design §10.2).

   Output goes to Serial (USB; KSP's link is SerialUSB1). One line per event:
       [   12.345] KSP   connected
   Errors always print. Everything else prints only while debug is on (UPTLM XMIT, or
   `log … on` at the console), per category. Lines are rate-limited so a flood cannot
   stall the loop, and a write that would block is dropped instead (counted).

   Console: type a command and Enter. Tabs register their own commands with
   consoleRegister(); `help` lists them.
****************************************************************************************/
#pragma once
#include <Arduino.h>

enum DbgCat : uint8_t {
  DBG_MODULES = 0,   // 1  found / missing, wrong type, re-probe
  DBG_INTMAP,        // 2  learned INT lines
  DBG_HEALTH,        // 3  module faults, missed packets
  DBG_SWITCH,        // 4  SWITCH ERR detail, switch faults
  DBG_KSP,           // 5  link, scene, vessel
  DBG_AP,            // 6  autopilots, HOLD
  DBG_INT,           // 7  INT traffic and packets
  DBG_STATE,         // 8  master state changes
  DBG_TIMING,        // 9  loop time
  DBG_TOKSP,         // 10 messages sent to KSP
  DBG_FROMKSP,       // 11 inbound channel health
  DBG_I2C,           // 12 bus errors, recovery
  DBG_DISPLAY,       // 13 display traffic
  DBG_OWNER,         // 14 throttle / attitude ownership, interlocks
  DBG_BOARD,         // 15 boot banner, power, temperature, fan, watchdog
  DBG_LIFE,          // 16 module lifecycle commands
  DBG_KEYS,          // 17 keyboard events
  DBG_STICKS,        // 18 stick positions (demo or debug)
  DBG_BOOT,          //    boot lines (always printed; mirrored from boot.cpp)
  DBG_COUNT
};

void dbgInit();
void dbgService();                                   // console input, rate-limit window, timing line
bool dbgEnabled(DbgCat cat);                         // would a non-error line in cat print?
void dbg(DbgCat cat, const char *fmt, ...);          // gated by debug mode and category
void dbgErr(DbgCat cat, const char *fmt, ...);       // always printed
void dbgLoopTime(uint32_t us);                       // one loop pass, for the timing line
const char *dbgCatName(DbgCat cat);

// A float as text with `dp` decimals, for log and console lines (no float printf needed).
// Eight rotating buffers: up to eight fstr() results in one call are safe.
const char *fstr(float v, uint8_t dp);

// Console output: always printed, not rate-limited (the user asked for it).
void conPrintf(const char *fmt, ...);
typedef void (*ConsoleFn)(const char *args);
bool consoleRegister(const char *name, const char *help, ConsoleFn fn);
void consoleExec(const char *line);                  // also used by the selftest
uint32_t dbgDropped();                               // lines dropped (rate limit or full buffer)

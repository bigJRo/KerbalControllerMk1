/***************************************************************************************
   debug.cpp — debug log and serial console.
****************************************************************************************/
#include "debug.h"
#include "state.h"
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>

static const uint16_t DBG_LINES_PER_S = 200;   // non-error lines per second before suppressing
static const uint8_t  CON_MAX_CMDS    = 16;
static const uint8_t  CON_LINE_MAX    = 64;

static const char *const kCatName[DBG_COUNT] = {
  "MOD", "INTMAP", "HEALTH", "SWITCH", "KSP", "AP", "INT", "STATE", "TIMING",
  "TOKSP", "FRKSP", "I2C", "DISP", "OWNER", "BOARD", "LIFE", "KEYS", "STICKS", "BOOT"
};

static uint32_t g_catOff;            // bit per category switched off at the console
static uint32_t g_catForce;          // bit per category forced on at the console
static uint32_t g_winStartMs;
static uint16_t g_winLines;
static uint32_t g_suppressed;
static uint32_t g_dropped;
static uint32_t g_tStartMs, g_tSumUs, g_tWorstUs, g_tPasses;

struct ConCmd { const char *name; const char *help; ConsoleFn fn; };
static ConCmd  g_cmds[CON_MAX_CMDS];
static uint8_t g_cmdCount;
static char    g_line[CON_LINE_MAX + 1];
static uint8_t g_lineLen;

const char *dbgCatName(DbgCat cat) { return cat < DBG_COUNT ? kCatName[cat] : "?"; }
uint32_t dbgDropped() { return g_dropped; }

static void writeLine(const char *s) {
  size_t n = strlen(s);
  // Never block the loop on a USB host that is not reading: drop the line instead.
  if ((size_t)Serial.availableForWrite() < n + 2) { g_dropped++; return; }
  Serial.write((const uint8_t *)s, n);
  Serial.write((const uint8_t *)"\r\n", 2);
}

static void emit(DbgCat cat, bool isErr, const char *fmt, va_list ap) {
  char msg[160];
  vsnprintf(msg, sizeof msg, fmt, ap);
  uint32_t t = millis();
  char line[192];
  snprintf(line, sizeof line, "[%6lu.%03lu] %-6s %s%s", (unsigned long)(t / 1000),
           (unsigned long)(t % 1000), dbgCatName(cat), isErr ? "ERROR " : "", msg);
  writeLine(line);
}

bool dbgEnabled(DbgCat cat) {
  if (cat >= DBG_COUNT) return false;
  if (g_catForce & (1UL << cat)) return true;
  if (g_catOff & (1UL << cat)) return false;
  return g_master.debug;
}

static void rollWindow() {
  uint32_t now = millis();
  if (now - g_winStartMs >= 1000) {
    if (g_suppressed) {
      char b[48];
      snprintf(b, sizeof b, "[%6lu.%03lu] DEBUG  %lu lines suppressed", (unsigned long)(now / 1000),
               (unsigned long)(now % 1000), (unsigned long)g_suppressed);
      writeLine(b);
      g_suppressed = 0;
    }
    g_winStartMs = now;
    g_winLines = 0;
  }
}

static bool budget() {
  rollWindow();
  if (g_winLines >= DBG_LINES_PER_S) { g_suppressed++; return false; }
  g_winLines++;
  return true;
}

void dbg(DbgCat cat, const char *fmt, ...) {
  if (!dbgEnabled(cat) || !budget()) return;
  va_list ap; va_start(ap, fmt); emit(cat, false, fmt, ap); va_end(ap);
}

void dbgErr(DbgCat cat, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); emit(cat, true, fmt, ap); va_end(ap);
}

void conPrintf(const char *fmt, ...) {
  char b[192];
  va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
  writeLine(b);
}

void dbgLoopTime(uint32_t us) {
  g_tSumUs += us; g_tPasses++;
  if (us > g_tWorstUs) g_tWorstUs = us;
  uint32_t now = millis();
  if (now - g_tStartMs < 1000) return;
  if (g_tPasses) dbg(DBG_TIMING, "loop avg %lu us, worst %lu us, %lu passes",
                     (unsigned long)(g_tSumUs / g_tPasses), (unsigned long)g_tWorstUs,
                     (unsigned long)g_tPasses);
  g_tStartMs = now; g_tSumUs = g_tWorstUs = g_tPasses = 0;
}

/***************************************************************************************
   Console
****************************************************************************************/
bool consoleRegister(const char *name, const char *help, ConsoleFn fn) {
  if (g_cmdCount >= CON_MAX_CMDS) return false;
  g_cmds[g_cmdCount++] = { name, help, fn };
  return true;
}

static void cmdHelp(const char *) {
  conPrintf("Commands:");
  for (uint8_t i = 0; i < g_cmdCount; i++) conPrintf("  %-10s %s", g_cmds[i].name, g_cmds[i].help);
}

static int catByName(const char *s, size_t n) {
  for (uint8_t i = 0; i < DBG_COUNT; i++)
    if (strlen(kCatName[i]) == n && strncasecmp(kCatName[i], s, n) == 0) return i;
  return -1;
}

// log                    — list categories and their state
// log <cat|all> on|off   — force a category (or all) on / off; `log <cat|all> auto` undoes it
static void cmdLog(const char *args) {
  while (*args == ' ') args++;
  if (!*args) {
    conPrintf("Debug %s. Categories:", g_master.debug ? "ON" : "off");
    for (uint8_t i = 0; i < DBG_COUNT; i++)
      conPrintf("  %-6s %s", kCatName[i],
                (g_catForce & (1UL << i)) ? "on (forced)" : (g_catOff & (1UL << i)) ? "off (forced)"
                                                                                    : "follows debug");
    return;
  }
  const char *sp = strchr(args, ' ');
  if (!sp) { conPrintf("usage: log <cat|all> on|off|auto"); return; }
  const char *val = sp + 1;
  while (*val == ' ') val++;
  uint32_t mask;
  if ((size_t)(sp - args) == 3 && strncasecmp(args, "all", 3) == 0) mask = (1UL << DBG_COUNT) - 1;
  else {
    int c = catByName(args, (size_t)(sp - args));
    if (c < 0) { conPrintf("unknown category"); return; }
    mask = 1UL << c;
  }
  if (strncasecmp(val, "on", 2) == 0)       { g_catForce |= mask;  g_catOff &= ~mask; }
  else if (strncasecmp(val, "off", 3) == 0) { g_catOff |= mask;    g_catForce &= ~mask; }
  else if (strncasecmp(val, "auto", 4) == 0){ g_catOff &= ~mask;   g_catForce &= ~mask; }
  else { conPrintf("usage: log <cat|all> on|off|auto"); return; }
  conPrintf("ok");
}

void consoleExec(const char *line) {
  while (*line == ' ') line++;
  if (!*line) return;
  const char *sp = strchr(line, ' ');
  size_t n = sp ? (size_t)(sp - line) : strlen(line);
  for (uint8_t i = 0; i < g_cmdCount; i++) {
    if (strlen(g_cmds[i].name) == n && strncasecmp(g_cmds[i].name, line, n) == 0) {
      g_cmds[i].fn(sp ? sp + 1 : "");
      return;
    }
  }
  conPrintf("unknown command: type help");
}

void dbgInit() {
  g_catOff = g_catForce = 0;
  g_winStartMs = g_tStartMs = millis();
  g_winLines = 0; g_suppressed = g_dropped = 0;
  g_tSumUs = g_tWorstUs = g_tPasses = 0;
  g_cmdCount = 0; g_lineLen = 0;
  consoleRegister("help", "this list", cmdHelp);
  consoleRegister("log", "log [<cat|all> on|off|auto] — debug categories", cmdLog);
}

void dbgService() {
  rollWindow();                                // closes the rate-limit window on time
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c < 0) break;
    if (c == '\r' || c == '\n') {
      g_line[g_lineLen] = 0;
      if (g_lineLen) consoleExec(g_line);
      g_lineLen = 0;
    } else if (g_lineLen < CON_LINE_MAX) {
      g_line[g_lineLen++] = (char)c;
    }
  }
}

const char *fstr(float v, uint8_t dp) {
  static char buf[8][16];
  static uint8_t slot;
  char *b = buf[slot++ & 7];
  if (v != v) { strcpy(b, "nan"); return b; }
  if (dp > 4) dp = 4;
  bool neg = v < 0;
  if (neg) v = -v;
  uint32_t scale = 1;
  for (uint8_t i = 0; i < dp; i++) scale *= 10;
  if (v > 2.0e9f / scale) { snprintf(b, 16, "%s>2e9", neg ? "-" : ""); return b; }
  uint32_t n = (uint32_t)(v * scale + 0.5f);
  if (dp) snprintf(b, 16, "%s%lu.%0*lu", neg ? "-" : "", (unsigned long)(n / scale), dp, (unsigned long)(n % scale));
  else    snprintf(b, 16, "%s%lu", neg ? "-" : "", (unsigned long)n);
  return b;
}

/***************************************************************************************
   Messages.ino -- the status screen's message area (design §4.3)

   Two lines of Roboto Black 32. The list is rebuilt from the model on every frame and
   each entry shows for MSG_MS. Problems come first, worst first (red, orange, yellow),
   then demo / debug. With no problem: ALL SYSTEMS / NOMINAL, then the service items.
****************************************************************************************/
static uint8_t  g_msgIdx;
static uint32_t g_msgMs;
static bool     g_msgStarted;

static void msgAdd(Msg *out, uint8_t &n, uint8_t rank, uint16_t fg, uint16_t bg, const char *l1, const char *l2) {
  if (n >= SI_MSG_MAX) return;
  Msg &m = out[n++];
  strncpy(m.l1, l1, sizeof m.l1 - 1); m.l1[sizeof m.l1 - 1] = 0;
  strncpy(m.l2, l2, sizeof m.l2 - 1); m.l2[sizeof m.l2 - 1] = 0;
  m.fg = fg; m.bg = bg; m.rank = rank;
}

static uint16_t rankColour(uint8_t rank) {
  return rank == MR_RED ? C_RED : rank == MR_ORANGE ? C_ORANGE : C_YELLOW;
}

void fmtVolts(char *t, size_t n, uint16_t cV)  { snprintf(t, n, "%u.%02u", cV / 100, cV % 100); }
void fmtAmps(char *t, size_t n, int16_t mA)    { uint16_t a = mA < 0 ? 0 : (uint16_t)mA; snprintf(t, n, "%u.%02u", a / 1000, (a % 1000) / 10); }
void fmtUptime(char *t, size_t n, uint32_t s)  { snprintf(t, n, "%lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60), (unsigned long)(s % 60)); }

uint8_t linksUp()   { return (uint8_t)__builtin_popcount(g_m.st.links & 0x1F); }
uint8_t modsPresent() {
  uint16_t mask = g_m.st.modCount >= 16 ? 0xFFFF : (uint16_t)((1u << g_m.st.modCount) - 1);
  return (uint8_t)__builtin_popcount(g_m.st.present & mask);
}
static const char *modName(uint8_t i, char *buf, size_t n) {
  if (i < MOD_NAMED) return MOD_NAME[i];
  snprintf(buf, n, "MODULE %u", i + 1);
  return buf;
}

// Every problem, by rank, appended to out. Shared with the red check of the backlight.
static void msgProblems(Msg *out, uint8_t &n, uint32_t now) {
  const StatusModel &s = g_m.st;
  char a[8], b[16];      // a: a formatted number, at most "655.35"
  if (!s.have) return;
  // power
  if (!(s.valid & VALID_POWER)) {
    msgAdd(out, n, MR_YELLOW, C_YELLOW, C_BLACK, "POWER MON", "NO READING");
  } else {
    if (s.cV < V_CAUTION_CV) {
      uint8_t r = s.cV < V_WARNING_CV ? MR_RED : MR_YELLOW;
      fmtVolts(a, sizeof a, s.cV);
      snprintf(b, sizeof b, "%s V", a);
      msgAdd(out, n, r, rankColour(r), C_BLACK, "SUPPLY LOW", b);
    }
    if (s.mA > A_CAUTION_MA) {
      uint8_t r = s.mA > A_WARNING_MA ? MR_RED : MR_YELLOW;
      fmtAmps(a, sizeof a, s.mA);
      snprintf(b, sizeof b, "HIGH %s A", a);
      msgAdd(out, n, r, rankColour(r), C_BLACK, "CURRENT", b);
    }
  }
  // temperature and fan (both from the EMC2101)
  if (!(s.valid & VALID_TEMP) || !(s.valid & VALID_FAN)) {
    msgAdd(out, n, MR_YELLOW, C_YELLOW, C_BLACK, "FAN CTRL", "NO READING");
  }
  if ((s.valid & VALID_TEMP) && s.tempC >= T_CAUTION_C) {
    uint8_t r = s.tempC >= T_WARNING_C ? MR_RED : MR_YELLOW;
    snprintf(b, sizeof b, "%d C", s.tempC);
    msgAdd(out, n, r, rankColour(r), C_BLACK, "BOARD TEMP", b);
  }
  if ((s.valid & VALID_FAN) && s.rpm == 0 && (!s.ext || s.fanDuty > 0)) {
    if (s.ext) snprintf(b, sizeof b, "DUTY %u %%", s.fanDuty); else strcpy(b, "0 RPM");
    msgAdd(out, n, MR_RED, C_RED, C_BLACK, "FAN STOPPED", b);
  }
  // KSP links: bit 0 the master's own (not expected in demo), 1..4 the 7" displays
  bool demo = g_m.ctrl & CTRL_DEMO;
  if (!(s.links & 0x01) && !demo) msgAdd(out, n, MR_ORANGE, C_ORANGE, C_BLACK, LINK_NAME[0], "NO KSP LINK");
  for (uint8_t i = 1; i < SI_LINK_COUNT; i++) {
    if (!(s.known & (1u << i)))      msgAdd(out, n, MR_RED, C_RED, C_BLACK, LINK_NAME[i], "NO ANSWER");
    else if (!(s.links & (1u << i))) msgAdd(out, n, MR_ORANGE, C_ORANGE, C_BLACK, LINK_NAME[i], "NO KSP LINK");
  }
  // modules
  if (s.ext) {
    for (uint8_t i = 0; i < s.modCount; i++) {
      char nb[16];
      if (!(s.present & (1u << i)))   msgAdd(out, n, MR_RED, C_RED, C_BLACK, modName(i, nb, sizeof nb), "MISSING");
      else if (s.fault & (1u << i))   msgAdd(out, n, MR_ORANGE, C_ORANGE, C_BLACK, modName(i, nb, sizeof nb), "FAULT");
    }
    if (g_m.i2cRiseMs && now - g_m.i2cRiseMs < I2C_RISE_MS) {
      snprintf(b, sizeof b, "%u ERRORS", s.i2cErr);
      msgAdd(out, n, MR_YELLOW, C_YELLOW, C_BLACK, "I2C BUS", b);
    }
  }
}

bool statusAnyRed() {
  if (!g_m.bootDone) return false;
  Msg list[SI_MSG_MAX];
  uint8_t n = 0;
  msgProblems(list, n, millis());
  for (uint8_t i = 0; i < n; i++) if (list[i].rank == MR_RED) return true;
  return false;
}

// The whole list, sorted by rank (stable). Returns its length (at least 1).
uint8_t msgBuild(Msg *out, uint32_t now) {
  Msg raw[SI_MSG_MAX];
  uint8_t n = 0;
  if (!g_m.st.have) {            // BOOT_DONE, but no STATUS yet
    msgAdd(out, n, MR_SERVICE, C_LGREY, C_BLACK, "WAITING FOR", "STATUS");
    return n;
  }
  msgProblems(raw, n, now);
  bool problem = n > 0;
  if (g_m.ctrl & CTRL_DEMO)  msgAdd(raw, n, MR_MODE, C_WHITE, C_BLUE, "DEMO MODE", "ON");
  if (g_m.ctrl & CTRL_DEBUG) msgAdd(raw, n, MR_MODE, C_SKY, C_BLACK, "DEBUG", "ON");
  if (!problem) {
    const StatusModel &s = g_m.st;
    char b[16];
    msgAdd(raw, n, MR_NOMINAL, C_GREEN, C_BLACK, "ALL SYSTEMS", "NOMINAL");
    fmtUptime(b, sizeof b, s.ext ? s.uptimeS : sincePowerOnMs(now) / 1000);
    msgAdd(raw, n, MR_SERVICE, C_LGREY, C_BLACK, "UPTIME", b);
    if (s.ext) {
      snprintf(b, sizeof b, "v%u.%u", s.fwMajor, s.fwMinor);
      msgAdd(raw, n, MR_SERVICE, C_LGREY, C_BLACK, "MASTER FW", b);
    }
    msgAdd(raw, n, MR_SERVICE, C_LGREY, C_BLACK, "SYSINFO FW", "v" SYSINFO_VERSION);
    if (s.ext && (s.valid & VALID_FAN)) {
      snprintf(b, sizeof b, "%u %%", s.fanDuty);
      msgAdd(raw, n, MR_SERVICE, C_LGREY, C_BLACK, "FAN DUTY", b);
    }
    if (s.ext) {
      snprintf(b, sizeof b, "%u", s.i2cErr);
      msgAdd(raw, n, MR_SERVICE, C_LGREY, C_BLACK, "I2C ERRORS", b);
    }
  }
  uint8_t k = 0;
  for (uint8_t r = MR_RED; r <= MR_SERVICE; r++)
    for (uint8_t i = 0; i < n; i++) if (raw[i].rank == r) out[k++] = raw[i];
  return k;
}

// The entry to show now; steps every MSG_MS.
const Msg &msgCurrent(Msg *list, uint8_t n, uint32_t now) {
  if (!g_msgStarted) { g_msgStarted = true; g_msgMs = now; }
  if (now - g_msgMs >= MSG_MS) { g_msgMs = now; g_msgIdx++; }
  return list[g_msgIdx % n];
}

/***************************************************************************************
   Frames.ino -- the frame queue and the model (I2C spec §15.4)

   The I2C receive callback (interrupt context) copies each write into a queue slot and
   nothing else; loop() drains the queue here and updates the model. One producer, one
   consumer: a full queue drops the NEW frame and counts it, it never touches the slot
   the loop is reading.

   Master -> Sys Info Display, byte 0 = frame id:
     0xB0 CONTROL     [B0, ctrl]          bit 0 debug, 1 demo, 2 flight scene
     0xB1 BOOT_LINE   [B1, seq, attr, len, text]   attr 1:0 severity, bit 6 REPLAY
                                          (first line of a replay: clear first),
                                          bit 7 REPLACE (overwrite the newest line)
     0xB2 BOOT_DONE   [B2]
     0xB3 STATUS      13 bytes, or 27 with fan duty, modules, I2C errors, uptime, version
     0xB4 SWITCH_ERR  [B4, n, {id, expected, actual} x n]
     0xB8 BACKLIGHT   [B8, flight %, standby %, 0]
   Any of these resets the MASTER SILENT timer; anything else is counted and ignored.
****************************************************************************************/
Model g_m;

static const uint8_t QUEUE_LEN = 32;     // a burst of boot lines during one 20 ms push
struct QSlot { uint8_t n; uint8_t b[SI_FRAME_MAX]; };
static QSlot g_q[QUEUE_LEN];
static volatile uint8_t g_qHead, g_qTail;   // head: next to write (ISR); tail: next to read

static uint32_t g_powerOnMs;

void framesBegin() {
  memset(&g_m, 0, sizeof g_m);
  g_qHead = g_qTail = 0;
  g_powerOnMs = millis();
}

uint32_t sincePowerOnMs(uint32_t now) { return now - g_powerOnMs; }

// From the I2C receive callback.
void framesPush(const uint8_t *b, uint8_t n) {
  uint8_t next = (uint8_t)((g_qHead + 1) % QUEUE_LEN);
  if (next == g_qTail) { g_m.dropped++; return; }
  if (n > SI_FRAME_MAX) n = SI_FRAME_MAX;
  g_q[g_qHead].n = n;
  memcpy(g_q[g_qHead].b, b, n);
  g_qHead = next;
}

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static void bootClear() { memset(&g_m.boot, 0, sizeof g_m.boot); }

static void bootAddLine(uint8_t attr, const char *text) {
  BootModel &b = g_m.boot;
  if (attr & ATTR_REPLAY) bootClear();
  uint8_t sev = attr & ATTR_SEV;
  if ((attr & ATTR_REPLACE) && b.lines) {
    if (b.count[b.lastSev]) b.count[b.lastSev]--;   // the replaced line no longer counts
  } else {
    b.lines++;
  }
  strncpy(b.last, text, SI_TEXT_MAX);
  b.last[SI_TEXT_MAX] = 0;
  b.lastSev = sev;
  b.count[sev]++;
  if (sev >= SEV_WARN) {
    memcpy(b.prob, b.last, sizeof b.prob);
    b.probSev = sev;
    b.hasProb = true;
  }
  g_m.firstLine = true;
}

static bool frameBootLine(const uint8_t *f, uint8_t n) {
  if (n < 4) return false;
  uint8_t len = f[3];
  if (len > SI_TEXT_MAX || 4 + len > n) return false;
  char t[SI_TEXT_MAX + 1];
  for (uint8_t i = 0; i < len; i++) t[i] = (f[4 + i] >= 0x20 && f[4 + i] < 0x7F) ? (char)f[4 + i] : '?';
  t[len] = 0;
  bootAddLine(f[2], t);
  return true;
}

static bool frameStatus(const uint8_t *f, uint8_t n, uint32_t now) {
  if (n < SI_STATUS_LEN) return false;
  StatusModel &s = g_m.st;
  s.valid = f[1];
  s.cV = rd16(f + 2);
  s.mA = (int16_t)rd16(f + 4);
  s.dW = rd16(f + 6);
  s.tempC = (int8_t)f[8];
  s.rpm = rd16(f + 9);
  s.links = f[11];
  s.known = f[12];
  s.ext = n >= SI_STATUS_EXT;
  if (s.ext) {
    uint16_t errs = rd16(f + 19);
    if (s.have && errs > s.i2cErr) g_m.i2cRiseMs = now ? now : 1;
    s.fanDuty = f[13];
    s.modCount = f[14] > SI_MOD_MAX ? SI_MOD_MAX : f[14];
    s.present = rd16(f + 15);
    s.fault = rd16(f + 17);
    s.i2cErr = errs;
    s.uptimeS = rd32(f + 21);
    s.fwMajor = f[25];
    s.fwMinor = f[26];
  }
  s.have = true;
  g_m.bootDone = true;      // STATUS is only sent after BOOT_DONE (a display that missed it)
  return true;
}

static bool frameSwitchErr(const uint8_t *f, uint8_t n) {
  if (n < 2) return false;
  uint8_t k = f[1] > SI_SWERR_MAX ? SI_SWERR_MAX : f[1];
  if (2 + 3 * k > n) return false;
  for (uint8_t i = 0; i < k; i++) {
    g_m.sw[i].id = f[2 + 3 * i];
    g_m.sw[i].expected = f[3 + 3 * i];
    g_m.sw[i].actual = f[4 + 3 * i];
  }
  g_m.swN = k;
  return true;
}

void frameHandle(const uint8_t *f, uint8_t n, uint32_t now) {
  if (n == 0) return;
  bool ok = false;
  switch (f[0]) {
    case SI_FRAME_CONTROL:    if (n >= 2) { g_m.ctrl = f[1] & (CTRL_DEBUG | CTRL_DEMO | CTRL_FLIGHT); ok = true; } break;
    case SI_FRAME_BOOT_LINE:  ok = frameBootLine(f, n); break;
    case SI_FRAME_BOOT_DONE:  g_m.bootDone = true; ok = true; break;
    case SI_FRAME_STATUS:     ok = frameStatus(f, n, now); break;
    case SI_FRAME_SWITCH_ERR: ok = frameSwitchErr(f, n); break;
    case SI_FRAME_BACKLIGHT:  if (n >= 3) { backlightSetLevels(f[1], f[2]); ok = true; } break;
    default: break;
  }
  if (!ok) { g_m.bad++; return; }
  g_m.frames++;
  g_m.heard = true;
  g_m.lastFrameMs = now;
}

void framesDrain(uint32_t now) {
  while (g_qTail != g_qHead) {
    QSlot &s = g_q[g_qTail];
    frameHandle(s.b, s.n, now);
    g_qTail = (uint8_t)((g_qTail + 1) % QUEUE_LEN);
  }
}

// No frame for SILENT_MS (from power-up, if none has come yet).
bool masterSilent(uint32_t now) {
  uint32_t since = g_m.heard ? g_m.lastFrameMs : g_powerOnMs;
  return now - since >= SILENT_MS;
}

uint32_t silentSeconds(uint32_t now) {
  uint32_t since = g_m.heard ? g_m.lastFrameMs : g_powerOnMs;
  return (now - since) / 1000;
}

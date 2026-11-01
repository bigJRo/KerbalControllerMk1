// sysinfo_host.inc -- host harness for KCMk1_SysInfo, appended to the concatenated sketch
// by tools/sysinfo_host.py. It plays the master over the Wire1 stub.
//
//   exe render NAME out.ppm    draw scenario NAME and write the panel as a PPM
//   exe test NAME              run behaviour check NAME; exit 0 = pass
//   exe list                   the names, one "render NAME" / "test NAME" per line
//
// Every scenario and test also fails if any string was drawn in a font below the 22 px
// cap rule or outside the margins (g_gfxSmallText / g_gfxOverflow, Gfx.ino).
#include <string>
#include <vector>
#include <functional>

static int g_fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

static void run(uint32_t ms) {               // loop() every 5 ms for ms
  uint32_t end = stubMillis() + ms;
  while (stubMillis() < end) { stubMillis() += 5; loop(); }
}
static void send(const std::vector<uint8_t> &b) { Wire1.hostWrite(b); }
static void control(uint8_t c) { send({ SI_FRAME_CONTROL, c }); }
static void line(uint8_t sev, const char *t, bool replace = false, bool replay = false) {
  static uint8_t seq;
  std::vector<uint8_t> f = { SI_FRAME_BOOT_LINE, ++seq, (uint8_t)(sev | (replace ? ATTR_REPLACE : 0) | (replay ? ATTR_REPLAY : 0)), (uint8_t)strlen(t) };
  for (const char *p = t; *p; p++) f.push_back((uint8_t)*p);
  send(f);
}
static void bootDone() { send({ SI_FRAME_BOOT_DONE }); }
static void put16(std::vector<uint8_t> &f, uint16_t v) { f.push_back(v & 0xFF); f.push_back(v >> 8); }

struct St {
  uint8_t valid = 7; uint16_t cV = 1208; int16_t mA = 1840; uint16_t dW = 223; int8_t t = 38; uint16_t rpm = 2140;
  uint8_t links = 0x1F, known = 0x1F; uint8_t duty = 40, mods = 13; uint16_t present = 0x1FFF, fault = 0, i2c = 0;
  uint32_t up = 5025; uint8_t maj = 0, min = 10; bool ext = true;
};
static void status(const St &s) {
  std::vector<uint8_t> f = { SI_FRAME_STATUS, s.valid };
  put16(f, s.cV); put16(f, (uint16_t)s.mA); put16(f, s.dW); f.push_back((uint8_t)s.t); put16(f, s.rpm);
  f.push_back(s.links); f.push_back(s.known);
  if (s.ext) {
    f.push_back(s.duty); f.push_back(s.mods); put16(f, s.present); put16(f, s.fault); put16(f, s.i2c);
    for (int k = 0; k < 4; k++) f.push_back((uint8_t)(s.up >> (8 * k)));
    f.push_back(s.maj); f.push_back(s.min);
  }
  send(f);
}
static void swerr(const std::vector<std::vector<uint8_t>> &e) {
  std::vector<uint8_t> f = { SI_FRAME_SWITCH_ERR, (uint8_t)e.size() };
  for (auto &x : e) f.insert(f.end(), x.begin(), x.end());
  send(f);
}
static void backlightFrame(uint8_t fl, uint8_t sb) { send({ SI_FRAME_BACKLIGHT, fl, sb, 0 }); }

static void start() { stubMillis() = 0; setup(); }
// Power up and run a short, clean boot through BOOT_DONE and one STATUS.
static void booted(const St &s = St(), uint8_t ctrl = 0) {
  start();
  run(100);
  control(ctrl);
  line(SEV_INFO, "MASTER v0.10.0");
  line(SEV_OK, "MODULES 13/13");
  run(2500);
  bootDone();
  status(s);
  run(200);
}
// Keep the master talking (heartbeat + status every 500 ms) for ms.
static void talk(uint32_t ms, const St &s, uint8_t ctrl = 0) {
  for (uint32_t t = 0; t < ms; t += 500) { control(ctrl); status(s); run(500); }
}
static uint32_t now() { return stubMillis(); }

static void writePpm(const char *path) {
  FILE *f = fopen(path, "wb");
  fprintf(f, "P6\n%d %d\n255\n", SI_W, SI_H);
  for (int i = 0; i < SI_W * SI_H; i++) {
    uint16_t c = hostPanel()[i];
    uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31) };
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

/*======================================================================================
  Render scenarios (the design mockups, drawn by the firmware)
======================================================================================*/
static const std::vector<std::pair<const char *, const char *>> BOOT_LINES = {
  { "\x00", "MASTER v0.10.0" }, { "\x00", "RESET: POWER ON" }, { "\x01", "I2C 400 kHz" },
  { "\x01", "POWER MON OK" }, { "\x01", "FAN CTRL OK" }, { "\x00", "INT MAP STORED" },
  { "\x01", "UI OK" }, { "\x01", "FUNCTION OK" }, { "\x01", "ACTION OK" }, { "\x01", "STABILITY OK" },
  { "\x01", "VEHICLE OK" }, { "\x01", "TIME OK" }, { "\x01", "AUX OK" }, { "\x01", "ROTATION OK" },
  { "\x01", "TRANSLATION OK" }, { "\x03", "GPWS MISSING" }, { "\x01", "PRE-WARP OK" },
  { "\x02", "THROTTLE INT LEARNED" }, { "\x01", "DUAL ENC OK" }, { "\x02", "MODULES 12/13" },
  { "\x01", "ANNUNCIATOR OK KSP UP" }, { "\x01", "INFO DISP 1 OK KSP UP" }, { "\x01", "INFO DISP 2 OK KSP UP" },
  { "\x03", "RES DISP NO ANSWER" }, { "\x01", "DISP INT 3/4" }, { "\x01", "ENGINE SAFE A/P INHIB" },
  { "\x01", "CTRL MODE SPC" }, { "\x00", "KSP CONNECTING" }, { "\x01", "WATCHDOG 2 S" },
};

static St faults() {
  St s; s.cV = 1084; s.mA = 8310; s.dW = 901; s.t = 52; s.rpm = 0; s.duty = 100;
  s.links = 0x07; s.known = 0x0F; s.present = 0x1FFF & ~(1u << 9);
  return s;
}

static std::vector<std::pair<std::string, std::function<void()>>> renders() {
  return {
    { "SPLASH", [] { start(); run(500); } },
    { "BOOT", [] {
        start(); run(100); control(0);
        for (int i = 0; i < 15; i++) line((uint8_t)BOOT_LINES[i].first[0], BOOT_LINES[i].second);
        run(2000); } },
    { "BOOT_WAIT", [] {
        start(); run(100);
        for (auto &l : BOOT_LINES) line((uint8_t)l.first[0], l.second);
        line(SEV_INFO, "KSP WAIT 0 S");
        for (int s = 1; s <= 74; s++) { control(0); run(1000); char t[24]; snprintf(t, sizeof t, "KSP WAIT %d S", s); line(SEV_INFO, t, true); }
        line(SEV_WARN, "CHECK KSP, SIMPIT, USB"); line(SEV_INFO, "KSP WAIT 74 S");
        run(6000 - 1000); control(0); run(200); } },
    { "BOOT_HUNG", [] {
        start(); run(100);
        for (int i = 0; i < 9; i++) line((uint8_t)BOOT_LINES[i].first[0], BOOT_LINES[i].second);
        run(12000); } },
    { "STATUS", [] { booted(); talk(2000, St()); } },
    { "STATUS_SERVICE", [] { booted(); talk(2500, St()); } },
    { "STATUS_FAULTS", [] { booted(faults()); talk(500, faults()); } },
    { "STATUS_FAULTS_2", [] {
        St s = faults(); s.cV = 1132; s.mA = 7240; s.dW = 820; s.links = 0x0F; s.known = 0x0F; s.present = 0x1FFF;
        booted(s); talk(500, s); } },
    { "STATUS_DEMO", [] { St s; s.links = 0x1E; booted(s, CTRL_DEMO); talk(500, s, CTRL_DEMO); } },
    { "STATUS_DEMO_2", [] { St s; s.links = 0x1E; booted(s, CTRL_DEMO); talk(3000, s, CTRL_DEMO); } },
    { "STATUS_STALE", [] { booted(); talk(1000, St()); run(7000); } },
    { "STATUS_OLD_MASTER", [] { St s; s.ext = false; booted(s); talk(500, s); } },
    { "SWITCH_ERR", [] { booted(); swerr({ { 10, 1, 0 }, { 13, 1, 0 }, { 11, 1, 0 } }); talk(500, St()); } },
    { "SWITCH_ERR_PAGED", [] {
        booted();
        swerr({ { 9, 1, 0 }, { 10, 1, 0 }, { 11, 1, 0 }, { 12, 1, 0 }, { 15, 1, 0 }, { 13, 1, 0 }, { 14, 1, 0 } });
        stubMillis() = (stubMillis() / 6000 + 1) * 6000; talk(500, St()); } },   // page 1 of 2
  };
}

/*======================================================================================
  Behaviour checks
======================================================================================*/
static uint8_t shownCount(uint8_t sev) { return (uint8_t)g_m.boot.count[sev]; }

static std::vector<std::pair<std::string, std::function<void()>>> tests() {
  return {
    { "splash_then_boot", [] {
        start(); run(500);
        CHECK(splashShowing(now()));
        line(SEV_OK, "UI OK"); run(1000);
        CHECK(splashShowing(now()));                  // the 2 s minimum
        run(1000);
        CHECK(!splashShowing(now()));
        CHECK(!g_m.bootDone);
        start(); run(5100);                           // never a line: gone at 5 s regardless
        CHECK(!splashShowing(now()));
      } },
    { "boot_counts_replace_replay", [] {
        start(); run(100);
        line(SEV_OK, "A OK"); line(SEV_OK, "B OK"); line(SEV_WARN, "C WARN"); line(SEV_ERROR, "D ERR");
        line(SEV_INFO, "KSP WAIT 0 S");
        for (int i = 1; i < 20; i++) line(SEV_INFO, "KSP WAIT n S", true);
        run(50);
        CHECK(shownCount(SEV_OK) == 2 && shownCount(SEV_WARN) == 1 && shownCount(SEV_ERROR) == 1);
        CHECK(g_m.boot.lines == 5);
        CHECK(strcmp(g_m.boot.last, "KSP WAIT n S") == 0);
        CHECK(g_m.boot.hasProb && strcmp(g_m.boot.prob, "D ERR") == 0);
        line(SEV_WARN, "E WARN"); line(SEV_OK, "E OK", true);     // a replaced warning stops counting
        run(50);
        CHECK(shownCount(SEV_WARN) == 1 && shownCount(SEV_OK) == 3);
        line(SEV_INFO, "MASTER v0.10.0", false, true);           // a replay starts from scratch
        run(50);
        CHECK(g_m.boot.lines == 1 && shownCount(SEV_OK) == 0 && !g_m.boot.hasProb);
      } },
    { "wrap", [] {
        char out[3][SI_TEXT_MAX + 1];
        CHECK(gfxWrap(F32, "KSP RUNNING? SIMPIT COM PORT? USB?", SI_W - 4, out, 3) == 3);
        CHECK(gfxWrap(F32, "RES DISP NO ANSWER", SI_W - 4, out, 3) == 2);
        CHECK(strcmp(out[0], "RES DISP NO") == 0 && strcmp(out[1], "ANSWER") == 0);
        CHECK(gfxWrap(F32, "   KSP    UP   ", SI_W - 4, out, 3) == 1 && strcmp(out[0], "KSP UP") == 0);   // spaces collapse
        uint8_t n = gfxWrap(F32, "WWWWWWWWWWWWWWWWWWWW", SI_W - 4, out, 3);   // one word wider than a row: cut
        CHECK(n == 1 && gfxTextWidth(F32, out[0]) <= SI_W - 4);
        CHECK(gfxWrap(F32, "A B C D E F G H I J K L M N O P Q R S T U V W X Y Z 1 2 3 4 5 6", SI_W - 4, out, 3) == 3);
      } },
    { "short_boot_lines_fit_two_rows", [] {             // design §8.2: every short form, two rows
        const char *lines[] = { "MASTER v0.10.0", "RESET: WATCHDOG x3", "HUNG IN MODULES @2C", "I2C BUS RECOVERED",
          "I2C 400 kHz", "POWER MON MISSING", "FAN CTRL MISSING", "INT MAP DEFAULT", "TRANSLATION OK",
          "TRANSLATION INT LEARNED", "TRANSLATION MISSING", "TRANSLATION WRONG TYPE", "MODULES 12/13",
          "ANNUNCIATOR OK KSP DOWN", "INFO DISP 1 OK KSP DOWN", "RES DISP NO ANSWER", "ANNUNCIATOR NO ANSWER",
          "DISP INT 3/4", "ENGINE SAFE A/P INHIB", "SW GRP 1 NOT READ", "MODE NOT READ: SPC", "WATCHDOG 2 S",
          "KSP CONNECTING", "KSP: DEMO", "KSP WAIT 9999 S", "CHECK KSP, SIMPIT, USB", "KSP CONNECTED",
          "HALTED: 3 WDT RESETS", "FIX, THEN POWER CYCLE", "RESET: OVER TEMPERATURE", "RESET: RESET BUTTON" };
        char out[3][SI_TEXT_MAX + 1];
        for (const char *l : lines) {
          uint8_t n = gfxWrap(F32, l, SI_W - 2 * SI_MARGIN, out, 3);
          if (n > 2) fprintf(stderr, "  three rows: %s\n", l);
          CHECK(n <= 2);
        }
      } },
    { "master_silent", [] {
        start(); run(4900);
        CHECK(!masterSilent(now()));
        run(200);
        CHECK(masterSilent(now()));                    // nothing since power-up
        control(0); run(10);
        CHECK(!masterSilent(now()));
        booted(); talk(1000, St());             // the last frame came 500 ms before talk() returned
        run(4300); CHECK(!masterSilent(now()));
        run(300);  CHECK(masterSilent(now()));
        CHECK(silentSeconds(now()) == 5);
        status(St()); run(10);
        CHECK(!masterSilent(now()));
      } },
    { "status_parse", [] {
        St s; s.ext = false; booted(s);
        CHECK(g_m.bootDone && g_m.st.have && !g_m.st.ext && g_m.st.cV == 1208 && g_m.st.mA == 1840);
        St x; x.i2c = 3; x.up = 123456; x.present = 0x1F7F; x.fault = 0x0800; x.maj = 2; x.min = 7;
        status(x); run(10);
        CHECK(g_m.st.ext && g_m.st.modCount == 13 && g_m.st.present == 0x1F7F && g_m.st.fault == 0x0800);
        CHECK(g_m.st.i2cErr == 3 && g_m.st.uptimeS == 123456 && g_m.st.fwMajor == 2 && g_m.st.fwMinor == 7);
        CHECK(modsPresent() == 12);
        uint32_t bad = g_m.bad;
        send({ SI_FRAME_STATUS, 7, 1, 2 }); run(10);    // too short
        send({ 0x99, 1 }); run(10);                     // unknown id
        CHECK(g_m.bad == bad + 2);
        // a STATUS without a BOOT_DONE (a display that missed it) still leaves the boot screen
        start(); run(100); status(St()); run(10);
        CHECK(g_m.bootDone);
      } },
    { "messages_order_and_rotation", [] {
        St s = faults(); s.fault = 1u << 11;          // THROTTLE in FAULT too
        booted(s);
        Msg list[SI_MSG_MAX];
        uint8_t n = msgBuild(list, now());
        CHECK(n >= 7);
        for (uint8_t i = 1; i < n; i++) CHECK(list[i - 1].rank <= list[i].rank);
        CHECK(strcmp(list[0].l1, "SUPPLY LOW") == 0 && list[0].rank == MR_RED);
        bool gpws = false, thr = false, res = false, info2 = false, cur = false;
        for (uint8_t i = 0; i < n; i++) {
          if (!strcmp(list[i].l1, "GPWS")) gpws = !strcmp(list[i].l2, "MISSING");
          if (!strcmp(list[i].l1, "THROTTLE")) thr = !strcmp(list[i].l2, "FAULT") && list[i].rank == MR_ORANGE;
          if (!strcmp(list[i].l1, "RESOURCE")) res = !strcmp(list[i].l2, "NO ANSWER");
          if (!strcmp(list[i].l1, "INFO DISP 2")) info2 = !strcmp(list[i].l2, "NO KSP LINK");
          if (!strcmp(list[i].l1, "CURRENT")) cur = !strcmp(list[i].l2, "HIGH 8.31 A") && list[i].rank == MR_RED;
          CHECK(strcmp(list[i].l1, "ALL SYSTEMS") != 0);
        }
        CHECK(gpws && thr && res && info2 && cur);
        // nothing wrong: nominal, then the service items; rotation every 2.5 s
        booted();
        n = msgBuild(list, now());
        CHECK(strcmp(list[0].l1, "ALL SYSTEMS") == 0 && strcmp(list[1].l1, "UPTIME") == 0);
        CHECK(strcmp(list[1].l2, "1:23:45") == 0);
        uint32_t t = now();
        const Msg &a = msgCurrent(list, n, t);
        std::string first = a.l1;
        const Msg &b = msgCurrent(list, n, t + 2600);
        CHECK(first != std::string(b.l1));
        // demo: an entry on the DEMO blue, and no MASTER NO KSP LINK
        St d; d.links = 0x1E; booted(d, CTRL_DEMO);
        n = msgBuild(list, now());
        CHECK(strcmp(list[0].l1, "DEMO MODE") == 0 && list[0].bg == C_BLUE);
        for (uint8_t i = 0; i < n; i++) CHECK(strcmp(list[i].l1, "MASTER") != 0);
        // the current caution / warning edges (D7)
        St c; c.mA = 7000; booted(c); n = msgBuild(list, now()); CHECK(strcmp(list[0].l1, "ALL SYSTEMS") == 0);
        c.mA = 7001; status(c); run(10); n = msgBuild(list, now()); CHECK(strcmp(list[0].l1, "CURRENT") == 0 && list[0].rank == MR_YELLOW);
        c.mA = 8001; status(c); run(10); n = msgBuild(list, now()); CHECK(list[0].rank == MR_RED);
        // I2C errors rising: shown for a minute
        St e; booted(e); e.i2c = 4; status(e); run(10);
        n = msgBuild(list, now()); CHECK(strcmp(list[0].l1, "I2C BUS") == 0 && strcmp(list[0].l2, "4 ERRORS") == 0);
        talk(61000, e);
        n = msgBuild(list, now()); CHECK(strcmp(list[0].l1, "ALL SYSTEMS") == 0);
      } },
    { "switch_err", [] {
        booted();
        swerr({ { 10, 1, 0 } }); run(150);
        CHECK(g_m.swN == 1);
        swerr({ { 17, 1, 0 }, { 0, 0, 1 } }); run(150);    // unknown ids: SW nn
        CHECK(g_m.swN == 2);
        std::vector<std::vector<uint8_t>> ten;
        for (uint8_t i = 1; i <= 12; i++) ten.push_back({ i, 1, 0 });
        swerr(ten); run(150);
        CHECK(g_m.swN == SI_SWERR_MAX);                    // capped at 10
        for (int k = 0; k < 3; k++) { talk(3000, St()); }  // pages turn
        swerr({}); run(150);
        CHECK(g_m.swN == 0);
      } },
    { "backlight", [] {
        start(); run(100);
        backlightFrame(80, 1); run(10);
        CHECK(g_blFlight == 80 && g_blStandby == KCM_BL_MIN_PCT);   // clamped to the floor
        run(1000);
        CHECK(backlightPct() == 80);                       // boot screen: flight level
        line(SEV_OK, "X"); run(2000); bootDone(); talk(1000, St());
        CHECK(backlightPct() == KCM_BL_MIN_PCT);           // status, not in flight: standby
        CHECK(stubPwm(PIN_BL) == KCM_BL_MIN_PCT * 10);
        talk(1000, St(), CTRL_FLIGHT);
        CHECK(backlightPct() == 80);                       // flight scene
        talk(1000, St());
        CHECK(backlightPct() == KCM_BL_MIN_PCT);
        St hot; hot.t = 61; talk(1000, hot);
        CHECK(backlightPct() == 80);                       // a red reading is never dimmed
        talk(1000, St()); swerr({ { 10, 1, 0 } }); talk(1000, St());
        CHECK(backlightPct() == 80);                       // SWITCH ERR
        swerr({}); talk(1000, St()); run(6000);
        CHECK(backlightPct() == 80);                       // MASTER SILENT
        // the ramp: 300 ms for the full range, so 80 -> 5 % takes about 225 ms
        talk(500, St());
        start(); backlightFrame(100, 5); run(100); line(SEV_OK, "X"); run(2000); bootDone(); status(St());
        run(100); CHECK(backlightPct() > 5);
        run(400); CHECK(backlightPct() == 5);
        CHECK(stubPwmRange() == 1000 && stubPwmFreq() == BL_PWM_HZ);
      } },
    { "anti_sticking_shift", [] {
        booted();
        gfxBeginFrame(0);             CHECK(g_ox == 0 && g_oy == 0);
        gfxBeginFrame(SHIFT_MS);      CHECK(g_ox == 0 && g_oy == -2);
        gfxBeginFrame(2 * SHIFT_MS);  CHECK(g_ox == 2 && g_oy == 0);
        gfxBeginFrame(3 * SHIFT_MS);  CHECK(g_ox == 0 && g_oy == 2);
        gfxBeginFrame(4 * SHIFT_MS);  CHECK(g_ox == -2 && g_oy == 0);
        gfxBeginFrame(5 * SHIFT_MS);  CHECK(g_ox == 0 && g_oy == 0);
        // every screen at every offset stays inside the panel (layout check is pre-shift)
        stubMillis() = SHIFT_MS * 3 - 1000; talk(2000, faults());
        CHECK(g_ox == 0 && g_oy == 2);
      } },
    { "no_screen_saver", [] {        // D9: an hour of nothing changing never blanks the screen
        booted();
        talk(3600000, St());
        uint32_t lit = 0;
        for (int i = 0; i < SI_W * SI_H; i++) if (hostPanel()[i] != C_BLACK) lit++;
        CHECK(lit > 5000);
        CHECK(backlightPct() >= KCM_BL_MIN_PCT);
      } },
    { "push_only_on_change", [] {
        booted(); talk(200, St());
        uint32_t p = hostPanelPushes();
        run(400);                      // same message, same values: nothing to push
        CHECK(hostPanelPushes() == p);
        status(St()); run(200);
        CHECK(hostPanelPushes() == p);
        St s; s.cV = 1199; status(s); run(200);
        CHECK(hostPanelPushes() == p + 1);
      } },
    { "queue_full_drops_new", [] {
        start(); run(100);
        uint32_t before = g_m.frames;
        for (int i = 0; i < 40; i++) control(0);       // no loop() between: the queue fills
        run(10);
        CHECK(g_m.frames == before + 31 && g_m.dropped == 9);
      } },
    { "status_read", [] {
        start(); run(100);
        std::vector<uint8_t> r = Wire1.hostRead();
        CHECK(r.size() == 4 && r[0] == 0xAF && (r[1] & 0x01) && (r[1] & 0x04) && !(r[1] & 0x02));
        CHECK(r[2] == SYSINFO_VERSION_MAJOR && r[3] == SYSINFO_VERSION_MINOR);
        line(SEV_OK, "X"); run(2000); bootDone(); status(St()); run(200);
        r = Wire1.hostRead();
        CHECK(!(r[1] & 0x04));
        CHECK(Wire1.addr == 0x14 && Wire1.sda == 6 && Wire1.scl == 7 && Wire1.started);
        CHECK(rp2040.wdtMs == 2000 && rp2040.feeds > 0);
        CHECK(stubPinLevel(PIN_INT) == HIGH);
      } },
    { "demo_ksp_row_blue", [] {     // D11: the KSP row on the Annunciator's DEMO blue, white text
        St d; d.links = 0x1E; booted(d, CTRL_DEMO); talk(500, d, CTRL_DEMO);
        uint32_t blue = 0, white = 0;
        for (int y = 136; y < 176; y++) for (int x = 0; x < SI_W; x++) {
          uint16_t c = hostPanel()[y * SI_W + x];
          if (c == C_BLUE) blue++; else if (c == C_WHITE) white++;
        }
        CHECK(blue > 6000 && white > 300);
        booted(); talk(500, St());                   // not in demo: no blue band
        blue = 0;
        for (int y = 136; y < 176; y++) for (int x = 0; x < SI_W; x++) if (hostPanel()[y * SI_W + x] == C_BLUE) blue++;
        CHECK(blue == 0);
      } },
    { "worst_case_layout", [] {     // longest values and names on every screen: no overflow
        St s; s.cV = 9999; s.mA = 32767; s.dW = 65535; s.t = -40; s.rpm = 65535; s.links = 0; s.known = 0x01;
        s.present = 0; s.fault = 0xFFFF; s.i2c = 65535; s.up = 0xFFFFFFFF; s.maj = 255; s.min = 255; s.mods = 16; s.duty = 100;
        booted(s, CTRL_DEMO | CTRL_DEBUG);
        for (int k = 0; k < 40; k++) talk(2500, s, CTRL_DEMO | CTRL_DEBUG);
        St t; t.t = 127; t.rpm = 0; booted(t); talk(30000, t);
        std::vector<std::vector<uint8_t>> all;
        for (uint8_t i = 1; i <= 10; i++) all.push_back({ i, 0, 1 });
        swerr(all); talk(7000, St());
        all.clear();
        for (uint8_t i = 7; i <= 16; i++) all.push_back({ i, 0, 1 });
        swerr(all); talk(7000, St());
        all.clear();
        for (uint8_t i = 1; i <= 16; i++) all.push_back({ i, 0, 0 });
        swerr(all); talk(7000, St());
        start(); run(100);
        line(SEV_ERROR, "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW");
        line(SEV_WARN, "MMMM MMMM MMMM MMMM MMMM MMMM MMMM MMMM");
        run(13000);
      } },
  };
}

int main(int argc, char **argv) {
  if (argc >= 2 && !strcmp(argv[1], "list")) {
    for (auto &r : renders()) printf("render %s\n", r.first.c_str());
    for (auto &t : tests()) printf("test %s\n", t.first.c_str());
    return 0;
  }
  if (argc < 3) { fprintf(stderr, "usage: %s render NAME out.ppm | test NAME | list\n", argv[0]); return 2; }
  std::string mode = argv[1], name = argv[2];
  auto set = mode == "render" ? renders() : tests();
  for (auto &e : set) {
    if (e.first != name) continue;
    e.second();
    if (g_gfxSmallText) { fprintf(stderr, "  FAIL: %u strings below the %u px cap rule\n", (unsigned)g_gfxSmallText, SI_MIN_CAP_PX); g_fails++; }
    if (g_gfxOverflow)  { fprintf(stderr, "  FAIL: %u strings outside the margins\n", (unsigned)g_gfxOverflow); g_fails++; }
    if (mode == "render") { if (argc < 4) return 2; writePpm(argv[3]); }
    return g_fails ? 1 : 0;
  }
  fprintf(stderr, "no %s named %s\n", mode.c_str(), name.c_str());
  return 2;
}

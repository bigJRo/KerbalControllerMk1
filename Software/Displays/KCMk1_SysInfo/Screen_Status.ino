/***************************************************************************************
   Screen_Status.ino -- the status screen (design §4)

     0-45     bolt, bus voltage (Roboto Black 48)
     46-80    current left, power right (the current's colour: > 7 A yellow, > 8 A red)
     92-126   thermometer + board temperature; fan + rpm (STOP when stalled)
     136-175  link + KSP n/5 -- in demo on the Annunciator's DEMO blue, white text (D11)
     180-214  chip + MOD n/13
     226-297  the message area (Messages.ino), or the red MASTER / SILENT n S block

   No STATUS yet, or a reading whose valid bit is clear: "--" in grey. MASTER SILENT:
   every value grey, kept as last received.
****************************************************************************************/
void drawStatus(uint32_t now) {
  const StatusModel &s = g_m.st;
  bool silent = masterSilent(now);
  char t[20], u[16];

  // supply
  bool pv = s.have && (s.valid & VALID_POWER);
  uint16_t vc = !pv || silent ? C_GREY : s.cV < V_WARNING_CV ? C_RED : s.cV < V_CAUTION_CV ? C_YELLOW : C_GREEN;
  gfxIcon(SI_MARGIN, 8, ICON_BOLT, vc);
  if (pv) { fmtVolts(u, sizeof u, s.cV); snprintf(t, sizeof t, "%sV", u); } else strcpy(t, "--V");
  gfxText(SI_W - SI_MARGIN, -4, F48, t, vc, AL_RIGHT);
  uint16_t ac = !pv || silent ? C_GREY : s.mA > A_WARNING_MA ? C_RED : s.mA > A_CAUTION_MA ? C_YELLOW : C_WHITE;
  if (pv) { fmtAmps(u, sizeof u, s.mA); snprintf(t, sizeof t, "%sA", u); } else strcpy(t, "--A");
  gfxText(SI_MARGIN, 46, F32, t, ac, AL_LEFT);
  if (pv) snprintf(t, sizeof t, "%u.%uW", s.dW / 10, s.dW % 10); else strcpy(t, "--W");
  gfxText(SI_W - SI_MARGIN, 46, F32, t, ac, AL_RIGHT);

  // thermal
  bool tv = s.have && (s.valid & VALID_TEMP);
  uint16_t tc = !tv || silent ? C_GREY : s.tempC >= T_WARNING_C ? C_RED : s.tempC >= T_CAUTION_C ? C_YELLOW : C_GREEN;
  gfxIcon(SI_MARGIN, 98, ICON_THERM, tc);
  if (tv) snprintf(t, sizeof t, "%dC", s.tempC); else strcpy(t, "--C");
  gfxText(36, 92, F32, t, tc, AL_LEFT);
  bool fv = s.have && (s.valid & VALID_FAN);
  bool stop = fv && s.rpm == 0 && (!s.ext || s.fanDuty > 0);
  uint16_t fc = !fv || silent ? C_GREY : stop ? C_RED : C_WHITE;
  gfxIcon(112, 98, ICON_FAN, fc);
  if (!fv) strcpy(t, "--"); else if (stop) strcpy(t, "STOP"); else snprintf(t, sizeof t, "%u", s.rpm);
  gfxText(SI_W - SI_MARGIN, 92, F32, t, fc, AL_RIGHT);

  // KSP links up
  uint16_t kc = !s.have || silent ? C_GREY : linksUp() == SI_LINK_COUNT ? C_GREEN : C_ORANGE;
  if ((g_m.ctrl & CTRL_DEMO) && !silent) {
    gfxRect(0, 136, SI_W, 40, C_BLUE);
    kc = C_WHITE;
  }
  gfxIcon(SI_MARGIN, 144, ICON_LINK, kc);
  gfxText(40, 138, F32, "KSP", kc, AL_LEFT);
  if (s.have) snprintf(t, sizeof t, "%u/%u", linksUp(), SI_LINK_COUNT); else strcpy(t, "--");
  gfxText(SI_W - SI_MARGIN, 138, F32, t, kc, AL_RIGHT);

  // modules present
  uint16_t mc;
  if (!s.ext || silent) mc = C_GREY;
  else if (modsPresent() < s.modCount) mc = C_RED;
  else if (s.fault) mc = C_ORANGE;
  else mc = C_GREEN;
  gfxIcon(SI_MARGIN, 186, ICON_CHIP, mc);
  gfxText(40, 180, F32, "MOD", mc, AL_LEFT);
  if (s.ext) snprintf(t, sizeof t, "%u/%u", modsPresent(), s.modCount); else strcpy(t, "--");
  gfxText(SI_W - SI_MARGIN, 180, F32, t, mc, AL_RIGHT);

  // messages
  if (silent) { silentBlock(230, 90, now); return; }
  static Msg list[SI_MSG_MAX];
  uint8_t n = msgBuild(list, now);
  const Msg &m = msgCurrent(list, n, now);
  if (m.bg != C_BLACK) gfxRect(0, 226, SI_W, 72, m.bg);
  gfxText(SI_MARGIN, 228, F32, m.l1, m.fg, AL_LEFT);
  gfxText(SI_MARGIN, 228 + TEXT_PITCH, F32, m.l2, m.fg, AL_LEFT);
}

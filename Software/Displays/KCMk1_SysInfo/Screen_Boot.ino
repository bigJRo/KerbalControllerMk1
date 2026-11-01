/***************************************************************************************
   Screen_Boot.ino -- the boot screen (design §3)

     0-43     BOOT, seconds since power-up (white on navy)
     42-145   the newest boot line, up to 3 rows, in its severity colour
     160-200  counts: OK (green), warnings (yellow), errors (red); 0 is grey
     206-319  the newest warning / error line, up to 3 rows; NO FAULTS if none;
              the red MASTER / SILENT n S block when no frame has come for 5 s
****************************************************************************************/
static const uint16_t SEV_COLOUR[4] = { C_LGREY, C_GREEN, C_YELLOW, C_RED };

static void drawWrapped(int16_t y, const char *text, uint16_t c, uint8_t rows) {
  char out[3][SI_TEXT_MAX + 1];
  uint8_t n = gfxWrap(F32, text, SI_W - 2 * SI_MARGIN, out, rows);
  for (uint8_t i = 0; i < n; i++) gfxText(SI_MARGIN, y + i * TEXT_PITCH, F32, out[i], c, AL_LEFT);
}

void drawBoot(uint32_t now) {
  const BootModel &b = g_m.boot;
  char t[16];
  snprintf(t, sizeof t, "%lu S", (unsigned long)(sincePowerOnMs(now) / 1000));
  titleBar("BOOT", t, C_NAVY);

  if (b.lines) drawWrapped(42, b.last, SEV_COLOUR[b.lastSev & 3], 3);
  else         drawWrapped(42, "WAITING FOR MASTER", C_LGREY, 3);

  static Icon *const icons[3] = { &ICON_OK, &ICON_WARN, &ICON_ERR };
  static const uint16_t cols[3] = { C_GREEN, C_YELLOW, C_RED };
  for (uint8_t k = 0; k < 3; k++) {
    uint16_t n = b.count[k + 1];
    int16_t x = SI_MARGIN + k * 80;
    gfxIcon(x, 168, *icons[k], n ? cols[k] : C_DGREY);
    snprintf(t, sizeof t, "%u", n > 999 ? 999 : n);
    gfxText(x + 36, 160, F32, t, n ? cols[k] : C_GREY, AL_LEFT);
  }

  if (masterSilent(now))  silentBlock(212, 108, now);
  else if (b.hasProb)     drawWrapped(206, b.prob, SEV_COLOUR[b.probSev & 3], 3);
  else                    gfxText(SI_MARGIN, 206, F32, "NO FAULTS", C_GREEN, AL_LEFT);
}

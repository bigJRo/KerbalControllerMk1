/***************************************************************************************
   SdSplash.ino -- the power-on splash (design §3.4, D1)

   Shown for SPLASH_MIN_MS, or until the first BOOT_LINE, whichever is later, and never
   longer than SPLASH_MAX_MS. Frames that arrive meanwhile still update the model.

   Built in: the KCMk1 wordmark (Roboto Black 48) with SYS INFO and the version.
   From the card, if one mounts and holds /KCMK1/SPLASH.BMP: an uncompressed 240 x 320
   BMP, 24-bit, or 16-bit RGB565 (BI_BITFIELDS), bottom-up or top-down. The card is read
   once here, before the I2C slave starts, and never again, so the loop never shares SPI.
   No card, or a file it cannot use: the wordmark. Neither is an error.
****************************************************************************************/
static const char *const SPLASH_PATH = "/KCMK1/SPLASH.BMP";
static bool      g_cardMounted;
static uint16_t *g_splash;        // 240 x 320 from the card, freed when the splash ends
static bool      g_splashDone;

bool splashCardMounted() { return g_cardMounted; }

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static bool splashLoad(File &f) {
  uint8_t h[54];
  if (f.read(h, sizeof h) != (int)sizeof h || h[0] != 'B' || h[1] != 'M') return false;
  uint32_t off = le32(h + 10);
  int32_t w = (int32_t)le32(h + 18), hh = (int32_t)le32(h + 22);
  uint16_t bpp = le16(h + 28);
  uint32_t comp = le32(h + 30);
  bool topDown = hh < 0;
  if (topDown) hh = -hh;
  if (w != SI_W || hh != SI_H) return false;
  if (!((bpp == 24 && comp == 0) || (bpp == 16 && comp == 3))) return false;
  uint32_t rowBytes = ((uint32_t)w * (bpp / 8) + 3) & ~3u;
  uint16_t *buf = (uint16_t *)malloc((size_t)SI_W * SI_H * 2);
  if (!buf) return false;
  static uint8_t row[SI_W * 3 + 4];
  for (int32_t r = 0; r < SI_H; r++) {
    int32_t y = topDown ? r : SI_H - 1 - r;
    if (!f.seek(off + (uint32_t)r * rowBytes) || f.read(row, rowBytes) != (int)rowBytes) { free(buf); return false; }
    for (int32_t x = 0; x < SI_W; x++) {
      uint16_t c;
      if (bpp == 24) {
        uint8_t b = row[3 * x], g = row[3 * x + 1], rr = row[3 * x + 2];
        c = (uint16_t)(((rr & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
      } else {
        c = le16(row + 2 * x);
      }
      buf[y * SI_W + x] = c;
    }
  }
  g_splash = buf;
  return true;
}

void splashBegin() {
  g_splashDone = false;
  if (g_splash) { free(g_splash); g_splash = nullptr; }
  g_cardMounted = SD.begin(PIN_SD_CS);
  if (!g_cardMounted) return;
  File f = SD.open(SPLASH_PATH, FILE_READ);
  if (!f) return;
  if (!splashLoad(f)) Serial.println("SPLASH.BMP: not a 240 x 320 24-bit or RGB565 BMP; using the wordmark");
  f.close();
}

bool splashShowing(uint32_t now) {
  if (g_splashDone) return false;
  uint32_t t = sincePowerOnMs(now);
  if (t >= SPLASH_MAX_MS || (t >= SPLASH_MIN_MS && g_m.firstLine)) {
    g_splashDone = true;
    if (g_splash) { free(g_splash); g_splash = nullptr; }
    return false;
  }
  return true;
}

bool splashFromCard() { return g_splash != nullptr; }

void splashDraw() {
  if (g_splash) {
    memcpy(gfxBuffer(), g_splash, (size_t)SI_W * SI_H * 2);   // whole screen, no shift
    return;
  }
  gfxText(SI_W / 2, 100, F48, "KCMk1", C_WHITE, AL_CENTRE);
  gfxText(SI_W / 2, 170, F32, "SYS INFO", C_LGREY, AL_CENTRE);
  gfxText(SI_W / 2, 214, F32, "v" SYSINFO_VERSION, C_GREY, AL_CENTRE);
}

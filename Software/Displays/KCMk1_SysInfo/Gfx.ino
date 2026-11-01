/***************************************************************************************
   Gfx.ino -- frame buffer, text, icons, anti-sticking shift, push to the ST7789

   Every frame is drawn whole into a 240 x 320 RGB565 buffer (150 KB of the RP2350's
   520 KB) and pushed to the panel only if it differs from the last one pushed (a hash).
   No partial redraws, no flicker.

   Text: ILI9341_t3-format fonts (the shared Roboto Black 32 / 48), drawn transparent,
   glyph row 0 at y, as KerbalDisplayCommon's kcmDrawString.

   Two checks run on every string, on the device and in the host tests (design D4):
     g_gfxSmallText  a font whose capital H is shorter than SI_MIN_CAP_PX (22 px)
     g_gfxOverflow   a string outside the 2 px margins, in layout coordinates
   tools/sysinfo_host.py fails any scenario that leaves either above zero.

   Anti-sticking (datasheet §8.3, design §2.4): every SHIFT_MS the whole image moves to
   the next of five offsets -- centre, 2 px up, right, down, left.
****************************************************************************************/
static Adafruit_ST7789 g_tft(&SPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
static uint16_t g_fb[SI_W * SI_H];
static int16_t  g_ox, g_oy;
static uint32_t g_lastHash;
static bool     g_pushedOnce;
uint32_t g_gfxSmallText, g_gfxOverflow, g_gfxPushes;

static const int8_t SHIFT_X[5] = { 0, 0, 2, 0, -2 };
static const int8_t SHIFT_Y[5] = { 0, -2, 0, 2, 0 };

void gfxBegin() {
  g_tft.init(SI_W, SI_H);
  g_tft.setRotation(TFT_ROTATION);
  g_tft.setSPISpeed(TFT_SPI_HZ);
  g_tft.fillScreen(C_BLACK);
}

uint16_t *gfxBuffer() { return g_fb; }

void gfxBeginFrame(uint32_t now) {
  uint8_t k = (uint8_t)((now / SHIFT_MS) % 5);
  g_ox = SHIFT_X[k];
  g_oy = SHIFT_Y[k];
  for (int32_t i = 0; i < (int32_t)SI_W * SI_H; i++) g_fb[i] = C_BLACK;
}

void gfxRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  x += g_ox; y += g_oy;
  int16_t x0 = max(x, (int16_t)0), y0 = max(y, (int16_t)0);
  int16_t x1 = min((int16_t)(x + w), SI_W), y1 = min((int16_t)(y + h), SI_H);
  for (int16_t yy = y0; yy < y1; yy++)
    for (int16_t xx = x0; xx < x1; xx++) g_fb[yy * SI_W + xx] = c;
}

static inline void gfxPixel(int16_t x, int16_t y, uint16_t c) {
  x += g_ox; y += g_oy;
  if (x >= 0 && x < SI_W && y >= 0 && y < SI_H) g_fb[y * SI_W + x] = c;
}

/*--------------------------------------------------------------------------------------
  Fonts
--------------------------------------------------------------------------------------*/
static uint32_t fbits(const uint8_t *p, uint32_t index, uint32_t n) {
  uint32_t v = 0;
  for (uint32_t i = 0; i < n; i++) v = (v << 1) | ((p[(index + i) >> 3] >> (7 - ((index + i) & 7))) & 1u);
  return v;
}

static bool glyphOf(const ILI9341_t3_font_t *f, char ch, Glyph &g) {
  uint8_t code = (uint8_t)ch;
  uint32_t ip;
  if (code >= f->index1_first && code <= f->index1_last) ip = code - f->index1_first;
  else if (f->index2_last >= f->index2_first && code >= f->index2_first && code <= f->index2_last)
    ip = (uint32_t)(f->index1_last - f->index1_first + 1) + (code - f->index2_first);
  else return false;
  g.d = f->data + fbits(f->index, ip * f->bits_index, f->bits_index);
  uint32_t bo = 3;
  g.w = (uint16_t)fbits(g.d, bo, f->bits_width);  bo += f->bits_width;
  g.h = (uint16_t)fbits(g.d, bo, f->bits_height); bo += f->bits_height;
  bo += f->bits_xoffset + f->bits_yoffset;
  g.delta = (uint16_t)fbits(g.d, bo, f->bits_delta); bo += f->bits_delta;
  g.bmp = bo;
  return true;
}

static inline bool glyphBit(const Glyph &g, uint16_t r, uint16_t c) {
  uint32_t bi = g.bmp + (uint32_t)r * (1u + g.w) + 1u + c;   // each row: a flag bit, then w bits
  return (g.d[bi >> 3] >> (7 - (bi & 7))) & 1u;
}

// The height of the font's capital H in pixels, measured once per font.
uint8_t gfxCapHeight(const ILI9341_t3_font_t *f) {
  static const ILI9341_t3_font_t *cf[4];
  static uint8_t cv[4];
  for (uint8_t i = 0; i < 4; i++) if (cf[i] == f) return cv[i];
  Glyph g;
  int16_t top = -1, bot = -1;
  if (glyphOf(f, 'H', g))
    for (uint16_t r = 0; r < g.h; r++)
      for (uint16_t c = 0; c < g.w; c++)
        if (glyphBit(g, r, c)) { if (top < 0) top = r; bot = r; }
  uint8_t cap = top < 0 ? 0 : (uint8_t)(bot - top + 1);
  for (uint8_t i = 0; i < 4; i++) if (!cf[i]) { cf[i] = f; cv[i] = cap; break; }
  return cap;
}

int16_t gfxTextWidth(const ILI9341_t3_font_t *f, const char *s) {
  int16_t w = 0;
  Glyph g;
  for (; *s; s++) if (glyphOf(f, *s, g)) w += g.delta;
  return w;
}

// Draws s with its glyph row 0 at y; x is the left edge, right edge or centre by align.
// Returns the x after the string.
int16_t gfxText(int16_t x, int16_t y, const ILI9341_t3_font_t *f, const char *s, uint16_t c, Align a) {
  int16_t w = gfxTextWidth(f, s);
  if (a == AL_RIGHT) x -= w;
  else if (a == AL_CENTRE) x -= w / 2;
  if (gfxCapHeight(f) < SI_MIN_CAP_PX) g_gfxSmallText++;
  if (w > 0 && (x < SI_MARGIN - 1 || x + w > SI_W - SI_MARGIN + 1)) g_gfxOverflow++;
  Glyph g;
  for (; *s; s++) {
    if (!glyphOf(f, *s, g)) continue;
    for (uint16_t r = 0; r < g.h; r++)
      for (uint16_t col = 0; col < g.w; col++)
        if (glyphBit(g, r, col)) gfxPixel(x + col, y + r, c);
    x += g.delta;
  }
  return x;
}

// A 16 x 16 icon at 2x.
void gfxIcon(int16_t x, int16_t y, const Icon &ic, uint16_t c) {
  for (uint8_t r = 0; r < 16; r++)
    for (uint8_t k = 0; k < 16 && ic[r][k]; k++)
      if (ic[r][k] == '#') gfxRect(x + 2 * k, y + 2 * r, 2, 2, c);
}

// Word wrap by pixel width into up to `rows` lines of `out` (each SI_TEXT_MAX + 1);
// runs of spaces collapse; a word wider than a row is cut. Returns the rows used.
uint8_t gfxWrap(const ILI9341_t3_font_t *f, const char *text, int16_t width, char out[][SI_TEXT_MAX + 1], uint8_t rows) {
  uint8_t n = 0;
  char cur[SI_TEXT_MAX + 1] = "";
  const char *p = text;
  while (*p && n < rows) {
    while (*p == ' ') p++;
    if (!*p) break;
    char word[SI_TEXT_MAX + 1];
    uint8_t wl = 0;
    while (*p && *p != ' ' && wl < SI_TEXT_MAX) word[wl++] = *p++;
    word[wl] = 0;
    char cand[2 * SI_TEXT_MAX + 2];
    snprintf(cand, sizeof cand, "%s%s%s", cur, cur[0] ? " " : "", word);
    if (gfxTextWidth(f, cand) <= width && strlen(cand) <= SI_TEXT_MAX) {
      strcpy(cur, cand);
      continue;
    }
    if (cur[0]) {
      strcpy(out[n++], cur);
      cur[0] = 0;
      if (n >= rows) break;
    }
    while (wl > 1 && gfxTextWidth(f, word) > width) word[--wl] = 0;
    strcpy(cur, word);
  }
  if (cur[0] && n < rows) strcpy(out[n++], cur);
  return n;
}

/*--------------------------------------------------------------------------------------
  Push
--------------------------------------------------------------------------------------*/
static uint32_t fbHash() {
  uint32_t h = 2166136261u;
  const uint32_t *w = (const uint32_t *)g_fb;
  for (int32_t i = 0; i < (int32_t)(SI_W * SI_H / 2); i++) { h ^= w[i]; h *= 16777619u; }
  return h;
}

void gfxPush() {
  uint32_t h = fbHash();
  if (g_pushedOnce && h == g_lastHash) return;
  g_lastHash = h;
  g_pushedOnce = true;
  g_tft.startWrite();
  g_tft.setAddrWindow(0, 0, SI_W, SI_H);
  g_tft.writePixels(g_fb, (uint32_t)SI_W * SI_H, true, false);
  g_tft.endWrite();
  g_gfxPushes++;
}

/*--------------------------------------------------------------------------------------
  Shared pieces
--------------------------------------------------------------------------------------*/
static const ILI9341_t3_font_t *const F32 = &Roboto_Black_32;
static const ILI9341_t3_font_t *const F48 = &Roboto_Black_48;
static const int16_t TEXT_PITCH = 34;           // Roboto Black 32 rows (caps 23 px)

void titleBar(const char *left, const char *right, uint16_t bg) {
  gfxRect(0, 0, SI_W, 44, bg);
  gfxText(6, 0, F32, left, C_WHITE, AL_LEFT);
  if (right) gfxText(SI_W - 6, 0, F32, right, C_WHITE, AL_RIGHT);
}

// The red MASTER / SILENT n S block, at y, h tall.
void silentBlock(int16_t y, int16_t h, uint32_t now) {
  char t[24];
  gfxRect(0, y, SI_W, h, C_RED);
  gfxText(SI_W / 2, y + 8, F32, "MASTER", C_WHITE, AL_CENTRE);
  snprintf(t, sizeof t, "SILENT %lu S", (unsigned long)silentSeconds(now));
  gfxText(SI_W / 2, y + 48, F32, t, C_WHITE, AL_CENTRE);
}

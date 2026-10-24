// Framebuffer-backed stand-in for RA8876_t41_p, for tools/render_screen.py: every
// drawing call the KCMk1 panels and KerbalDisplayCommon make lands in a 1024x600
// RGB565 buffer the harness dumps to disk. Same surface as ../host_stubs/RA8876_t41_p.h,
// but it draws.
//
// FIDELITY. Each primitive follows wwatson4506/TeensyRA8876-GFX-Common
// (src/RA8876_common.cpp), the library the panels link against, including its
// clipping rules, which are not the textbook ones:
//   - Parameters the library takes as ru16 (uint16) wrap: a negative coordinate
//     becomes a large positive one. Coordinates written to the controller are
//     13-bit, so the hardware sees (value & 0x1FFF).
//   - drawLine is dropped whole if its START point is off-screen; the end point
//     is not checked. drawPixel is not clipped in software.
//   - drawTriangle / fillTriangle are dropped whole if any vertex has x <= 0 or
//     x >= 1024, or y >= 600 (so a vertex at x = 0, or any negative y, drops it).
//   - drawCircle / fillCircle are dropped only if entirely off-screen; r < 2 draws
//     one pixel; r is capped at 299.
//   - drawRect / fillRect clamp to the screen, so a rectangle hanging off the left
//     or top edge gains an edge AT the screen boundary.
//   - writeRect is a 2D-engine (BTE) write relative to `currentPage` with a page
//     width of 1024: it ignores the active window and a row running past x = 1023
//     wraps onto the next row.
//   - Text (ILI9341_t3 fonts) is opaque after setTextColor(fg, bg) — each glyph
//     paints a delta x line_space box of bg — and transparent after
//     setTextColor(fg). The cursor clamps at 0; a glyph that would cross x = 1024
//     is not drawn (wrap is off); '\n' is ignored and '\r' starts a new line.
//     Print's number formatting is reproduced (integers in decimal, floats to two
//     places).
// Everything else the geometry engine draws is clipped by the active window
// (activeWindowXY/WH), after the canvas origin the panel set with
// canvasImageStartAddress() (the InfoDisp shifts its origin past a left-hand
// sidebar that way).
//
// One behaviour is NOT known from the library source: the font path issues
// zero-width / zero-height fills (fillRect(x, y, 0, n)), which reach the controller
// as a rectangle whose end corner precedes its start. How the RA8876 fills an
// inverted rectangle is undocumented there; this stub draws nothing for it.
//
// Coordinates that wrap negative are logged to stderr (once per call site kind) so
// a render that depends on them can be spotted.
#pragma once
#include <Arduino.h>
#include <vector>
#include <cstdio>
#include "kcm_ili9341_font.h"

class RA8876_t41_p {
 public:
  static const int W = 1024, H = 600;
  uint32_t currentPage = 0;
  std::vector<uint16_t> fb;
  RA8876_t41_p(int, int, int) : fb((size_t)W * H, 0) {}
  void setBusWidth(int) {}
  void begin(int = 20) {}
  void setRotation(int) {}

  // ---- canvas origin, active window, raw pixel ---------------------------------
  // All three double-buffer pages map onto this one framebuffer (the BTE copy
  // between them is then the identity); a page base is a multiple of PAGE_BYTES, so
  // the in-page byte offset / 2 is the pixel offset of the canvas origin.
  static const uint32_t PAGE_BYTES = (uint32_t)W * H * 2;
  int originX = 0, winX = 0, winY = 0, winW = W, winH = H;
  inline void px(int x, int y, uint16_t c) {           // geometry engine: window-clipped
    if (x < winX || y < winY || x >= winX + winW || y >= winY + winH) return;
    x += originX;
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    fb[(size_t)y * W + x] = c;
  }
  static inline int u16(int v) { return (uint16_t)v; }           // ru16 parameter
  static inline int hw(int v) { return v & 0x1FFF; }             // 13-bit register
  unsigned warnBits = 0;
  void warn(unsigned bit, const char *what) {
    if (warnBits & bit) return;
    warnBits |= bit;
    fprintf(stderr, "[stub] %s\n", what);
  }

  // ---- rectangles ----------------------------------------------------------------
  // Library fillRect/drawRect: clamp to the screen, then the controller fills the
  // inclusive box (x, y)-(x_end, y_end).
  bool clampRect(int &x, int &y, int &xe, int &ye, int w, int h) {
    x = (int16_t)x; y = (int16_t)y;
    xe = (int16_t)(x + w - 1); ye = (int16_t)(y + h - 1);
    if (x >= W || y >= H || xe < 0 || ye < 0) return false;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (xe > W) xe = W;
    if (ye > H) ye = H;
    return xe >= x && ye >= y;   // inverted box: undocumented on the controller, drawn as nothing
  }
  void fillBox(int x, int y, int xe, int ye, uint16_t c) {
    for (int j = y; j <= ye; j++) for (int i = x; i <= xe; i++) px(i, j, c);
  }
  void fillRect(int x, int y, int w, int h, uint16_t c) {
    int xe, ye;
    if (clampRect(x, y, xe, ye, w, h)) fillBox(x, y, xe, ye, c);
  }
  void drawRect(int x, int y, int w, int h, uint16_t c) {
    int xe, ye;
    if (!clampRect(x, y, xe, ye, w, h)) return;
    for (int i = x; i <= xe; i++) { px(i, y, c); px(i, ye, c); }
    for (int j = y; j <= ye; j++) { px(x, j, c); px(xe, j, c); }
  }
  // Screen-sized fill of the scroll region (the whole canvas), window-clipped.
  void fillScreen(uint16_t c) { fillBox(0, 0, W - 1, H - 1, c); }
  void drawFastHLine(int x, int y, int w, uint16_t c) {
    x = (int16_t)x; y = (int16_t)y; w = (int16_t)w;
    if (y < 0 || x >= W || y >= H) return;
    if (x < 0) { w -= -x; x = 0; }
    if (x + w - 1 >= W) w = W - x;
    if (w < 1) w = 1;
    if (w < 2) drawPixel(x, y, c); else drawLine(x, y, x + w - 1, y, c);
  }
  void drawFastVLine(int x, int y, int h, uint16_t c) { drawLine(x, y, x, y + h - 1, c); }

  // ---- pixels and lines ----------------------------------------------------------
  void drawPixel(int x, int y, uint16_t c) { px(hw(u16(x)), hw(u16(y)), c); }
  void bresenham(int x0, int y0, int x1, int y1, uint16_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
      px(x0, y0, c);
      if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
    if (x1 < 0 || y1 < 0) warn(1, "drawLine end point negative: controller sees a wrapped coordinate");
    x0 = u16(x0); y0 = u16(y0); x1 = u16(x1); y1 = u16(y1);
    if (x0 >= W || y0 >= H) return;                    // start off-screen: dropped whole
    if (x0 == x1 && y0 == y1) { drawPixel(x0, y0, c); return; }
    bresenham(x0, y0, hw(x1), hw(y1), c);
  }

  // ---- circles ---------------------------------------------------------------------
  // 0 = dropped, 1 = single pixel (r < 2), 2 = draw with (cx, cy, r) as the controller sees them.
  int circleClip(int &x0, int &y0, int &r) {
    x0 = u16(x0); y0 = u16(y0); r = u16(r);
    int16_t dia = (int16_t)(2 * r);
    int16_t xs = (int16_t)(x0 - r), ys = (int16_t)(y0 + r);
    int16_t xe = (int16_t)(xs + dia), ye = (int16_t)(ys - dia);
    if (xs >= W || ys >= H || xe < 0 || ye < 0) return 0;
    if (r < 2) return 1;
    if (r > H / 2) r = H / 2 - 1;
    x0 = hw(x0); y0 = hw(y0);
    return 2;
  }
  void drawCircle(int cx, int cy, int r, uint16_t c) {
    int k = circleClip(cx, cy, r);
    if (k == 1) { drawPixel(cx, cy, c); return; }
    if (k == 0) return;
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
      px(cx + x, cy + y, c); px(cx + y, cy + x, c); px(cx - y, cy + x, c); px(cx - x, cy + y, c);
      px(cx - x, cy - y, c); px(cx - y, cy - x, c); px(cx + y, cy - x, c); px(cx + x, cy - y, c);
      y++;
      if (err < 0) err += 2 * y + 1; else { x--; err += 2 * (y - x) + 1; }
    }
  }
  void fillCircle(int cx, int cy, int r, uint16_t c) {
    int k = circleClip(cx, cy, r);
    if (k == 1) { drawPixel(cx, cy, c); return; }
    if (k == 0) return;
    for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++)
      if (i * i + j * j <= r * r) px(cx + i, cy + j, c);
  }

  // ---- triangles -------------------------------------------------------------------
  bool triClip(int &x0, int &y0, int &x1, int &y1, int &x2, int &y2) {
    if (y0 < 0 || y1 < 0 || y2 < 0) warn(2, "triangle vertex with negative y: dropped whole by the library");
    x0 = u16(x0); y0 = u16(y0); x1 = u16(x1); y1 = u16(y1); x2 = u16(x2); y2 = u16(y2);
    if (x0 <= 0 || x1 <= 0 || x2 <= 0 || x0 >= W || x1 >= W || x2 >= W) return false;
    if (y0 >= H || y1 >= H || y2 >= H) return false;
    return true;
  }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    if (!triClip(x0, y0, x1, y1, x2, y2)) return;
    if (x0 == x1 && y0 == y1 && x0 == x2 && y0 == y2) { drawPixel(x0, y0, c); return; }
    int minY = y0, maxY = y0;
    if (y1 < minY) minY = y1; if (y2 < minY) minY = y2;
    if (y1 > maxY) maxY = y1; if (y2 > maxY) maxY = y2;
    for (int y = minY; y <= maxY; y++) {
      int xs[3]; int n = 0;
      auto edge = [&](int ax, int ay, int bx, int by) {
        if (ay == by) return;
        if ((y >= ay && y <= by) || (y >= by && y <= ay))
          xs[n++] = ax + (int)((long)(y - ay) * (bx - ax) / (by - ay));
      };
      edge(x0, y0, x1, y1); edge(x1, y1, x2, y2); edge(x2, y2, x0, y0);
      if (n < 2) { if (y == y0 && y == y1) { int lo = x0 < x1 ? x0 : x1, hi = x0 < x1 ? x1 : x0; for (int i = lo; i <= hi; i++) px(i, y, c); } continue; }
      int lo = xs[0], hi = xs[0];
      for (int k = 1; k < n; k++) { if (xs[k] < lo) lo = xs[k]; if (xs[k] > hi) hi = xs[k]; }
      for (int i = lo; i <= hi; i++) px(i, y, c);
    }
  }
  void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    if (!triClip(x0, y0, x1, y1, x2, y2)) return;
    bresenham(x0, y0, x1, y1, c); bresenham(x1, y1, x2, y2, c); bresenham(x2, y2, x0, y0, c);
  }
  void fillRoundRect(int x, int y, int w, int h, int, uint16_t c) { fillRect(x, y, w, h, c); }
  void drawRoundRect(int x, int y, int w, int h, int, uint16_t c) { drawRect(x, y, w, h, c); }

  // ---- ILI9341_t3 text (RA8876_common::drawFontChar, 1 bpp path) -------------------
  const ILI9341_t3_font_t *font = nullptr;
  uint16_t textFg = 0xFFFF, textBg = 0;
  bool textTransparent = false;
  int cursorX = 0, cursorY = 0;
  void setFont(const ILI9341_t3_font_t &f) { font = &f; }
  void setTextColor(uint16_t fg, uint16_t bg) { textFg = fg; textBg = bg; textTransparent = false; }
  void setTextColor(uint16_t fg) { textFg = fg; textTransparent = true; }
  void setCursor(int x, int y) { cursorX = (int16_t)x < 0 ? 0 : (int16_t)x; cursorY = (int16_t)y < 0 ? 0 : (int16_t)y; }
  static uint32_t fetchbit(const uint8_t *p, uint32_t i) { return (p[i >> 3] >> (7 - (i & 7))) & 1u; }
  static uint32_t fetchbits_unsigned(const uint8_t *p, uint32_t i, uint32_t n) {
    uint32_t v = 0; for (uint32_t k = 0; k < n; k++) v = (v << 1) | fetchbit(p, i + k); return v;
  }
  static int32_t fetchbits_signed(const uint8_t *p, uint32_t i, uint32_t n) {
    uint32_t v = fetchbits_unsigned(p, i, n);
    if (v & (1u << (n - 1))) return (int32_t)v - (int32_t)(1u << n);
    return (int32_t)v;
  }
  // RA8876_common::drawFontBits: runs of set bits as fg fills, runs of clear bits as
  // bg fills when opaque.
  void drawFontBits(bool opaque, uint32_t bits, uint32_t numbits, int x, int y, uint32_t repeat) {
    if (bits == 0) { if (opaque) fillRect(x, y, numbits, repeat, textBg); return; }
    int x1 = x, w = 0, bgw = 0; uint32_t n = numbits;
    do {
      n--;
      if (bits & (1u << n)) { if (bgw > 0) { if (opaque) fillRect(x1 - bgw, y, bgw, repeat, textBg); bgw = 0; } w++; }
      else { if (w > 0) { fillRect(x1 - w, y, w, repeat, textFg); w = 0; } bgw++; }
      x1++;
    } while (n > 0);
    if (w > 0) fillRect(x1 - w, y, w, repeat, textFg);
    if (bgw > 0 && opaque) fillRect(x1 - bgw, y, bgw, repeat, textBg);
  }
  void drawFontChar(unsigned int c) {
    if (!font) return;
    uint32_t bo = 0;
    if (c >= font->index1_first && c <= font->index1_last) bo = (c - font->index1_first) * font->bits_index;
    else if (c >= font->index2_first && c <= font->index2_last)
      bo = (c - font->index2_first + font->index1_last - font->index1_first + 1) * font->bits_index;
    const uint8_t *data = font->data + fetchbits_unsigned(font->index, bo, font->bits_index);
    if (fetchbits_unsigned(data, 0, 3) != 0) return;
    uint32_t off = 3;
    uint32_t width = fetchbits_unsigned(data, off, font->bits_width);   off += font->bits_width;
    uint32_t height = fetchbits_unsigned(data, off, font->bits_height); off += font->bits_height;
    int32_t xoffset = fetchbits_signed(data, off, font->bits_xoffset);  off += font->bits_xoffset;
    int32_t yoffset = fetchbits_signed(data, off, font->bits_yoffset);  off += font->bits_yoffset;
    uint32_t delta = fetchbits_unsigned(data, off, font->bits_delta);   off += font->bits_delta;
    if (cursorX < 0) cursorX = 0;
    int origin_x = cursorX + xoffset;
    if (origin_x < 0) { cursorX -= xoffset; origin_x = 0; }
    if (origin_x + (int)width > W) return;               // wrap is off in the library
    if (cursorY >= H) return;
    int origin_y = cursorY + font->cap_height - (int)height - yoffset;
    int32_t linecount = (int32_t)height;
    int y = origin_y;
    bool opaque = !textTransparent;
    if (opaque) fillRect(cursorX, cursorY, delta, y - cursorY, textBg);
    while (linecount > 0) {
      uint32_t n = 1;
      if (fetchbit(data, off++) != 0) { n = fetchbits_unsigned(data, off, 3) + 2; off += 3; }
      if (opaque) fillRect(cursorX, y, origin_x - cursorX, n, textBg);
      uint32_t x = 0;
      do {
        int32_t xsize = (int32_t)width - (int32_t)x;
        if (xsize > 32) xsize = 32;
        uint32_t bits = fetchbits_unsigned(data, off, xsize);
        drawFontBits(opaque, bits, xsize, origin_x + x, y, n);
        off += xsize; x += xsize;
      } while (x < width);
      if (opaque && (int)(width + xoffset) < (int)delta) fillRect(origin_x + x, y, delta - (width + xoffset), n, textBg);
      y += n; linecount -= n;
    }
    if (opaque) fillRect(cursorX, y, delta, cursorY + font->line_space - y, textBg);
    cursorX += (int)delta;
  }
  // Print::write -> RA8876_common::_fontWrite: '\n' ignored, '\r' is a new line.
  size_t writeStr(const char *s) {
    size_t n = 0;
    for (; s && *s; s++, n++) {
      uint8_t ch = (uint8_t)*s;
      if (ch == '\n') continue;
      if (ch == 13) { if (font) { cursorY += font->line_space; cursorX = 0; } continue; }
      drawFontChar(ch);
    }
    return n;
  }
  size_t print(const char *s) { return writeStr(s); }
  size_t print(char *s) { return writeStr(s); }
  size_t print(const String &s) { return writeStr(s.c_str()); }
  size_t print(char ch) { char b[2] = { ch, 0 }; return writeStr(b); }
  size_t print(unsigned char v) { return printNum((unsigned long)v); }
  size_t print(int v) { return printNum((long)v); }
  size_t print(unsigned int v) { return printNum((unsigned long)v); }
  size_t print(long v) { char b[24]; snprintf(b, sizeof b, "%ld", v); return writeStr(b); }
  size_t print(unsigned long v) { char b[24]; snprintf(b, sizeof b, "%lu", v); return writeStr(b); }
  size_t print(double v, int digits = 2) { char b[40]; snprintf(b, sizeof b, "%.*f", digits, v); return writeStr(b); }
  size_t print(float v) { return print((double)v, 2); }
  size_t printNum(long v) { return print(v); }
  size_t printNum(unsigned long v) { return print(v); }
  template <class T> size_t println(const T &v) { size_t n = print(v); return n + writeStr("\r\n"); }

  // ---- 2D-engine blit (RA8876_common::writeRect -> bteMpuWriteWithROPData16) ---------
  void writeRect(int x, int y, int w, int h, const uint16_t *p) {
    uint32_t base = (currentPage % PAGE_BYTES) / 2;
    uint32_t sx = (uint16_t)x, sy = (uint16_t)y;
    for (int j = 0; j < h; j++) {
      uint32_t row = base + (sy + j) * (uint32_t)W + sx;
      for (int i = 0; i < w; i++) {
        uint32_t idx = row + i;
        if (idx < (uint32_t)W * H) fb[idx] = p[(size_t)j * w + i];
      }
    }
  }

  // ---- canvas / page control -------------------------------------------------------
  void activeWindowXY(int x, int y) { winX = x; winY = y; }
  void activeWindowWH(int w, int h) { winW = w; winH = h; }
  int  canvasImageWidth() { return W; }
  void canvasImageWidth(int) {}
  void canvasImageWidth(int, int) {}
  void canvasImageStartAddress(uint32_t a) { originX = (int)((a % PAGE_BYTES) / 2); }
  void displayImageStartAddress(uint32_t) {}
  void displayImageWidth(int) {}
  void displayWindowStartXY(int, int) {}
  void check2dBusy() {}
  void checkWriteFifoEmpty() {}
  void bteMemoryCopy(uint32_t, int, int, int, uint32_t, int, int, int, int, int) {}

  // Dump as binary PPM (P6), RGB565 -> RGB888 with bit replication.
  bool writePPM(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    std::vector<uint8_t> row((size_t)W * 3);
    for (int y = 0; y < H; y++) {
      for (int x = 0; x < W; x++) {
        uint16_t c = fb[(size_t)y * W + x];
        uint8_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
        row[x * 3 + 0] = (r << 3) | (r >> 2);
        row[x * 3 + 1] = (g << 2) | (g >> 4);
        row[x * 3 + 2] = (b << 3) | (b >> 2);
      }
      fwrite(row.data(), 1, row.size(), f);
    }
    fclose(f);
    return true;
  }
};

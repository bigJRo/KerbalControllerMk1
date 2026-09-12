// Framebuffer-backed stand-in for RA8876_t41_p, for tools/render_screen.py: every
// drawing call the KCMk1 panels and KerbalDisplayCommon make lands in a 1024x600
// RGB565 buffer the harness dumps to disk. Same surface as ../host_stubs/RA8876_t41_p.h,
// but it draws — geometry, writeRect blits (text and BMPs) and the ILI9341_t3 glyph
// renderer drawButton() prints through.
#pragma once
#include <Arduino.h>
#include <vector>
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
  // Canvas origin and active window. The InfoDisp shifts its drawing origin by
  // re-pointing the canvas base address (canvasContentRegion(): base + CONTENT_X*2
  // bytes) and clamps the active window to the content width; every page base is
  // a multiple of the page size, so the in-page byte offset / 2 is the x origin.
  // All three double-buffer pages map onto this one framebuffer (the BTE copy
  // between them is then the identity).
  static const uint32_t PAGE_BYTES = (uint32_t)W * H * 2;
  int originX = 0, winX = 0, winY = 0, winW = W, winH = H;
  inline void px(int x, int y, uint16_t c) {
    if (x < winX || y < winY || x >= winX + winW || y >= winY + winH) return;
    x += originX;
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    fb[(size_t)y * W + x] = c;
  }
  void fillScreen(uint16_t c) { fillRect(winX, winY, winW, winH, c); }
  void fillRect(int x, int y, int w, int h, uint16_t c) {
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) px(i, j, c);
  }
  void drawRect(int x, int y, int w, int h, uint16_t c) {
    drawFastHLine(x, y, w, c); drawFastHLine(x, y + h - 1, w, c);
    drawFastVLine(x, y, h, c); drawFastVLine(x + w - 1, y, h, c);
  }
  void drawFastHLine(int x, int y, int w, uint16_t c) { for (int i = x; i < x + w; i++) px(i, y, c); }
  void drawFastVLine(int x, int y, int h, uint16_t c) { for (int j = y; j < y + h; j++) px(x, j, c); }
  void drawPixel(int x, int y, uint16_t c) { px(x, y, c); }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {   // Bresenham, as GFX does
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
  void drawCircle(int cx, int cy, int r, uint16_t c) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
      px(cx + x, cy + y, c); px(cx + y, cy + x, c); px(cx - y, cy + x, c); px(cx - x, cy + y, c);
      px(cx - x, cy - y, c); px(cx - y, cy - x, c); px(cx + y, cy - x, c); px(cx + x, cy - y, c);
      y++;
      if (err < 0) err += 2 * y + 1; else { x--; err += 2 * (y - x) + 1; }
    }
  }
  void fillCircle(int cx, int cy, int r, uint16_t c) {
    for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++)
      if (i * i + j * j <= r * r) px(cx + i, cy + j, c);
  }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
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
      if (n < 2) continue;
      int lo = xs[0], hi = xs[0];
      for (int k = 1; k < n; k++) { if (xs[k] < lo) lo = xs[k]; if (xs[k] > hi) hi = xs[k]; }
      drawFastHLine(lo, y, hi - lo + 1, c);
    }
  }
  void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    drawLine(x0, y0, x1, y1, c); drawLine(x1, y1, x2, y2, c); drawLine(x2, y2, x0, y0, c);
  }
  void fillRoundRect(int x, int y, int w, int h, int, uint16_t c) { fillRect(x, y, w, h, c); }
  void drawRoundRect(int x, int y, int w, int h, int, uint16_t c) { drawRect(x, y, w, h, c); }
  // ILI9341_t3-format text renderer (the GFX-Common library's drawFontChar):
  // drawButton() and drawVerticalText() print through it. Set bits are drawn in
  // the text colour; the callers fill the background themselves.
  const ILI9341_t3_font_t *font = nullptr;
  uint16_t textFg = 0xFFFF, textBg = 0;
  int cursorX = 0, cursorY = 0;
  void setFont(const ILI9341_t3_font_t &f) { font = &f; }
  void setTextColor(uint16_t fg, uint16_t bg) { textFg = fg; textBg = bg; }
  void setTextColor(uint16_t fg) { textFg = fg; textBg = fg; }
  void setCursor(int x, int y) { cursorX = x; cursorY = y; }
  static uint32_t fetchbit(const uint8_t *p, uint32_t i) { return (p[i >> 3] >> (7 - (i & 7))) & 1u; }
  static uint32_t fetchbits_unsigned(const uint8_t *p, uint32_t i, uint32_t n) {
    uint32_t v = 0; for (uint32_t k = 0; k < n; k++) v = (v << 1) | fetchbit(p, i + k); return v;
  }
  static int32_t fetchbits_signed(const uint8_t *p, uint32_t i, uint32_t n) {
    uint32_t v = fetchbits_unsigned(p, i, n);
    if (v & (1u << (n - 1))) return (int32_t)v - (int32_t)(1u << n);
    return (int32_t)v;
  }
  void drawFontBits(const uint8_t *d, uint32_t bits, uint32_t off, int x, int y, uint32_t rep) {
    for (uint32_t c = 0; c < bits; c++)
      if (fetchbit(d, off + c)) for (uint32_t r = 0; r < rep; r++) px(x + (int)c, y + (int)r, textFg);
  }
  void drawFontChar(unsigned int c) {
    if (!font) return;
    uint32_t bo;
    if (c >= font->index1_first && c <= font->index1_last) bo = c - font->index1_first;
    else if (c >= font->index2_first && c <= font->index2_last)
      bo = c - font->index2_first + font->index1_last - font->index1_first + 1;
    else return;
    const uint8_t *data = font->data + fetchbits_unsigned(font->index, bo * font->bits_index, font->bits_index);
    uint32_t encoding = fetchbits_unsigned(data, 0, 3);
    if (encoding != 0) return;
    uint32_t off = 3;
    uint32_t width = fetchbits_unsigned(data, off, font->bits_width);   off += font->bits_width;
    uint32_t height = fetchbits_unsigned(data, off, font->bits_height); off += font->bits_height;
    int32_t xoffset = fetchbits_signed(data, off, font->bits_xoffset);  off += font->bits_xoffset;
    int32_t yoffset = fetchbits_signed(data, off, font->bits_yoffset);  off += font->bits_yoffset;
    uint32_t delta = fetchbits_unsigned(data, off, font->bits_delta);   off += font->bits_delta;
    int origin_x = cursorX + xoffset;
    int origin_y = cursorY + font->cap_height - (int)height - yoffset;
    int32_t linecount = (int32_t)height; int y = origin_y;
    while (linecount > 0) {
      uint32_t b = fetchbit(data, off++);
      if (b == 0) { drawFontBits(data, width, off, origin_x, y, 1); off += width; linecount--; y++; }
      else {
        uint32_t n = fetchbits_unsigned(data, off, 3) + 2; off += 3;
        drawFontBits(data, width, off, origin_x, y, n); off += width; linecount -= (int32_t)n; y += (int)n;
      }
    }
    cursorX += (int)delta;
  }
  size_t print(const char *s) { for (; s && *s; s++) drawFontChar((uint8_t)*s); return 0; }
  size_t print(const String &s) { return print(s.c_str()); }
  template <size_t N> size_t print(const char (&s)[N]) { return print((const char *)s); }   // char[] beats the generic template
  size_t print(char ch) { char b[2] = { ch, 0 }; return print(b); }
  template <class T> size_t print(const T &) { return 0; }
  template <class T> size_t println(const T &) { return 0; }
  void writeRect(int x, int y, int w, int h, const uint16_t *p) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j, p[(size_t)j * w + i]);
  }
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

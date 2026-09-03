#include "KerbalDisplayCommon.h"
#include <cfloat>   // DBL_MAX -- used in _bodyTable Kerbol entry

/***************************************************************************************
   KerbalDisplayCommon Library - Implementation
   See KerbalDisplayCommon.h for full documentation.
****************************************************************************************/

// Debug mode flag — set via setKDCDebugMode(true) from the sketch.
// When false: only error messages are printed (SD failures, BMP errors).
// When true:  verbose setup diagnostics are also printed (touch init, I2C scan, etc.).
static bool _kdcDebugMode = false;
void setKDCDebugMode(bool enable) { _kdcDebugMode = enable; }

// Missing-glyph diagnostic counter for getFontCharWidth (#A4). Rate-limited to
// keep Serial readable during long renders. Counter persists across calls and
// resets only on reboot.
static uint8_t _missingGlyphsLogged = 0;
static const uint8_t _MISSING_GLYPH_LOG_LIMIT = 16;

const byte TEXT_BORDER = 8;  // horizontal padding from edge


/***************************************************************************************
   DISPLAY SETUP
   Initialises the RA8876 display (16-bit FlexIO 8080), sets the background colour,
   and clears the screen. Call once from setup() before any drawing functions.
   The KCM_TFT object must already be constructed with its (RS,CS,RESET) pins.
****************************************************************************************/
void setupDisplay(KCM_TFT &tft, uint16_t backColor) {
  kcmDisplayBegin(tft, backColor);   // setBusWidth(16) + begin() + rotation + backlight
  if (_kdcDebugMode) Serial.println(F("KDC: setupDisplay: RA8876 ready"));
}

/***************************************************************************************
   FONT MEASUREMENT
   Reads the per-glyph advance ("delta") from the ILI9341_t3 font data. The fonts
   are converted from the original sumotoy tFont with delta == glyph width, so the
   measured advance matches the rendered advance (see fonts_ili/).
****************************************************************************************/

// Read `required` bits (MSB-first) starting at bit `index` from a bit-packed
// byte array — mirrors ILI9341_t3's fetchbits_unsigned so measurement matches
// the renderer.
static uint32_t _fetchbits_unsigned(const uint8_t *p, uint32_t index, uint32_t required) {
  uint32_t val = 0;
  for (uint32_t i = 0; i < required; i++) {
    uint32_t b = (p[(index + i) >> 3] >> (7 - ((index + i) & 7))) & 1u;
    val = (val << 1) | b;
  }
  return val;
}

int16_t getFontCharWidth(const ILI9341_t3_font_t *font, char c) {
  uint8_t code = (uint8_t)c;
  uint32_t idxPos;
  if (code >= font->index1_first && code <= font->index1_last) {
    idxPos = code - font->index1_first;
  } else if (font->index2_last >= font->index2_first &&
             code >= font->index2_first && code <= font->index2_last) {
    idxPos = (font->index1_last - font->index1_first + 1) + (code - font->index2_first);
  } else {
    // Glyph not in font — width 0 drifts right/centre alignment. (#A4)
    if (_kdcDebugMode && _missingGlyphsLogged < _MISSING_GLYPH_LOG_LIMIT) {
      Serial.print(F("KDC: getFontCharWidth: missing glyph 0x"));
      Serial.println(code, HEX);
      _missingGlyphsLogged++;
      if (_missingGlyphsLogged == _MISSING_GLYPH_LOG_LIMIT)
        Serial.println(F("KDC: (further missing-glyph warnings suppressed)"));
    }
    return 0;
  }
  // Locate the glyph blob, then skip to and read the delta (advance) field.
  uint32_t byteOffset = _fetchbits_unsigned(font->index, idxPos * font->bits_index,
                                            font->bits_index);
  const uint8_t *d = font->data + byteOffset;
  uint32_t bo = 3 + font->bits_width + font->bits_height +
                font->bits_xoffset + font->bits_yoffset;   // skip encoding+w+h+xoff+yoff
  return (int16_t)_fetchbits_unsigned(d, bo, font->bits_delta);
}

int16_t getFontStringWidth(const ILI9341_t3_font_t *font, const char *str) {
  int16_t total = 0;
  while (*str) {
    total += getFontCharWidth(font, *str++);
  }
  return total;
}


/***************************************************************************************
   DRAW VERTICAL BAR GRAPH
   Draws a vertical bar graph that updates only when the value changes, erasing the
   previous bar and drawing the new one. Values are in the range 0–scale (default 1000).
   Call from updateScreen*() only when the value has changed — prevVal is erased then
   newVal is drawn. The caller must update their prevVal copy after each call.
   - x0, y0      top-left corner of the bar area
   - w, h        width and height of the bar area
   - prevVal     previous value (used to erase old bar) — update caller's copy after call
   - newVal      new value to display
   - barColor    colour of the filled bar
   - drawBorder  if true, draws a white border around the full bar area
****************************************************************************************/
void drawVertBarGraph(KCM_TFT &tft,
                      uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                      int32_t prevVal, int32_t newVal,
                      uint16_t barColor, bool drawBorder,
                      int32_t scale) {
  if (drawBorder) tft.drawRect(x0, y0, w, h, TFT_WHITE);
  uint16_t inner_x = x0 + 1;
  uint16_t inner_y = y0 + 1;
  uint16_t inner_w = w - 2;
  uint16_t inner_h = h - 2;

  // Clamp values
  if (newVal  < 0)     newVal  = 0;
  if (newVal  > scale) newVal  = scale;
  if (prevVal < 0)     prevVal = 0;
  if (prevVal > scale) prevVal = scale;

  uint16_t newBarH  = (uint16_t)((int32_t)inner_h * newVal  / scale);
  uint16_t prevBarH = (uint16_t)((int32_t)inner_h * prevVal / scale);

  if (newBarH == prevBarH) return;

  // Bar fills from the bottom
  uint16_t newTop  = inner_y + (inner_h - newBarH);
  uint16_t prevTop = inner_y + (inner_h - prevBarH);

  if (newBarH > prevBarH) {
    // Bar grew — fill the new segment
    tft.fillRect(inner_x, newTop, inner_w, prevTop - newTop, barColor);
  } else {
    // Bar shrunk — erase the removed segment
    tft.fillRect(inner_x, prevTop, inner_w, newTop - prevTop, TFT_BLACK);
  }
}


/***************************************************************************************
   DRAW ARC DISPLAY
   Draws a sweeping arc indicator centred at (cx, cy) with the arc extending upward.
   The arc spans from minVal to maxVal; the needle is drawn at curVal.
   Previous needle is erased before drawing the new one.
   - cx, cy      centre point of the arc (typically below the display area)
   - radius      radius of the arc in pixels
   - needleW     half-width of the needle in pixels
   - minVal, maxVal  value range mapped to arcMinDeg..arcMaxDeg (±90° from straight up)
   - prevVal, curVal values to erase/draw
   - color       needle and arc colour

   IMPORTANT (#A8): No internal change detection. Caller must gate on value
   change (see header comment for details).
****************************************************************************************/
static void _drawArcNeedle(KCM_TFT &tft, int16_t cx, int16_t cy,
                            uint16_t radius, uint16_t needleW,
                            float minVal, float maxVal, float val,
                            uint16_t color) {
  // Defensive: zero-range arc would divide by zero. Caller error, but
  // guard so we don't produce NaN coordinates. (#A9)
  if (maxVal == minVal) {
    return;
  }
  // Map value to angle: 0° = straight up (12 o'clock), +90° = right, -90° = left.
  // Clamp val to [minVal, maxVal] so out-of-range telemetry doesn't rotate
  // the needle past the arc track. (#A9)
  float frac = (val - minVal) / (maxVal - minVal);
  if (frac < 0.0f) frac = 0.0f;
  if (frac > 1.0f) frac = 1.0f;
  float angleDeg = -90.0f + frac * 180.0f;            // -90° left … +90° right
  float angleRad = angleDeg * 3.14159f / 180.0f;
  int16_t tipX = cx + (int16_t)(radius * sinf(angleRad));
  int16_t tipY = cy - (int16_t)(radius * cosf(angleRad));
  // Draw a thick line by offsetting perpendicular
  float perpRad = angleRad + 1.5708f;  // +90°
  int16_t ox = (int16_t)(needleW * sinf(perpRad));
  int16_t oy = (int16_t)(needleW * cosf(perpRad));
  tft.drawLine(cx + ox, cy + oy, tipX + ox, tipY + oy, color);
  tft.drawLine(cx,      cy,      tipX,      tipY,      color);
  tft.drawLine(cx - ox, cy - oy, tipX - ox, tipY - oy, color);
}

void drawArcDisplay(KCM_TFT &tft,
                    int16_t cx, int16_t cy,
                    uint16_t radius, uint16_t needleW,
                    float minVal, float maxVal,
                    float prevVal, float curVal,
                    uint16_t color) {
  // Draw arc track
  for (int16_t deg = -90; deg <= 90; deg += 2) {
    float rad = deg * 3.14159f / 180.0f;
    int16_t px = cx + (int16_t)(radius * sinf(rad));
    int16_t py = cy - (int16_t)(radius * cosf(rad));
    tft.fillCircle(px, py, 1, TFT_DARK_GREY);
  }
  // Erase previous needle, draw new one
  _drawArcNeedle(tft, cx, cy, radius, needleW, minVal, maxVal, prevVal, TFT_BLACK);
  _drawArcNeedle(tft, cx, cy, radius, needleW, minVal, maxVal, curVal,  color);
  // Centre dot
  tft.fillCircle(cx, cy, needleW + 1, color);
}


/***************************************************************************************
   DRAW BUTTON
   Draws a filled rectangle with centered, word-wrapped text in on or off state.
   Colors, border, and text are taken from the ButtonLabel struct.
   (#A10) Lines wider than the button width are skipped rather than overflowing
   into adjacent UI. A diagnostic is logged on Serial when debug mode is on.
   To avoid skipped lines, use shorter labels or larger buttons.
****************************************************************************************/

void drawButton(KCM_TFT &tft, int16_t x, int16_t y, int16_t w, int16_t h,
                const ButtonLabel &label, const ILI9341_t3_font_t *font, bool isOn) {
  const int16_t PADDING = 4;
  uint16_t fgColor     = isOn ? label.fontColorOn       : label.fontColorOff;
  uint16_t bgColor     = isOn ? label.backgroundColorOn : label.backgroundColorOff;
  uint16_t borderColor = isOn ? label.borderColorOn     : label.borderColorOff;

  tft.fillRect(x, y, w, h, bgColor);
  if (borderColor != NO_BORDER) {
    tft.drawRect(x, y, w, h, borderColor);
  }
  // (rev 2) The RA8876 GFX library does not leave a clipping window after
  // fillRect/drawRect the way the RA8875 library did, so no reset is needed here.
  tft.setFont(*font);

  int16_t charH  = (int16_t)font->cap_height;
  int16_t availW = w - (PADDING * 2);

  // --- Word splitting ---
  // MAX_WORDS / MAX_WORDCH: labels exceeding these limits are silently truncated. (#67)
  // Increase if button labels longer than 31 chars or 31 words are ever needed.
  const uint8_t MAX_WORDS = 32, MAX_WORDCH = 32;
  char words[MAX_WORDS][MAX_WORDCH];
  uint8_t wordCount = 0;
  memset(words, 0, sizeof(words));
  const char *src = label.text;
  uint8_t wLen = 0;
  while (*src) {
    char c = *src++;
    if (c == ' ' || c == '\n') {
      if (wLen > 0 && wordCount < MAX_WORDS) {
        words[wordCount][wLen] = '\0';
        wordCount++;
        wLen = 0;
      }
    } else {
      // Guard wordCount too: once MAX_WORDS words are captured, further characters
      // would write into words[MAX_WORDS][...] — one row past the array.
      if (wordCount < MAX_WORDS && wLen < MAX_WORDCH - 1) words[wordCount][wLen++] = c;
    }
  }
  if (wLen > 0 && wordCount < MAX_WORDS) {
    words[wordCount][wLen] = '\0';
    wordCount++;
  }

  // --- Word wrapping using proportional widths ---
  const uint8_t MAX_LINES = 8, MAX_LINECH = 64;
  char lines[MAX_LINES][MAX_LINECH];
  uint8_t lineCount = 0;
  memset(lines, 0, sizeof(lines));
  for (uint8_t wi = 0; wi < wordCount && lineCount < MAX_LINES; wi++) {
    int16_t wordPixW    = getFontStringWidth(font, words[wi]);
    int16_t curLinePixW = getFontStringWidth(font, lines[lineCount]);
    int16_t spaceW      = (curLinePixW > 0) ? getFontCharWidth(font, ' ') : 0;
    int16_t needed      = curLinePixW + spaceW + wordPixW;
    size_t  wordLen     = strlen(words[wi]);
    uint8_t curLen      = strlen(lines[lineCount]);
    // Fit on the current line only if it fits by pixel width AND stays within the
    // MAX_LINECH byte buffer (pixel width alone doesn't bound the char count).
    if (needed <= availW && (size_t)curLen + 1 + wordLen < MAX_LINECH) {
      if (curLen > 0) lines[lineCount][curLen++] = ' ';
      memcpy(&lines[lineCount][curLen], words[wi], wordLen);
      lines[lineCount][curLen + wordLen] = '\0';
    } else {
      lineCount++;
      if (lineCount < MAX_LINES) {
        // Truncate an over-long single word to the line buffer rather than overrun.
        size_t n = (wordLen < MAX_LINECH - 1) ? wordLen : (MAX_LINECH - 1);
        memcpy(lines[lineCount], words[wi], n);
        lines[lineCount][n] = '\0';
      }
    }
  }
  if (wordCount > 0) lineCount++;

  // --- Vertical centering ---
  const int16_t LINE_SPACING = charH + 1;
  int16_t totalTextH = (lineCount * LINE_SPACING) - 1;
  int16_t startY     = y + (h - totalTextH) / 2;

  // --- Draw each line centered horizontally ---
  tft.setTextColor(fgColor, bgColor);
  for (uint8_t i = 0; i < lineCount && i < MAX_LINES; i++) {
    if (strlen(lines[i]) == 0) continue;
    int16_t linePixW = getFontStringWidth(font, lines[i]);
    // (#A10) If a line is wider than the button (single word too long, or
    // wrapping was forced beyond the column width), skip it rather than
    // overflowing into adjacent UI. Log a warning so the developer can
    // shorten the label or enlarge the button.
    if (linePixW > availW) {
      if (_kdcDebugMode) {
        Serial.print(F("KDC: drawButton: line too wide ("));
        Serial.print(linePixW);
        Serial.print(F("px > "));
        Serial.print(availW);
        Serial.print(F("px), skipping: '"));
        Serial.print(lines[i]);
        Serial.println(F("'"));
      }
      continue;
    }
    int16_t drawX    = x + (w - linePixW) / 2;
    int16_t drawY    = startY + (i * LINE_SPACING);
    if (drawX < x + PADDING) drawX = x + PADDING;
    // Clear the line cell with bgColor before printing to avoid artefacts from
    // inter-line gaps and glyph height mismatches. Clamp the clear so it never
    // covers the button's border rows (top y / bottom y+h-1): when text nearly
    // fills the button (large font / multi-line wrap) an unclamped clear would
    // erase a strip of the border, leaving visible breaks. The horizontal
    // inset (x+1, w-2) already protects the left/right border the same way.
    int16_t clrY = drawY;
    int16_t clrH = charH;
    if (clrY < y + 1)          { clrH -= (y + 1 - clrY); clrY = y + 1; }
    if (clrY + clrH > y + h - 1) clrH = (y + h - 1) - clrY;
    if (clrH > 0) tft.fillRect(x + 1, clrY, w - 2, clrH, bgColor);
    tft.setCursor(drawX, drawY);
    tft.print(lines[i]);
  }
}




/***************************************************************************************
   DRAW LABELLED Y-AXIS
   Draws a vertical percentage axis with major ticks (every 10%) and minor ticks
   (every 5%). Major ticks get right-justified percentage labels. 0% is at the bottom,
   100% at the top. The axis line is drawn at x0 + axisW - 1.
   - x0, y0      top-left of the axis strip (labels + ticks fit within axisW px)
   - axisW       total width reserved for the axis strip
   - barTop      y coordinate of the 100% level
   - barBottom   y coordinate of the 0% level
   - font        font for percentage labels (Roboto_Black_12 recommended)
   - axisColor   colour for the axis line, ticks, and labels
   - backColor   background colour (used for text background)
****************************************************************************************/
void drawLabelledAxis(KCM_TFT &tft,
                      uint16_t x0, uint16_t axisW,
                      uint16_t barTop, uint16_t barBottom,
                      const ILI9341_t3_font_t *font,
                      uint16_t axisColor, uint16_t backColor) {
  const uint16_t AXIS_X      = x0 + axisW - 1;
  const uint16_t MAJOR_LEN   = 6;
  const uint16_t MINOR_LEN   = 3;
  const uint16_t LBL_MARGIN  = 2;
  const uint16_t barH        = barBottom - barTop;

  tft.drawLine(AXIS_X, barTop, AXIS_X, barBottom, axisColor);
  tft.setFont(*font);
  tft.setTextColor(axisColor, backColor);

  for (uint8_t pct = 0; pct <= 100; pct += 5) {
    uint16_t y = barBottom - (uint16_t)((uint32_t)barH * pct / 100);
    bool     major   = (pct % 10 == 0);
    uint16_t tickLen = major ? MAJOR_LEN : MINOR_LEN;

    tft.drawLine(AXIS_X - tickLen, y, AXIS_X, y, axisColor);

    if (major) {
      char lbl[7];
      snprintf(lbl, sizeof(lbl), "%d%%", pct);
      int16_t lw = getFontStringWidth(font, lbl);
      int16_t lh = (int16_t)font->cap_height;
      int16_t lx = (int16_t)AXIS_X - tickLen - LBL_MARGIN - lw;
      int16_t ly = (int16_t)y - lh / 2;
      if (lx >= 0 && ly >= 0) {
        tft.setCursor(lx, ly);
        tft.print(lbl);
      }
    }
  }
}


/***************************************************************************************
   DRAW VERTICAL TEXT
   Draws a string one character per line, vertically centred within a rectangle.
   Each character is horizontally centred within the strip width.
   Used for rotated-style section labels where the RA8876 has no native text rotation.
   - x0, y0      top-left of the strip rectangle
   - w, h        width and height of the strip (text is centred within both)
   - font        font to use for each character
   - text        null-terminated string to draw vertically
   - color       foreground color
   - backColor   background color (strip is filled before drawing)
****************************************************************************************/
void drawVerticalText(KCM_TFT &tft,
                      uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                      const ILI9341_t3_font_t *font,
                      const char *text,
                      uint16_t color, uint16_t backColor) {
  tft.fillRect(x0, y0, w, h, backColor);

  uint8_t len = strlen(text);
  if (len == 0) return;

  // Stack the letters at a tight pitch (~0.8x cap height) so a short vertical
  // label reads as one compact word rather than evenly-spread characters, then
  // centre the whole stack in the box. cap_height overstates the visible glyph
  // height for round/flat caps, so a sub-1.0 pitch closes the apparent gaps
  // without the drawn glyphs actually overlapping. If the stack would overflow
  // (long string), fall back to even distribution so nothing clips.
  const int16_t glyphH = (int16_t)font->cap_height;
  int16_t pitch  = (int16_t)(glyphH * 0.9f + 0.5f);
  int16_t stackH = pitch * (int16_t)(len - 1) + glyphH;  // first-top .. last-bottom
  int16_t startY;
  if (stackH <= (int16_t)h) {
    startY = y0 + ((int16_t)h - stackH) / 2;
  } else {
    pitch  = (len > 1) ? (int16_t)(((int16_t)h - glyphH) / (int16_t)(len - 1)) : 0;
    if (pitch < 0) pitch = 0;
    startY = y0;
  }
  tft.setFont(*font);
  tft.setTextColor(color, backColor);
  for (uint8_t i = 0; i < len; i++) {
    char ch[2] = { text[i], '\0' };
    int16_t  cw = getFontStringWidth(font, ch);
    uint16_t cx = x0 + (w > (uint16_t)cw ? (w - (uint16_t)cw) / 2 : 0);
    int16_t  cy = startY + (int16_t)i * pitch;
    if (cy < (int16_t)y0) cy = y0;
    tft.setCursor(cx, cy);
    tft.print(ch);
  }
}


/***************************************************************************************
   TEXT PRIMITIVES
   Core rendering functions — all other display functions build on these.
****************************************************************************************/

// ── Fast 1-bit glyph blitter ──────────────────────────────────────────────────
// The KCM fonts (converted by tfont_to_ili9341.py) are 1bpp, version 1, non-RLE,
// top-aligned (glyph origin_y = cursor_y), xoffset/yoffset = 0, delta = glyph
// width, and byte-aligned in data[]. tft.print()'s built-in renderer pushes glyph
// pixels the slow way (~0.8 ms/glyph on this bus); instead we decode the bitmap
// ourselves and push each row with writeRect — the same fast windowed transfer the
// BMP loader uses (~11 Mpx/s). fg for set bits, bg for clear bits (opaque, matching
// setTextColor(fg,bg)). Layout is identical to getFontCharWidth so alignment done
// by the callers (which measure with getFontStringWidth) is unchanged.
//
// Any non-version-1 font falls back to the library renderer, so this is safe if an
// anti-aliased font is ever added.
#define KCM_TEXT_ROWBUF_MAX 1024
static uint16_t _kcmRowBuf[KCM_TEXT_ROWBUF_MAX];

static void kcmDrawString(KCM_TFT &tft, const ILI9341_t3_font_t *font,
                          int16_t x, int16_t y, const char *str,
                          uint16_t fg, uint16_t bg) {
  if (!str || !*str) return;
  if (font->version != 1) {                 // safety fallback for other formats
    tft.setFont(*font);
    tft.setTextColor(fg, bg);
    tft.setCursor(x, y);
    tft.print(str);
    return;
  }

  const uint8_t bw = font->bits_width, bh = font->bits_height;
  const uint8_t bxo = font->bits_xoffset, byo = font->bits_yoffset, bd = font->bits_delta;
  const uint8_t bidx = font->bits_index;
  const uint8_t fh = font->cap_height;      // all glyphs are full height

  // Pre-decode each glyph once: data ptr, width, height, bitmap bit-offset, and
  // its left pen position relative to x (advance = delta).
  struct GlyphRef { const uint8_t *d; uint16_t w, h; uint32_t bmp; int16_t px; };
  static GlyphRef g[64];
  int n = 0; int16_t pen = 0;
  for (const char *s = str; *s && n < 64; ++s) {
    uint8_t code = (uint8_t)*s;
    uint32_t ip;
    if (code >= font->index1_first && code <= font->index1_last) {
      ip = (uint32_t)(code - font->index1_first);
    } else if (font->index2_last >= font->index2_first &&
               code >= font->index2_first && code <= font->index2_last) {
      ip = (uint32_t)(font->index1_last - font->index1_first + 1) + (code - font->index2_first);
    } else {
      continue;                             // missing glyph → no width (matches width fn)
    }
    uint32_t byteOff = _fetchbits_unsigned(font->index, ip * bidx, bidx);
    const uint8_t *d = font->data + byteOff;
    uint32_t bo = 3;                        // skip encoding field
    uint16_t w = (uint16_t)_fetchbits_unsigned(d, bo, bw); bo += bw;
    uint16_t h = (uint16_t)_fetchbits_unsigned(d, bo, bh); bo += bh;
    bo += bxo + byo;                        // xoffset/yoffset (0 for these fonts)
    uint16_t delta = (uint16_t)_fetchbits_unsigned(d, bo, bd); bo += bd;
    g[n].d = d; g[n].w = w; g[n].h = h; g[n].bmp = bo; g[n].px = pen;
    pen += (int16_t)delta;
    n++;
  }
  if (n == 0 || pen <= 0) return;
  int16_t totalW = pen;
  if (totalW > KCM_TEXT_ROWBUF_MAX) totalW = KCM_TEXT_ROWBUF_MAX;

  // Rasterize row by row into the shared row buffer, then blit each row in one
  // windowed writeRect.
  for (uint8_t r = 0; r < fh; ++r) {
    for (int16_t i = 0; i < totalW; ++i) _kcmRowBuf[i] = bg;
    for (int k = 0; k < n; ++k) {
      const GlyphRef &gr = g[k];
      if (r >= gr.h || gr.w == 0) continue;
      uint32_t rowBit = gr.bmp + (uint32_t)r * (1u + gr.w) + 1u;   // skip per-row flag bit
      for (uint16_t c = 0; c < gr.w; ++c) {
        int16_t px = gr.px + (int16_t)c;
        if (px < 0 || px >= totalW) continue;
        uint32_t bi = rowBit + c;
        if ((gr.d[bi >> 3] >> (7 - (bi & 7))) & 1u) _kcmRowBuf[px] = fg;
      }
    }
    tft.writeRect(x, y + r, (uint16_t)totalW, 1, _kcmRowBuf);
  }
}

void textLeft(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
              const String &value, uint16_t foreColor, uint16_t backColor) {
  int16_t textH = font->cap_height;
  int16_t drawX = x0 + TEXT_BORDER;
  int16_t drawY = y0 + (h - textH) / 2;
  kcmDrawString(tft, font, drawX, drawY, value.c_str(), foreColor, backColor);
}

void textRight(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               const String &value, uint16_t foreColor, uint16_t backColor) {
  int16_t textW = getFontStringWidth(font, value.c_str());
  int16_t textH = font->cap_height;
  int16_t drawX = x0 + w - textW - TEXT_BORDER;
  int16_t drawY = y0 + (h - textH) / 2;
  kcmDrawString(tft, font, drawX, drawY, value.c_str(), foreColor, backColor);
}

void textCenter(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                const String &value, uint16_t foreColor, uint16_t backColor) {
  int16_t textW = getFontStringWidth(font, value.c_str());
  int16_t textH = font->cap_height;
  int16_t drawX = x0 + (w - textW) / 2;
  int16_t drawY = y0 + (h - textH) / 2;
  kcmDrawString(tft, font, drawX, drawY, value.c_str(), foreColor, backColor);
}

void eraseCenteredValue(KCM_TFT &tft, const ILI9341_t3_font_t *font,
                        int16_t x0, int16_t y0, int16_t w, int16_t h,
                        const char *oldStr, uint16_t bg) {
  int16_t tw   = getFontStringWidth(font, oldStr);
  int16_t capH = (int16_t)font->cap_height;
  int16_t bx   = x0 + (w - tw) / 2;
  int16_t by   = y0 + (h - capH) / 2;
  tft.fillRect(bx - 1, by, tw + 2, capH, bg);
}

void drawDiamondMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t half, uint16_t color) {
  tft.fillTriangle(cx - half, cy, cx + half, cy, cx, cy - half, color);  // top half
  tft.fillTriangle(cx - half, cy, cx + half, cy, cx, cy + half, color);  // bottom half
}

// Straight line of stroke width `w`, drawn as a filled quad (two triangles) about the
// ideal line with round end-caps. The quad is gap-free at every angle (unlike parallel
// offset draws, which collapse or gap at diagonals), and the caps close the joints where
// segments meet — so multi-segment shapes (arcs, prongs, X's) have no seams.
void drawThickLine(KCM_TFT &tft, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   int16_t w, uint16_t color, bool caps) {
  if (w <= 1) { tft.drawLine(x0, y0, x1, y1, color); return; }
  float dx = (float)(x1 - x0), dy = (float)(y1 - y0);
  float len = sqrtf(dx * dx + dy * dy);
  float hw  = w * 0.5f;
  int16_t cap = (int16_t)(hw + 0.5f);
  if (len < 0.5f) { tft.fillCircle(x0, y0, cap, color); return; }
  float px = -dy / len * hw, py = dx / len * hw;        // half-width perpendicular
  int16_t ax = (int16_t)lroundf(x0 + px), ay = (int16_t)lroundf(y0 + py);
  int16_t bx = (int16_t)lroundf(x0 - px), by = (int16_t)lroundf(y0 - py);
  int16_t ex = (int16_t)lroundf(x1 - px), ey = (int16_t)lroundf(y1 - py);
  int16_t fx = (int16_t)lroundf(x1 + px), fy = (int16_t)lroundf(y1 + py);
  tft.fillTriangle(ax, ay, bx, by, ex, ey, color);
  tft.fillTriangle(ax, ay, ex, ey, fx, fy, color);
  // Round end-caps close joints between connected segments (arcs, triangle corners); on a
  // free-ended spoke they read as a small blob, so callers can suppress them with caps=false.
  if (caps && cap >= 1) { tft.fillCircle(x0, y0, cap, color); tft.fillCircle(x1, y1, cap, color); }
}


/***************************************************************************************
   FILL ARC
   Annular sector between two angles, scan-converted row by row. A pixel belongs to
   the sector when its distance from the centre is in [rIn, rOut) and its bearing is
   inside the sweep; bearings are tested with cross products against the start and
   end direction vectors rather than atan2 per pixel. For a sweep up to 180 degrees
   the pixel must be clockwise of the start AND anticlockwise of the end; past 180
   the sector is everything but its complement, so EITHER test suffices. Consecutive
   qualifying pixels on a row go out as one horizontal run.
****************************************************************************************/
void fillArc(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t rIn, int16_t rOut,
             float a0Deg, float a1Deg, uint16_t color) {
  if (rOut <= 0 || rIn >= rOut) return;
  if (rIn < 0) rIn = 0;
  float sweep = a1Deg - a0Deg;
  if (sweep <= 0.0f) return;
  bool  full  = sweep >= 360.0f;
  bool  wide  = sweep > 180.0f;
  float sx = cosf(a0Deg * DEG_TO_RAD), sy = sinf(a0Deg * DEG_TO_RAD);
  float ex = cosf(a1Deg * DEG_TO_RAD), ey = sinf(a1Deg * DEG_TO_RAD);
  int32_t rIn2 = (int32_t)rIn * rIn, rOut2 = (int32_t)rOut * rOut;

  for (int16_t dy = -rOut; dy <= rOut; dy++) {
    int32_t dy2 = (int32_t)dy * dy;
    if (dy2 >= rOut2) continue;
    int16_t xo = (int16_t)sqrtf((float)(rOut2 - dy2));      // outer half-width this row
    int16_t runStart = 0;
    bool    inRun = false;
    for (int16_t dx = -xo; dx <= xo + 1; dx++) {
      bool on = false;
      if (dx <= xo) {
        int32_t d2 = (int32_t)dx * dx + dy2;
        if (d2 >= rIn2 && d2 < rOut2) {
          if (full) on = true;
          else {
            bool afterStart = (sx * dy - sy * dx) >= 0.0f;   // clockwise of the start ray
            bool beforeEnd  = (ex * dy - ey * dx) <= 0.0f;   // anticlockwise of the end ray
            on = wide ? (afterStart || beforeEnd) : (afterStart && beforeEnd);
          }
        }
      }
      if (on && !inRun) { runStart = dx; inRun = true; }
      else if (!on && inRun) {
        tft.drawFastHLine(cx + runStart, cy + dy, dx - runStart, color);
        inRun = false;
      }
    }
  }
}

// Sub-element sizing shared by the KSP markers: stroke width, centre-dot radius and
// spoke overshoot all scale from the ring/prong radius `r` so the marker stays
// proportional (and legibly thick) whether it is a tiny legend key or a full-size
// navball marker.
static inline int16_t _mkStroke(int16_t r) { return (r >= 16) ? 3 : 2; }
static inline int16_t _mkDot(int16_t r)    { return (r / 4 > 2) ? r / 4 : 2; }
static inline int16_t _mkSpoke(int16_t r)  { return ((2 * r) / 3 > 4) ? (2 * r) / 3 : 4; }
static const float _MK_D2R = 0.01745329252f;   // degrees -> radians

// KSP prograde marker: a ring with a filled centre dot and three spokes pointing up,
// right and left (the vertical-top + two-horizontal-sides shape KSP uses). Used for
// the velocity/prograde marker (green) and the maneuver-node marker (blue). `r` is the
// ring radius; the spokes extend to r + _mkSpoke(r).
void drawProgradeMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  int16_t w = _mkStroke(r), ext = _mkSpoke(r);
  for (int16_t i = 0; i < w; i++) tft.drawCircle(cx, cy, r - i, color);
  tft.fillCircle(cx, cy, _mkDot(r), color);            // centre dot
  static const int16_t spoke[3] = { -90, 0, 180 };     // screen degrees: up, right, left
  for (uint8_t i = 0; i < 3; i++) {
    float a = (float)spoke[i] * 0.01745329252f;        // deg -> rad
    drawThickLine(tft, cx + (int16_t)(r         * cosf(a)), cy + (int16_t)(r         * sinf(a)),
                       cx + (int16_t)((r + ext) * cosf(a)), cy + (int16_t)((r + ext) * sinf(a)), w, color, false);
  }
}

// KSP target marker: a ring drawn as four ~66-degree arc segments with gaps at top,
// bottom, left and right — as if a "+" were cut through the circle — plus a centre dot.
// Used for the target / docking-port marker (magenta).
void drawTargetMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  int16_t w = _mkStroke(r);
  tft.fillCircle(cx, cy, _mkDot(r), color);    // centre dot (matches the other markers)
  const float GAP  = 15.0f;                    // half-gap at each "+" arm, degrees
  const float STEP = 6.0f * 0.01745329252f;    // arc plot step, radians
  for (uint8_t q = 0; q < 4; q++) {
    float a0 = ((float)(q * 90) + GAP)         * 0.01745329252f;
    float a1 = ((float)(q * 90) + 90.0f - GAP) * 0.01745329252f;
    // Draw the arc as connected thick segments (round-capped) — a solid w-px band with
    // no radial or joint gaps, even at the diagonal / bottom portions of the ring.
    int16_t px = cx + (int16_t)(r * cosf(a0)), py = cy + (int16_t)(r * sinf(a0));
    for (float a = a0 + STEP; a < a1; a += STEP) {
      int16_t x = cx + (int16_t)(r * cosf(a)), y = cy + (int16_t)(r * sinf(a));
      drawThickLine(tft, px, py, x, y, w, color); px = x; py = y;
    }
    int16_t x = cx + (int16_t)(r * cosf(a1)), y = cy + (int16_t)(r * sinf(a1));
    drawThickLine(tft, px, py, x, y, w, color);
  }
}

// KSP maneuver-node marker: a centre dot with three prongs pointing up, lower-left
// and lower-right, each ending in a short perpendicular crossbar. No ring. Used for
// the maneuver marker (blue). `r` is the prong length from centre to crossbar.
void drawManeuverMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  int16_t w = _mkStroke(r);
  int16_t cap = (r / 3 > 4) ? r / 3 : 4;                // half-length of the end crossbar
  int16_t ri  = r / 2;                                  // prong start (gap from the centre dot)
  tft.fillCircle(cx, cy, _mkDot(r), color);             // centre dot
  static const int16_t prong[3] = { -90, 30, 150 };     // screen degrees: up, lower-right, lower-left
  for (uint8_t i = 0; i < 3; i++) {
    float a  = (float)prong[i] * _MK_D2R;
    int16_t ix = cx + (int16_t)(ri * cosf(a)), iy = cy + (int16_t)(ri * sinf(a));
    int16_t tx = cx + (int16_t)(r  * cosf(a)), ty = cy + (int16_t)(r  * sinf(a));
    drawThickLine(tft, ix, iy, tx, ty, w, color, false);   // prong (gap between dot and prong)
    float pa = a + 1.57079633f;                            // perpendicular to the prong
    drawThickLine(tft, tx - (int16_t)(cap * cosf(pa)), ty - (int16_t)(cap * sinf(pa)),
                       tx + (int16_t)(cap * cosf(pa)), ty + (int16_t)(cap * sinf(pa)), w, color, false);
  }
}

// KSP retrograde marker: a ring with an X through the centre and three spokes at up,
// lower-right and lower-left (120 deg apart), same length as the prograde spokes. No
// centre dot (the X fills the centre). Used for the retrograde marker (green).
void drawRetrogradeMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  int16_t w = _mkStroke(r), ext = _mkSpoke(r);
  for (int16_t i = 0; i < w; i++) tft.drawCircle(cx, cy, r - i, color);
  int16_t d = (r * 7) / 10;                             // X half-extent inside the ring
  drawThickLine(tft, cx - d, cy - d, cx + d, cy + d, w, color, false);
  drawThickLine(tft, cx - d, cy + d, cx + d, cy - d, w, color, false);
  static const int16_t spoke[3] = { -90, 30, 150 };     // up, lower-right, lower-left
  for (uint8_t i = 0; i < 3; i++) {
    float a = (float)spoke[i] * _MK_D2R;
    drawThickLine(tft, cx + (int16_t)(r         * cosf(a)), cy + (int16_t)(r         * sinf(a)),
                       cx + (int16_t)((r + ext) * cosf(a)), cy + (int16_t)((r + ext) * sinf(a)), w, color, false);
  }
}

// KSP normal marker: a hollow upward-pointing triangle with a centre dot (magenta).
void drawNormalMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  int16_t w = _mkStroke(r);
  int16_t ax = cx,                        ay = cy - (int16_t)(r * 1.20f);   // apex (top)
  int16_t bx = cx - (int16_t)(r * 1.10f), by = cy + (int16_t)(r * 0.72f);   // base-left
  int16_t ex = cx + (int16_t)(r * 1.10f), ey = by;                          // base-right
  drawThickLine(tft, ax, ay, bx, by, w, color);
  drawThickLine(tft, bx, by, ex, ey, w, color);
  drawThickLine(tft, ex, ey, ax, ay, w, color);
  tft.fillCircle(cx, (ay + by + ey) / 3, _mkDot(r), color);                 // centroid dot
}

// KSP anti-normal marker: a hollow downward-pointing triangle with a centre dot and a
// spoke projecting outward from the midpoint of each of the three faces (magenta).
void drawAntiNormalMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  int16_t w = _mkStroke(r);
  int16_t ax = cx,                        ay = cy + (int16_t)(r * 1.20f);   // apex (bottom)
  int16_t bx = cx - (int16_t)(r * 1.10f), by = cy - (int16_t)(r * 0.72f);   // top-left
  int16_t ex = cx + (int16_t)(r * 1.10f), ey = by;                          // top-right
  drawThickLine(tft, ax, ay, bx, by, w, color);
  drawThickLine(tft, bx, by, ex, ey, w, color);
  drawThickLine(tft, ex, ey, ax, ay, w, color);
  tft.fillCircle(cx, (ay + by + ey) / 3, _mkDot(r), color);                 // centroid dot
  int16_t L  = (r * 3) / 5;                                                 // face-spoke length
  float   a2 = 150.0f * _MK_D2R, a3 = 30.0f * _MK_D2R;                      // outward normals
  drawThickLine(tft, cx, by, cx, by - L, w, color, false);                 // top face -> up
  int16_t m2x = (ax + bx) / 2, m2y = (ay + by) / 2;                         // lower-left mid
  int16_t m3x = (ax + ex) / 2, m3y = (ay + ey) / 2;                         // lower-right mid
  drawThickLine(tft, m2x, m2y, m2x + (int16_t)(L * cosf(a2)), m2y + (int16_t)(L * sinf(a2)), w, color, false);
  drawThickLine(tft, m3x, m3y, m3x + (int16_t)(L * cosf(a3)), m3y + (int16_t)(L * sinf(a3)), w, color, false);
}

// KSP radial-in marker: a ring with four short spokes at the diagonals pointing inward
// toward the centre; no centre dot (cyan).
void drawRadialInMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  int16_t w = _mkStroke(r);
  for (int16_t i = 0; i < w; i++) tft.drawCircle(cx, cy, r - i, color);
  int16_t ri = r / 2, ro = r - w;                       // inner (toward centre), outer (at ring)
  static const int16_t diag[4] = { 45, 135, 225, 315 };
  for (uint8_t i = 0; i < 4; i++) {
    float a = (float)diag[i] * _MK_D2R;
    drawThickLine(tft, cx + (int16_t)(ri * cosf(a)), cy + (int16_t)(ri * sinf(a)),
                       cx + (int16_t)(ro * cosf(a)), cy + (int16_t)(ro * sinf(a)), w, color, false);
  }
}

// KSP radial-out marker: a ring with a centre dot and four short spokes at the diagonals
// pointing outward from the ring (cyan).
void drawRadialOutMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  int16_t w = _mkStroke(r);
  for (int16_t i = 0; i < w; i++) tft.drawCircle(cx, cy, r - i, color);
  tft.fillCircle(cx, cy, _mkDot(r), color);
  int16_t ext = r / 3;
  static const int16_t diag[4] = { 45, 135, 225, 315 };
  for (uint8_t i = 0; i < 4; i++) {
    float a = (float)diag[i] * _MK_D2R;
    drawThickLine(tft, cx + (int16_t)(r         * cosf(a)), cy + (int16_t)(r         * sinf(a)),
                       cx + (int16_t)((r + ext) * cosf(a)), cy + (int16_t)((r + ext) * sinf(a)), w, color, false);
  }
}

// KSP anti-target marker: a centre dot with three spokes (upper-left, upper-right and
// straight down), each separated from the dot by a gap; no ring (magenta).
void drawAntiTargetMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  int16_t w = _mkStroke(r);
  tft.fillCircle(cx, cy, _mkDot(r), color);
  int16_t ri = r / 2;                                   // spoke start (gap from the dot)
  static const int16_t spoke[3] = { -30, 90, 210 };     // upper-right, down, upper-left
  for (uint8_t i = 0; i < 3; i++) {
    float a = (float)spoke[i] * _MK_D2R;
    drawThickLine(tft, cx + (int16_t)(ri * cosf(a)), cy + (int16_t)(ri * sinf(a)),
                       cx + (int16_t)(r  * cosf(a)), cy + (int16_t)(r  * sinf(a)), w, color, false);
  }
}

// KSP level indicator (nose/waterline reticle): two horizontal wings with a downward
// centre dip, and a dot on the wing line marking the exact nose direction. `r` sets the
// overall size. Drawn in yellow-gold.
void drawLevelIndicator(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  // (cx,cy) is the nose dot itself — the wings run horizontally through it and the dip
  // hangs below, so the marker points at exactly (cx,cy). Lines are capless (no blobs on
  // the free wing tips); the interior joints get a small fill so they stay closed.
  int16_t w   = (r / 9 > 3) ? r / 9 : 3;   // thicker stroke than the ADI markers
  int16_t wtx = (r * 3) / 2;           // wing-tip half-span
  int16_t inx = (r * 11) / 20;         // inner x (start of the dip)
  int16_t dip = (r * 3) / 5;           // dip depth below the wing line
  int16_t jc  = w / 2;                 // joint fill radius
  drawThickLine(tft, cx - wtx, cy,       cx - inx, cy,       w, color, false);  // left wing
  drawThickLine(tft, cx - inx, cy,       cx,       cy + dip, w, color, false);  // left dip
  drawThickLine(tft, cx,       cy + dip, cx + inx, cy,       w, color, false);  // right dip
  drawThickLine(tft, cx + inx, cy,       cx + wtx, cy,       w, color, false);  // right wing
  tft.fillCircle(cx - inx, cy,       jc, color);                         // left  wing/dip joint
  tft.fillCircle(cx + inx, cy,       jc, color);                         // right wing/dip joint
  tft.fillCircle(cx,       cy + dip, jc, color);                         // dip apex joint
  tft.fillCircle(cx, cy, (r / 7 > 2) ? r / 7 : 2, color);               // small nose dot on the wing line
}

void reticleDrawBase(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r,
                     int16_t gap, int16_t tickLen) {
  int16_t r4 = r / 4, r2 = r / 2, r34 = (r * 3) / 4, arm = r - 1;

  // Black disc + inner good-zone fill
  tft.fillCircle(cx, cy, r,  TFT_BLACK);
  tft.fillCircle(cx, cy, r4, TFT_OFF_BLACK);

  // Concentric rings (r/4 good-zone green, then grey). The outer grey boundary at
  // radius r is drawn once by the bezel-ring block below.
  tft.drawCircle(cx, cy, r4,  TFT_DARK_GREEN);
  tft.drawCircle(cx, cy, r2,  TFT_DARK_GREY);
  tft.drawCircle(cx, cy, r34, TFT_DARK_GREY);

  // Cardinal lines with a centre gap for the crosshair
  tft.drawLine(cx,       cy - arm, cx,       cy - gap, TFT_DARK_GREY);
  tft.drawLine(cx,       cy + gap, cx,       cy + arm, TFT_DARK_GREY);
  tft.drawLine(cx - arm, cy,       cx - gap, cy,       TFT_DARK_GREY);
  tft.drawLine(cx + gap, cy,       cx + arm, cy,       TFT_DARK_GREY);

  // Nose crosshair symbol at centre
  tft.drawLine(cx - gap + 2, cy,           cx - 4,       cy,           TFT_GREY);
  tft.drawLine(cx + 4,       cy,           cx + gap - 2, cy,           TFT_GREY);
  tft.drawLine(cx,           cy - gap + 2, cx,           cy - 4,       TFT_GREY);
  tft.drawLine(cx,           cy + 4,       cx,           cy + gap - 2, TFT_GREY);
  tft.fillCircle(cx, cy, 2, TFT_GREY);

  // Minor ticks at 30° increments on the outer ring (skip cardinals)
  for (int16_t deg = 30; deg < 360; deg += 30) {
    if (deg % 90 == 0) continue;
    float rad = (deg - 90) * DEG_TO_RAD;
    int16_t ox = cx + (int16_t)(r * cosf(rad));
    int16_t oy = cy + (int16_t)(r * sinf(rad));
    int16_t ix = cx + (int16_t)((r - tickLen) * cosf(rad));
    int16_t iy = cy + (int16_t)((r - tickLen) * sinf(rad));
    tft.drawLine(ox, oy, ix, iy, TFT_DARK_GREY);
  }

  // Bezel ring
  tft.drawCircle(cx, cy, r,     TFT_GREY);
  tft.drawCircle(cx, cy, r + 1, TFT_DARK_GREY);
}

float reticleRepair(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r,
                    int16_t gap, int16_t bx, int16_t by, uint8_t bh) {
  int16_t boxX0 = bx, boxX1 = bx + 2 * bh, boxY0 = by, boxY1 = by + 2 * bh;

  float px = (float)constrain((int)cx, (int)boxX0, (int)boxX1);
  float py = (float)constrain((int)cy, (int)boxY0, (int)boxY1);
  float distToCentre = sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy));
  float ddx = fmaxf(fabsf((float)(boxX0 - cx)), fabsf((float)(boxX1 - cx)));
  float ddy = fmaxf(fabsf((float)(boxY0 - cy)), fabsf((float)(boxY1 - cy)));
  float distFar = sqrtf(ddx * ddx + ddy * ddy);

  const uint16_t rings[] = { (uint16_t)(r / 4), (uint16_t)(r / 2),
                             (uint16_t)((r * 3) / 4), (uint16_t)r };
  const uint16_t rcols[] = { TFT_DARK_GREEN, TFT_DARK_GREY, TFT_DARK_GREY, TFT_GREY };
  for (uint8_t i = 0; i < 4; i++) {
    float rr = (float)rings[i];
    if (distToCentre <= rr + 1.5f && distFar >= rr - 1.5f)
      tft.drawCircle(cx, cy, rings[i], rcols[i]);
  }

  // Good-zone refill FIRST — when the erase box is near centre the inner disc is
  // repainted OFF_BLACK. This must happen before the cardinal lines and crosshair
  // are restored, otherwise the refill wipes the target lines back out (they pass
  // straight through the inner circle) and they vanish while a marker crosses it.
  if (distToCentre <= (r / 4) - 1) {
    tft.fillCircle(cx, cy, (r / 4) - 1, TFT_OFF_BLACK);
    tft.drawCircle(cx, cy, (r / 4),     TFT_DARK_GREEN);
    // The refill just wiped the inner cross. Restore it UNCONDITIONALLY here: the
    // erase box can sit in a diagonal quadrant that straddles neither axis, in
    // which case the conditional cardinal/crosshair redraws below are both skipped
    // and the cross would stay erased. Redraw the inner cardinal stubs (gap..r/4),
    // the grey crosshair, and the centre dot.
    int16_t r4i = r / 4;
    tft.drawLine(cx - r4i, cy, cx - gap, cy, TFT_DARK_GREY);
    tft.drawLine(cx + gap, cy, cx + r4i, cy, TFT_DARK_GREY);
    tft.drawLine(cx, cy - r4i, cx, cy - gap, TFT_DARK_GREY);
    tft.drawLine(cx, cy + gap, cx, cy + r4i, TFT_DARK_GREY);
    tft.drawLine(cx - gap + 2, cy, cx - 4,       cy, TFT_GREY);
    tft.drawLine(cx + 4,       cy, cx + gap - 2, cy, TFT_GREY);
    tft.drawLine(cx, cy - gap + 2, cx, cy - 4,       TFT_GREY);
    tft.drawLine(cx, cy + 4,       cx, cy + gap - 2, TFT_GREY);
    tft.fillCircle(cx, cy, 2, TFT_GREY);
  }

  int16_t carm = r - 1;
  if (boxY0 <= cy && cy <= boxY1) {
    if (boxX0 < (int16_t)(cx - gap))
      tft.drawLine(cx - carm, cy, cx - gap, cy, TFT_DARK_GREY);
    if (boxX1 > (int16_t)(cx + gap))
      tft.drawLine(cx + gap, cy, cx + carm, cy, TFT_DARK_GREY);
    if (boxX0 <= cx || boxX1 >= cx) {
      tft.drawLine(cx - gap + 2, cy, cx - 4,       cy, TFT_GREY);
      tft.drawLine(cx + 4,       cy, cx + gap - 2, cy, TFT_GREY);
    }
  }
  if (boxX0 <= cx && cx <= boxX1) {
    if (boxY0 < (int16_t)(cy - gap))
      tft.drawLine(cx, cy - carm, cx, cy - gap, TFT_DARK_GREY);
    if (boxY1 > (int16_t)(cy + gap))
      tft.drawLine(cx, cy + gap, cx, cy + carm, TFT_DARK_GREY);
    if (boxY0 <= cy || boxY1 >= cy) {
      tft.drawLine(cx, cy - gap + 2, cx, cy - 4,       TFT_GREY);
      tft.drawLine(cx, cy + 4,       cx, cy + gap - 2, TFT_GREY);
    }
  }
  if (boxX0 <= cx && cx <= boxX1 && boxY0 <= cy && cy <= boxY1)
    tft.fillCircle(cx, cy, 2, TFT_GREY);

  // Report the box's nearest distance to centre so callers can decide whether the
  // good-zone refill just painted over the innermost ring label (radius ≈ r/4).
  return distToCentre;
}


/***************************************************************************************
   BORESIGHT PROJECTION
   See the header for the contract and for why this replaced the old flat
   heading/pitch-difference scheme. Roll handedness is defined here and nowhere else.
****************************************************************************************/
void kspDirUnit(float headingDeg, float pitchDeg, float out[3]) {
  const float h = headingDeg * (float)DEG_TO_RAD, p = pitchDeg * (float)DEG_TO_RAD;
  const float cp = cosf(p);
  out[0] = cp * sinf(h);   // East
  out[1] = cp * cosf(h);   // North
  out[2] = sinf(p);        // Up
}

KspBodyAxes kspBodyAxes(float headingDeg, float pitchDeg, float rollDeg) {
  KspBodyAxes ax;
  kspDirUnit(headingDeg, pitchDeg, ax.fwd);

  // Unrolled basis: right lies on the horizon 90 deg clockwise of the nose heading,
  // up completes the right-handed set (right x fwd points at the local vertical when
  // the craft is level).
  float r0[3], u0[3];
  kspDirUnit(headingDeg + 90.0f, 0.0f, r0);
  u0[0] = r0[1]*ax.fwd[2] - r0[2]*ax.fwd[1];
  u0[1] = r0[2]*ax.fwd[0] - r0[0]*ax.fwd[2];
  u0[2] = r0[0]*ax.fwd[1] - r0[1]*ax.fwd[0];

  // Roll the basis about the nose. THE definition of roll handedness for the project:
  // a body-referenced display rotates world content by -roll, so the axes themselves
  // rotate by +roll. If markers spin the wrong way in KSP, negate rollDeg here.
  if (rollDeg != 0.0f) {
    const float a = rollDeg * (float)DEG_TO_RAD;
    const float c = cosf(a), sn = sinf(a);
    for (uint8_t i = 0; i < 3; i++) {
      ax.right[i] = r0[i]*c - u0[i]*sn;
      ax.up[i]    = r0[i]*sn + u0[i]*c;
    }
  } else {
    for (uint8_t i = 0; i < 3; i++) { ax.right[i] = r0[i]; ax.up[i] = u0[i]; }
  }
  return ax;
}

void kspBoresightAngles(const KspBodyAxes &ax, float dirHeadingDeg, float dirPitchDeg,
                        float &degRight, float &degUp) {
  float m[3];
  kspDirUnit(dirHeadingDeg, dirPitchDeg, m);

  const float cf = m[0]*ax.fwd[0]   + m[1]*ax.fwd[1]   + m[2]*ax.fwd[2];
  const float cr = m[0]*ax.right[0] + m[1]*ax.right[1] + m[2]*ax.right[2];
  const float cu = m[0]*ax.up[0]    + m[1]*ax.up[1]    + m[2]*ax.up[2];

  // Azimuthal equidistant: radius = the true angle off the boresight, direction = the
  // clock angle of the perpendicular component. atan2(perp, along) rather than acos so
  // it stays accurate for small offsets, and it spans the full 0..180.
  const float perp = sqrtf(cr*cr + cu*cu);
  if (perp < 1e-6f) {
    // Exactly on the boresight axis -- the clock angle is undefined. Ahead is the
    // centre; dead astern is pinned straight up so the caller's clamp has a direction.
    degRight = 0.0f;
    degUp    = (cf >= 0.0f) ? 0.0f : 180.0f;
    return;
  }
  const float theta = atan2f(perp, cf) * (float)RAD_TO_DEG;
  degRight = theta * (cr / perp);
  degUp    = theta * (cu / perp);
}

void drawRoundRectOutline(KCM_TFT &tft, int16_t x, int16_t y,
                                 int16_t w, int16_t h, int16_t r, uint16_t col) {
  const int16_t x1 = x + w - 1, y1 = y + h - 1;
  tft.drawLine(x + r, y,  x1 - r, y,  col);
  tft.drawLine(x + r, y1, x1 - r, y1, col);
  tft.drawLine(x,  y + r, x,  y1 - r, col);
  tft.drawLine(x1, y + r, x1, y1 - r, col);
  int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, px = 0, py = r;
  while (px < py) {
    if (f >= 0) { py--; ddF_y += 2; f += ddF_y; }
    px++; ddF_x += 2; f += ddF_x;
    tft.drawPixel(x1 - r + px, y1 - r + py, col);
    tft.drawPixel(x1 - r + py, y1 - r + px, col);
    tft.drawPixel(x  + r - px, y1 - r + py, col);
    tft.drawPixel(x  + r - py, y1 - r + px, col);
    tft.drawPixel(x1 - r + px, y  + r - py, col);
    tft.drawPixel(x1 - r + py, y  + r - px, col);
    tft.drawPixel(x  + r - px, y  + r - py, col);
    tft.drawPixel(x  + r - py, y  + r - px, col);
  }
}

float eadiHdgDelta(float a, float b) {
  float d = a - b;
  while (d >  180.0f) d -= 360.0f;
  while (d < -180.0f) d += 360.0f;
  return d;
}


/***************************************************************************************
   BODY ANGULAR RATES  -- see the header for why this differentiates the rotation
   rather than the Euler angles, and for the bench test that is still outstanding.
****************************************************************************************/
void kcmRateReset(KcmRateTracker &t) {
  t.primed = false;
  t.prevMs = 0;
  t.roll = t.pitch = t.yaw = 0.0f;
}

bool kcmRateUpdate(KcmRateTracker &t, float headingDeg, float pitchDeg, float rollDeg,
                   uint32_t nowMs, float tauMs, uint32_t staleMs) {
  const KspBodyAxes ax = kspBodyAxes(headingDeg, pitchDeg, rollDeg);

  // Right-handed order: (fwd, up, right). KspBodyAxes stores (fwd, right, up), which
  // is left-handed -- see the header.
  float cur[3][3];
  for (uint8_t i = 0; i < 3; i++) {
    cur[0][i] = ax.fwd[i];
    cur[1][i] = ax.up[i];
    cur[2][i] = ax.right[i];
  }

  if (!t.primed) {
    memcpy(t.prev, cur, sizeof(cur));
    t.prevMs = nowMs;
    t.primed = true;
    return false;
  }

  // Unchanged basis means no new telemetry, not "not rotating" -- hold, then decay.
  bool moved = false;
  for (uint8_t i = 0; i < 3 && !moved; i++)
    for (uint8_t j = 0; j < 3; j++)
      if (fabsf(cur[i][j] - t.prev[i][j]) > 1e-6f) { moved = true; break; }

  if (!moved) {
    if ((uint32_t)(nowMs - t.prevMs) < staleMs) return false;
    const bool wasNonZero = (fabsf(t.roll) + fabsf(t.pitch) + fabsf(t.yaw)) > 0.05f;
    t.roll = t.pitch = t.yaw = 0.0f;
    return wasNonZero;
  }

  const float dt = (float)(uint32_t)(nowMs - t.prevMs) / 1000.0f;

  // dR[i][j] = e_i(previous) . e_j(current) -- computed BEFORE prev is overwritten.
  float dR[3][3];
  for (uint8_t i = 0; i < 3; i++)
    for (uint8_t j = 0; j < 3; j++)
      dR[i][j] = t.prev[i][0]*cur[j][0] + t.prev[i][1]*cur[j][1] + t.prev[i][2]*cur[j][2];

  memcpy(t.prev, cur, sizeof(cur));
  t.prevMs = nowMs;

  // A long gap means the link stalled or the vessel changed: re-prime rather than
  // reporting the whole accumulated rotation as one enormous rate.
  if (dt <= 0.0f || dt > 2.0f) return false;

  // Skew-symmetric part = rotation vector, about (fwd, up, right).
  const float wFwd   = (dR[2][1] - dR[1][2]) * 0.5f / dt;
  const float wUp    = (dR[0][2] - dR[2][0]) * 0.5f / dt;
  const float wRight = (dR[1][0] - dR[0][1]) * 0.5f / dt;

  // Aviation convention. About +fwd the roof swings to starboard, which is a right
  // bank, so roll takes the sign as-is. About +right the nose rises, so pitch does
  // too. About +up the nose swings to PORT, so yaw is negated to make nose-right
  // positive.
  const float RAD2DEG = 57.29577951308232f;
  const float rRoll  =  wFwd   * RAD2DEG;
  const float rPitch =  wRight * RAD2DEG;
  const float rYaw   = -wUp    * RAD2DEG;

  const float a = (tauMs > 0.0f) ? (dt * 1000.0f) / (tauMs + dt * 1000.0f) : 1.0f;
  const float pr = t.roll, pp = t.pitch, py = t.yaw;
  t.roll  += a * (rRoll  - t.roll);
  t.pitch += a * (rPitch - t.pitch);
  t.yaw   += a * (rYaw   - t.yaw);

  return (fabsf(t.roll - pr) + fabsf(t.pitch - pp) + fabsf(t.yaw - py)) > 0.05f;
}


/***************************************************************************************
   SHARED RETICLE MARKER LAYER  (MNVR / DOCK / TGT)
****************************************************************************************/
void reticleProject(const ReticleGeom &g, float degRight, float degUp,
                    int16_t &sx, int16_t &sy) {
  sx = g.cx + (int16_t)( degRight * g.scale);
  sy = g.cy + (int16_t)(-degUp    * g.scale);
}

bool reticleClampDot(const ReticleGeom &g, int16_t &sx, int16_t &sy) {
  float dx = sx - g.cx, dy = sy - g.cy;
  float dist = sqrtf(dx*dx + dy*dy);
  float maxR = (float)(g.r - g.clampMargin);
  if (dist > maxR && dist > 0.5f) {
    float scale = maxR / dist;
    sx = g.cx + (int16_t)(dx * scale);
    sy = g.cy + (int16_t)(dy * scale);
    return true;                 // pinned: direction still honest, distance is not
  }
  return false;
}

void reticleRepairDotChrome(KCM_TFT &tft, const ReticleGeom &g,
                            int16_t bx, int16_t by, uint8_t bh) {
  int16_t boxX0 = bx, boxX1 = bx + 2*bh, boxY0 = by, boxY1 = by + 2*bh;

  float d = reticleRepair(tft, g.cx, g.cy, g.r, 18, bx, by, bh);

  const uint16_t lblR[4] = { (uint16_t)(g.r / 4), (uint16_t)(g.r / 2),
                             (uint16_t)((g.r * 3) / 4), (uint16_t)g.r };
  bool fontSet = false;
  for (uint8_t i = 0; i < 4; i++) {
    int16_t lx = g.cx + 3, ly = g.cy - lblR[i] + 3;
    bool boxHit      = (boxX1 >= lx && boxX0 <= lx + 26 && boxY1 >= ly && boxY0 <= ly + 20);
    bool goodZoneHit = (i == 0 && d <= (float)(g.r / 4));
    if (boxHit || goodZoneHit) {
      if (!fontSet) { tft.setFont(*g.lblFont); tft.setTextColor(TFT_LIGHT_GREY); fontSet = true; }
      tft.setCursor(lx, ly);
      tft.print(g.lbl[i]);
    }
  }
}

void reticleEraseDot(KCM_TFT &tft, const ReticleGeom &g,
                     int16_t curX, int16_t curY,
                     int16_t &prevX, int16_t &prevY, bool visible,
                     bool restyled) {
  const uint8_t EH = g.eraseHalf;
  if (!visible) {
    if (prevX != 9999) {
      tft.fillRect(prevX - EH, prevY - EH, EH*2+1, EH*2+1, TFT_BLACK);
      reticleRepairDotChrome(tft, g, prevX - EH, prevY - EH, EH);
      prevX = prevY = 9999;
    }
    return;
  }
  if (prevX == 9999 || restyled || abs(curX - prevX) > 1 || abs(curY - prevY) > 1) {
    if (prevX != 9999) {
      tft.fillRect(prevX - EH, prevY - EH, EH*2+1, EH*2+1, TFT_BLACK);
      reticleRepairDotChrome(tft, g, prevX - EH, prevY - EH, EH);
    }
    prevX = curX; prevY = curY;
  }
}

void reticleUpdateDots(KCM_TFT &tft, const ReticleGeom &g, ReticleDotCache &c,
                       const ReticleAngles &a) {
  // Primary dot: where is the target/port relative to your nose?
  int16_t pSX, pSY;
  reticleProject(g, a.priRight, a.priUp, pSX, pSY);
  const bool priPinned = reticleClampDot(g, pSX, pSY);

  // Vel dot: where is your relative-velocity vector relative to your nose?
  int16_t vSX, vSY;
  reticleProject(g, a.velRight, a.velUp, vSX, vSY);
  const bool velPinned = reticleClampDot(g, vSX, vSY);

  // Opposite (antipodal) marker positions + in-view test.
  const float maxR = (float)(g.r - g.clampMargin);
  int16_t aSX, aSY, rSX, rSY;
  reticleProject(g, a.antiRight,  a.antiUp,  aSX, aSY);
  reticleProject(g, a.retroRight, a.retroUp, rSX, rSY);
  bool antiInView  = ((float)(aSX-g.cx)*(aSX-g.cx) + (float)(aSY-g.cy)*(aSY-g.cy)) <= maxR*maxR;
  bool retroInView = ((float)(rSX-g.cx)*(rSX-g.cx) + (float)(rSY-g.cy)*(rSY-g.cy)) <= maxR*maxR;

  // Erase phase (all markers) then draw phase, so a moving marker's erase never clips a
  // neighbour. Draw order bottom-to-top: opposites, primary, velocity on top.
  reticleEraseDot(tft, g, aSX, aSY, c.antiX,    c.antiY,    antiInView);
  reticleEraseDot(tft, g, rSX, rSY, c.retroX,   c.retroY,   retroInView);
  reticleEraseDot(tft, g, pSX, pSY, c.primaryX, c.primaryY, !antiInView,
                  priPinned != c.primaryPinned);
  reticleEraseDot(tft, g, vSX, vSY, c.velX,     c.velY,     !retroInView,
                  velPinned != c.velPinned);
  c.primaryPinned = priPinned;
  c.velPinned     = velPinned;

  if (c.antiX    != 9999) drawAntiTargetMarker(tft, c.antiX,    c.antiY,    g.dotRPrimary, TFT_VIOLET);
  if (c.retroX   != 9999) drawRetrogradeMarker(tft, c.retroX,   c.retroY,   g.dotRVel,     TFT_NEON_GREEN);
  // A pinned marker is drawn at half brightness: it is clamped at the boundary, so its
  // direction is honest but its distance understates the real angle and it must not read
  // as a live value. Nos.Off / Brg / Elv carry the true number.
  if (c.primaryX != 9999) drawTargetMarker(tft,   c.primaryX, c.primaryY, g.dotRPrimary,
                                           c.primaryPinned ? TFT_DIM_VIOLET   : TFT_VIOLET);
  if (c.velX     != 9999) drawProgradeMarker(tft, c.velX,     c.velY,     g.dotRVel,
                                           c.velPinned     ? TFT_DIM_NEON_GRN : TFT_NEON_GREEN);

  // Redraw crosshair inner segments — the vel circle can clip them near centre.
  {
    static const uint16_t gp = 18;   // matches reticleDrawBase gap
    tft.drawLine(g.cx - gp + 2, g.cy, g.cx - 4, g.cy, TFT_GREY);
    tft.drawLine(g.cx + 4,      g.cy, g.cx + gp - 2, g.cy, TFT_GREY);
    tft.drawLine(g.cx, g.cy - gp + 2, g.cx, g.cy - 4, TFT_GREY);
    tft.drawLine(g.cx, g.cy + 4,      g.cx, g.cy + gp - 2, TFT_GREY);
  }
  tft.fillCircle(g.cx, g.cy, 2, TFT_GREY);
}


/***************************************************************************************
   FORMATTING HELPERS - BASIC
   Convert numeric values to formatted Strings for use with print functions.
****************************************************************************************/

String formatInt(uint16_t value) {
  // Route through the thousands-separator helper so any field that grows into
  // the thousands is delimited (e.g. "1,234"). Values < 1000 render unchanged.
  return formatSepI64((int64_t)value);
}

String formatFloat(float value, uint8_t decimals) {
  return String(value, decimals);
}

String formatPerc(uint16_t value) {
  return String(value) + "%";
}

String formatUnits(uint16_t value, String units) {
  return String(value) + units;
}

String formatFloatUnits(float value, uint8_t decimals, String units) {
  return String(value, decimals) + units;
}


/***************************************************************************************
   FORMATTING HELPERS - ADVANCED (KSP telemetry)
   formatSep() is a dependency of formatAlt() — keep together.
   formatTime() uses Kerbin day = 6 hours.
****************************************************************************************/

// formatSep(): for values >= 1000 the decimal part is dropped and a thousands
// separator is inserted instead (e.g. 1234.5 -> "1,234"). This is intentional —
// at that scale the decimal is noise on a display. (#64)
//
// (#A5) The float core delegates to a 64-bit integer helper so it no longer
// overflows for values >= 2^31. For exact formatting of large integer values
// (where float precision would quantize the digits), call formatSepI64()
// directly with an int64_t.

// Core implementation — operates on a signed 64-bit integer.
//
// Filled BACKWARDS from the end of one buffer, a digit at a time. The previous version
// built the string front-to-back and so had to re-emit everything it had already
// produced on each group: `sprintf(tempBuf, ",%03d%s", current, buf); strcpy(buf,
// tempBuf)` is two passes over the whole accumulated string per three digits, which
// makes it quadratic in the digit count and costs two 64-byte buffers plus a printf
// invocation per group. This is the hottest formatting path on the panel — every
// formatAlt() on every visible distance readout lands here every frame — so it is worth
// the few lines. Writing right-to-left also puts the separators exactly where the
// modulus already is, with no need to zero-pad a leading group that must not be padded.
//
// Buffer sizing: the widest possible result is INT64_MIN, "-9,223,372,036,854,775,808"
// — 1 sign + 19 digits + 6 separators + NUL = 27 bytes. 28 leaves a byte spare.
// Formats from the end of the caller's buffer and returns a pointer to the first
// character. The buffer needs 28 bytes for the full int64 range.
static const char *_sepInt64Into(int64_t value, char *buf, size_t n) {
  char *p = buf + n - 1;
  *p = '\0';

  const bool neg = (value < 0);
  // Negate into an UNSIGNED accumulator. The old code did `value = -value` on the signed
  // type, which is undefined for INT64_MIN — a limitation it documented ("not reachable
  // from any realistic Kerbal value") rather than fixed. Unsigned negation is modular and
  // well defined, so the whole range now works and the caveat goes away.
  uint64_t v = neg ? (uint64_t)0 - (uint64_t)value : (uint64_t)value;

  uint8_t inGroup = 0;
  do {
    if (inGroup == 3) { *--p = ','; inGroup = 0; }
    *--p = (char)('0' + (uint8_t)(v % 10u));
    v /= 10u;
    inGroup++;
  } while (v != 0);

  if (neg) *--p = '-';
  return p;
}

static String _formatSepInt64(int64_t value) {
  char buf[28];
  return String(_sepInt64Into(value, buf, sizeof(buf)));
}

// Buffer forms of the formatters below. The String forms wrap these, so the two can
// never disagree; a screen that formats every row every frame uses the buffer form
// and compares against its cache before it constructs a String at all.
void formatSepBuf(float value, char *buf, size_t n) {
  const bool neg = (value < 0);
  if (neg) value = -value;
  if (value < 1000) {
    char tempStr[16];
    dtostrf(value, 2, 2, tempStr);
    snprintf(buf, n, "%s%s", neg ? "-" : "", tempStr);
    return;
  }
  // Delegate to the int64 core. Float precision caps usable integer range
  // at ~1.6e7; large values are formatted as the nearest representable float.
  char tmp[28];
  snprintf(buf, n, "%s%s", neg ? "-" : "", _sepInt64Into((int64_t)value, tmp, sizeof(tmp)));
}

String formatSep(float value) {
  char buf[40];
  formatSepBuf(value, buf, sizeof(buf));
  return String(buf);
}

String formatSepI64(int64_t value) {
  return _formatSepInt64(value);
}

void formatTimeBuf(float timeVal, char *buf, size_t n) {
  const bool neg = (timeVal < 0);
  if (neg) timeVal = -timeVal;
  const uint16_t kerbinDay = 6;  // Kerbin day = 6 hours
  // #65 use int64_t to avoid 32-bit overflow beyond ~24.8 Kerbin days
  int64_t timeMillis = (int64_t)(fabsf(timeVal) * 1000.0f);
  int64_t calcSecs   = timeMillis / 1000;
  int64_t calcMins   = calcSecs   / 60;
  int64_t calcHrs    = calcMins   / 60;
  int64_t calcDays   = calcHrs    / kerbinDay;
  timeMillis %= 1000;
  calcSecs   %= 60;
  calcMins   %= 60;
  calcHrs    %= kerbinDay;
  char timeStr[40];
  if (calcDays != 0) {
    // Past 1 day, switch to a compact "Dd HH:MM" form so the string stays narrow
    // enough for tight value cells (the verbose "D d: HH h: MM m: SS s" overflows).
    sprintf(timeStr, "%lldd %02lld:%02lld",
            (long long)calcDays, (long long)calcHrs, (long long)calcMins);
  } else if (calcHrs != 0) {
    sprintf(timeStr, "%lld h: %02lld m: %02lld s",
            (long long)calcHrs, (long long)calcMins, (long long)calcSecs);
  } else if (calcMins != 0) {
    sprintf(timeStr, "%lld m: %02lld s",
            (long long)calcMins, (long long)calcSecs);
  } else {
    // Pure-seconds: strip decimal, add space before unit (#5C incorporates fmtTime improvements)
    sprintf(timeStr, "%lld s", (long long)calcSecs);
  }
  snprintf(buf, n, "%s%s", neg ? "-" : "", timeStr);
}

String formatTime(float timeVal) {
  char buf[48];
  formatTimeBuf(timeVal, buf, sizeof(buf));
  return String(buf);
}

// Compact time — for value cells too tight for formatTime()'s hours form
// ("H h: MM m: SS s" ≈ 241px at 36pt). Below 1 hour it is IDENTICAL to
// formatTime() (keeps seconds — the actionable range), so common readouts look
// unchanged. At/above 1 hour it drops to a 2-unit form ("Hh MMm" / "Dd HHh")
// that stays narrow (≤ ~175px). Use it wherever a time value can legitimately
// grow into the hours/days range and must still fit its cell.
void formatTimeCompactBuf(float timeVal, char *buf, size_t n) {
  int64_t secs = (int64_t)fabsf(timeVal);
  if (secs < 3600) { formatTimeBuf(timeVal, buf, n); return; }   // < 1 h: unchanged

  const bool neg = (timeVal < 0);
  const int64_t kerbinDay = 6;                    // Kerbin day = 6 hours
  int64_t hrsT = secs / 3600;
  int64_t days = hrsT / kerbinDay;
  int64_t hr   = hrsT % kerbinDay;
  int64_t mn   = (secs % 3600) / 60;
  char t[28];   // room for the full int64 day count ("%lldd") without truncation
  if      (days >= 10000) snprintf(t, sizeof(t), "%lldd", (long long)days);
  else if (days > 0)      snprintf(t, sizeof(t), "%lldd %02lldh", (long long)days, (long long)hr);
  else                    snprintf(t, sizeof(t), "%lldh %02lldm", (long long)hrsT, (long long)mn);
  snprintf(buf, n, "%s%s", neg ? "-" : "", t);
}

String formatTimeCompact(float timeVal) {
  char buf[40];
  formatTimeCompactBuf(timeVal, buf, sizeof(buf));
  return String(buf);
}

void formatAltBuf(float value, char *buf, size_t n) {
  const bool neg = (value < 0);
  if (neg) value = -value;
  float scaled; const char *unit;
  if      (value < 1000000)         { scaled = value;                          unit = " m";  }
  else if (value < 1000000000)      { scaled = (float)(value / 1000.0);        unit = " km"; }
  else if (value < 1000000000000.0) { scaled = (float)(value / 1000000.0);     unit = " Mm"; }
  else                              { scaled = (float)(value / 1000000000.0);  unit = " Gm"; }
  char num[40];
  formatSepBuf(scaled, num, sizeof(num));
  snprintf(buf, n, "%s%s%s", neg ? "-" : "", num, unit);
}

String formatAlt(float value) {
  char buf[48];
  formatAltBuf(value, buf, sizeof(buf));
  return String(buf);
}

/***************************************************************************************
   TIME WARP STRING
   Returns a human-readable warp rate string from a KerbalSimpit timeWarp index.
   Normal warp:   indices 0-7 → 1x, 5x, 10x, 50x, 100x, 1000x, 10000x, 100000x
   Physics warp:  indices 1-3 → PHYS-2x, PHYS-3x, PHYS-4x
   Index 0 is always 1x regardless of physTW. Returns "N/A" for unknown indices.
****************************************************************************************/
String twString(uint8_t twIndex, bool physTW) {
  if (physTW && twIndex > 0) {
    // Physical time warp — index 1-3 only
    switch (twIndex) {
      case 1: return "PHYS-2x";
      case 2: return "PHYS-3x";
      case 3: return "PHYS-4x";
      default: return "N/A";
    }
  } else {
    // Normal time warp — index 0-7
    switch (twIndex) {
      case 0: return "1x";
      case 1: return "5x";
      case 2: return "10x";
      case 3: return "50x";
      case 4: return "100x";
      case 5: return "1,000x";
      case 6: return "10,000x";
      case 7: return "100,000x";
      default: return "N/A";
    }
  }
}


/***************************************************************************************
   THRESHOLD COLOR SELECTOR
****************************************************************************************/

void thresholdColor(uint16_t value,
                    uint16_t lowVal,  uint16_t lowColor,  uint16_t lowBack,
                    uint16_t midVal,  uint16_t midColor,  uint16_t midBack,
                                      uint16_t highColor, uint16_t highBack,
                    uint16_t &foreColor, uint16_t &backColor) {
  if (value < lowVal) {
    foreColor = lowColor;
    backColor = lowBack;
  } else if (value < midVal) {
    foreColor = midColor;
    backColor = midBack;
  } else {
    foreColor = highColor;
    backColor = highBack;
  }
}

// Float overload (#42): clamps value to uint16_t range then delegates.
void thresholdColor(float value,
                    float lowVal,  uint16_t lowColor,  uint16_t lowBack,
                    float midVal,  uint16_t midColor,  uint16_t midBack,
                                   uint16_t highColor, uint16_t highBack,
                    uint16_t &foreColor, uint16_t &backColor) {
  thresholdColor((uint16_t)constrain((int32_t)value, 0, 65535),
                 (uint16_t)constrain((int32_t)lowVal, 0, 65535), lowColor, lowBack,
                 (uint16_t)constrain((int32_t)midVal, 0, 65535), midColor, midBack,
                 highColor, highBack, foreColor, backColor);
}


/***************************************************************************************
   DISPLAY BLOCK FUNCTIONS
****************************************************************************************/

void printDisp(KCM_TFT &tft, const ILI9341_t3_font_t *font,
               uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               const String &param, const String &value,
               uint16_t paramColor, uint16_t valColor, uint16_t valBack,
               uint16_t backColor, uint16_t borderColor,
               PrintState &ps) {
  int16_t paramW  = getFontStringWidth(font, param.c_str());
  int16_t regionX = x0 + TEXT_BORDER + paramW + 1;
  int16_t regionW = w - TEXT_BORDER - paramW - 2;

  // (#A11) If the param label is wider than the display block can accommodate
  // alongside any value text, regionW would go negative — when cast to
  // uint16_t for fillRect, that produces a giant fill that paints over
  // adjacent UI. Draw the label and border so the block is visible, then
  // skip value rendering. Diagnostic logged when debug mode is on.
  if (regionW < 1) {
    if (_kdcDebugMode) {
      Serial.print(F("KDC: printDisp: param '"));
      Serial.print(param);
      Serial.print(F("' too wide for block (paramW="));
      Serial.print(paramW);
      Serial.print(F("px, w="));
      Serial.print(w);
      Serial.println(F("px) — value omitted"));
    }
    tft.fillRect(x0 + 1, y0 + 1, w - 2, h - 2, backColor);
    textLeft(tft, font, x0, y0, w, h, param, paramColor, backColor);
    if (borderColor != NO_BORDER) {
      tft.drawRect(x0, y0, w, h, borderColor);
    }
    return;
  }

  int16_t  newTextW = getFontStringWidth(font, value.c_str());
  int16_t  newTextX = x0 + w - newTextW - TEXT_BORDER;
  if (newTextX < regionX) newTextX = regionX;
  uint16_t newH = (uint16_t)font->cap_height;

  bool bgChanged     = (valBack != ps.prevBg)    && (ps.prevBg     != 0x0001);
  bool heightChanged = (newH    != ps.prevHeight) && (ps.prevHeight != 0);

  // Always redraw the label area and background on printDisp — caller only
  // invokes this when content has changed so the full clear is acceptable.
  tft.fillRect(x0 + 1, y0 + 1, w - 2, h - 2, backColor);
  textLeft(tft, font, x0, y0, w, h, param, paramColor, backColor);

  // Value: flicker-free render — draw first, clean trailing strip after
  if (bgChanged || heightChanged) {
    tft.fillRect(regionX, y0 + 1, regionW, h - 2, backColor);
  }
  textRight(tft, font, x0, y0, w, h, value, valColor, valBack);
  if (!bgChanged && !heightChanged && (uint16_t)newTextW < ps.prevWidth) {
    tft.fillRect(regionX, y0 + 1, newTextX - regionX, h - 2, backColor);
  }

  if (borderColor != NO_BORDER) {
    tft.drawRect(x0, y0, w, h, borderColor);
  }

  ps.prevWidth  = (uint16_t)newTextW;
  ps.prevBg     = valBack;
  ps.prevHeight = newH;
}

void printDisp(KCM_TFT &tft, const ILI9341_t3_font_t *font,
               uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               const String &param, const String &value,
               uint16_t paramColor, uint16_t valColor, uint16_t valBack,
               uint16_t backColor, uint16_t borderColor,
               DispCache &cache, PrintState &ps) {
  if (cache.valid         &&
      cache.x0          == x0           && cache.y0         == y0          &&
      cache.w           == w            && cache.h          == h           &&
      cache.param       == param        && cache.value      == value       &&
      cache.paramColor  == paramColor   && cache.valColor   == valColor    &&
      cache.valBack     == valBack      && cache.backColor  == backColor   &&
      cache.borderColor == borderColor) {
    return;
  }
  printDisp(tft, font, x0, y0, w, h, param, value,
            paramColor, valColor, valBack, backColor, borderColor, ps);
  cache.x0 = x0; cache.y0 = y0; cache.w = w; cache.h = h;
  cache.param = param; cache.value = value;
  cache.paramColor = paramColor; cache.valColor = valColor;
  cache.valBack = valBack; cache.backColor = backColor;
  cache.borderColor = borderColor;
  cache.valid = true;
}

void printValue(KCM_TFT &tft, const ILI9341_t3_font_t *font,
                uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                const String &param, const String &value,
                uint16_t valColor, uint16_t valBack,
                uint16_t backColor,
                PrintState &ps) {
  int16_t paramW  = getFontStringWidth(font, param.c_str());
  int16_t regionX = x0 + TEXT_BORDER + paramW + 1;
  int16_t regionW = w - TEXT_BORDER - paramW - 2;

  // (#A11) Same negative-regionW guard as printDisp. printValue does not
  // redraw the param label or border, so we just skip the value render.
  if (regionW < 1) {
    if (_kdcDebugMode) {
      Serial.print(F("KDC: printValue: param '"));
      Serial.print(param);
      Serial.print(F("' too wide for block (paramW="));
      Serial.print(paramW);
      Serial.print(F("px, w="));
      Serial.print(w);
      Serial.println(F("px) — skipping"));
    }
    return;
  }

  int16_t  newTextW = getFontStringWidth(font, value.c_str());
  int16_t  newTextX = x0 + w - newTextW - TEXT_BORDER;   // where textRight will draw
  uint16_t newH     = (uint16_t)font->cap_height;

  bool bgChanged     = (valBack != ps.prevBg)    && (ps.prevBg     != 0x0001);
  bool heightChanged = (newH    != ps.prevHeight) && (ps.prevHeight != 0);

  // Clear from where the previous value actually started, not from regionX.
  // regionX reserves the label plus 1 px on the left, while textRight right-aligns
  // to x0+w-TEXT_BORDER — the two ends account for TEXT_BORDER asymmetrically, so a
  // value measuring up to TEXT_BORDER-1 px inside regionW is still painted left of
  // regionX. Those pixels carry the value's own opaque background, so a clear that
  // began at regionX left them behind: a narrower value replacing a wider one (e.g.
  // MNVR "-100°" → "-10°" on a red alarm background) ghosted a coloured bar at the
  // left edge of the value field.
  //
  // Starting the clear at the previous text's left edge repairs exactly the pixels
  // the previous value painted and nothing more — reaching left of regionX cannot
  // harm the caller's label, because whatever is there was already overwritten by
  // that value's background when it was drawn.
  int16_t prevTextX = (ps.prevHeight != 0)
                        ? (int16_t)(x0 + w - (int16_t)ps.prevWidth - TEXT_BORDER)
                        : regionX;
  int16_t clearX = (prevTextX < regionX) ? prevTextX : regionX;
  if (clearX < (int16_t)x0 + 1) clearX = (int16_t)x0 + 1;   // never leave the cell

  if (bgChanged || heightChanged) {
    tft.fillRect(clearX, y0 + 1, regionX + regionW - clearX, h - 2, backColor);
  }

  textRight(tft, font, x0, y0, w, h, value, valColor, valBack);

  if (!bgChanged && !heightChanged && (uint16_t)newTextW < ps.prevWidth &&
      newTextX > clearX) {
    tft.fillRect(clearX, y0 + 1, newTextX - clearX, h - 2, backColor);
  }

  ps.prevWidth  = (uint16_t)newTextW;
  ps.prevBg     = valBack;
  ps.prevHeight = newH;
}

void printDispChrome(KCM_TFT &tft, const ILI9341_t3_font_t *font,
                     uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                     String label,
                     uint16_t labelColor, uint16_t backColor,
                     uint16_t borderColor) {
  tft.fillRect(x0 + 1, y0 + 1, w - 2, h - 2, backColor);
  textLeft(tft, font, x0, y0, w, h, label, labelColor, backColor);
  if (borderColor != NO_BORDER) {
    tft.drawRect(x0, y0, w, h, borderColor);
  }
}


void printName(KCM_TFT &tft, const ILI9341_t3_font_t *font,
               uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               const String &value, uint16_t color, uint16_t backColor,
               uint16_t borderColor, byte maxLength) {
  String val = value;   // local copy — const String& cannot be mutated
  // (#A12) maxLength is in BYTES, not Unicode code points. UTF-8 input may
  // be truncated mid-multibyte-sequence. See encoding caveat in header.
  if (val.length() > maxLength) {
    val = val.substring(0, maxLength);
  }
  tft.fillRect(x0 + 1, y0 + 1, w - 2, h - 2, backColor);
  textLeft(tft, font, x0, y0, w, h, val, color, backColor);
  if (borderColor != NO_BORDER) {
    tft.drawRect(x0, y0, w, h, borderColor);
  }
}

void printTitle(KCM_TFT &tft, const ILI9341_t3_font_t *font,
                uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                const String &value, uint16_t color, uint16_t backColor,
                uint16_t borderColor) {
  tft.fillRect(x0 + 1, y0 + 1, w - 2, h - 2, backColor);
  textCenter(tft, font, x0, y0, w, h, value, color, backColor);
  if (borderColor != NO_BORDER) {
    tft.drawRect(x0, y0, w, h, borderColor);
  }
}


/***************************************************************************************
   SD CARD SETUP
   Initialises the Teensy 4.1 on-board microSD socket (SDIO / BUILTIN_SDCARD).
   Must be called once from setup() before any drawBMP() calls. Stores the result
   in a static flag that drawBMP() checks on every call.
   Returns true if SD initialised successfully, false otherwise.
   (rev 2) The carrier board exposes no SPI/SD lines to the TFT module; assets load
   from the Teensy's built-in card slot. The slot has its own card-detect, so the
   separate SD_DETECT_PIN check from the rev-1 SPI wiring is gone.
****************************************************************************************/
static bool _sdReady = false;

bool setupSD() {
  if (!SD.begin(KCM_SD_CS)) {   // KCM_SD_CS == BUILTIN_SDCARD
    Serial.println(F("KDC: setupSD: SD.begin(BUILTIN_SDCARD) failed — check card"));
    _sdReady = false;
    return false;
  }
  _sdReady = true;
  if (_kdcDebugMode) Serial.println(F("KDC: setupSD: SD card ready"));
  return true;
}


/***************************************************************************************
   BMP DRAWING
   Reads a 24-bit uncompressed BMP from the SD card and draws it to the RA8876 display.
   Only 24-bit BI_RGB (uncompressed) BMPs are supported.
   setupSD() must have been called and returned true before calling this function.
   Both bottom-up BMPs (standard) and top-down BMPs (negative height) are supported.
   When malloc succeeds, all rows are read sequentially then drawn in one pass for
   maximum SD throughput. On malloc failure, rows are sought and read individually.
   Image dimensions are clamped to 1024×600 (KCM_SCREEN_W/H) before drawing.
****************************************************************************************/

// Internal helper — read a little-endian 16-bit value from an open file.
static uint16_t _bmpRead16(File &f, bool &ok) {
  int b0 = f.read();
  int b1 = f.read();
  if (b0 < 0 || b1 < 0) { ok = false; return 0; }
  return (uint16_t)b0 | ((uint16_t)b1 << 8);
}

// Internal helper — read a little-endian 32-bit value from an open file.
static int32_t _bmpRead32(File &f, bool &ok) {
  int b0 = f.read(), b1 = f.read(), b2 = f.read(), b3 = f.read();
  if (b0 < 0 || b1 < 0 || b2 < 0 || b3 < 0) { ok = false; return 0; }
  return (int32_t)((uint32_t)b0 | ((uint32_t)b1 << 8) |
                   ((uint32_t)b2 << 16) | ((uint32_t)b3 << 24));
}

// Internal helper — convert 24-bit BGR to RGB565
static uint16_t _bgr24ToRgb565(uint8_t b, uint8_t g, uint8_t r) {
  return ((uint16_t)(r & 0xF8) << 8) |
         ((uint16_t)(g & 0xFC) << 3) |
         ((uint16_t)(b & 0xF8) >> 3);
}

// Internal helper — convert a row of 24-bit BGR bytes into RGB565 values.
// src points to imgW * 3 bytes of BGR data; dst receives imgW RGB565 values.
static void _bmpConvertRow(const uint8_t *src, uint16_t *dst, int32_t imgW) {
  for (int32_t c = 0; c < imgW; c++) {
    dst[c] = _bgr24ToRgb565(src[c*3], src[c*3+1], src[c*3+2]);
  }
}

// Internal helper — log a BMPResult error with filename to Serial
static void _bmpLogError(const char *filename, BMPResult err) {
  Serial.print(F("KDC: drawBMP ["));
  Serial.print(filename);
  Serial.print(F("]: "));
  switch (err) {
    case BMP_ERR_NO_CARD:   Serial.println(F("no SD card detected"));          break;
    case BMP_ERR_SD_INIT:   Serial.println(F("SD not ready — call setupSD()")); break;
    case BMP_ERR_FILE:      Serial.println(F("file not found"));                break;
    case BMP_ERR_SIGNATURE: Serial.println(F("not a valid BMP file"));          break;
    case BMP_ERR_DIB:       Serial.println(F("unsupported DIB header"));        break;
    case BMP_ERR_COMPRESSED:Serial.println(F("compressed BMP not supported"));  break;
    case BMP_ERR_DIMENSIONS:Serial.println(F("invalid image dimensions"));      break;
    case BMP_ERR_READ:      Serial.println(F("unexpected end of file"));        break;
    case BMP_ERR_NOT_24BIT: Serial.println(F("only 24-bit BMPs are supported")); break;
    default:                Serial.println(F("unknown error"));                  break;
  }
}

// Internal helper: restore display state on mid-draw BMP error (#61)
static void _bmpAbort(File &f, const char *filename, BMPResult err, KCM_TFT &tft) {
  (void)tft;   // (rev 2) writeRect-based blit needs no window/cursor restore
  f.close();
  _bmpLogError(filename, err);
}

/***************************************************************************************
   STANDBY SPLASH (#14)
   Draws the shared KCMk1 standby BMP from SD card. Clears the screen to black,
   then draws the full-panel 1024x600 BMP at (0,0). Shared across all panels so the
   filename and clear sequence are defined in exactly one place.
   (rev 2) Regenerate /StandbySplash_1024x600.bmp on the SD card at the new size.
****************************************************************************************/
void drawStandbySplash(KCM_TFT &tft) {
  tft.fillScreen(TFT_BLACK);
  drawBMP(tft, "/StandbySplash_1024x600.bmp", 0, 0);
}

BMPResult drawBMP(KCM_TFT &tft, const char *filename, uint16_t x, uint16_t y) {
  // --- Defensive checks ---
  if (filename == nullptr) return BMP_ERR_FILE;
  if (!_sdReady) {
    _bmpLogError(filename, BMP_ERR_SD_INIT);
    return BMP_ERR_SD_INIT;
  }
  if (!SD.exists(filename)) {
    _bmpLogError(filename, BMP_ERR_FILE);
    return BMP_ERR_FILE;
  }

  File file = SD.open(filename);
  if (!file) {
    _bmpLogError(filename, BMP_ERR_FILE);
    return BMP_ERR_FILE;
  }

  bool ok = true;

  // --- BMP file header (14 bytes) ---
  // (#A13) Check ok before evaluating the signature, so a short/unreadable
  // file is reported as BMP_ERR_READ rather than BMP_ERR_SIGNATURE.
  uint16_t signature = _bmpRead16(file, ok);
  if (!ok) { file.close(); _bmpLogError(filename, BMP_ERR_READ); return BMP_ERR_READ; }
  if (signature != 0x4D42) {
    file.close(); _bmpLogError(filename, BMP_ERR_SIGNATURE); return BMP_ERR_SIGNATURE;
  }
  (void)_bmpRead32(file, ok);  // file size (unused)
  (void)_bmpRead32(file, ok);  // reserved (unused)
  uint32_t dataOffset = (uint32_t)_bmpRead32(file, ok);
  if (!ok) { file.close(); _bmpLogError(filename, BMP_ERR_READ); return BMP_ERR_READ; }

  // --- DIB header ---
  int32_t dibSize = _bmpRead32(file, ok);
  if (!ok) { file.close(); _bmpLogError(filename, BMP_ERR_READ); return BMP_ERR_READ; }
  if (dibSize < 40) { file.close(); _bmpLogError(filename, BMP_ERR_DIB); return BMP_ERR_DIB; }

  int32_t  imgW        = _bmpRead32(file, ok);
  int32_t  imgH        = _bmpRead32(file, ok);
  (void)_bmpRead16(file, ok);                  // colour planes (always 1)
  uint16_t bitDepth    = _bmpRead16(file, ok);
  int32_t  compression = _bmpRead32(file, ok);
  if (!ok) { file.close(); _bmpLogError(filename, BMP_ERR_READ); return BMP_ERR_READ; }

  if (compression != 0) {
    file.close(); _bmpLogError(filename, BMP_ERR_COMPRESSED); return BMP_ERR_COMPRESSED;
  }
  if (bitDepth != 24) {
    file.close(); _bmpLogError(filename, BMP_ERR_NOT_24BIT); return BMP_ERR_NOT_24BIT;
  }
  if (imgW <= 0) {
    file.close(); _bmpLogError(filename, BMP_ERR_DIMENSIONS); return BMP_ERR_DIMENSIONS;
  }

  bool topDown = (imgH < 0);
  if (topDown) imgH = -imgH;
  if (imgH == 0) {
    file.close(); _bmpLogError(filename, BMP_ERR_DIMENSIONS); return BMP_ERR_DIMENSIONS;
  }
  // Guard against unreasonably large images before buffer allocation.
  // (#A15) Bounds use KCM_SCREEN_W/H so the check matches the fixed buffer
  // capacity below — both must move together if a smaller display is targeted.
  if (imgW > KCM_SCREEN_W || imgH > KCM_SCREEN_H) {
    file.close(); _bmpLogError(filename, BMP_ERR_DIMENSIONS); return BMP_ERR_DIMENSIONS;
  }

  // Skip remaining DIB header bytes (we've consumed 36 bytes of it so far)
  for (int32_t i = 40; i < dibSize; i++) {
    if (file.read() < 0) {
      file.close(); _bmpLogError(filename, BMP_ERR_READ); return BMP_ERR_READ;
    }
  }

  // --- Row stride — 24-bit BMPs pad each row to a multiple of 4 bytes ---
  uint32_t rowBytes = ((uint32_t)imgW * 3 + 3) & ~3UL;

  // --- Seek once to the start of pixel data ---
  if (!file.seek(dataOffset)) {
    file.close(); _bmpLogError(filename, BMP_ERR_READ); return BMP_ERR_READ;
  }

  // --- Buffers ---
  // (#A15) Fixed-size at the panel worst case rather than VLAs sized from
  // imgW. Eliminates a non-standard C++ feature and makes stack usage
  // statically knowable. Sizes track KCM_SCREEN_W (1024 → ~3072 + 2048 ≈ 5 KB).
  // (#A16) rowBuf includes 3 extra bytes for worst-case BMP row padding
  // (rows are padded to 4-byte boundaries), so we can read full padded rows
  // when consuming sequentially.
  // rowBuf: raw BGR bytes from SD (3 bytes per pixel + up to 3 padding).
  // pixBuf: converted RGB565 values for drawPixels() burst write.
  uint8_t  rowBuf[KCM_SCREEN_W * 3 + 3];
  uint16_t pixBuf[KCM_SCREEN_W];

  // (rev 2) Each row is blitted with writeRect(x, y+row, imgW, 1, pixBuf), which
  // addresses an explicit rectangle — no global active-window/cursor setup needed.

  if (!topDown) {
    uint8_t *allRows = (uint8_t *)malloc((size_t)imgH * rowBytes);
    if (!allRows) {
      // Fallback: seek per row
      for (int32_t row = 0; row < imgH; row++) {
        uint32_t rowAddr = dataOffset + (uint32_t)(imgH - 1 - row) * rowBytes;
        if (!file.seek(rowAddr)) {
          _bmpAbort(file, filename, BMP_ERR_READ, tft); return BMP_ERR_READ;
        }
        if (file.read(rowBuf, imgW * 3) != (size_t)(imgW * 3)) {
          _bmpAbort(file, filename, BMP_ERR_READ, tft); return BMP_ERR_READ;
        }
        _bmpConvertRow(rowBuf, pixBuf, imgW);
        tft.writeRect(x, y + (uint16_t)row, (uint16_t)imgW, 1, pixBuf);
      }
    } else {
      // Read all rows sequentially
      for (int32_t row = 0; row < imgH; row++) {
        if (file.read(allRows + (size_t)row * rowBytes, rowBytes) != (size_t)rowBytes) {
          free(allRows);
          _bmpAbort(file, filename, BMP_ERR_READ, tft); return BMP_ERR_READ;
        }
      }
      // Draw rows in reverse (BMP bottom-up → screen top-down)
      for (int32_t row = 0; row < imgH; row++) {
        uint8_t *src = allRows + (size_t)(imgH - 1 - row) * rowBytes;
        _bmpConvertRow(src, pixBuf, imgW);
        tft.writeRect(x, y + (uint16_t)row, (uint16_t)imgW, 1, pixBuf);
      }
      free(allRows);
    }
  } else {
    // Top-down BMP: read sequentially. (#A16) Use rowBytes (padded) for the
    // read so padding bytes are consumed and the next row aligns correctly.
    // _bmpConvertRow() only inspects the first imgW*3 bytes — padding ignored.
    for (int32_t row = 0; row < imgH; row++) {
      if (file.read(rowBuf, rowBytes) != (size_t)rowBytes) {
        _bmpAbort(file, filename, BMP_ERR_READ, tft); return BMP_ERR_READ;
      }
      _bmpConvertRow(rowBuf, pixBuf, imgW);
      tft.writeRect(x, y + (uint16_t)row, (uint16_t)imgW, 1, pixBuf);
    }
  }

  file.close();
  if (_kdcDebugMode) { Serial.print(F("KDC: drawBMP OK: ")); Serial.println(filename); }
  return BMP_OK;
}

// =============================================================================
// Celestial body parameters
// =============================================================================
//
// The body table and getBodyParams() lookups moved to the shared, header-only single
// source of truth in Software/Common/body_params.h (included via KerbalDisplayCommon.h),
// so the display firmware and the master-controller ascent autopilot share one table.


// =============================================================================
/***************************************************************************************
   BOOT SCREEN RENDERING HELPERS (#15, #16, #17)
   Shared terminal-aesthetic primitives for KCMk1 panel boot sequences.
   All functions stay in RA8876 graphics mode (setFont/setCursor/print).
****************************************************************************************/
void bsPrint(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t x, uint16_t y,
             const char *text, uint16_t col) {
  tft.setFont(*font);
  tft.setTextColor(col, TFT_BLACK);
  tft.setCursor(x, y);
  tft.print(text);
}

uint16_t bsLine(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t col_x,
                uint16_t y, uint16_t rowH, const char *text, uint16_t col) {
  tft.setFont(*font);
  tft.setTextColor(col, TFT_BLACK);
  tft.setCursor(col_x, y);
  tft.print(text);
  return y + rowH;
}

uint16_t bsBig(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t col_x,
               uint16_t y, const char *text, uint16_t col) {
  tft.setFont(*font);
  tft.setTextColor(col, TFT_BLACK);
  tft.setCursor(col_x, y);
  tft.print(text);
  // Advance past the glyph cell + a small leading. Font-aware so a taller heading
  // font can't overlap the next line (was a fixed 38, which only suited a 32px cap).
  return y + font->cap_height + 5;
}

uint16_t bsBlank(uint16_t y, uint16_t rowH) {
  return y + rowH;
}

uint16_t bsWrap(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t col_x,
                uint16_t y, uint16_t rowH,
                const char *text, uint16_t col, uint16_t maxW) {
  // Word-wraps text within maxW pixels. (#A3)
  // Input contract:
  //   - Words separated by ASCII space (' ').
  //   - '\n' is a forced line break — flushes the current line and continues
  //     wrapping on the next row (blank '\n' produces a blank line).
  //   - Words longer than 31 characters are silently truncated to fit word[].
  //   - Empty tokens (consecutive spaces, leading space) are silently skipped.
  tft.setFont(*font);
  tft.setTextColor(col, TFT_BLACK);

  char    word[32];
  char    line[128] = "";
  uint8_t wi        = 0;
  int16_t lineW     = 0;                            // px width of current line
  int16_t spaceW    = getFontCharWidth(font, ' ');  // width of one space glyph
  const char *p     = text;

  while (true) {
    char c = *p++;
    bool end       = (c == '\0');
    bool forceWrap = (c == '\n');
    bool space     = (c == ' ') || end || forceWrap;

    if (space) {
      word[wi] = '\0';
      if (wi > 0) {
        int16_t wordW  = getFontStringWidth(font, word);
        int16_t needed = (lineW > 0) ? (lineW + spaceW + wordW) : wordW;
        if (needed > (int16_t)maxW && lineW > 0) {
          // Commit current line, start fresh with this word
          tft.setCursor(col_x, y); tft.print(line); y += rowH;
          strncpy(line, word, sizeof(line) - 1);
          line[sizeof(line) - 1] = '\0';
          lineW = wordW;
        } else {
          // Append word to current line (with leading space if non-empty)
          uint8_t ll = strlen(line);
          if (ll > 0 && ll + 1 < (int)sizeof(line)) { line[ll++] = ' '; line[ll] = '\0'; }
          strncat(line, word, sizeof(line) - strlen(line) - 1);
          lineW = needed;
        }
      }
      wi = 0;
      if (forceWrap) {
        // Flush current line (or emit a blank line if empty), then continue
        tft.setCursor(col_x, y); tft.print(line); y += rowH;
        line[0] = '\0';
        lineW   = 0;
      }
      if (end) {
        if (line[0]) { tft.setCursor(col_x, y); tft.print(line); y += rowH; }
        break;
      }
    } else {
      if (wi < (uint8_t)(sizeof(word) - 1)) word[wi++] = c;
    }
  }
  return y;
}

void bsShuffle(uint8_t *arr, uint8_t n) {
  if (n < 2) return;  // nothing to shuffle — guards n==0 underflow (#A2)
  for (uint8_t i = n - 1; i > 0; i--) {
    uint8_t j = (uint8_t)(random(i + 1));
    uint8_t tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
  }
}

/***************************************************************************************
   SYSTEM UTILITIES
   Teensy 4.1 (IMXRT1062) specific. Uses hardware registers only (SCB_AIRCR,
   USB1_USBCMD) — no Teensy core-internal functions, so safe to call from library code.
   Do not call from within an ISR.
****************************************************************************************/

void executeReboot() {
  SCB_AIRCR = 0x05FA0004;
}

void disconnectUSB() {
  USB1_USBCMD = 0;  // disable USB controller
  USB1_USBCMD = 2;  // begin USB controller reset
  delay(20);
}

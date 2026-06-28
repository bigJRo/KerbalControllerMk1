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
  tft.setFont(font);

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
      if (wLen < MAX_WORDCH - 1) words[wordCount][wLen++] = c;
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
    if (needed <= availW) {
      uint8_t curLen = strlen(lines[lineCount]);
      if (curLen > 0) lines[lineCount][curLen++] = ' ';
      memcpy(&lines[lineCount][curLen], words[wi], strlen(words[wi]));
      lines[lineCount][curLen + strlen(words[wi])] = '\0';
    } else {
      lineCount++;
      if (lineCount < MAX_LINES) {
        memcpy(lines[lineCount], words[wi], strlen(words[wi]));
        lines[lineCount][strlen(words[wi])] = '\0';
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
    // Clear the full line cell height with bgColor before printing to avoid
    // artefacts from inter-line gaps and glyph height mismatches
    tft.fillRect(x + 1, drawY, w - 2, charH, bgColor);
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
  tft.setFont(font);
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
   Used for rotated-style section labels where the RA8875 has no native text rotation.
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

  uint8_t  len   = strlen(text);
  if (len == 0) return;

  uint16_t charH = font->cap_height;
  uint16_t textH = len * charH;
  uint16_t startY = y0 + (h > textH ? (h - textH) / 2 : 0);

  for (uint8_t i = 0; i < len; i++) {
    char ch[2] = { text[i], '\0' };
    int16_t  cw = getFontStringWidth(font, ch);
    uint16_t cx = x0 + (w > (uint16_t)cw ? (w - (uint16_t)cw) / 2 : 0);
    uint16_t cy = startY + i * charH;
    tft.setFont(font);
    tft.setTextColor(color, backColor);
    tft.setCursor(cx, cy);
    tft.print(ch);
  }
}


/***************************************************************************************
   TEXT PRIMITIVES
   Core rendering functions — all other display functions build on these.
****************************************************************************************/

void textLeft(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
              const String &value, uint16_t foreColor, uint16_t backColor) {
  int16_t textH = font->cap_height;
  int16_t drawX = x0 + TEXT_BORDER;
  int16_t drawY = y0 + (h - textH) / 2;
  tft.setFont(font);
  tft.setTextColor(foreColor, backColor);
  tft.setCursor(drawX, drawY);
  tft.print(value);
}

void textRight(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               const String &value, uint16_t foreColor, uint16_t backColor) {
  int16_t textW = getFontStringWidth(font, value.c_str());
  int16_t textH = font->cap_height;
  int16_t drawX = x0 + w - textW - TEXT_BORDER;
  int16_t drawY = y0 + (h - textH) / 2;
  tft.setFont(font);
  tft.setTextColor(foreColor, backColor);
  tft.setCursor(drawX, drawY);
  tft.print(value);
}

void textCenter(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                const String &value, uint16_t foreColor, uint16_t backColor) {
  int16_t textW = getFontStringWidth(font, value.c_str());
  int16_t textH = font->cap_height;
  int16_t drawX = x0 + (w - textW) / 2;
  int16_t drawY = y0 + (h - textH) / 2;
  tft.setFont(font);
  tft.setTextColor(foreColor, backColor);
  tft.setCursor(drawX, drawY);
  tft.print(value);
}


/***************************************************************************************
   FORMATTING HELPERS - BASIC
   Convert numeric values to formatted Strings for use with print functions.
****************************************************************************************/

String formatInt(uint16_t value) {
  return String(value);
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

// Internal helper — extracts sign and makes value positive
static String _getSign(float &value) {
  if (value < 0) {
    value *= -1;
    return "-";
  }
  return "";
}

// formatSep(): for values >= 1000 the decimal part is dropped and a thousands
// separator is inserted instead (e.g. 1234.5 -> "1,234"). This is intentional —
// at that scale the decimal is noise on a display. (#64)
//
// (#A5) The float core delegates to a 64-bit integer helper so it no longer
// overflows for values >= 2^31. For exact formatting of large integer values
// (where float precision would quantize the digits), call formatSepI64()
// directly with an int64_t.

// Core implementation — operates on a signed 64-bit integer.
// Buffer sizing (#A6): int64 max is 19 digits → ~6 separator groups → 24
// chars of commas + 3-digit leading group = 27 chars worst case. buf[64]
// gives ample margin. INT64_MIN is unsupported (negation overflows); not
// reachable from any realistic Kerbal value.
static String _formatSepInt64(int64_t value) {
  char buf[64]     = "";
  char tempBuf[64] = "";
  String sign;
  if (value < 0) { value = -value; sign = "-"; }
  if (value < 1000) {
    char small[16];
    sprintf(small, "%lld", (long long)value);
    return sign + String(small);
  }
  while (value >= 1000) {
    int current = (int)(value % 1000);
    value /= 1000;
    sprintf(tempBuf, ",%03d%s", current, buf);
    strcpy(buf, tempBuf);
  }
  char output[64];
  sprintf(output, "%lld%s", (long long)value, buf);
  return sign + String(output);
}

String formatSep(float value) {
  String sign = _getSign(value);
  if (value < 1000) {
    char tempStr[16];
    dtostrf(value, 2, 2, tempStr);
    return sign + String(tempStr);
  }
  // Delegate to the int64 core. Float precision caps usable integer range
  // at ~1.6e7; large values are formatted as the nearest representable float.
  return sign + _formatSepInt64((int64_t)value);
}

String formatSepI64(int64_t value) {
  return _formatSepInt64(value);
}

String formatTime(float timeVal) {
  String sign = _getSign(timeVal);
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
    sprintf(timeStr, "%lld d: %02lld h: %02lld m: %02lld s",
            (long long)calcDays, (long long)calcHrs, (long long)calcMins, (long long)calcSecs);
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
  return sign + String(timeStr);
}

String formatAlt(float value) {
  String sign = _getSign(value);
  if (value < 1000000) {
    return sign + formatSep(value) + " m";
  } else if (value < 1000000000) {
    return sign + formatSep(value / 1000.0) + " km";
  } else if (value < 1000000000000.0) {
    return sign + formatSep(value / 1000000.0) + " Mm";
  } else {
    return sign + formatSep(value / 1000000000.0) + " Gm";
  }
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
      case 5: return "1000x";
      case 6: return "10000x";
      case 7: return "100000x";
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
  int16_t  newTextX = x0 + w - newTextW - TEXT_BORDER;
  if (newTextX < regionX) newTextX = regionX;
  uint16_t newH = (uint16_t)font->cap_height;

  bool bgChanged     = (valBack != ps.prevBg)    && (ps.prevBg     != 0x0001);
  bool heightChanged = (newH    != ps.prevHeight) && (ps.prevHeight != 0);

  if (bgChanged || heightChanged) {
    tft.fillRect(regionX, y0 + 1, regionW, h - 2, backColor);
  }

  textRight(tft, font, x0, y0, w, h, value, valColor, valBack);

  if (!bgChanged && !heightChanged && (uint16_t)newTextW < ps.prevWidth) {
    tft.fillRect(regionX, y0 + 1, newTextX - regionX, h - 2, backColor);
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
   Reads a 24-bit uncompressed BMP from the SD card and draws it to the RA8875 display.
   Only 24-bit BI_RGB (uncompressed) BMPs are supported.
   setupSD() must have been called and returned true before calling this function.
   Both bottom-up BMPs (standard) and top-down BMPs (negative height) are supported.
   When malloc succeeds, all rows are read sequentially then drawn in one pass for
   maximum SD throughput. On malloc failure, rows are sought and read individually.
   Image dimensions are clamped to 800×480 before VLA stack allocation.
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
  // statically knowable. Sizes track KCM_SCREEN_W (800 → 2400 + 1600 = 4 KB).
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

// Body table field order matches BodyParams struct declaration:
// soiName, dispName, image, cond,
// minSafe, flyHigh, lowSpace, highSpace, reentryAlt, soiAlt,
// radius, gravity, escapeVelocity,
// synchronousOrbit, synodicPeriod, orbitInclination,
// hasAtmo, hasO2, hasSurface,
// highQThreshold
//
// All altitudes in metres, gravity in m/s², velocity in m/s, period in seconds.
// soiAlt is double. All others are float except bools.
// Values sourced from KSP wiki (canonical). reentryAlt and highQThreshold
// are engineering estimates — calibrate highQThreshold per body from flight test.
// synodicPeriod is relative to Kerbin. 0 = not applicable (Kerbol, Kerbin).

static const BodyParams _bodyTable[] = {
  //         soiName    dispName  image                             cond
  //         minSafe   flyHigh  lowSpace  highSpace reentryAlt  soiAlt
  //         radius     gravity   escVel
  //         syncOrb     synodic      incl
  //         atmo   o2     surf   highQ

  { "Kerbol", "KERBOL", "/Kerbol-Display_240x168.bmp", "Plasma",
    1000000,  18000,   600000,   1000000000, 600000,   DBL_MAX,
    261600000, 17.1f,   94672.01f,
    1508045286, 0,          0.0f,
    true,  false, false, 0.0f },

  { "Moho",   "MOHO",   "/Moho-Display_240x168.bmp",   "Vacuum",
    6900,     0,       0,        80000,      0,        9646663.0,
    250000,    2.7f,    1161.41f,
    0,          2918346,    7.0f,
    false, false, true,  0.0f },

  { "Eve",    "EVE",    "/Eve-Display_240x168.bmp",    "Atmosphere",
    7600,     22000,   90000,    400000,     57000,    85109365.0,
    700000,    16.7f,   4831.96f,
    10328472,   14687035,   2.10f,
    true,  false, true,  0.0f },

  { "Gilly",  "GILLY",  "/Gilly-Display_240x168.bmp",  "Vacuum",
    7500,     0,       0,        6000,       0,        126123.0,
    13000,     0.049f,  35.71f,
    42138,      417243,     12.0f,
    false, false, true,  0.0f },

  { "Kerbin", "KERBIN", "/Kerbin-Display_240x168.bmp", "Breathable",
    6800,     18000,   70000,    250000,     45000,    84159286.0,
    600000,    9.81f,   3431.03f,
    2863334,    0,          0.0f,
    true,  true,  true,  0.0f },

  { "Mun",    "MUN",    "/Mun-Display_240x168.bmp",    "Vacuum",
    7100,     0,       0,        60000,      0,        2429559.0,
    200000,    1.63f,   807.08f,
    0,          141115,     0.0f,
    false, false, true,  0.0f },

  { "Minmus", "MINMUS", "/Minmus-Display_240x168.bmp", "Vacuum",
    5800,     0,       0,        30000,      0,        2247428.0,
    60000,     0.491f,  242.61f,
    357940,     1220131,    6.0f,
    false, false, true,  0.0f },

  { "Duna",   "DUNA",   "/Duna-Display_240x168.bmp",   "Atmosphere",
    8300,     12000,   50000,    140000,     20000,    47921949.0,
    320000,    2.94f,   1372.41f,
    2879999,    19645697,   0.06f,
    true,  false, true,  0.0f },

  { "Ike",    "IKE",    "/Ike-Display_240x168.bmp",    "Vacuum",
    12800,    0,       0,        50000,      0,        1049599.0,
    130000,    1.1f,    534.48f,
    0,          65766,      0.2f,
    false, false, true,  0.0f },

  { "Dres",   "DRES",   "/Dres-Display_240x168.bmp",   "Vacuum",
    5700,     0,       0,        25000,      0,        32832840.0,
    138000,    1.13f,   558.00f,
    732244,     11392903,   5.0f,
    false, false, true,  0.0f },

  { "Jool",   "JOOL",   "/Jool-Display_240x168.bmp",   "Atmosphere",
    120000,   120000,  200000,   4000000,    150000,   2455985200.0,
    6000000,   7.85f,   9704.43f,
    15010461,   10090901,   0.05f,
    true,  false, false, 0.0f },

  { "Laythe", "LAYTHE", "/Laythe-Display_240x168.bmp", "Breathable",
    6100,     10000,   50000,    200000,     38000,    3723646.0,
    500000,    7.85f,   2801.43f,
    0,          53007,      0.0f,
    true,  true,  true,  0.0f },

  { "Vall",   "VALL",   "/Vall-Display_240x168.bmp",   "Vacuum",
    8000,     0,       0,        90000,      0,        2406401.0,
    300000,    2.31f,   1176.10f,
    0,          106069,     0.0f,
    false, false, true,  0.0f },

  { "Tylo",   "TYLO",   "/Tylo-Display_240x168.bmp",   "Vacuum",
    13000,    0,       0,        250000,     0,        10856518.0,
    600000,    7.85f,   3068.81f,
    0,          212356,     0.025f,
    false, false, true,  0.0f },

  { "Bop",    "BOP",    "/Bop-Display_240x168.bmp",    "Vacuum",
    21800,    0,       0,        25000,      0,        1221061.0,
    65000,     0.589f,  276.62f,
    0,          547355,     15.0f,
    false, false, true,  0.0f },

  { "Pol",    "POL",    "/Pol-Display_240x168.bmp",    "Vacuum",
    4900,     0,       0,        22000,      0,        1042139.0,
    44000,     0.373f,  181.12f,
    0,          909742,     4.25f,
    false, false, true,  0.0f },

  { "Eeloo",  "EELOO",  "/Eeloo-Display_240x168.bmp",  "Vacuum",
    3800,     0,       0,        60000,      0,        119082940.0,
    210000,    1.69f,   841.83f,
    683690,     9776696,    6.15f,
    false, false, true,  0.0f },
};

static const uint8_t _bodyTableLen = sizeof(_bodyTable) / sizeof(_bodyTable[0]);

static const BodyParams _bodyUnknown = {
  "", "", "", "",
  0, 0, 0, 0, 0, 0.0,
  0, 0.0f, 0.0f,
  0, 0, 0.0f,
  false, false, false,
  0.0f
};

/***************************************************************************************
   GET BODY PARAMETERS
   Looks up the SOI string in the body table and returns a copy of the matching entry.
   Returns a zeroed/empty BodyParams (_bodyUnknown) if the SOI is not recognised.

   (#A22) Two overloads. The const char* version is the primary — callers
   parsing Simpit packets into char buffers can pass them directly without
   allocating a String. The const String& version delegates via .c_str() and
   exists for backward compatibility with existing callers.
****************************************************************************************/
BodyParams getBodyParams(const char* SOI) {
  if (SOI == nullptr) return _bodyUnknown;
  for (uint8_t i = 0; i < _bodyTableLen; i++) {
    if (strcmp(SOI, _bodyTable[i].soiName) == 0) {
      return _bodyTable[i];
    }
  }
  return _bodyUnknown;
}

BodyParams getBodyParams(const String& SOI) {
  return getBodyParams(SOI.c_str());
}


// =============================================================================
/***************************************************************************************
   BOOT SCREEN RENDERING HELPERS (#15, #16, #17)
   Shared terminal-aesthetic primitives for KCMk1 panel boot sequences.
   All functions stay in RA8875 graphics mode (setFont/setCursor/print).
****************************************************************************************/
void bsPrint(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t x, uint16_t y,
             const char *text, uint16_t col) {
  tft.setFont(font);
  tft.setTextColor(col, TFT_BLACK);
  tft.setCursor(x, y);
  tft.print(text);
}

uint16_t bsLine(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t col_x,
                uint16_t y, uint16_t rowH, const char *text, uint16_t col) {
  tft.setFont(font);
  tft.setTextColor(col, TFT_BLACK);
  tft.setCursor(col_x, y);
  tft.print(text);
  return y + rowH;
}

uint16_t bsBig(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t col_x,
               uint16_t y, const char *text, uint16_t col) {
  tft.setFont(font);
  tft.setTextColor(col, TFT_BLACK);
  tft.setCursor(col_x, y);
  tft.print(text);
  return y + 38;
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
  tft.setFont(font);
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
          if (ll > 0 && ll + 1 < sizeof(line)) { line[ll++] = ' '; line[ll] = '\0'; }
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
   Teensy 4.0 (IMXRT1062) specific. Uses hardware registers only (SCB_AIRCR,
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

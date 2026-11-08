/**
 * @file        KTC_UI.cpp
 * @version     1.0.0
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Touchscreen implementation. Panel set-up follows the universal
 *              tester, where it was confirmed on hardware: ST7789V (not
 *              ILI9341), tft.init(240, 320), rotation 0, inversion off, FT6236
 *              coordinates passed straight through. If taps land mirrored on a
 *              different panel, change ktcRawTouch() only.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include "KTC_UI.h"
#include "KTC_Config.h"
#include <stdarg.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_FT6206.h>

static Adafruit_ST7789 tft(KTC_PIN_TFT_CS, KTC_PIN_TFT_DC, -1);
static Adafruit_FT6206 ctp;
static bool _wasTouched = false;

// ------------------------------------------------------------
//  Colour helpers
// ------------------------------------------------------------
uint16_t ktc565(const RGBColor& c) {
    return (uint16_t)(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
}

uint16_t ktcLed565(const RGBColor& c) {
    uint8_t m = c.r;
    if (c.g > m) m = c.g;
    if (c.b > m) m = c.b;
    if (m == 0) return KTC_C_OFF;
    RGBColor s = { (uint8_t)((uint16_t)c.r * 255 / m),
                   (uint8_t)((uint16_t)c.g * 255 / m),
                   (uint8_t)((uint16_t)c.b * 255 / m) };
    return ktc565(s);
}

static uint16_t _textOn(uint16_t fill) {
    uint8_t r = (uint8_t)(((fill >> 11) & 0x1F) << 3);
    uint8_t g = (uint8_t)(((fill >> 5) & 0x3F) << 2);
    uint8_t b = (uint8_t)((fill & 0x1F) << 3);
    uint16_t luma = (uint16_t)((r * 77 + g * 151 + b * 28) >> 8);
    return (luma >= 112) ? KTC_C_BG : KTC_C_TEXT;
}

// ------------------------------------------------------------
//  Text helpers
// ------------------------------------------------------------
static void _trunc(char* out, uint8_t outLen, const char* s, int maxChars) {
    if (maxChars > (int)outLen - 1) maxChars = outLen - 1;
    int i = 0;
    for (; s && i < maxChars && s[i]; i++) out[i] = s[i];
    out[i] = '\0';
}

void ktcUiText(int x, int y, const char* s, uint16_t fg, uint8_t size, uint16_t bg) {
    tft.setTextSize(size);
    tft.setTextColor(fg, bg);
    tft.setCursor(x, y);
    tft.print(s);
}

void ktcUiTextC(int cx, int cy, const char* s, uint16_t fg, uint8_t size, uint16_t bg) {
    int w = (int)strlen(s) * 6 * size;
    ktcUiText(cx - w / 2, cy - 4 * size, s, fg, size, bg);
}

// ------------------------------------------------------------
//  Touch
// ------------------------------------------------------------
static bool ktcRawTouch(int16_t& x, int16_t& y) {
    if (!ctp.touched()) return false;
    TS_Point p = ctp.getPoint();
    x = p.x;          // 0..239
    y = p.y;          // 0..319
    return true;
}

static bool _press(int16_t& x, int16_t& y) {
    int16_t tx = 0, ty = 0;
    bool down = ktcRawTouch(tx, ty);
    bool edge = down && !_wasTouched;
    _wasTouched = down;
    if (edge) { x = tx; y = ty; }
    return edge;
}

// ------------------------------------------------------------
//  Set-up and fixed regions
// ------------------------------------------------------------
void ktcUiBegin() {
    pinMode(KTC_PIN_BACKLITE, OUTPUT);
    digitalWrite(KTC_PIN_BACKLITE, HIGH);
    tft.init(240, 320);
    tft.setRotation(0);
    tft.invertDisplay(false);
    tft.fillScreen(KTC_C_BG);
    tft.setTextWrap(false);
    ctp.begin(40);       // Wire already started by ktcBusBegin()
    _wasTouched = false;
}

void ktcUiClearBody() {
    tft.fillRect(0, 29, KTC_W, KTC_H - 29, KTC_C_BG);
}

void ktcUiTitle(const char* left, const char* right) {
    char l[41];
    tft.fillRect(0, 0, KTC_W, 14, KTC_C_PANEL);
    int rw = right ? (int)strlen(right) * 6 : 0;
    _trunc(l, sizeof(l), left, (KTC_W - 8 - rw - 6) / 6);
    ktcUiText(4, 3, l, KTC_C_TEXT, 1, KTC_C_PANEL);
    if (right) ktcUiText(KTC_W - 4 - rw, 3, right, KTC_C_ACCENT, 1, KTC_C_PANEL);
    tft.drawFastHLine(0, 28, KTC_W, KTC_C_PANEL2);
}

void ktcUiPowerBar(const char* left, const KtcPower& p) {
    char buf[24];
    tft.fillRect(0, 14, KTC_W, 14, KTC_C_PANEL);
    if (left) {
        char l[24];
        _trunc(l, sizeof(l), left, 20);
        ktcUiText(4, 17, l, KTC_C_DIM, 1, KTC_C_PANEL);
    }
    if (p.ok) snprintf(buf, sizeof(buf), "%.2fV %3.0fmA", (double)p.volts, (double)(p.amps * 1000.0f));
    else      snprintf(buf, sizeof(buf), "--V --mA");
    ktcUiText(KTC_W - 4 - (int)strlen(buf) * 6, 17, buf, p.ok ? KTC_C_ACCENT : KTC_C_DIM, 1, KTC_C_PANEL);
}

void ktcUiHeading(const char* s, uint16_t color) {
    char h[21];
    tft.fillRect(0, KTC_HEAD_Y, KTC_W, 18, KTC_C_BG);
    _trunc(h, sizeof(h), s, 19);
    ktcUiText(4, KTC_HEAD_Y, h, color, 2);
}

void ktcUiInstr(const char* s) {
    tft.fillRect(0, KTC_INSTR_Y, KTC_W, 38, KTC_C_BG);
    if (!s) return;
    const char* p = s;
    char line[40];
    for (uint8_t ln = 0; ln < 3 && *p; ln++) {
        int len = (int)strlen(p);
        int brk = len;
        if (len > 38) {
            brk = 38;
            for (int i = 38; i > 0; i--) if (p[i] == ' ') { brk = i; break; }
        }
        // Honour explicit line breaks.
        for (int i = 0; i < brk; i++) if (p[i] == '\n') { brk = i; break; }
        _trunc(line, sizeof(line), p, brk);
        ktcUiText(4, KTC_INSTR_Y + ln * 12, line, KTC_C_ACCENT);
        p += brk;
        while (*p == ' ' || *p == '\n') p++;
    }
}

void ktcUiLiveClear() {
    tft.fillRect(0, KTC_LIVE_Y, KTC_W, KTC_LIVE_H, KTC_C_BG);
}

void ktcUiRow(uint8_t row, const char* s, uint16_t color) {
    if (row >= KTC_LIVE_ROWS) return;
    int y = KTC_LIVE_Y + row * KTC_ROW_H;
    tft.fillRect(0, y, KTC_W, KTC_ROW_H, KTC_C_BG);
    char l[41];
    _trunc(l, sizeof(l), s, 39);
    ktcUiText(4, y + 2, l, color);
}

void ktcUiRowf(uint8_t row, uint16_t color, const char* fmt, ...) {
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ktcUiRow(row, buf, color);
}

void ktcUiFill(int x, int y, int w, int h, uint16_t color) {
    tft.fillRect(x, y, w, h, color);
}

void ktcUiChip(int x, int y, int w, int h, const char* label, uint16_t fill, uint16_t outline) {
    tft.fillRoundRect(x, y, w, h, 4, fill);
    if (outline) {
        tft.drawRoundRect(x, y, w, h, 4, outline);
        tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 3, outline);
    }
    char l[16];
    _trunc(l, sizeof(l), label, (w - 4) / 6);
    ktcUiTextC(x + w / 2, y + h / 2, l, (fill == KTC_C_OFF) ? KTC_C_DIM : _textOn(fill), 1, fill);
}

void ktcUiGridChip(uint8_t idx, uint8_t cols, int top, const char* label,
                   uint16_t fill, uint16_t outline) {
    if (cols == 0) cols = 1;
    int w = (KTC_W - 4 * (cols + 1)) / cols;
    int x = 4 + (idx % cols) * (w + 4);
    int y = KTC_LIVE_Y + top + (idx / cols) * 26;
    // Clear the outline band in case a previous outline was wider.
    tft.fillRect(x, y, w, 22, KTC_C_BG);
    ktcUiChip(x, y, w, 22, label, fill, outline);
}

void ktcUiBar(int y, const char* label, float frac, bool centered, const char* valueText,
              uint16_t fill) {
    const int lx = 4, bx = 52, bw = 124, h = 12;
    tft.fillRect(0, y, KTC_W, h, KTC_C_BG);
    char l[9];
    _trunc(l, sizeof(l), label, 7);
    ktcUiText(lx, y + 2, l, KTC_C_TEXT);
    tft.fillRect(bx, y, bw, h, KTC_C_PANEL);
    tft.drawRect(bx, y, bw, h, KTC_C_DIM);
    if (centered) {
        if (frac < -1) frac = -1;
        if (frac > 1) frac = 1;
        int mid = bx + bw / 2;
        tft.drawFastVLine(mid, y, h, KTC_C_DIM);
        int len = (int)((bw / 2 - 2) * (frac < 0 ? -frac : frac) + 0.5f);
        if (len > 0) {
            if (frac >= 0) tft.fillRect(mid + 1, y + 1, len, h - 2, fill);
            else           tft.fillRect(mid - len, y + 1, len, h - 2, fill);
        }
    } else {
        if (frac < 0) frac = 0;
        if (frac > 1) frac = 1;
        int len = (int)((bw - 2) * frac + 0.5f);
        if (len > 0) tft.fillRect(bx + 1, y + 1, len, h - 2, fill);
    }
    if (valueText) {
        char v[11];
        _trunc(v, sizeof(v), valueText, 10);
        ktcUiText(bx + bw + 4, y + 2, v, KTC_C_TEXT);
    }
}

void ktcUiSwatch(int x, int y, int w, int h, uint16_t color565, const char* caption) {
    tft.fillRoundRect(x, y, w, h, 6, color565);
    tft.drawRoundRect(x, y, w, h, 6, KTC_C_DIM);
    if (caption) {
        tft.fillRect(x - 20, y + h + 2, w + 40, 10, KTC_C_BG);
        ktcUiTextC(x + w / 2, y + h + 7, caption, KTC_C_TEXT);
    }
}

void ktcUiToast(const char* msg, uint16_t color) {
    tft.fillRect(0, KTC_TOAST_Y, KTC_W, 14, KTC_C_BG);
    if (!msg) return;
    char m[41];
    _trunc(m, sizeof(m), msg, 39);
    ktcUiText(4, KTC_TOAST_Y + 3, m, color);
}

// ------------------------------------------------------------
//  Bottom button row
// ------------------------------------------------------------
struct BtnDef { uint16_t bit; const char* label; uint16_t color; };
static const BtnDef BTN_DEFS[] = {
    { KTC_BTN_START,  "START",  KTC_C_PASS   },
    { KTC_BTN_RUNALL, "RUN ALL", KTC_C_PASS  },
    { KTC_BTN_PASS,   "PASS",   KTC_C_PASS   },
    { KTC_BTN_FAIL,   "FAIL",   KTC_C_FAIL   },
    { KTC_BTN_NEXT,   "NEXT",   KTC_C_ACCENT },
    { KTC_BTN_ENABLE, "ENABLE", KTC_C_ACCENT },
    { KTC_BTN_BULB,   "BULB",   KTC_C_WARN   },
    { KTC_BTN_ZERO,   "ZERO",   KTC_C_ACCENT },
    { KTC_BTN_RETRY,  "RETRY",  KTC_C_WARN   },
    { KTC_BTN_SKIP,   "SKIP",   KTC_C_OFF    },
    { KTC_BTN_LIVE,   "LIVE",   KTC_C_NAV    },
    { KTC_BTN_MENU,   "MENU",   KTC_C_NAV    },
    { KTC_BTN_BACK,   "BACK",   KTC_C_NAV    },
    { KTC_BTN_ABORT,  "ABORT",  KTC_C_OFF    },
};
static const uint8_t BTN_DEF_N = sizeof(BTN_DEFS) / sizeof(BTN_DEFS[0]);
static const uint8_t BTN_MAX = 5;
static uint16_t _btnBits[BTN_MAX];
static int16_t  _btnX[BTN_MAX], _btnW[BTN_MAX];
static uint8_t  _btnCount = 0;

void ktcUiButtons(uint16_t mask) {
    tft.fillRect(0, KTC_BTN_Y - 2, KTC_W, KTC_H - KTC_BTN_Y + 2, KTC_C_BG);
    _btnCount = 0;
    uint8_t idx[BTN_MAX];
    for (uint8_t i = 0; i < BTN_DEF_N && _btnCount < BTN_MAX; i++)
        if (mask & BTN_DEFS[i].bit) idx[_btnCount++] = i;
    if (_btnCount == 0) return;
    int w = (KTC_W - 4 * (_btnCount + 1)) / _btnCount;
    int x = 4;
    for (uint8_t j = 0; j < _btnCount; j++) {
        const BtnDef& d = BTN_DEFS[idx[j]];
        _btnBits[j] = d.bit;
        _btnX[j] = x;
        _btnW[j] = w;
        tft.fillRoundRect(x, KTC_BTN_Y, w, KTC_BTN_H, 5, d.color);
        tft.drawRoundRect(x, KTC_BTN_Y, w, KTC_BTN_H, 5, KTC_C_TEXT);
        ktcUiTextC(x + w / 2, KTC_BTN_Y + KTC_BTN_H / 2, d.label,
                   (d.color == KTC_C_OFF) ? KTC_C_TEXT : _textOn(d.color), 1, d.color);
        x += w + 4;
    }
}

static uint16_t _hitButton(int16_t x, int16_t y) {
    if (y < KTC_BTN_Y || y >= KTC_BTN_Y + KTC_BTN_H) return 0;
    for (uint8_t j = 0; j < _btnCount; j++)
        if (x >= _btnX[j] && x < _btnX[j] + _btnW[j]) return _btnBits[j];
    return 0;
}

uint16_t ktcUiPollButtons() {
    int16_t x, y;
    if (!_press(x, y)) return 0;
    return _hitButton(x, y);
}

// ------------------------------------------------------------
//  Menu list
// ------------------------------------------------------------
#define MENU_Y0   34
#define MENU_RH   20

void ktcUiMenuRow(uint8_t row, const char* label, const char* mark, uint16_t markColor) {
    int y = MENU_Y0 + row * MENU_RH;
    tft.fillRect(0, y, KTC_W, MENU_RH - 2, KTC_C_PANEL);
    char l[33];
    _trunc(l, sizeof(l), label, 31);
    ktcUiText(6, y + 5, l, KTC_C_TEXT, 1, KTC_C_PANEL);
    if (mark && *mark) {
        int mw = (int)strlen(mark) * 6;
        ktcUiText(KTC_W - 6 - mw, y + 5, mark, markColor, 1, KTC_C_PANEL);
    }
}

int ktcUiPollMenu(uint8_t rows, uint16_t& btnOut) {
    btnOut = 0;
    int16_t x, y;
    if (!_press(x, y)) return -1;
    btnOut = _hitButton(x, y);
    if (btnOut) return -1;
    if (y < MENU_Y0) return -1;
    int r = (y - MENU_Y0) / MENU_RH;
    return (r >= 0 && r < (int)rows) ? r : -1;
}

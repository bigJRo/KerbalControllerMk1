/**
 * @file        TesterUI.cpp
 * @version     2.0.0
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Touchscreen UI implementation (Adafruit_GFX + FT6206) for the
 *              Module Tester. Pure rendering + touch hit-testing; all protocol
 *              logic lives in TesterHW. Screens: splash, scan/select, the
 *              per-module test dashboard, and the construction-test view.
 *
 *              Target board: Seeed XIAO RA4M1 (Arduino UNO R4 core), 2.8"
 *              ER-TFT028A3-4 (ST7789V, 240x320 4-wire SPI), FT6236 capacitive
 *              touch (I2C 0x38). The display is used in PORTRAIT — 240 wide x
 *              320 tall, rotation 0, confirmed on hardware.
 *
 *              CONTROLLER NOTE: this panel is an ST7789V, NOT an ILI9341.
 *              The two share the basic MIPI-DCS command set, so an ILI9341
 *              driver "mostly works" — but its init programs ILI9341-specific
 *              display-function / power / VCOM registers that mean something
 *              different on the ST7789V, leaving the gate/line drive
 *              misconfigured (garbage band on the panel bottom, odd rotation
 *              direction). Use Adafruit_ST7789 only.
 *
 * @note        Dependencies: Adafruit_GFX + Adafruit ST7735/ST7789 Library +
 *              Adafruit_FT6206 (+ Adafruit_BusIO). LovyanGFX is NOT used — it
 *              does not support the renesas_uno architecture ('HardwareSPI'
 *              does not name a type) and its global `using RGBColor` alias
 *              collides with KerbalModuleCommon's struct RGBColor.
 *
 *              The ST7789V uses hardware SPI on the XIAO's fixed SPI pins
 *              (SCK/MISO/MOSI = D8/D9/D10) with CS/DC from TesterConfig.h and
 *              no reset line (software reset). The FT6236 shares Wire (begun
 *              by hwBegin(); do not call Wire.begin() here). uiBegin() clears
 *              the full 240x320 GRAM once at boot so no stale noise band can
 *              show below partially-repainted screens. If colours appear
 *              inverted on hardware, toggle tft.invertDisplay() in uiBegin()
 *              — ST7789 panels vary.
 *
 *              TOUCH MAPPING: with rotation(0) the FT6236 native portrait
 *              coordinates map straight through: sx = p.x (0..239),
 *              sy = p.y (0..319). If taps land mirrored on hardware, try
 *              sx = 239 - p.x and/or sy = 319 - p.y in rawTouch() — it is
 *              the single place the mapping lives.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */

#include <Arduino.h>

#include "TesterConfig.h"
#include "TesterUI.h"            // pulls in ModuleCatalog.h / TesterHW.h
#include <KerbalModuleCommon.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_FT6206.h>

// ============================================================
//  Devices — ST7789V on hardware SPI (fixed XIAO SPI pins), no
//  panel RST (software reset). FT6236 speaks the FT6206 protocol.
// ============================================================
static Adafruit_ST7789 tft(PIN_TFT_CS, PIN_TFT_DC, -1);
static Adafruit_FT6206 ctp;

// ============================================================
//  Colour palette — 16-bit RGB565, dark, high contrast.
// ============================================================
static constexpr uint16_t C_BG     = 0x0000;   // black background
static constexpr uint16_t C_PANEL  = 0x18E3;   // dark slate panel
static constexpr uint16_t C_PANEL2 = 0x2104;   // slightly lighter panel
static constexpr uint16_t C_TEXT   = 0xFFFF;   // white text
static constexpr uint16_t C_DIM    = 0x9CD3;   // dim grey text
static constexpr uint16_t C_ACCENT = 0x05BF;   // cyan accent (toast / power)
static constexpr uint16_t C_ON     = 0x07E0;   // green (active / chip on)
static constexpr uint16_t C_OFF    = 0x4208;   // dark grey (chip off)
static constexpr uint16_t C_WARN   = 0xFD20;   // amber
static constexpr uint16_t C_ALERT  = 0xF800;   // red
static constexpr uint16_t C_BAR    = 0x047F;   // bar fill (blue)
static constexpr uint16_t C_BARBG  = 0x18E3;   // bar track

// RGB565 from a canonical KerbalModuleCommon RGB colour (same formula the
// module catalog uses) for the LED colours the UI references directly.
static inline uint16_t rgb565k(const RGBColor& c) {
    return (uint16_t)(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
}
static const uint16_t C_INFO      = rgb565k(KMC_SKY);    // calm info (toast)
static const uint16_t C_LED_GREEN = rgb565k(KMC_GREEN);  // GPWS full mode
static const uint16_t C_LED_AMBER = rgb565k(KMC_AMBER);  // GPWS prox-only mode
static constexpr uint16_t C_NAV   = 0x435C;              // royal blue (#4169E1) — Back button

// ============================================================
//  Layout constants (portrait 240x320)
//
//  Classic font metrics: size 1 glyph cell = 6x8 px (max 40 chars
//  across 240px); size 2 = 12x16 (max 20 chars). Row pitch >= 12 px
//  for size 1, >= 20 px for size 2 — no two baselines share a band.
// ============================================================

// Top area: two stacked 14px bars (title, power) + divider line.
static constexpr int TITLE_BAR_Y = 0;
static constexpr int TITLE_BAR_H = 14;
static constexpr int PWR_BAR_Y   = TITLE_BAR_H;               // 14
static constexpr int PWR_BAR_H   = 14;
static constexpr int TOP_H       = TITLE_BAR_H + PWR_BAR_H;   // 28 (divider at y=28)
static constexpr int CONTENT_Y   = TOP_H + 2;                 // 30

// Dashboard header status line (lifecycle / FAULT / tx).
static constexpr int HDR_Y = CONTENT_Y;                       // 30
static constexpr int HDR_H = 12;                              // band 30..41

// Scan list
static constexpr int SCAN_TOP     = TOP_H + 6;                // 34
static constexpr int SCAN_ROW_H   = 30;                       // row box height
static constexpr int SCAN_ROW_GAP = 4;                        // pitch 34
static constexpr int SCAN_MAXROWS = 7;                        // 34..272, above Rescan

// Dashboard control buttons: NINE as a 3x3 grid at the bottom.
static constexpr int CTRL_COUNT  = 9;
static constexpr int CTRL_PERROW = 3;
static constexpr int CTRL_ROWS   = 3;
static constexpr int CTRL_H      = 28;                        // per-button pitch
static constexpr int CTRL_AREA_H = CTRL_ROWS * CTRL_H;        // 84
static constexpr int CTRL_AREA_Y = UI_H - CTRL_AREA_H;        // 236 (..319)
static constexpr int CTRL_W      = UI_W / CTRL_PERROW;        // 80
static constexpr int TOAST_Y     = CTRL_AREA_Y - 18;          // 218 (band 218..233)

// Construction-test screen control buttons: up to 5 as one row of 3
// over one row of 2, each >= 70x34 for finger targets.
static constexpr int CT_BTN_H   = 34;
static constexpr int CT_BTN_GAP = 4;
static constexpr int CT_MAXBTNS = 5;

// ============================================================
//  Hit-test rectangle
// ============================================================
struct Rect {
    int16_t x, y, w, h;
    bool hit(int16_t px, int16_t py) const {
        return px >= x && px < (x + w) && py >= y && py < (y + h);
    }
};

// ============================================================
//  File-local screen state
// ============================================================
static Rect _scanRows[SCAN_MAXROWS];
static uint8_t _scanRowCount = 0;
static Rect _rescanBtn = {0, 0, 0, 0};

static Rect _ctrlBtns[CTRL_COUNT];
static const char* const _ctrlLabels[CTRL_COUNT] = {
    "Enbl", "Dsbl", "Slp", "Wake", "Rst", "Bulb", "LED", "Test", "Back"
};
static const UIAction _ctrlActions[CTRL_COUNT] = {
    UI_ENABLE, UI_DISABLE, UI_SLEEP, UI_WAKE, UI_RESET,
    UI_BULB, UI_LEDCYCLE, UI_TEST, UI_BACK
};

// Construction-test screen: shown-button rects + their CtButton id, in the
// fixed render order. Only entries [0.._ctBtnCount) are valid after a render.
static Rect     _ctBtnRects[CT_MAXBTNS];
static CtButton _ctBtnIds[CT_MAXBTNS];
static uint8_t  _ctBtnCount = 0;

// Press-edge debounce: shared was-touched latch. Each touch poller only
// fires on a fresh press (transition from not-touched to touched).
static bool _wasTouched = false;

// Running encoder totals (reset on uiDashboardBegin).
static long _encTotal[2] = {0, 0};

// Current input area geometry (set by uiDashboardBegin).
static int _inputAreaY = HDR_Y + HDR_H + 2;                   // 44
static int _inputAreaH = TOAST_Y - (HDR_Y + HDR_H + 2);       // 44..218

// ============================================================
//  Text helpers (Adafruit_GFX classic 5x7 font)
//
//  Adafruit_GFX has no drawString/textWidth/datum support, so text is
//  placed with setCursor + print. Width = strlen * 6 * size (each glyph
//  cell is 6 px wide incl. spacing), glyph height = 8 * size — exact for
//  the built-in classic font. print() wraps at the right edge, so
//  strings are truncated to their field width before drawing.
// ============================================================

/** @brief Rendered width of a string at a given text size (classic font). */
static int txtW(const char* s, uint8_t size) {
    return (int)strlen(s) * 6 * size;
}

/** @brief Copy s into buf truncated to maxChars (buf holds >= maxChars+1). */
static void truncStr(char* buf, uint8_t bufLen, const char* s, int maxChars) {
    if (maxChars > (int)bufLen - 1) maxChars = bufLen - 1;
    if (maxChars < 0) maxChars = 0;
    int i = 0;
    for (; i < maxChars && s && s[i]; i++) buf[i] = s[i];
    buf[i] = '\0';
}

/** @brief Draw text with its top-left corner at (x, y). */
static void drawTextTL(int x, int y, const char* s,
                       uint16_t fg, uint16_t bg, uint8_t size = 1) {
    tft.setTextSize(size);
    tft.setTextColor(fg, bg);
    tft.setCursor(x, y);
    tft.print(s);
}

/** @brief Draw text vertically centered on cy, left-aligned at x. */
static void drawTextML(int x, int cy, const char* s,
                       uint16_t fg, uint16_t bg, uint8_t size = 1) {
    drawTextTL(x, cy - 4 * size, s, fg, bg, size);
}

/** @brief Draw text centered on (cx, cy). */
static void drawTextMC(int cx, int cy, const char* s,
                       uint16_t fg, uint16_t bg, uint8_t size = 1) {
    drawTextTL(cx - txtW(s, size) / 2, cy - 4 * size, s, fg, bg, size);
}

/** @brief Draw text vertically centered on cy, right-aligned at rx. */
static void drawTextMR(int rx, int cy, const char* s,
                       uint16_t fg, uint16_t bg, uint8_t size = 1) {
    drawTextTL(rx - txtW(s, size), cy - 4 * size, s, fg, bg, size);
}

/**
 * @brief  Draw a size-1 string on up to two lines of maxChars each,
 *         breaking at the last space that fits (hard cut otherwise).
 *         Anything beyond two lines is dropped.
 */
static void drawTextWrapped2(int x, int y, const char* s, int maxChars,
                             int lineH, uint16_t fg, uint16_t bg) {
    if (!s) return;
    int len = (int)strlen(s);
    char line[41];   // size-1 max 40 chars across 240px

    if (len <= maxChars) {
        truncStr(line, sizeof(line), s, maxChars);
        drawTextTL(x, y, line, fg, bg, 1);
        return;
    }
    // Break at the last space at or before maxChars.
    int brk = maxChars;
    for (int i = maxChars; i > 0; i--) {
        if (s[i] == ' ') { brk = i; break; }
    }
    truncStr(line, sizeof(line), s, brk);
    drawTextTL(x, y, line, fg, bg, 1);
    const char* rest = s + brk;
    while (*rest == ' ') rest++;
    truncStr(line, sizeof(line), rest, maxChars);
    drawTextTL(x, y + lineH, line, fg, bg, 1);
}

// ============================================================
//  Touch helpers
// ============================================================

/**
 * @brief  Raw touch read in portrait rotation(0) coordinates.
 *
 *         The FT6236 reports native portrait coordinates that map straight
 *         through: sx = p.x (0..239), sy = p.y (0..319).
 *
 *         NOTE: FT6236 panels vary in origin; if taps are mirrored on
 *         hardware, try sx = 239 - p.x and/or sy = 319 - p.y. This is the
 *         single place the mapping lives.
 */
static bool rawTouch(int16_t& sx, int16_t& sy) {
    if (!ctp.touched()) return false;
    TS_Point p = ctp.getPoint();
    sx = p.x;            // 0..239
    sy = p.y;            // 0..319
    return true;
}

/** @brief Read a debounced press-edge touch. Returns true once per press. */
static bool readPress(int16_t& x, int16_t& y) {
    int16_t tx, ty;
    bool down = rawTouch(tx, ty);
    bool edge = false;
    if (down && !_wasTouched) {
        x = tx;
        y = ty;
        edge = true;
    }
    _wasTouched = down;
    return edge;
}

// ============================================================
//  Small drawing helpers (file-local)
// ============================================================

/** @brief Pick a readable text colour against an RGB565 fill (BT.601 luma). */
static uint16_t chipTextColor(uint16_t fill) {
    uint8_t r = (uint8_t)(((fill >> 11) & 0x1F) << 3);
    uint8_t g = (uint8_t)(((fill >> 5)  & 0x3F) << 2);
    uint8_t b = (uint8_t)((fill & 0x1F) << 3);
    uint16_t luma = (uint16_t)((r * 77 + g * 151 + b * 28) >> 8);   // 0..255
    return (luma >= 112) ? C_BG : C_TEXT;   // dark text on bright fills
}

/**
 * @brief Draw a labelled "chip"; the label is truncated to fit the chip.
 * @param onFill  RGB565 ACTIVE fill matching the module's NeoPixel colour
 *                for this input; 0 = no LED / unknown -> neutral C_ON style.
 */
static void drawChip(int x, int y, int w, int h, const char* label, bool on,
                     uint16_t onFill = 0) {
    uint16_t fill = on ? (onFill ? onFill : C_ON) : C_OFF;
    uint16_t txt  = on ? chipTextColor(fill) : C_DIM;
    tft.fillRoundRect(x, y, w, h, 4, fill);
    char lbl[16];
    truncStr(lbl, sizeof(lbl), label, (w - 4) / 6);   // size-1 chars that fit
    drawTextMC(x + w / 2, y + h / 2, lbl, txt, fill, 1);
}

/** @brief Catalog ACTIVE colour for input i (0 if none / no colors table). */
static uint16_t inputColor(const ModuleInfo* info, uint8_t i) {
    return (info && info->colors && i < info->inputCount) ? info->colors[i] : 0;
}

/** @brief Draw a horizontal bar with a track and value fill (0..1 fraction). */
static void drawBar(int x, int y, int w, int h, float frac, uint16_t fill) {
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    tft.fillRect(x, y, w, h, C_BARBG);
    tft.drawRect(x, y, w, h, C_DIM);
    int fw = (int)((w - 2) * frac + 0.5f);
    if (fw > 0) tft.fillRect(x + 1, y + 1, fw, h - 2, fill);
}

/** @brief Draw a centered signed bar (center origin) for joystick axes. */
static void drawCenterBar(int x, int y, int w, int h, float frac, uint16_t fill) {
    if (frac < -1) frac = -1;
    if (frac > 1) frac = 1;
    tft.fillRect(x, y, w, h, C_BARBG);
    tft.drawRect(x, y, w, h, C_DIM);
    int mid = x + w / 2;
    tft.drawFastVLine(mid, y, h, C_DIM);
    int half = (w / 2) - 2;
    int len  = (int)(half * (frac < 0 ? -frac : frac) + 0.5f);
    if (len > 0) {
        if (frac >= 0) tft.fillRect(mid + 1, y + 1, len, h - 2, fill);
        else           tft.fillRect(mid - len, y + 1, len, h - 2, fill);
    }
}

/** @brief Clear the live input area to background. */
static void clearInputArea() {
    tft.fillRect(0, _inputAreaY, UI_W, _inputAreaH, C_BG);
}

/** @brief Decode lifecycle byte to short text. */
static const char* lifecycleText(uint8_t lifecycle) {
    switch (lifecycle & KMC_STATUS_LIFECYCLE_MASK) {
        case KMC_STATUS_ACTIVE:     return "ACTIVE";
        case KMC_STATUS_SLEEPING:   return "SLEEPING";
        case KMC_STATUS_DISABLED:   return "DISABLED";
        case KMC_STATUS_BOOT_READY: return "BOOT";
        default:                    return "?";
    }
}

/** @brief Draw the title bar (top 14px) and the divider under the top area. */
static void drawTopBarTitle(const char* title) {
    char t[41];                       // size-1 max 40 chars across 240px
    truncStr(t, sizeof(t), title, 39);
    tft.fillRect(0, TITLE_BAR_Y, UI_W, TITLE_BAR_H, C_PANEL);
    drawTextML(4, TITLE_BAR_Y + TITLE_BAR_H / 2, t, C_TEXT, C_PANEL, 1);
    tft.drawFastHLine(0, TOP_H, UI_W, C_PANEL2);
}

// ============================================================
//  Lifecycle
// ============================================================
void uiBegin() {
    pinMode(PIN_BACKLITE, OUTPUT);
    digitalWrite(PIN_BACKLITE, HIGH);   // backlight on

    tft.init(240, 320);                 // ST7789V, full 240x320 glass
    tft.setRotation(0);                 // portrait -> 240x320
    // ST7789 panels differ in inversion polarity; the library enables
    // inversion for 240x320 by default. If colours look inverted on this
    // panel, uncomment:
    // tft.invertDisplay(false);

    // Clear the ENTIRE 240x320 GRAM once at boot so no stale noise band
    // can show even if a later screen doesn't repaint everything.
    tft.fillScreen(C_BG);

    // FT6236 on the shared Wire bus (already begun by hwBegin()).
    ctp.begin(40);                      // touch threshold 40

    tft.setTextSize(1);
    tft.setTextWrap(false);

    // Calibrate any initial touch state so the first poll is not a stale press.
    _wasTouched = false;
}

void uiSplash() {
    tft.fillScreen(C_BG);
    // "KCMk1 Module Tester" @ size 2 = 19 chars * 12 = 228px < 240.
    drawTextMC(UI_W / 2, UI_H / 2 - 30, "KCMk1 Module Tester", C_TEXT, C_BG, 2);
    drawTextMC(UI_W / 2, UI_H / 2 + 4,  "v" TESTER_VERSION_STR, C_ACCENT, C_BG, 1);
    drawTextMC(UI_W / 2, UI_H / 2 + 24, "Jeb's Controller Works", C_DIM, C_BG, 1);
}

// ============================================================
//  Scan / select screen
// ============================================================
void uiScanBegin() {
    tft.fillScreen(C_BG);
    drawTopBarTitle("Select Module");

    // Power bar placeholder until the first uiPowerBar() call.
    tft.fillRect(0, PWR_BAR_Y, UI_W, PWR_BAR_H, C_PANEL);

    // Rescan button: 200x32, centered at the bottom.
    _rescanBtn = { (int16_t)((UI_W - 200) / 2), (int16_t)(UI_H - 36), 200, 32 };
    tft.fillRoundRect(_rescanBtn.x, _rescanBtn.y, _rescanBtn.w, _rescanBtn.h, 5, C_PANEL2);
    tft.drawRoundRect(_rescanBtn.x, _rescanBtn.y, _rescanBtn.w, _rescanBtn.h, 5, C_ACCENT);
    drawTextMC(_rescanBtn.x + _rescanBtn.w / 2, _rescanBtn.y + _rescanBtn.h / 2,
               "Rescan", C_TEXT, C_PANEL2, 1);

    _scanRowCount = 0;
}

void uiScanList(const ModuleInfo* const* infos, const uint8_t* addrs,
                const uint8_t* typeIds, uint8_t count) {
    // Clear the list region (above the Rescan button).
    int listBottom = UI_H - 40;
    tft.fillRect(0, SCAN_TOP, UI_W, listBottom - SCAN_TOP, C_BG);

    if (count == 0) {
        _scanRowCount = 0;
        // Two short centered lines (one long line would exceed 240px).
        drawTextMC(UI_W / 2, SCAN_TOP + 60, "No modules found", C_DIM, C_BG, 1);
        drawTextMC(UI_W / 2, SCAN_TOP + 74, "connect a module", C_DIM, C_BG, 1);
        return;
    }

    uint8_t rows = count;
    if (rows > SCAN_MAXROWS) rows = SCAN_MAXROWS;
    _scanRowCount = rows;

    char buf[16];
    char name[37];
    for (uint8_t i = 0; i < rows; i++) {
        int y = SCAN_TOP + i * (SCAN_ROW_H + SCAN_ROW_GAP);
        Rect r = { 2, (int16_t)y, (int16_t)(UI_W - 4), (int16_t)SCAN_ROW_H };
        _scanRows[i] = r;

        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, C_PANEL);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, C_PANEL2);

        // Line 1 (y+4..y+11): module name, truncated to the row width.
        truncStr(name, sizeof(name),
                 (infos[i] != nullptr) ? infos[i]->name : "Unknown", 36);
        drawTextTL(r.x + 8, r.y + 4, name, C_TEXT, C_PANEL, 1);

        // Line 2 (y+17..y+24): address + type, dim.
        snprintf(buf, sizeof(buf), "0x%02X  t:%02X", addrs[i], typeIds[i]);
        drawTextTL(r.x + 8, r.y + 17, buf, C_DIM, C_PANEL, 1);
    }
}

int uiScanTouch(uint8_t count) {
    int16_t x, y;
    if (!readPress(x, y)) return -1;

    if (_rescanBtn.hit(x, y)) return -2;

    uint8_t rows = (count < _scanRowCount) ? count : _scanRowCount;
    for (uint8_t i = 0; i < rows; i++) {
        if (_scanRows[i].hit(x, y)) return (int)i;
    }
    return -1;
}

// ============================================================
//  Dashboard screen
// ============================================================
void uiDashboardBegin(const ModuleInfo* info, uint8_t addr,
                      uint8_t fwMajor, uint8_t fwMinor, uint8_t caps) {
    tft.fillScreen(C_BG);

    // Title bar: module name + address + firmware version + capability
    // flags from the identity reply (e.g. "GPWS Input 0x2A fw2.0 c:10").
    char t[41];
    if (fwMajor || fwMinor || caps) {
        snprintf(t, sizeof(t), "%s 0x%02X fw%u.%u c:%02X",
                 (info && info->name) ? info->name : "Module", addr,
                 fwMajor, fwMinor, caps);
    } else {
        snprintf(t, sizeof(t), "%s 0x%02X",
                 (info && info->name) ? info->name : "Module", addr);
    }
    drawTopBarTitle(t);
    // Power bar placeholder until the first uiPowerBar() call.
    tft.fillRect(0, PWR_BAR_Y, UI_W, PWR_BAR_H, C_PANEL);

    // Reset encoder running totals.
    _encTotal[0] = 0;
    _encTotal[1] = 0;

    // Control button grid (3 rows x 3 cols) at the bottom: y 236..319.
    // Back is royal blue so the navigation action stands out from the
    // module-command buttons.
    for (uint8_t i = 0; i < CTRL_COUNT; i++) {
        int row = i / CTRL_PERROW;
        int col = i % CTRL_PERROW;
        int x = col * CTRL_W;
        int y = CTRL_AREA_Y + row * CTRL_H;
        Rect r = { (int16_t)(x + 1), (int16_t)(y + 1),
                   (int16_t)(CTRL_W - 2), (int16_t)(CTRL_H - 2) };   // ~78x26
        _ctrlBtns[i] = r;
        uint16_t fill = (_ctrlActions[i] == UI_BACK) ? C_NAV : C_PANEL2;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 3, fill);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 3, C_DIM);
        drawTextMC(r.x + r.w / 2, r.y + r.h / 2, _ctrlLabels[i], C_TEXT, fill, 1);
    }

    // Static label for the input area depending on kind.
    _inputAreaY = HDR_Y + HDR_H + 2;
    _inputAreaH = TOAST_Y - _inputAreaY;
    clearInputArea();

    if (!info) return;
    switch (info->kind) {                       // all <= 39 chars @ size 1
        case MK_BUTTON12:    drawTextTL(4, _inputAreaY, "Buttons (12)", C_DIM, C_BG); break;
        case MK_BUTTON24:    drawTextTL(4, _inputAreaY, "Buttons + switches (24)", C_DIM, C_BG); break;
        case MK_JOYSTICK:    drawTextTL(4, _inputAreaY, "Axes X / Y / Z + buttons", C_DIM, C_BG); break;
        case MK_DISPLAY:     drawTextTL(4, _inputAreaY, "Buttons + value", C_DIM, C_BG); break;
        case MK_THROTTLE:    drawTextTL(4, _inputAreaY, "Throttle: value + flags", C_DIM, C_BG); break;
        case MK_DUAL_ENCODER:drawTextTL(4, _inputAreaY, "Encoders + buttons", C_DIM, C_BG); break;
        default: break;
    }
}

void uiDashboardHeader(const ModuleState& st) {
    // Repaint the header status line under the top bars (band 30..41).
    tft.fillRect(0, HDR_Y, UI_W, HDR_H, C_BG);

    // Worst case "SLEEPING" + "FAULT" + "tx:NN" = ~22 chars in 40 available.
    int x = 4;
    const char* lc = lifecycleText(st.lifecycle);
    drawTextML(x, HDR_Y + HDR_H / 2, lc, C_TEXT, C_BG, 1);
    x += txtW(lc, 1) + 12;

    if (st.fault) {
        drawTextML(x, HDR_Y + HDR_H / 2, "FAULT", C_ALERT, C_BG, 1);
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "tx:%02u", (unsigned)st.txCounter);
    drawTextMR(UI_W - 4, HDR_Y + HDR_H / 2, buf, C_DIM, C_BG, 1);
}

// ------------------------------------------------------------
//  Per-kind input renderers
// ------------------------------------------------------------

/**
 * @brief Draw a grid of button chips for indices with non-empty labels.
 *        4 chips per row (~55x22 each, pitch 26), up to 6 rows for 24
 *        inputs: grid spans y = 58 .. 58+5*26+22 = 210, just inside the
 *        toast line at 218.
 */
static void drawButtonGrid(const ModuleInfo* info, const ModuleState& st) {
    const int cols = 4;
    const int gap = 4;
    const int gx = 4;
    const int chipW = (UI_W - 2 * gx - (cols - 1) * gap) / cols;   // 55
    const int chipH = 22;
    const int gyTop = _inputAreaY + 14;

    uint8_t n = info->inputCount;
    if (n > 24) n = 24;
    for (uint8_t i = 0; i < n; i++) {
        const char* lbl = info->labels ? info->labels[i] : nullptr;
        if (!lbl || lbl[0] == '\0') continue;
        int col = i % cols;
        int row = i / cols;
        int x = gx + col * (chipW + gap);
        int y = gyTop + row * (chipH + gap);
        bool on = (st.events >> i) & 1u;
        drawChip(x, y, chipW, chipH, lbl, on, inputColor(info, i));
    }
}

/**
 * @brief Draw N button chips in a single row from labels[0..n-1], sized
 *        to share the 240px width evenly (2 -> 113px, 3 -> 73, 4 -> 53).
 * @param plane  bitfield giving the on/off state for each chip (bit i =
 *               chip i). Callers pass st.events for momentary state, or
 *               st.flags for persistent (held) state (joystick buttons).
 */
static void drawButtonRowMask(const ModuleInfo* info, uint32_t plane,
                              uint8_t n, int y) {
    if (n == 0) return;
    const int gap = 6;
    const int x0 = 4;
    const int chipW = (UI_W - 2 * x0 - (n - 1) * gap) / n;
    const int chipH = 22;
    int x = x0;
    for (uint8_t i = 0; i < n; i++) {
        const char* lbl = (info->labels && i < info->inputCount && info->labels[i])
                              ? info->labels[i] : "btn";
        bool on = (plane >> i) & 1u;
        drawChip(x, y, chipW, chipH, lbl, on, inputColor(info, i));
        x += chipW + gap;
    }
}

/** @brief Convenience: draw a button row from the momentary st.events plane. */
static void drawButtonRow(const ModuleInfo* info, const ModuleState& st,
                          uint8_t n, int y) {
    drawButtonRowMask(info, st.events, n, y);
}

void uiDashboardInputs(const ModuleInfo* info, const ModuleState& st) {
    if (!info) return;
    clearInputArea();

    switch (info->kind) {

        case MK_BUTTON12:
        case MK_BUTTON24:
            drawTextTL(4, _inputAreaY,
                       info->kind == MK_BUTTON24 ? "Buttons + switches" : "Buttons",
                       C_DIM, C_BG);
            drawButtonGrid(info, st);
            break;

        case MK_JOYSTICK: {
            drawTextTL(4, _inputAreaY, "Axes X / Y / Z", C_DIM, C_BG);
            const char* axn[3] = { "X", "Y", "Z" };
            int by = _inputAreaY + 16;              // bar rows: pitch 26
            for (uint8_t a = 0; a < 3; a++) {
                drawTextML(4, by + 8, axn[a], C_TEXT, C_BG, 1);
                // Signed axis mapped to [-1, 1]; bar spans between the axis
                // letter and the numeric readout ("-32768" = 36px + margin).
                float frac = (float)st.axis[a] / 32767.0f;
                drawCenterBar(16, by, UI_W - 16 - 44, 16, frac, C_BAR);
                char nb[8];
                snprintf(nb, sizeof(nb), "%d", (int)st.axis[a]);
                drawTextMR(UI_W - 2, by + 8, nb, C_DIM, C_BG, 1);
                by += 26;
            }
            // Joystick buttons use the PERSISTENT state plane (st.flags bits
            // 0..2) so a held button shows steadily, not just on the edge.
            drawButtonRowMask(info, (uint32_t)st.flags, 3, by + 4);
            break;
        }

        case MK_DISPLAY: {
            drawTextTL(4, _inputAreaY, "Buttons", C_DIM, C_BG);
            if (info->typeId == KMC_TYPE_GPWS_INPUT) {
                // GPWS: buttons 0..2 render from PERSISTENT state in st.flags
                // (bits 1:0 = BTN01 cycle: 0 off, 1 full GPWS -> GREEN,
                //  2 proximity-only -> AMBER; bit2 = BTN02; bit3 = BTN03),
                // matching the board's own LEDs. Chip 3 (encoder button)
                // stays an event blip with no LED colour.
                const int gap = 6, x0 = 4, n = 4;
                const int chipW = (UI_W - 2 * x0 - (n - 1) * gap) / n;
                const int cy = _inputAreaY + 14;
                uint8_t cyc = st.flags & 0x03;
                for (uint8_t i = 0; i < n; i++) {
                    const char* lbl = (info->labels && info->labels[i])
                                          ? info->labels[i] : "btn";
                    bool on;
                    uint16_t fill;
                    if (i == 0)      { on = (cyc != 0);
                                       fill = (cyc == 2) ? C_LED_AMBER : C_LED_GREEN; }
                    else if (i == 1) { on = (st.flags >> 2) & 1u;
                                       fill = inputColor(info, i); }
                    else if (i == 2) { on = (st.flags >> 3) & 1u;
                                       fill = inputColor(info, i); }
                    else             { on = (st.events >> 3) & 1u;
                                       fill = 0; }   // encoder btn — no LED
                    drawChip(x0 + i * (chipW + gap), cy, chipW, 22, lbl, on, fill);
                }
            } else {
                drawButtonRow(info, st, 3, _inputAreaY + 14);
            }
            // Large numeric readout of value ("65535" @ size 3 = 90px).
            char nb[12];
            snprintf(nb, sizeof(nb), "%u", (unsigned)st.value);
            drawTextMC(UI_W / 2, _inputAreaY + 80, nb, C_TEXT, C_BG, 3);
            // Optional flags bits.
            char fb[16];
            snprintf(fb, sizeof(fb), "flags 0x%02X", st.flags);
            drawTextMC(UI_W / 2, _inputAreaY + 110, fb, C_DIM, C_BG, 1);
            break;
        }

        case MK_THROTTLE: {
            drawTextTL(4, _inputAreaY, "Buttons", C_DIM, C_BG);
            drawButtonRow(info, st, 4, _inputAreaY + 14);
            int by = _inputAreaY + 44;
            // Value bar 0..1000 with the number to its right ("1000" = 24px).
            float frac = (float)st.value / 1000.0f;
            drawBar(4, by, UI_W - 4 - 34, 18, frac, C_BAR);
            char nb[8];
            snprintf(nb, sizeof(nb), "%u", (unsigned)st.value);
            drawTextMR(UI_W - 2, by + 9, nb, C_TEXT, C_BG, 1);
            // Flag tags: bit0 enabled, bit1 precision, bit2 touch, bit3 motor.
            const char* tags[4] = { "EN", "PREC", "TCH", "MOT" };
            const int tagW = (UI_W - 8 - 3 * 6) / 4;   // 53, 4 across
            int tx = 4, ty = by + 26;
            for (uint8_t b = 0; b < 4; b++) {
                bool on = (st.flags >> b) & 1u;
                drawChip(tx, ty, tagW, 20, tags[b], on);
                tx += tagW + 6;
            }
            break;
        }

        case MK_DUAL_ENCODER: {
            drawTextTL(4, _inputAreaY, "Buttons", C_DIM, C_BG);
            drawButtonRow(info, st, 2, _inputAreaY + 14);
            // Accumulate signed deltas.
            _encTotal[0] += st.enc[0];
            _encTotal[1] += st.enc[1];
            int by = _inputAreaY + 48;
            char nb[20];                     // <= 19 chars @ size 2 = 228px
            for (uint8_t e = 0; e < 2; e++) {
                snprintf(nb, sizeof(nb), "Enc %u: %ld", (unsigned)e, _encTotal[e]);
                drawTextTL(8, by, nb, C_TEXT, C_BG, 2);
                by += 26;                    // size-2 pitch >= 20
            }
            break;
        }

        default:
            break;
    }
}

UIAction uiDashboardTouch() {
    int16_t x, y;
    if (!readPress(x, y)) return UI_NONE;

    for (uint8_t i = 0; i < CTRL_COUNT; i++) {
        if (_ctrlBtns[i].hit(x, y)) return _ctrlActions[i];
    }
    return UI_NONE;
}

// ============================================================
//  Construction-test screen (generic, driven by ConstructionTest)
//
//  Full-screen step view: header, accent instruction (up to two
//  lines), status body, and control buttons selected by btnMask at
//  the bottom — up to 5 laid out as one row of 3 over one row of 2,
//  each >= 70x34 for finger targets. Fixed order: PASS, FAIL, NEXT,
//  RETRY, ABORT.
// ============================================================
namespace {
struct CtBtnDef { uint8_t mask; CtButton id; const char* label; uint16_t color; };
const CtBtnDef CT_DEFS[CT_MAXBTNS] = {
    { CTB_PASS,  CT_PASS,  "PASS",  C_ON     },
    { CTB_FAIL,  CT_FAIL,  "FAIL",  C_ALERT  },
    { CTB_NEXT,  CT_NEXT,  "NEXT",  C_ACCENT },
    { CTB_RETRY, CT_RETRY, "RETRY", C_WARN   },
    { CTB_ABORT, CT_ABORT, "ABORT", C_OFF    },
};
}

void uiCtRender(const char* header, const char* instruction,
                const char* const* status, uint8_t statusCount, uint8_t btnMask) {
    tft.fillScreen(C_BG);

    // Header line (top), truncated to the 40-char screen width.
    char hdr[41];
    truncStr(hdr, sizeof(hdr), header ? header : "", 39);
    drawTextTL(4, 4, hdr, C_TEXT, C_BG, 1);
    tft.drawFastHLine(0, 16, UI_W, C_PANEL2);

    // Instruction (accent) — up to two wrapped lines of 38 chars
    // (bands 22..29 and 34..41).
    drawTextWrapped2(4, 22, instruction, 38, 12, C_ACCENT, C_BG);

    // Collect the shown buttons first so the body knows its bottom edge.
    uint8_t defIdx[CT_MAXBTNS];
    _ctBtnCount = 0;
    for (uint8_t i = 0; i < CT_MAXBTNS; i++) {
        if (btnMask & CT_DEFS[i].mask) {
            defIdx[_ctBtnCount] = i;
            _ctBtnIds[_ctBtnCount] = CT_DEFS[i].id;
            _ctBtnCount++;
        }
    }
    uint8_t btnRows = (_ctBtnCount > 3) ? 2 : (_ctBtnCount > 0 ? 1 : 0);
    int btnAreaTop = UI_H - btnRows * (CT_BTN_H + CT_BTN_GAP) - 2;

    // Status body — left-aligned size-1 lines (39 chars max, pitch 14).
    int bodyTop = 50;
    int lineH   = 14;
    int maxLines = (btnAreaTop - 4 - bodyTop) / lineH;
    if (maxLines < 0) maxLines = 0;
    char line[41];
    for (uint8_t i = 0; i < statusCount && status && i < (uint8_t)maxLines; i++) {
        truncStr(line, sizeof(line), status[i] ? status[i] : "", 39);
        drawTextTL(6, bodyTop + i * lineH, line, C_TEXT, C_BG, 1);
    }

    // Bottom control buttons: first row holds up to 3, second the rest.
    if (_ctBtnCount > 0) {
        uint8_t row0n = (_ctBtnCount > 3) ? 3 : _ctBtnCount;
        uint8_t row1n = _ctBtnCount - row0n;
        for (uint8_t row = 0; row < btnRows; row++) {
            uint8_t nIn  = (row == 0) ? row0n : row1n;
            uint8_t base = (row == 0) ? 0 : row0n;
            int btnW = (UI_W - CT_BTN_GAP * (nIn + 1)) / nIn;   // 3->74, 2->114
            int y = btnAreaTop + row * (CT_BTN_H + CT_BTN_GAP);
            int x = CT_BTN_GAP;
            for (uint8_t j = 0; j < nIn; j++) {
                uint8_t bi = base + j;
                const CtBtnDef& d = CT_DEFS[defIdx[bi]];
                Rect r = { (int16_t)x, (int16_t)y, (int16_t)btnW, (int16_t)CT_BTN_H };
                _ctBtnRects[bi] = r;
                tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, d.color);
                tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, C_TEXT);
                // Readable label colour against the fill.
                uint16_t txt = (d.color == C_OFF) ? C_TEXT : C_BG;
                drawTextMC(r.x + r.w / 2, r.y + r.h / 2, d.label, txt, d.color, 1);
                x += btnW + CT_BTN_GAP;
            }
        }
    }
}

CtButton uiCtPoll(uint8_t btnMask) {
    int16_t x, y;
    if (!readPress(x, y)) return CT_NONE;

    for (uint8_t i = 0; i < _ctBtnCount; i++) {
        // Respect the caller's current mask as well as what was rendered, so
        // a stale rect for a now-hidden button can't fire.
        bool maskOk = false;
        for (uint8_t d = 0; d < CT_MAXBTNS; d++) {
            if (CT_DEFS[d].id == _ctBtnIds[i]) { maskOk = (btnMask & CT_DEFS[d].mask) != 0; break; }
        }
        if (maskOk && _ctBtnRects[i].hit(x, y)) return _ctBtnIds[i];
    }
    return CT_NONE;
}

// ============================================================
//  Shared widgets
// ============================================================
void uiPowerBar(const PowerReading& p) {
    // Repaint only the second top bar (y 14..27).
    tft.fillRect(0, PWR_BAR_Y, UI_W, PWR_BAR_H, C_PANEL);

    char buf[24];                       // "12.04V 0.25A 3.0W" = 17 chars
    uint16_t fg;
    if (p.ok) {
        snprintf(buf, sizeof(buf), "%.2fV %.2fA %.1fW",
                 (double)p.volts, (double)p.amps, (double)p.watts);
        fg = C_ACCENT;
    } else {
        snprintf(buf, sizeof(buf), "--V --A --W");
        fg = C_DIM;
    }
    drawTextMR(UI_W - 4, PWR_BAR_Y + PWR_BAR_H / 2, buf, fg, C_PANEL, 1);
}

void uiToast(const char* msg) {
    // Status line just above the dashboard control area (band 218..233).
    // Calm SKY-blue informational colour — red/hot hues are reserved for
    // actual failures (FAULT text, FAIL button).
    tft.fillRect(0, TOAST_Y, UI_W, 16, C_BG);
    if (!msg) return;
    char m[41];
    truncStr(m, sizeof(m), msg, 39);
    drawTextML(6, TOAST_Y + 8, m, C_INFO, C_BG, 1);
}

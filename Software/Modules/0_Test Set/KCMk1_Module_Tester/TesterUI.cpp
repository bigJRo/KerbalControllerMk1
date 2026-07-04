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
 *              ER-TFT028A3-4 (ILI9341, 240x320 4-wire SPI), FT6236 capacitive
 *              touch (I2C 0x38). The display is driven in landscape (rotation
 *              1 -> 320x240).
 *
 * @note        Dependencies: Adafruit_GFX + Adafruit_ILI9341 + Adafruit_FT6206
 *              (+ Adafruit_BusIO). LovyanGFX is NOT used — it does not support
 *              the renesas_uno architecture ('HardwareSPI' does not name a
 *              type) and its global `using RGBColor` alias collides with
 *              KerbalModuleCommon's struct RGBColor.
 *
 *              The ILI9341 uses hardware SPI on the XIAO's fixed SPI pins
 *              (SCK/MISO/MOSI = D8/D9/D10) with CS/DC from TesterConfig.h and
 *              no reset line (software reset). The FT6236 shares Wire (begun
 *              by hwBegin(); do not call Wire.begin() here).
 *
 *              TOUCH MAPPING: the FT6236 reports native portrait coordinates
 *              (p.x 0..239, p.y 0..319). rawTouch() maps them to rotation(1)
 *              landscape as sx = p.y, sy = 239 - p.x. FT6236 panels vary in
 *              origin — if taps land mirrored on hardware, flip the mapping
 *              in rawTouch() (try sx = 319 - p.y and/or sy = p.x) and
 *              re-flash. rawTouch() is the single place the mapping lives.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */

#include <Arduino.h>

#include "TesterConfig.h"
#include "TesterUI.h"            // pulls in ModuleCatalog.h / TesterHW.h
#include <KerbalModuleCommon.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>

// ============================================================
//  Devices — ILI9341 on hardware SPI (fixed XIAO SPI pins), no
//  panel RST (software reset). FT6236 speaks the FT6206 protocol.
// ============================================================
static Adafruit_ILI9341 tft(PIN_TFT_CS, PIN_TFT_DC);
static Adafruit_FT6206  ctp;

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

// ============================================================
//  Layout constants (landscape 320x240)
// ============================================================
static constexpr int TOPBAR_H   = 22;          // top status bar height
static constexpr int PWR_X      = 150;         // x where the power readout starts
static constexpr int HDR_Y      = TOPBAR_H + 2;
static constexpr int HDR_H      = 20;          // dashboard header line height

// Scan list
static constexpr int SCAN_ROW_H = 34;
static constexpr int SCAN_TOP   = TOPBAR_H + 6;
static constexpr int SCAN_MAXROWS = 5;         // fits above the Rescan button row

// Dashboard control buttons.
// Nine buttons — laid out as two rows (5 on the top row, 4 on the bottom
// row) so each chip stays comfortably touchable on a 320px-wide panel.
static constexpr int CTRL_COUNT  = 9;
static constexpr int CTRL_PERROW = 5;
static constexpr int CTRL_ROWS   = 2;
static constexpr int CTRL_H      = 28;                          // per-button height
static constexpr int CTRL_AREA_H = CTRL_ROWS * CTRL_H + 2;      // 58
static constexpr int CTRL_AREA_Y = UI_H - CTRL_AREA_H;          // 182
static constexpr int CTRL_W      = UI_W / CTRL_PERROW;          // 64
static constexpr int TOAST_Y     = CTRL_AREA_Y - 16;           // toast line above buttons

// Construction-test screen control buttons (bottom row, up to 5).
static constexpr int CT_BTN_H    = 36;
static constexpr int CT_BTN_Y    = UI_H - CT_BTN_H;
static constexpr int CT_MAXBTNS  = 5;

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

// Current input area geometry (set by uiDashboardBegin per kind).
static int _inputAreaY = HDR_Y + HDR_H + 2;
static int _inputAreaH = TOAST_Y - (HDR_Y + HDR_H + 2);

// ============================================================
//  Text helpers (Adafruit_GFX classic 5x7 font)
//
//  Adafruit_GFX has no drawString/textWidth/datum support, so text is
//  placed with setCursor + print. Width is approximated as
//  strlen * 6 * size (each glyph cell is 6 px wide incl. spacing) and
//  glyph height as 8 * size — exact for the built-in classic font.
// ============================================================

/** @brief Rendered width of a string at a given text size (classic font). */
static int txtW(const char* s, uint8_t size) {
    return (int)strlen(s) * 6 * size;
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

// ============================================================
//  Touch helpers
// ============================================================

/**
 * @brief  Raw touch read mapped to landscape (rotation 1) coordinates.
 *
 *         The FT6236 reports native portrait coordinates (p.x 0..239,
 *         p.y 0..319). Map portrait -> rotation(1) landscape 320x240:
 *         sx = p.y, sy = 239 - p.x.
 *
 *         NOTE: FT6236 panels vary in origin; if taps are mirrored on
 *         hardware, try sx = 319 - p.y and/or sy = p.x. This is the single
 *         place the mapping lives.
 */
static bool rawTouch(int16_t& sx, int16_t& sy) {
    if (!ctp.touched()) return false;
    TS_Point p = ctp.getPoint();
    sx = p.y;            // 0..319
    sy = 239 - p.x;      // 0..239
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

/** @brief Draw a labelled "chip" (button/state indicator). */
static void drawChip(int x, int y, int w, int h, const char* label, bool on) {
    uint16_t fill = on ? C_ON : C_OFF;
    uint16_t txt  = on ? C_BG : C_DIM;
    tft.fillRoundRect(x, y, w, h, 4, fill);
    drawTextMC(x + w / 2, y + h / 2, label, txt, fill, 1);
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

// Cached top-bar title so uiPowerBar can leave the left side untouched.
static char _title[24] = "";

/** @brief Draw the left side (title) of the top bar. */
static void drawTopBarTitle(const char* title) {
    strncpy(_title, title, sizeof(_title) - 1);
    _title[sizeof(_title) - 1] = '\0';
    tft.fillRect(0, 0, PWR_X, TOPBAR_H, C_PANEL);
    drawTextML(4, TOPBAR_H / 2, _title, C_TEXT, C_PANEL, 1);
}

// ============================================================
//  Lifecycle
// ============================================================
void uiBegin() {
    pinMode(PIN_BACKLITE, OUTPUT);
    digitalWrite(PIN_BACKLITE, HIGH);   // backlight on

    tft.begin();                        // hardware SPI, library default speed
    tft.setRotation(1);                 // landscape -> 320x240

    // FT6236 on the shared Wire bus (already begun by hwBegin()).
    ctp.begin(40);                      // touch threshold 40

    // NOTE: confirm on first flash that rawTouch() lands taps where they
    // were made in landscape (x 0..319, y 0..239). See file-header @note.

    tft.setTextSize(1);
    tft.setTextWrap(false);
    tft.fillScreen(C_BG);

    // Calibrate any initial touch state so the first poll is not a stale press.
    _wasTouched = false;
}

void uiSplash() {
    tft.fillScreen(C_BG);
    drawTextMC(UI_W / 2, UI_H / 2 - 24, "KCMk1 Module Tester", C_TEXT, C_BG, 2);
    drawTextMC(UI_W / 2, UI_H / 2 + 4,  "v" TESTER_VERSION_STR, C_ACCENT, C_BG, 1);
    drawTextMC(UI_W / 2, UI_H / 2 + 24, "Jeb's Controller Works", C_DIM, C_BG, 1);
}

// ============================================================
//  Scan / select screen
// ============================================================
void uiScanBegin() {
    tft.fillScreen(C_BG);
    drawTopBarTitle("Select Module");

    // Power area placeholder until the first uiPowerBar() call.
    tft.fillRect(PWR_X, 0, UI_W - PWR_X, TOPBAR_H, C_PANEL);

    // Rescan button (bottom-right, ~80x32).
    _rescanBtn = { (int16_t)(UI_W - 84), (int16_t)(UI_H - 36), 80, 32 };
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
        drawTextMC(UI_W / 2, SCAN_TOP + 50,
                   "No modules found -- connect a module", C_DIM, C_BG, 1);
        return;
    }

    uint8_t rows = count;
    if (rows > SCAN_MAXROWS) rows = SCAN_MAXROWS;
    _scanRowCount = rows;

    char buf[40];
    for (uint8_t i = 0; i < rows; i++) {
        int y = SCAN_TOP + i * SCAN_ROW_H;
        Rect r = { 4, (int16_t)y, (int16_t)(UI_W - 8), (int16_t)(SCAN_ROW_H - 4) };
        _scanRows[i] = r;

        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, C_PANEL);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, C_PANEL2);

        const char* name = (infos[i] != nullptr) ? infos[i]->name : "Unknown";
        drawTextML(r.x + 8, r.y + r.h / 2, name, C_TEXT, C_PANEL, 1);

        snprintf(buf, sizeof(buf), "0x%02X  type 0x%02X", addrs[i], typeIds[i]);
        drawTextMR(r.x + r.w - 8, r.y + r.h / 2, buf, C_DIM, C_PANEL, 1);
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
void uiDashboardBegin(const ModuleInfo* info, uint8_t addr) {
    tft.fillScreen(C_BG);

    // Top bar: module name + address.
    char t[24];
    snprintf(t, sizeof(t), "%s 0x%02X",
             (info && info->name) ? info->name : "Module", addr);
    drawTopBarTitle(t);
    tft.fillRect(PWR_X, 0, UI_W - PWR_X, TOPBAR_H, C_PANEL);

    // Reset encoder running totals.
    _encTotal[0] = 0;
    _encTotal[1] = 0;

    // Control button grid (two rows: 5 + 4).
    for (uint8_t i = 0; i < CTRL_COUNT; i++) {
        int row = i / CTRL_PERROW;
        int col = i % CTRL_PERROW;
        int x = col * CTRL_W;
        int y = CTRL_AREA_Y + row * CTRL_H;
        Rect r = { (int16_t)x, (int16_t)y, (int16_t)(CTRL_W - 2), (int16_t)(CTRL_H - 2) };
        _ctrlBtns[i] = r;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 3, C_PANEL2);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 3, C_DIM);
        drawTextMC(r.x + r.w / 2, r.y + r.h / 2, _ctrlLabels[i], C_TEXT, C_PANEL2, 1);
    }

    // Static labels for the input area depending on kind.
    _inputAreaY = HDR_Y + HDR_H + 2;
    _inputAreaH = TOAST_Y - _inputAreaY;
    clearInputArea();

    if (!info) return;
    switch (info->kind) {
        case MK_BUTTON12:    drawTextTL(4, _inputAreaY, "Buttons (12)", C_DIM, C_BG); break;
        case MK_BUTTON24:    drawTextTL(4, _inputAreaY, "Buttons + switches (24)", C_DIM, C_BG); break;
        case MK_JOYSTICK:    drawTextTL(4, _inputAreaY, "Axes X / Y / Z + buttons", C_DIM, C_BG); break;
        case MK_DISPLAY:     drawTextTL(4, _inputAreaY, "Buttons + value", C_DIM, C_BG); break;
        case MK_THROTTLE:    drawTextTL(4, _inputAreaY, "Throttle: buttons + value + flags", C_DIM, C_BG); break;
        case MK_DUAL_ENCODER:drawTextTL(4, _inputAreaY, "Encoders + buttons", C_DIM, C_BG); break;
        default: break;
    }
}

void uiDashboardHeader(const ModuleState& st) {
    // Repaint the header line under the top bar.
    tft.fillRect(0, HDR_Y, UI_W, HDR_H, C_BG);

    int x = 4;
    const char* lc = lifecycleText(st.lifecycle);
    drawTextML(x, HDR_Y + HDR_H / 2, lc, C_TEXT, C_BG, 1);
    x += txtW(lc, 1) + 12;

    if (st.fault) {
        drawTextML(x, HDR_Y + HDR_H / 2, "FAULT", C_ALERT, C_BG, 1);
        x += txtW("FAULT", 1) + 12;
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "tx:%02u", (unsigned)st.txCounter);
    drawTextMR(UI_W - 4, HDR_Y + HDR_H / 2, buf, C_DIM, C_BG, 1);
}

// ------------------------------------------------------------
//  Per-kind input renderers
// ------------------------------------------------------------

/** @brief Draw a grid of button chips for indices with non-empty labels. */
static void drawButtonGrid(const ModuleInfo* info, const ModuleState& st) {
    const int cols = 6;
    const int chipW = (UI_W - 8) / cols - 4;
    const int chipH = 22;
    const int gx = 4, gyTop = _inputAreaY + 16;
    const int gap = 6;

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
        drawChip(x, y, chipW, chipH, lbl, on);
    }
}

/**
 * @brief Draw N button chips in a single row from labels[0..n-1].
 * @param plane  bitfield giving the on/off state for each chip (bit i = chip i).
 *               Callers pass st.events for momentary state, or st.flags for
 *               persistent (held) button state (e.g. joystick buttons).
 */
static void drawButtonRowMask(const ModuleInfo* info, uint32_t plane,
                              uint8_t n, int y) {
    const int chipW = 70;
    const int chipH = 22;
    const int gap = 8;
    int x = 4;
    for (uint8_t i = 0; i < n; i++) {
        const char* lbl = (info->labels && i < info->inputCount && info->labels[i])
                              ? info->labels[i] : "btn";
        bool on = (plane >> i) & 1u;
        drawChip(x, y, chipW, chipH, lbl, on);
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
            int by = _inputAreaY + 18;
            for (uint8_t a = 0; a < 3; a++) {
                drawTextML(4, by + 8, axn[a], C_TEXT, C_BG, 1);
                // Signed axis: map int16 range to [-1, 1].
                float frac = (float)st.axis[a] / 32767.0f;
                drawCenterBar(24, by, UI_W - 60, 16, frac, C_BAR);
                char nb[8];
                snprintf(nb, sizeof(nb), "%d", (int)st.axis[a]);
                drawTextMR(UI_W - 4, by + 8, nb, C_DIM, C_BG, 1);
                by += 22;
            }
            // Joystick buttons use the PERSISTENT state plane (st.flags bits
            // 0..2) so a held button shows steadily, not just on the edge.
            drawButtonRowMask(info, (uint32_t)st.flags, 3, by + 2);
            break;
        }

        case MK_DISPLAY: {
            drawTextTL(4, _inputAreaY, "Buttons", C_DIM, C_BG);
            drawButtonRow(info, st, 3, _inputAreaY + 16);
            // Large numeric readout of value.
            char nb[12];
            snprintf(nb, sizeof(nb), "%u", (unsigned)st.value);
            drawTextMC(UI_W / 2, _inputAreaY + 78, nb, C_TEXT, C_BG, 3);
            // Optional flags bits.
            char fb[20];
            snprintf(fb, sizeof(fb), "flags 0x%02X", st.flags);
            drawTextMC(UI_W / 2, _inputAreaY + 108, fb, C_DIM, C_BG, 1);
            break;
        }

        case MK_THROTTLE: {
            drawTextTL(4, _inputAreaY, "Buttons", C_DIM, C_BG);
            drawButtonRow(info, st, 4, _inputAreaY + 16);
            int by = _inputAreaY + 46;
            // Value bar 0..1000.
            float frac = (float)st.value / 1000.0f;
            drawBar(4, by, UI_W - 70, 18, frac, C_BAR);
            char nb[8];
            snprintf(nb, sizeof(nb), "%u", (unsigned)st.value);
            drawTextMR(UI_W - 4, by + 9, nb, C_TEXT, C_BG, 1);
            // Flag tags: bit0 enabled, bit1 precision, bit2 touch, bit3 motor.
            const char* tags[4] = { "EN", "PREC", "TCH", "MOT" };
            int tx = 4, ty = by + 26;
            for (uint8_t b = 0; b < 4; b++) {
                bool on = (st.flags >> b) & 1u;
                drawChip(tx, ty, 50, 20, tags[b], on);
                tx += 56;
            }
            break;
        }

        case MK_DUAL_ENCODER: {
            drawTextTL(4, _inputAreaY, "Buttons", C_DIM, C_BG);
            drawButtonRow(info, st, 2, _inputAreaY + 16);
            // Accumulate signed deltas.
            _encTotal[0] += st.enc[0];
            _encTotal[1] += st.enc[1];
            int by = _inputAreaY + 50;
            char nb[24];
            for (uint8_t e = 0; e < 2; e++) {
                snprintf(nb, sizeof(nb), "Enc %u: %ld", (unsigned)e, _encTotal[e]);
                drawTextTL(8, by, nb, C_TEXT, C_BG, 2);
                by += 30;
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
//  Full-screen step view: header, accent instruction, status body, and a
//  bottom row of control buttons selected by btnMask. Buttons are laid out
//  in a fixed order (PASS, FAIL, NEXT, RETRY, ABORT), evenly spread across
//  whatever subset is shown.
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

    // Header line (top).
    drawTextTL(4, 4, header ? header : "", C_TEXT, C_BG, 1);
    tft.drawFastHLine(0, 18, UI_W, C_PANEL2);

    // Instruction line (accent).
    if (instruction) {
        drawTextTL(4, 24, instruction, C_ACCENT, C_BG, 1);
    }

    // Status body — left-aligned lines.
    int bodyTop = 42;
    int lineH   = 16;
    int maxLines = (CT_BTN_Y - 4 - bodyTop) / lineH;
    if (maxLines < 0) maxLines = 0;
    for (uint8_t i = 0; i < statusCount && status && i < (uint8_t)maxLines; i++) {
        const char* s = status[i] ? status[i] : "";
        drawTextTL(6, bodyTop + i * lineH, s, C_TEXT, C_BG, 1);
    }

    // Bottom control buttons — only those whose mask bit is set, laid out
    // evenly across the full width.
    _ctBtnCount = 0;
    for (uint8_t i = 0; i < CT_MAXBTNS; i++) {
        if (btnMask & CT_DEFS[i].mask) {
            _ctBtnIds[_ctBtnCount] = CT_DEFS[i].id;
            // Remember which def index this is for drawing below.
            _ctBtnRects[_ctBtnCount].x = (int16_t)i;   // temp stash def index
            _ctBtnCount++;
        }
    }

    if (_ctBtnCount > 0) {
        int gap = 6;
        int totalGap = gap * (_ctBtnCount + 1);
        int btnW = (UI_W - totalGap) / _ctBtnCount;
        int x = gap;
        for (uint8_t i = 0; i < _ctBtnCount; i++) {
            uint8_t defIdx = (uint8_t)_ctBtnRects[i].x;   // recover stashed index
            const CtBtnDef& d = CT_DEFS[defIdx];
            Rect r = { (int16_t)x, (int16_t)CT_BTN_Y, (int16_t)btnW, (int16_t)(CT_BTN_H - 2) };
            _ctBtnRects[i] = r;
            tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, d.color);
            tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, C_TEXT);
            // Choose readable label colour against the fill.
            uint16_t txt = (d.color == C_OFF) ? C_TEXT : C_BG;
            drawTextMC(r.x + r.w / 2, r.y + r.h / 2, d.label, txt, d.color, 1);
            x += btnW + gap;
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
    // Repaint only the right portion of the top bar.
    tft.fillRect(PWR_X, 0, UI_W - PWR_X, TOPBAR_H, C_PANEL);

    char buf[28];
    uint16_t fg;
    if (p.ok) {
        snprintf(buf, sizeof(buf), "%.2fV  %.2fA  %.1fW",
                 (double)p.volts, (double)p.amps, (double)p.watts);
        fg = C_ACCENT;
    } else {
        snprintf(buf, sizeof(buf), "--V  --A  --W");
        fg = C_DIM;
    }
    drawTextMR(UI_W - 4, TOPBAR_H / 2, buf, fg, C_PANEL, 1);
}

void uiToast(const char* msg) {
    // Status line just above the control buttons, in accent colour.
    tft.fillRect(0, TOAST_Y, UI_W, 16, C_BG);
    if (!msg) return;
    drawTextML(6, TOAST_Y + 8, msg, C_ACCENT, C_BG, 1);
}

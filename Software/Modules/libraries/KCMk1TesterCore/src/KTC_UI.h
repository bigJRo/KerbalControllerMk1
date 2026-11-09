/**
 * @file        KTC_UI.h
 * @version     1.0.0
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Touchscreen layer: ST7789V 240x320 portrait (Adafruit_ST7789)
 *              and FT6236 touch (Adafruit_FT6206). Fixed screen regions and
 *              small widgets the runner and the test steps draw with.
 *
 *              Screen regions (portrait 240 x 320):
 *                  0..13   title bar       module name | step n/N
 *                 14..27   power bar       address/firmware | V A W
 *                 32..47   heading         step name, size 2
 *                 52..87   instruction     up to 3 wrapped lines
 *                 92..259  live area       14 text rows of 12 px, or widgets
 *                262..275  toast line      one-line messages
 *                280..317  button row      up to 5 touch buttons
 *
 *              Text uses the Adafruit_GFX classic font: 6 x 8 px per glyph at
 *              size 1 (40 characters across), 12 x 16 at size 2.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#pragma once
#include <Arduino.h>
#include <KerbalModuleCommon.h>
#include "KTC_Bus.h"

// ------------------------------------------------------------
//  Geometry
// ------------------------------------------------------------
#define KTC_W            240
#define KTC_H            320
#define KTC_HEAD_Y       32
#define KTC_INSTR_Y      52
#define KTC_LIVE_Y       92
#define KTC_LIVE_H       168
#define KTC_ROW_H        12
#define KTC_LIVE_ROWS    14
#define KTC_TOAST_Y      262
#define KTC_BTN_Y        280
#define KTC_BTN_H        36

// ------------------------------------------------------------
//  Colours (RGB565)
// ------------------------------------------------------------
#define KTC_C_BG      0x0000
#define KTC_C_PANEL   0x18E3
#define KTC_C_PANEL2  0x2104
#define KTC_C_TEXT    0xFFFF
#define KTC_C_DIM     0x9CD3
#define KTC_C_ACCENT  0x05BF
#define KTC_C_PASS    0x07E0
#define KTC_C_FAIL    0xF800
#define KTC_C_WARN    0xFD20
#define KTC_C_OFF     0x4208
#define KTC_C_BAR     0x047F
#define KTC_C_NAV     0x435C

// ------------------------------------------------------------
//  Touch buttons (bit masks; one row, drawn in this order)
// ------------------------------------------------------------
#define KTC_BTN_START   0x0001
#define KTC_BTN_PASS    0x0002
#define KTC_BTN_FAIL    0x0004
#define KTC_BTN_NEXT    0x0008
#define KTC_BTN_RETRY   0x0010
#define KTC_BTN_SKIP    0x0020
#define KTC_BTN_ABORT   0x0040
#define KTC_BTN_RUNALL  0x0080
#define KTC_BTN_LIVE    0x0100
#define KTC_BTN_MENU    0x0200
#define KTC_BTN_BACK    0x0400
#define KTC_BTN_ENABLE  0x0800
#define KTC_BTN_BULB    0x1000
#define KTC_BTN_ZERO    0x2000

/** @brief RGB565 of a palette colour. */
uint16_t ktc565(const RGBColor& c);
/** @brief RGB565 of an LED colour scaled up so dim colours (BACKLIT) stay visible on screen. */
uint16_t ktcLed565(const RGBColor& c);

void ktcUiBegin();

/** @brief Clear everything below the two top bars. */
void ktcUiClearBody();
/** @brief Title bar: left text and right text (e.g. "3/12"). */
void ktcUiTitle(const char* left, const char* right);
/** @brief Power bar: left text (address / fw / lifecycle) and the live V A W reading. */
void ktcUiPowerBar(const char* left, const KtcPower& p);
/** @brief Size-2 heading line. */
void ktcUiHeading(const char* s, uint16_t color = KTC_C_TEXT);
/** @brief Instruction block, wrapped to 38 chars, up to 3 lines. */
void ktcUiInstr(const char* s);
/** @brief Clear the live area. */
void ktcUiLiveClear();

/** @brief One text row in the live area (row 0..13). Clears the row first. */
void ktcUiRow(uint8_t row, const char* s, uint16_t color = KTC_C_TEXT);
/** @brief printf into a live-area row. */
void ktcUiRowf(uint8_t row, uint16_t color, const char* fmt, ...);
/** @brief Text at an absolute position (size 1 or 2), background filled. */
void ktcUiText(int x, int y, const char* s, uint16_t fg, uint8_t size = 1, uint16_t bg = KTC_C_BG);
/** @brief Centered text at (cx, cy). */
void ktcUiTextC(int cx, int cy, const char* s, uint16_t fg, uint8_t size = 1, uint16_t bg = KTC_C_BG);

/** @brief Filled rectangle (raw). */
void ktcUiFill(int x, int y, int w, int h, uint16_t color);
/** @brief Labelled chip: rounded box, label centered; outline drawn if outline != 0. */
void ktcUiChip(int x, int y, int w, int h, const char* label, uint16_t fill, uint16_t outline = 0);

/**
 * @brief Grid chip in the live area. Chips are laid out `cols` per row from
 *        live-area y offset `top`, 22 px high on a 26 px pitch.
 */
void ktcUiGridChip(uint8_t idx, uint8_t cols, int top, const char* label,
                   uint16_t fill, uint16_t outline = 0);

/** @brief Horizontal bar 0..1 (centered = signed -1..1 about the middle). */
void ktcUiBar(int y, const char* label, float frac, bool centered, const char* valueText,
              uint16_t fill = KTC_C_BAR);

/** @brief Colour swatch with a caption below it (LED walk / state previews). */
void ktcUiSwatch(int x, int y, int w, int h, uint16_t color565, const char* caption);

/** @brief Draw the bottom button row for the given mask (0 clears it). */
void ktcUiButtons(uint16_t mask);
/** @brief Press-edge poll of the bottom button row. Returns one KTC_BTN_* bit or 0. */
uint16_t ktcUiPollButtons();

/**
 * @brief  Menu list: rows of 20 px from y=32 to 275. Each row shows a label
 *         and a right-hand mark (e.g. "PASS"), coloured by markColor.
 */
void ktcUiMenuRow(uint8_t row, const char* label, const char* mark, uint16_t markColor);
/** @brief Press-edge poll: returns the touched menu row, or -1. Also reports a
 *         bottom-row button through btnOut. */
int ktcUiPollMenu(uint8_t rows, uint16_t& btnOut);

/** @brief One-line message just above the button row (y 262..275). */
void ktcUiToast(const char* msg, uint16_t color = KTC_C_ACCENT);

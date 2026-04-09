/**
 * @file        LEDs.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Indicator Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       NeoPixel LED control for the Indicator Module.
 *
 *              16 SK6812mini-012 RGB LEDs driven from NEOPIX_CMD (PA5).
 *              The controller sends CMD_SET_LED_STATE with a standard
 *              8-byte nibble-packed payload (1 nibble per pixel, 2 per
 *              byte). Each nibble is a KBC LED state value (0x0-0x6).
 *              The module maps each state to a per-pixel color.
 *
 *              State-to-color mapping:
 *                OFF           (0x0) — unlit
 *                ENABLED       (0x1) — dim white (all pixels)
 *                ACTIVE        (0x2) — per-pixel active color (see sketch)
 *                WARNING       (0x3) — amber, 500ms/500ms flash
 *                ALERT         (0x4) — red, 150ms/150ms flash
 *                ARMED         (0x5) — cyan, static
 *                PARTIAL_DEPLOY(0x6) — amber, static
 *
 *              All flash timing is handled on the module. The controller
 *              only sends state values — it has no knowledge of timing.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "Config.h"

// ============================================================
//  RGB color struct
// ============================================================

struct RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// ============================================================
//  System color palette — RGB format for SK6812mini-012
// ============================================================

static const RGBColor IND_OFF           = {  0,   0,   0};
static const RGBColor IND_WHITE_DIM     = { 32,  32,  32};  // ENABLED state
static const RGBColor IND_AMBER         = {245, 158,  11};  // WARNING / PARTIAL_DEPLOY
static const RGBColor IND_RED           = {239,  68,  68};  // ALERT
static const RGBColor IND_CYAN          = {  6, 182, 212};  // ARMED

// ============================================================
//  Per-pixel active colors (state 0x2 = ACTIVE)
//  Index matches pixel number 0-15.
// ============================================================

static const RGBColor IND_ACTIVE_COLORS[16] = {
    { 14, 165, 233},  //  0: COMM ACTIVE    — SKY
    { 20, 184, 166},  //  1: USB ACTIVE     — TEAL
    { 34, 197,  94},  //  2: THRTL ENA      — GREEN
    {163, 230,  53},  //  3: AUTO THRTL     — CHARTREUSE
    {245, 158,  11},  //  4: PREC INPUT     — AMBER
    {139,  92, 246},  //  5: AUDIO          — PURPLE
    {234, 179,   8},  //  6: LIGHT ENA      — YELLOW
    {239,  68,  68},  //  7: BRAKE LOCK     — RED
    { 34, 197,  94},  //  8: LNDG GEAR LOCK — GREEN
    {245, 158,  11},  //  9: CHUTE ARM      — AMBER
    { 34, 197,  94},  // 10: CTRL           — GREEN
    {255,   0, 255},  // 11: DEBUG          — MAGENTA
    { 99, 102, 241},  // 12: DEMO           — INDIGO
    {239,  68,  68},  // 13: SWITCH ERROR   — RED
    {150, 255, 216},  // 14: RCS            — MINT
    { 34, 197,  94},  // 15: SAS            — GREEN
};

// ============================================================
//  Public interface
// ============================================================

/**
 * @brief Initialise NeoPixel chain and set all pixels to OFF.
 */
void ledsBegin();

/**
 * @brief Update flash states and push to NeoPixel hardware.
 *        Call every loop iteration for smooth flash timing.
 */
void ledsUpdate();

/**
 * @brief Set the LED state for all 16 pixels from the
 *        8-byte nibble-packed CMD_SET_LED_STATE payload.
 * @param payload  8-byte array from I2C write transaction.
 */
void ledsSetFromPayload(const uint8_t* payload);

/**
 * @brief Set a single pixel state.
 * @param index  Pixel index 0-15.
 * @param state  KBC LED state nibble (0x0-0x6).
 */
void ledsSetState(uint8_t index, uint8_t state);

/**
 * @brief Set all pixels to OFF and render.
 */
void ledsClearAll();

/**
 * @brief Run bulb test — all pixels full white for durationMs.
 * @param durationMs  Test duration in milliseconds.
 */
void ledsBulbTest(uint16_t durationMs);

/**
 * @brief Set all pixels to ENABLED dim white and render.
 */
void ledsSetEnabled();

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
#include <KerbalModuleCommon.h>

// ============================================================
//  System color palette and per-pixel active colors
//
//  System colors reference KMC_* canonical names from
//  KerbalModuleCommon. IND_WHITE_DIM is module-specific —
//  the ENABLED dim state for RGB hardware (no W channel).
// ============================================================

static const RGBColor IND_OFF           = KMC_OFF;
static const RGBColor IND_WHITE_DIM     = scaleColor(KMC_WHITE_COOL, 32); // ENABLED state
static const RGBColor IND_AMBER         = KMC_AMBER;   // WARNING / PARTIAL_DEPLOY
static const RGBColor IND_RED           = KMC_RED;     // ALERT
static const RGBColor IND_CYAN          = KMC_CYAN;    // ARMED

// ============================================================
//  Per-pixel active colors (state 0x2 = ACTIVE)
//  Index matches pixel number 0-15.
// ============================================================

static const RGBColor IND_ACTIVE_COLORS[16] = {
    KMC_SKY,        //  0: COMM ACTIVE    — SKY
    KMC_TEAL,       //  1: USB ACTIVE     — TEAL
    KMC_GREEN,      //  2: THRTL ENA      — GREEN
    KMC_CHARTREUSE, //  3: AUTO THRTL     — CHARTREUSE
    KMC_AMBER,      //  4: PREC INPUT     — AMBER
    KMC_PURPLE,     //  5: AUDIO          — PURPLE
    KMC_YELLOW,     //  6: LIGHT ENA      — YELLOW
    KMC_RED,        //  7: BRAKE LOCK     — RED
    KMC_GREEN,      //  8: LNDG GEAR LOCK — GREEN
    KMC_AMBER,      //  9: CHUTE ARM      — AMBER
    KMC_GREEN,      // 10: CTRL           — GREEN
    KMC_MAGENTA,    // 11: DEBUG          — MAGENTA
    KMC_INDIGO,     // 12: DEMO           — INDIGO
    KMC_RED,        // 13: SWITCH ERROR   — RED
    KMC_MINT,       // 14: RCS            — MINT (was {150,255,216}; corrected to KMC_MINT)
    KMC_GREEN,      // 15: SAS            — GREEN
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

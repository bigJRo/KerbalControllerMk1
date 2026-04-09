/**
 * @file        KJC_Colors.h
 * @version     2.0.0
 * @date        2026-04-09
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Color aliases for the KerbalJoystickCore library.
 *
 *              The canonical color palette, RGBColor struct, scaleColor(),
 *              and LED nibble pack helpers are defined in KerbalModuleCommon.
 *              This file includes that library and provides KJC_* aliases
 *              so existing joystick module sketches require no changes.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the KerbalJoystickCore (KJC) library.
 *              Depends on: KerbalModuleCommon
 */

#pragma once
#include <KerbalModuleCommon.h>

// ============================================================
//  KJC_* aliases → KMC_* canonical names
//
//  These allow all existing KJC library code and module sketches
//  to continue compiling without modification.
// ============================================================

// --- Utility ---
static const RGBColor KJC_OFF           = KMC_OFF;
static const RGBColor KJC_WHITE_COOL    = KMC_WHITE_COOL;

// --- Semantic ---
static const RGBColor KJC_GREEN         = KMC_GREEN;
static const RGBColor KJC_RED           = KMC_RED;
static const RGBColor KJC_AMBER         = KMC_AMBER;

// --- Warm family ---
static const RGBColor KJC_YELLOW        = KMC_YELLOW;
static const RGBColor KJC_GOLD          = KMC_GOLD;
static const RGBColor KJC_ORANGE        = KMC_ORANGE;

// --- Green family ---
static const RGBColor KJC_CHARTREUSE    = KMC_CHARTREUSE;
static const RGBColor KJC_LIME          = KMC_LIME;
static const RGBColor KJC_MINT          = KMC_MINT;

// --- Blue / cyan family ---
static const RGBColor KJC_BLUE          = KMC_BLUE;
static const RGBColor KJC_SKY           = KMC_SKY;
static const RGBColor KJC_TEAL          = KMC_TEAL;
static const RGBColor KJC_CYAN          = KMC_CYAN;

// --- Purple family ---
static const RGBColor KJC_PURPLE        = KMC_PURPLE;
static const RGBColor KJC_INDIGO        = KMC_INDIGO;

// --- Pink / red family ---
static const RGBColor KJC_CORAL         = KMC_CORAL;
static const RGBColor KJC_ROSE          = KMC_ROSE;
static const RGBColor KJC_PINK          = KMC_PINK;
static const RGBColor KJC_MAGENTA       = KMC_MAGENTA;

// --- Whites ---
static const RGBColor KJC_WHITE_WARM    = KMC_WHITE_WARM;
static const RGBColor KJC_WHITE_SOFT    = KMC_WHITE_SOFT;

// ============================================================
//  kjcScaleColor() — backward-compatible alias
// ============================================================

/** @brief Scale an RGBColor by a brightness factor. Alias for scaleColor(). */
inline RGBColor kjcScaleColor(RGBColor c, uint8_t brightness) {
    return scaleColor(c, brightness);
}

// ============================================================
//  kjcPackGet / kjcPackSet — backward-compatible aliases
// ============================================================

/** @brief Get LED state nibble from payload. Alias for kmcLedPackGet(). */
inline uint8_t kjcPackGet(const uint8_t* payload, uint8_t btn) {
    return kmcLedPackGet(payload, btn);
}

/** @brief Set LED state nibble in payload. Alias for kmcLedPackSet(). */
inline void kjcPackSet(uint8_t* payload, uint8_t btn, uint8_t state) {
    kmcLedPackSet(payload, btn, state);
}

// ============================================================
//  KJC_COLOR() macro — backward-compatible alias
// ============================================================

#define KJC_COLOR(r, g, b)  (RGBColor{ (r), (g), (b) })

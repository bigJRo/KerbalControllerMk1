/**
 * @file        KBC_Colors.h
 * @version     2.0.0
 * @date        2026-04-09
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Color aliases for the KerbalButtonCore library.
 *
 *              The canonical color palette, RGBColor/GRBWColor structs,
 *              scaleColor(), and LED nibble pack helpers are defined in
 *              KerbalModuleCommon. This file includes that library and
 *              provides KBC_* aliases so existing module sketches and
 *              library code require no changes.
 *
 *              To reference a color in a KBC module sketch, use either:
 *                KBC_GREEN       — KBC-prefixed alias (existing code)
 *                KMC_GREEN       — canonical name (new code)
 *              Both refer to the same constant.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the KerbalButtonCore (KBC) library.
 *              Depends on: KerbalModuleCommon
 */

#ifndef KBC_COLORS_H
#define KBC_COLORS_H

#include <KerbalModuleCommon.h>

// ============================================================
//  KBC_* aliases → KMC_* canonical names
//
//  These allow all existing KBC library code and module sketches
//  to continue compiling without modification.
// ============================================================

// --- Utility ---
static const RGBColor KBC_OFF           = KMC_OFF;
static const RGBColor KBC_DISCRETE_ON   = KMC_DISCRETE_ON;

// --- Semantic ---
static const RGBColor KBC_GREEN         = KMC_GREEN;
static const RGBColor KBC_RED           = KMC_RED;
static const RGBColor KBC_AMBER         = KMC_AMBER;

// --- Extended LED state color (ARMED) ---
/** @brief ARMED extended state color. Alias for KMC_CYAN. */
static const RGBColor KBC_STATE_ARMED   = KMC_CYAN;

// --- Whites ---
static const RGBColor KBC_WHITE_COOL    = KMC_WHITE_COOL;
static const RGBColor KBC_WHITE_NEUTRAL = KMC_WHITE_NEUTRAL;
static const RGBColor KBC_WHITE_WARM    = KMC_WHITE_WARM;
static const RGBColor KBC_WHITE_SOFT    = KMC_WHITE_SOFT;

// --- Warm family ---
static const RGBColor KBC_ORANGE        = KMC_ORANGE;
static const RGBColor KBC_YELLOW        = KMC_YELLOW;
static const RGBColor KBC_GOLD          = KMC_GOLD;

// --- Green family ---
static const RGBColor KBC_CHARTREUSE    = KMC_CHARTREUSE;
static const RGBColor KBC_LIME          = KMC_LIME;
static const RGBColor KBC_MINT          = KMC_MINT;

// --- Blue / cyan family ---
static const RGBColor KBC_BLUE          = KMC_BLUE;
static const RGBColor KBC_SKY           = KMC_SKY;
static const RGBColor KBC_TEAL          = KMC_TEAL;
static const RGBColor KBC_CYAN          = KMC_CYAN;

// --- Purple family ---
static const RGBColor KBC_PURPLE        = KMC_PURPLE;
static const RGBColor KBC_INDIGO        = KMC_INDIGO;
static const RGBColor KBC_VIOLET        = KMC_VIOLET;

// --- Pink / red family ---
static const RGBColor KBC_CORAL         = KMC_CORAL;
static const RGBColor KBC_ROSE          = KMC_ROSE;
static const RGBColor KBC_PINK          = KMC_PINK;

// ============================================================
//  KBC_scaleColor() — backward-compatible alias
//
//  New code should call scaleColor() from KerbalModuleCommon.
// ============================================================

/**
 * @brief Scale an RGBColor by a brightness factor (0-255).
 *        Alias for scaleColor() in KerbalModuleCommon.
 */
inline RGBColor KBC_scaleColor(RGBColor color, uint8_t brightness) {
    return scaleColor(color, brightness);
}

// ============================================================
//  KBC_COLOR() macro — backward-compatible alias
// ============================================================

/** @brief Construct an RGBColor inline. Alias retained for compatibility. */
#define KBC_COLOR(r, g, b)  (RGBColor{ (r), (g), (b) })

#endif // KBC_COLORS_H

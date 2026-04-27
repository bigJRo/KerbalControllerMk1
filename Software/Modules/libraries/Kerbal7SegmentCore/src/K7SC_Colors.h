/**
 * @file        K7SC_Colors.h
 * @version     2.0.0
 * @date        2026-04-09
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Color aliases for the Kerbal7SegmentCore library.
 *
 *              The canonical color palette and RGBColor struct are defined
 *              in KerbalModuleCommon. This file includes that library and
 *              provides K7SC_* GRBWColor aliases for SK6812MINI-EA hardware
 *              via toGRBW(). The GRBWColor struct is also defined in
 *              KerbalModuleCommon.
 *
 *              All K7SC_* palette constants are produced by toGRBW(KMC_*)
 *              which explicitly handles the RGB → GRBW wire-order mapping.
 *              This replaces the v1.0.0 raw struct initializer approach
 *              that silently swapped the R and G channels in every color.
 *
 *              ENABLED state uses the W channel only via KMC_WHITE_ONLY()
 *              for a clean neutral backlight without RGB tint.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the Kerbal7SegmentCore (K7SC) library.
 *              Depends on: KerbalModuleCommon
 */

#pragma once
#include <KerbalModuleCommon.h>

// ============================================================
//  Utility
// ============================================================

/** @brief Fully off. All channels zero. */
static const GRBWColor K7SC_OFF           = { 0, 0, 0, 0 };

/**
 * @brief ENABLED state — dim neutral white via equal R/G/B.
 *        W channel is not used (NEO_GRB 3-byte mode on KC-01-1880 v2.0).
 *        Brightness set by K7SC_ENABLED_BRIGHTNESS in K7SC_Config.h.
 */
static const GRBWColor K7SC_ENABLED_COLOR = {
    K7SC_ENABLED_BRIGHTNESS,  // r
    K7SC_ENABLED_BRIGHTNESS,  // g
    K7SC_ENABLED_BRIGHTNESS,  // b
    0                          // w — unused in GRB mode
};

// ============================================================
//  System palette — GRBW wire format via toGRBW()
//
//  All constants are produced by toGRBW(KMC_*) which correctly
//  maps RGBColor{r,g,b} into GRBWColor{g,r,b,0} wire order.
//  W channel is always 0 for ACTIVE state colors.
// ============================================================

static const GRBWColor K7SC_GREEN         = toGRBW(KMC_GREEN);
static const GRBWColor K7SC_RED           = toGRBW(KMC_RED);
static const GRBWColor K7SC_AMBER         = toGRBW(KMC_AMBER);
static const GRBWColor K7SC_YELLOW        = toGRBW(KMC_YELLOW);
static const GRBWColor K7SC_GOLD          = toGRBW(KMC_GOLD);
static const GRBWColor K7SC_ORANGE        = toGRBW(KMC_ORANGE);
static const GRBWColor K7SC_CHARTREUSE    = toGRBW(KMC_CHARTREUSE);
static const GRBWColor K7SC_LIME          = toGRBW(KMC_LIME);
static const GRBWColor K7SC_MINT          = toGRBW(KMC_MINT);
static const GRBWColor K7SC_BLUE          = toGRBW(KMC_BLUE);
static const GRBWColor K7SC_SKY           = toGRBW(KMC_SKY);
static const GRBWColor K7SC_TEAL          = toGRBW(KMC_TEAL);
static const GRBWColor K7SC_CYAN          = toGRBW(KMC_CYAN);
static const GRBWColor K7SC_PURPLE        = toGRBW(KMC_PURPLE);
static const GRBWColor K7SC_INDIGO        = toGRBW(KMC_INDIGO);
static const GRBWColor K7SC_CORAL         = toGRBW(KMC_CORAL);
static const GRBWColor K7SC_ROSE          = toGRBW(KMC_ROSE);
static const GRBWColor K7SC_PINK          = toGRBW(KMC_PINK);
static const GRBWColor K7SC_MAGENTA       = toGRBW(KMC_MAGENTA);

// ============================================================
//  k7scScaleColor() — backward-compatible alias
// ============================================================

/** @brief Scale a GRBWColor by a brightness factor. Alias for scaleColorGRBW(). */
inline GRBWColor k7scScaleColor(GRBWColor c, uint8_t brightness) {
    return scaleColorGRBW(c, brightness);
}

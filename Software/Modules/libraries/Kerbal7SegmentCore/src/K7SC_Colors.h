/**
 * @file        K7SC_Colors.h
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       GRBW color palette for the Kerbal7SegmentCore library.
 *
 *              The SK6812MINI-EA LED uses GRBW color order with a
 *              dedicated white channel (4 bytes per pixel). This is
 *              distinct from the WS2811/SK6812 RGB used elsewhere.
 *
 *              ENABLED state uses the W channel only (RGB=0,0,0)
 *              for a clean neutral dim backlight. ACTIVE states use
 *              the RGB channels with W=0.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include <stdint.h>

// ============================================================
//  GRBWColor struct
//  Field order matches SK6812MINI-EA wire order.
// ============================================================

struct GRBWColor {
    uint8_t g;
    uint8_t r;
    uint8_t b;
    uint8_t w;
};

// ============================================================
//  Convenience macro
// ============================================================

#define K7SC_COLOR(r, g, b)   GRBWColor{(g), (r), (b), 0}
#define K7SC_WHITE_ONLY(w)    GRBWColor{0, 0, 0, (w)}

// ============================================================
//  Utility
// ============================================================

/** @brief Fully off. */
static const GRBWColor K7SC_OFF        = {0, 0, 0, 0};

/**
 * @brief ENABLED state — white channel only, dim.
 *        Provides a clean neutral backlight without RGB tint.
 *        Brightness controlled by K7SC_ENABLED_BRIGHTNESS.
 */
static const GRBWColor K7SC_ENABLED_COLOR = {0, 0, 0, K7SC_ENABLED_BRIGHTNESS};

// ============================================================
//  System palette — RGB channels, W=0
//  Colors match the Kerbal Controller Mk1 system palette.
// ============================================================

static const GRBWColor K7SC_GREEN      = { 197,  34,  94, 0};
static const GRBWColor K7SC_RED        = {  68, 239,  68, 0};
static const GRBWColor K7SC_AMBER      = { 158, 245,  11, 0};
static const GRBWColor K7SC_YELLOW     = { 179, 234,   8, 0};
static const GRBWColor K7SC_GOLD       = { 200, 255,   0, 0};
static const GRBWColor K7SC_ORANGE     = { 115, 249,  22, 0};
static const GRBWColor K7SC_CHARTREUSE = { 230, 163,  53, 0};
static const GRBWColor K7SC_LIME       = { 204, 132,  22, 0};
static const GRBWColor K7SC_MINT       = { 255, 150, 200, 0};
static const GRBWColor K7SC_BLUE       = { 130,  59, 246, 0};
static const GRBWColor K7SC_SKY        = { 165,  14, 233, 0};
static const GRBWColor K7SC_TEAL       = { 184,  20, 166, 0};
static const GRBWColor K7SC_CYAN       = { 182,   6, 212, 0};
static const GRBWColor K7SC_PURPLE     = {  92, 139, 246, 0};
static const GRBWColor K7SC_INDIGO     = { 102,  99, 241, 0};
static const GRBWColor K7SC_CORAL      = { 100, 255,  54, 0};
static const GRBWColor K7SC_ROSE       = {  80, 255, 120, 0};
static const GRBWColor K7SC_PINK       = {  72, 236, 153, 0};
static const GRBWColor K7SC_MAGENTA    = {   0, 255, 255, 0};

// ============================================================
//  Color scaling helper
// ============================================================

inline GRBWColor k7scScaleColor(GRBWColor c, uint8_t brightness) {
    return {
        (uint8_t)((uint16_t)c.g * brightness / 255),
        (uint8_t)((uint16_t)c.r * brightness / 255),
        (uint8_t)((uint16_t)c.b * brightness / 255),
        (uint8_t)((uint16_t)c.w * brightness / 255)
    };
}

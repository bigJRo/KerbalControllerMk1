/**
 * @file        KJC_Colors.h
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       RGB color palette and helpers for KerbalJoystickCore.
 *
 *              Colors match the Kerbal Controller Mk1 system palette
 *              defined in KBC_Colors.h. Defined independently here
 *              so joystick module sketches have no KBC library dependency.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include <stdint.h>

// ============================================================
//  RGBColor struct
// ============================================================

struct RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// ============================================================
//  Convenience macro
// ============================================================

#define KJC_COLOR(r, g, b)  RGBColor{(r), (g), (b)}

// ============================================================
//  Utility
// ============================================================

static const RGBColor KJC_OFF           = {   0,   0,   0 };
static const RGBColor KJC_WHITE_COOL    = { 255, 255, 255 };

// ============================================================
//  System palette — matches KBC_Colors.h
// ============================================================

static const RGBColor KJC_GREEN        = {  34, 197,  94 };
static const RGBColor KJC_RED          = { 239,  68,  68 };
static const RGBColor KJC_AMBER        = { 245, 158,  11 };
static const RGBColor KJC_YELLOW       = { 234, 179,   8 };
static const RGBColor KJC_GOLD         = { 255, 200,   0 };
static const RGBColor KJC_ORANGE       = { 249, 115,  22 };
static const RGBColor KJC_CHARTREUSE   = { 163, 230,  53 };
static const RGBColor KJC_LIME         = { 132, 204,  22 };
static const RGBColor KJC_MINT         = { 150, 255, 200 };
static const RGBColor KJC_BLUE         = {  59, 130, 246 };
static const RGBColor KJC_SKY          = {  14, 165, 233 };
static const RGBColor KJC_TEAL         = {  20, 184, 166 };
static const RGBColor KJC_CYAN         = {   6, 182, 212 };
static const RGBColor KJC_PURPLE       = { 139,  92, 246 };
static const RGBColor KJC_INDIGO       = {  99, 102, 241 };
static const RGBColor KJC_VIOLET       = { 180, 106, 255 };
static const RGBColor KJC_CORAL        = { 255, 100,  54 };
static const RGBColor KJC_ROSE         = { 255,  80, 120 };
static const RGBColor KJC_PINK         = { 236,  72, 153 };
static const RGBColor KJC_MAGENTA      = { 255,   0, 255 };
static const RGBColor KJC_WHITE_WARM   = { 255, 200, 150 };
static const RGBColor KJC_WHITE_SOFT   = { 180, 184, 204 };

// ============================================================
//  Color scaling helper
// ============================================================

inline RGBColor kjcScaleColor(RGBColor c, uint8_t brightness) {
    return {
        (uint8_t)((uint16_t)c.r * brightness / 255),
        (uint8_t)((uint16_t)c.g * brightness / 255),
        (uint8_t)((uint16_t)c.b * brightness / 255)
    };
}

// ============================================================
//  Nibble pack/unpack helpers
// ============================================================

inline uint8_t kjcPackGet(const uint8_t* payload, uint8_t btn) {
    return (btn % 2 == 0)
        ? (payload[btn / 2] >> 4) & 0x0F
        : (payload[btn / 2]) & 0x0F;
}

inline void kjcPackSet(uint8_t* payload, uint8_t btn, uint8_t state) {
    if (btn % 2 == 0)
        payload[btn / 2] = (payload[btn / 2] & 0x0F) | ((state & 0x0F) << 4);
    else
        payload[btn / 2] = (payload[btn / 2] & 0xF0) | (state & 0x0F);
}

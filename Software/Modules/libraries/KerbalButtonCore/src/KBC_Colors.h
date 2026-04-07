/**
 * @file        KBC_Colors.h
 * @version     1.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Named RGB color constants and RGBColor struct for the
 *              KerbalButtonCore library. Defines the full KBC palette
 *              used across all button input modules.
 *
 *              Colors are stored as full-brightness RGB values (0-255 per
 *              channel). All brightness scaling for the ENABLED dim state
 *              is handled at render time by KBC_LEDControl and must never
 *              be applied to values stored here.
 *
 *              Palette is organized into functional families:
 *                - Semantic    : system-wide consistent meaning (GREEN/RED/AMBER)
 *                - Whites      : ENABLED state and special-use whites
 *                - Warm        : orange, yellow, gold
 *                - Green       : chartreuse, lime, mint
 *                - Blue/Cyan   : blue, sky, teal, cyan
 *                - Purple      : purple, indigo, violet
 *                - Pink/Red    : coral, rose, pink
 *                - Utility     : off / unlit
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the KerbalButtonCore (KBC) library.
 *              Protocol defined in KBC_Protocol_Spec.md v1.1
 *              Button color assignments defined in KBC_Button_Color_Assignments.csv
 */

#ifndef KBC_COLORS_H
#define KBC_COLORS_H

#include <stdint.h>

// ============================================================
//  RGBColor struct
// ============================================================

/**
 * @struct RGBColor
 * @brief  Represents a full-brightness RGB color value.
 *
 *         All three channels are 8-bit unsigned integers (0-255).
 *         RGBW LED buttons use RGB channels only; the W channel
 *         is not addressable via this library.
 */
struct RGBColor {
    uint8_t r;  ///< Red channel   (0-255)
    uint8_t g;  ///< Green channel (0-255)
    uint8_t b;  ///< Blue channel  (0-255)
};

// ============================================================
//  Utility
// ============================================================

/** @brief Unlit / not installed. Use to explicitly clear a button. */
static const RGBColor KBC_OFF            = {   0,   0,   0 };

// ============================================================
//  Semantic — system-wide consistent meaning
//
//  These colors carry fixed semantic meaning across ALL modules.
//  Do not repurpose them for other uses.
//
//    KBC_GREEN  : default active / go / nominal state
//    KBC_RED    : irreversible action (cut, release, jettison)
//    KBC_AMBER  : awareness / caution (not dangerous, but notable)
// ============================================================

/** @brief Default active state. Nominal / go / confirmed. */
static const RGBColor KBC_GREEN          = {  34, 197,  94 };

/** @brief Irreversible action indicator. */
static const RGBColor KBC_RED            = { 239,  68,  68 };

/**
 * @brief Awareness / caution indicator.
 *
 *        Also used as the color for WARNING extended LED state (flashing)
 *        and PARTIAL_DEPLOY extended LED state (static). The distinction
 *        between these uses is made by the LED state machine, not the color.
 */
static const RGBColor KBC_AMBER          = { 245, 158,  11 };

// ============================================================
//  Extended LED state colors — library defined
//
//  These are the colors used by the LED state machine for
//  extended states (0x3-0x6). They are defined here for
//  reference and used internally by KBC_LEDControl.
//  Do not use these directly in module sketches for ACTIVE state.
//
//    WARNING      : KBC_AMBER, 500ms on / 500ms off flash
//    ALERT        : KBC_RED,   150ms on / 150ms off flash
//    ARMED        : KBC_CYAN,  full brightness static
//    PARTIAL_DEPLOY: KBC_AMBER, full brightness static
// ============================================================

/** @brief ARMED extended state color. Full brightness static cyan. */
static const RGBColor KBC_STATE_ARMED    = {   6, 182, 212 };

// ============================================================
//  Whites — ENABLED state and special use
//
//  KBC_WHITE_COOL is the standard ENABLED state backlight color.
//  It is scaled down at render time by the enabledBrightness
//  setting (set via CMD_SET_BRIGHTNESS). The other whites are
//  available for use as ACTIVE colors where appropriate.
// ============================================================

/** @brief Cool white. Standard ENABLED state backlight. Equal RGB mix. */
static const RGBColor KBC_WHITE_COOL     = { 255, 255, 255 };

/** @brief Neutral white. Slight warmth. Natural daylight feel. */
static const RGBColor KBC_WHITE_NEUTRAL  = { 255, 240, 216 };

/**
 * @brief Warm white. Amber-tinted incandescent feel.
 *        Used as ACTIVE color for time warp rate Forward / Back buttons.
 */
static const RGBColor KBC_WHITE_WARM     = { 255, 200, 150 };

/** @brief Soft white. Dimmed blue-tinted. Easy on eyes in dark environments. */
static const RGBColor KBC_WHITE_SOFT     = { 180, 184, 204 };

// ============================================================
//  Warm family — orange, yellow, gold
// ============================================================

/**
 * @brief Orange.
 *        Used for: Engine Alt Mode, Engine Group 1 (Function Control),
 *        Radiator (Vehicle Control).
 */
static const RGBColor KBC_ORANGE         = { 249, 115,  22 };

/**
 * @brief Yellow.
 *        Used for: Warp targets (Time Control), Lights (Vehicle Control),
 *        Engine Group 1 (Function Control).
 */
static const RGBColor KBC_YELLOW         = { 234, 179,   8 };

/**
 * @brief Gold. Distinct from yellow. Power generation association.
 *        Used for: Solar Array (Vehicle Control).
 */
static const RGBColor KBC_GOLD           = { 255, 200,   0 };

// ============================================================
//  Green family — chartreuse, lime, mint
// ============================================================

/**
 * @brief Chartreuse. Yellow-green. Mechanical variant association.
 *        Used for: Engine Group 2 (Function Control),
 *        Physics warp (Time Control).
 */
static const RGBColor KBC_CHARTREUSE     = { 163, 230,  53 };

/**
 * @brief Lime. Yellow-adjacent green. Positive / terrain association.
 *        Used for: Ladder (Vehicle Control), Save (Time Control).
 */
static const RGBColor KBC_LIME           = { 132, 204,  22 };

/**
 * @brief Mint. Cool light green. Restoration / recovery association.
 *        Used for: Load quicksave (Time Control).
 */
static const RGBColor KBC_MINT           = { 150, 255, 200 };

// ============================================================
//  Blue / cyan family — blue, sky, teal, cyan
// ============================================================

/**
 * @brief Blue. Deep cool blue. Science data association.
 *        Used for: Science Group 2 (Function Control).
 */
static const RGBColor KBC_BLUE           = {  59, 130, 246 };

/**
 * @brief Sky blue. Bright mid blue. Navigation / map association.
 *        Used for: Map family buttons (UI Control).
 */
static const RGBColor KBC_SKY            = {  14, 165, 233 };

/**
 * @brief Teal. Blue-green. Ship navigation / atmosphere association.
 *        Used for: Ship family (UI Control), Air Intake (Function Control).
 */
static const RGBColor KBC_TEAL           = {  20, 184, 166 };

/**
 * @brief Cyan. Bright blue-green. Atmosphere / ARMED state association.
 *        Used for: Air Brake (Function Control).
 *        Also used internally as the ARMED extended LED state color.
 */
static const RGBColor KBC_CYAN           = {   6, 182, 212 };

// ============================================================
//  Purple family — purple, indigo, violet
// ============================================================

/**
 * @brief Purple. Science / discovery association.
 *        Used for: Science Collect (Function Control).
 */
static const RGBColor KBC_PURPLE         = { 139,  92, 246 };

/**
 * @brief Indigo. Mid purple-blue. Science group association.
 *        Used for: Science Group 1 (Function Control).
 */
static const RGBColor KBC_INDIGO         = {  99, 102, 241 };

/**
 * @brief Violet. Light purple. Mechanical access association.
 *        Used for: Cargo Door (Vehicle Control).
 */
static const RGBColor KBC_VIOLET         = { 180, 106, 255 };

// ============================================================
//  Pink / red family — coral, rose, pink
// ============================================================

/**
 * @brief Coral. Warm orange-red. Mode shift association.
 *        Used for: IVA (UI Control).
 */
static const RGBColor KBC_CORAL          = { 255, 100,  54 };

/**
 * @brief Rose. Cool pink-red. Vessel configuration association.
 *        Used for: CP PRI and CP ALT (Action Control).
 */
static const RGBColor KBC_ROSE           = { 255,  80, 120 };

/**
 * @brief Pink. Mid pink. Communications association.
 *        Used for: Antenna (Vehicle Control).
 */
static const RGBColor KBC_PINK           = { 236,  72, 153 };

// ============================================================
//  Helper macro — define a color inline
//
//  Usage:  RGBColor myColor = KBC_COLOR(255, 128, 0);
// ============================================================

#define KBC_COLOR(r, g, b)  ((RGBColor){ (r), (g), (b) })

// ============================================================
//  Helper function — scale a color by a brightness factor
//
//  Scales all three channels by (brightness / 255).
//  Used internally by KBC_LEDControl for ENABLED state dimming.
//  Available to module sketches for custom scaling needs.
//
//  @param color      Source RGBColor at full brightness
//  @param brightness Scale factor 0-255 (255 = no change, 0 = off)
//  @return           Scaled RGBColor
// ============================================================

inline RGBColor KBC_scaleColor(RGBColor color, uint8_t brightness) {
    return {
        (uint8_t)((uint16_t)color.r * brightness / 255),
        (uint8_t)((uint16_t)color.g * brightness / 255),
        (uint8_t)((uint16_t)color.b * brightness / 255)
    };
}

#endif // KBC_COLORS_H

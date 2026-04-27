/**
 * @file        KerbalModuleCommon.h
 * @version     1.0.0
 * @date        2026-04-09
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Shared color types, system palette, and LED utilities
 *              for all Kerbal Controller Mk1 target modules.
 *
 *              This is the single source of truth for the Kerbal
 *              Controller Mk1 color palette. All module libraries
 *              (KerbalButtonCore, KerbalJoystickCore, Kerbal7SegmentCore)
 *              and standalone module sketches (EVA, Indicator, etc.)
 *              include this file rather than defining colors locally.
 *
 *              Contents:
 *                - RGBColor and GRBWColor structs
 *                - KMC_* system palette constants (canonical RGB)
 *                - toGRBW() — explicit RGB → GRBW wire-format conversion
 *                - scaleColor() — brightness scaling for ENABLED dim state
 *                - LED state nibble values (KMC_LED_*)
 *                - LED payload nibble pack / unpack helpers
 *
 *              Hardware color formats used across the system:
 *
 *                RGB  (WS2811, NEO_RGB)
 *                  Used by: KerbalButtonCore (KC-01-1822)
 *                           KerbalJoystickCore (KC-01-1831/1832)
 *                           EVA Module (KC-01-1852)
 *                           Indicator Module (SK6812mini-012, NEO_RGB)
 *
 *                GRBW (SK6812MINI-EA, NEO_GRBW)
 *                  Used by: Kerbal7SegmentCore (KC-01-1881/1882)
 *
 *              The canonical palette is stored as RGBColor (RGB is the
 *              natural human-readable representation). Use toGRBW() to
 *              convert for GRBW hardware — the function name makes the
 *              format conversion explicit at every use site.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include <stdint.h>

// ============================================================
//  Color structs
// ============================================================

/**
 * @struct RGBColor
 * @brief  Full-brightness RGB color value.
 *
 *         Used natively by WS2811 and SK6812mini-012 hardware.
 *         All channels 0-255. Brightness scaling for the ENABLED
 *         dim state is applied at render time via scaleColor() —
 *         never pre-scale values stored in palette constants.
 */
struct RGBColor {
    uint8_t r;  ///< Red   (0-255)
    uint8_t g;  ///< Green (0-255)
    uint8_t b;  ///< Blue  (0-255)
};

/**
 * @struct GRBWColor
 * @brief  Full-brightness GRBW color value for SK6812MINI-EA hardware.
 *
 *         Field order matches the SK6812MINI-EA wire protocol (G, R, B, W).
 *         Do not initialize this struct with raw {r, g, b, w} literals —
 *         use toGRBW() to convert from RGBColor, or use KMC_WHITE_ONLY()
 *         for the white-channel-only ENABLED state. The field order
 *         mismatch between human intent and wire order is the root cause
 *         of the v1.0.0 K7SC_Colors.h palette bug; toGRBW() exists to
 *         make that conversion impossible to get wrong silently.
 */
struct GRBWColor {
    uint8_t g;  ///< Green (wire byte 0)
    uint8_t r;  ///< Red   (wire byte 1)
    uint8_t b;  ///< Blue  (wire byte 2)
    uint8_t w;  ///< White (wire byte 3)
};

// ============================================================
//  Color construction helpers
// ============================================================

/**
 * @brief  Convert an RGBColor to GRBWColor wire format.
 *
 *         This is the only sanctioned way to produce a GRBWColor from
 *         RGB channel values. It makes the format conversion explicit
 *         at every call site and eliminates the possibility of
 *         accidentally initializing GRBWColor fields in RGB order.
 *
 *         White channel is always 0 — ACTIVE state colors use RGB only.
 *         For the ENABLED white-channel backlight use KMC_WHITE_ONLY().
 *
 * @param  c  Source RGBColor (full brightness).
 * @return    GRBWColor suitable for SK6812MINI-EA tinyNeoPixel calls.
 */
inline GRBWColor toGRBW(RGBColor c) {
    return { c.g, c.r, c.b, 0 };
}

/**
 * @brief  Produce a GRBWColor that uses only the white channel.
 *
 *         Used for the ENABLED dim state on SK6812MINI-EA hardware,
 *         which has a dedicated W channel that produces cleaner neutral
 *         white than mixing RGB. RGB channels are set to 0.
 *
 * @param  w  White channel brightness (0-255).
 * @return    GRBWColor with only the W channel set.
 */
inline GRBWColor KMC_WHITE_ONLY(uint8_t w) {
    return { 0, 0, 0, w };
}

/**
 * @brief  Scale an RGBColor by a brightness factor.
 *
 *         Used for the ENABLED dim state on RGB hardware. Multiplies
 *         all three channels by (brightness / 255). A brightness of
 *         255 returns the original color; 0 returns black.
 *
 *         Never apply scaling to palette constants themselves — always
 *         scale at render time so the stored value remains full-brightness.
 *
 * @param  color       Source RGBColor at full brightness.
 * @param  brightness  Scale factor 0-255.
 * @return             Scaled RGBColor.
 */
inline RGBColor scaleColor(RGBColor color, uint8_t brightness) {
    return {
        (uint8_t)((uint16_t)color.r * brightness / 255),
        (uint8_t)((uint16_t)color.g * brightness / 255),
        (uint8_t)((uint16_t)color.b * brightness / 255)
    };
}

/**
 * @brief  Scale a GRBWColor by a brightness factor.
 *
 *         Used when scaling GRBW colors at render time. Scales all
 *         four channels proportionally.
 *
 * @param  color       Source GRBWColor at full brightness.
 * @param  brightness  Scale factor 0-255.
 * @return             Scaled GRBWColor.
 */
inline GRBWColor scaleColorGRBW(GRBWColor color, uint8_t brightness) {
    return {
        (uint8_t)((uint16_t)color.g * brightness / 255),
        (uint8_t)((uint16_t)color.r * brightness / 255),
        (uint8_t)((uint16_t)color.b * brightness / 255),
        (uint8_t)((uint16_t)color.w * brightness / 255)
    };
}

// ============================================================
//  System palette — canonical RGB values
//
//  These values have been validated on SK6812MINI-EA hardware
//  (KC-01-1882 v2.0, NEO_GRB 3-byte mode) and adjusted to match
//  the intended perceptual colour on the physical LED.
//
//  Semantic colors carry fixed meaning across ALL modules:
//    KMC_GREEN  : active / go / nominal state
//    KMC_RED    : irreversible action (cut, release, jettison)
//    KMC_AMBER  : caution / awareness / WARNING flash state
//
//  Extended LED state colors (used by the LED state machine):
//    WARNING      : KMC_AMBER, 500ms/500ms flash
//    ALERT        : KMC_RED,   150ms/150ms flash
//    ARMED        : KMC_CYAN,  static full brightness
//    PARTIAL_DEPLOY: KMC_AMBER, static full brightness
// ============================================================

// --- Utility ---
/** @brief Fully unlit. Use to explicitly clear a position. */
static const RGBColor KMC_OFF           = {   0,   0,   0 };

/**
 * @brief Discrete indicator ON color.
 *        Any non-zero value drives a discrete LED HIGH. Uses {1,1,1}
 *        to communicate "colorless indicator" rather than implying a
 *        specific color. Do not use for NeoPixel positions.
 */
static const RGBColor KMC_DISCRETE_ON  = {   1,   1,   1 };

// --- Semantic — fixed meaning across all modules ---
/** @brief Active / go / nominal. Default active state color. */
static const RGBColor KMC_GREEN         = {   0, 255,   0 };

/** @brief Irreversible action. Cut, release, jettison. */
static const RGBColor KMC_RED           = { 255,   0,   0 };

/**
 * @brief Caution / awareness.
 *        Also the color for WARNING (flashing) and PARTIAL_DEPLOY
 *        (static) extended LED states.
 */
static const RGBColor KMC_AMBER         = { 255,  80,   0 };

// --- Whites ---
/** @brief Cool white. Standard ENABLED state backlight for RGB hardware. */
static const RGBColor KMC_WHITE_COOL    = { 255, 180, 255 };

/** @brief Neutral white. Slight warmth. Natural daylight feel. */
static const RGBColor KMC_WHITE_NEUTRAL = { 255, 160, 180 };

/** @brief Warm white. Amber-tinted incandescent feel. Time warp rate buttons. */
static const RGBColor KMC_WHITE_WARM    = { 255, 140, 100 };

/** @brief Soft white. Warm dim amber — night vision safe. Minimises rod
 *         bleaching for dark cockpit use. Low brightness, warm colour
 *         temperature preserves scotopic adaptation. */
static const RGBColor KMC_WHITE_SOFT    = { 120,  70,  20 };

// --- Warm family ---
/** @brief Orange. Engine Alt Mode, Engine Group 1, Radiator. */
static const RGBColor KMC_ORANGE        = { 255,  60,   0 };

/** @brief Yellow. Warp targets, Lights, Engine Group 1. */
static const RGBColor KMC_YELLOW        = { 234, 179,   8 };

/** @brief Gold. Power generation association. Solar Array. */
static const RGBColor KMC_GOLD          = { 255, 160,   0 };

// --- Green family ---
/** @brief Chartreuse. Mechanical variant / physics warp. */
static const RGBColor KMC_CHARTREUSE    = { 163, 230,  53 };

/** @brief Lime. Positive / terrain. Ladder, Quicksave. */
static const RGBColor KMC_LIME          = { 132, 204,  22 };

/** @brief Mint. Restoration / recovery. RCS, Quickload. */
static const RGBColor KMC_MINT          = { 100, 255, 160 };

// --- Blue / cyan family ---
/** @brief Blue. Science data. */
static const RGBColor KMC_BLUE          = {   0,   0, 255 };

/** @brief Sky blue. Navigation / map. */
static const RGBColor KMC_SKY           = {  14, 165, 233 };

/** @brief Teal. Ship navigation / atmosphere. */
static const RGBColor KMC_TEAL          = {  20, 184, 166 };

/**
 * @brief Cyan. Atmosphere / ARMED state.
 *        Also used as the ARMED extended LED state color.
 */
static const RGBColor KMC_CYAN          = {   6, 182, 212 };

// --- Purple family ---
/** @brief Purple. Science / discovery. */
static const RGBColor KMC_PURPLE        = { 120,   0, 220 };

/** @brief Indigo. Science group. */
static const RGBColor KMC_INDIGO        = {  60,   0, 200 };

/** @brief Violet. Mechanical access. Cargo Door. */
static const RGBColor KMC_VIOLET        = { 160,   0, 255 };

// --- Pink / red family ---
/** @brief Coral. Mode shift. IVA. */
static const RGBColor KMC_CORAL         = { 255, 100,  54 };

/** @brief Rose. Vessel configuration. Control Points. */
static const RGBColor KMC_ROSE          = { 255,  80, 120 };

/** @brief Pink. Communications. Antenna. */
static const RGBColor KMC_PINK          = { 236,  72, 153 };

/** @brief Magenta. Debug / development indicator. */
static const RGBColor KMC_MAGENTA       = { 255,   0, 255 };

// ============================================================
//  LED state nibble values
//
//  The canonical KBC I2C protocol LED state values. Defined
//  here as a single source of truth shared by all modules and
//  libraries. Each module's Config.h / Protocol.h may alias
//  these under a module-prefixed name for local clarity.
//
//  Core states (all modules):
//    0x0  OFF             — unlit
//    0x1  ENABLED         — dim white backlight
//    0x2  ACTIVE          — full brightness, per-button color
//
//  Extended states (capability-flagged modules only):
//    0x3  WARNING         — flashing amber, 500ms on/off
//    0x4  ALERT           — flashing red,   150ms on/off
//    0x5  ARMED           — static cyan
//    0x6  PARTIAL_DEPLOY  — static amber
//
//  0x7-0xF reserved.
// ============================================================

#define KMC_LED_OFF             0x0
#define KMC_LED_ENABLED         0x1
#define KMC_LED_ACTIVE          0x2
#define KMC_LED_WARNING         0x3
#define KMC_LED_ALERT           0x4
#define KMC_LED_ARMED           0x5
#define KMC_LED_PARTIAL_DEPLOY  0x6

// ============================================================
//  LED payload nibble pack / unpack helpers
//
//  The CMD_SET_LED_STATE payload is 8 bytes, nibble-packed:
//  two buttons per byte, high nibble = even button index,
//  low nibble = odd button index.
//
//  These replace the duplicate implementations previously in
//  KBC_Protocol.h, KJC_Colors.h, and EVA/Config.h.
// ============================================================

/**
 * @brief  Get the LED state nibble for button N from a packed payload.
 * @param  payload  Pointer to 8-byte payload array.
 * @param  button   Button index (0-15).
 * @return LED state nibble (0x0-0xF).
 */
inline uint8_t kmcLedPackGet(const uint8_t* payload, uint8_t button) {
    return (button % 2 == 0)
        ? (payload[button / 2] >> 4) & 0x0F
        :  payload[button / 2]       & 0x0F;
}

/**
 * @brief  Set the LED state nibble for button N in a packed payload.
 * @param  payload  Pointer to 8-byte payload array.
 * @param  button   Button index (0-15).
 * @param  state    LED state nibble (KMC_LED_*).
 */
inline void kmcLedPackSet(uint8_t* payload, uint8_t button, uint8_t state) {
    if (button % 2 == 0)
        payload[button / 2] = (payload[button / 2] & 0x0F) | ((state & 0x0F) << 4);
    else
        payload[button / 2] = (payload[button / 2] & 0xF0) |  (state & 0x0F);
}

// ============================================================
//  I2C command bytes
//
//  All commands are initiated by the controller. The command byte
//  is the first byte of every write transaction. All module types
//  implement the full base command set (0x01-0x08). Extended
//  commands (0x09 onward) are module-type-specific.
//
//  These are the canonical definitions shared by all modules.
//  Previously duplicated across KBC_Protocol.h, KJC_Config.h,
//  K7SC_Config.h, and every standalone module Config.h.
// ============================================================

/** @brief Query module identity. No payload. Returns 4-byte identity packet. */
#define KMC_CMD_GET_IDENTITY    0x01

/** @brief Set full LED state. Payload: 8 bytes nibble-packed. */
#define KMC_CMD_SET_LED_STATE   0x02

/** @brief Set ENABLED state brightness. Payload: 1 byte (0-255). */
#define KMC_CMD_SET_BRIGHTNESS  0x03

/** @brief Trigger bulb test. No payload or 0x01=start, 0x00=stop. */
#define KMC_CMD_BULB_TEST       0x04

/** @brief Enter low power sleep mode. No payload. */
#define KMC_CMD_SLEEP           0x05

/** @brief Resume normal operation from sleep. No payload. */
#define KMC_CMD_WAKE            0x06

/** @brief Reset module to default state. No payload. */
#define KMC_CMD_RESET           0x07

/** @brief Acknowledge and clear module fault flag. No payload. */
#define KMC_CMD_ACK_FAULT       0x08

/** @brief Enable module for active operation. No payload. */
#define KMC_CMD_ENABLE          0x09

/** @brief Disable module. No payload. */
#define KMC_CMD_DISABLE         0x0A

/**
 * @brief Throttle module only. Set target position.
 *        Payload: 2 bytes big-endian uint16 (0 to INT16_MAX).
 */
#define KMC_CMD_SET_THROTTLE    0x0B

/**
 * @brief Throttle module only. Toggle precision mode.
 *        Payload: 0x01=enter, 0x00=exit.
 */
#define KMC_CMD_SET_PRECISION   0x0C

/**
 * @brief Display modules only. Set displayed value.
 *        Payload: 2 bytes big-endian uint16 (0-9999).
 */
#define KMC_CMD_SET_VALUE       0x0D

// ============================================================
//  Module Type ID registry
//
//  Type IDs are independent of I2C address. The controller uses
//  the Type ID from the identity response to determine the data
//  packet format and read length for each address.
//
//  Previously: standard modules defined in KBC_Protocol.h;
//  standalone modules used raw hex literals in their Config.h.
//  Now: complete registry in one place.
// ============================================================

#define KMC_TYPE_RESERVED           0x00  ///< Reserved — must not be used
#define KMC_TYPE_UI_CONTROL         0x01  ///< UI Control         (0x20, 4 bytes)
#define KMC_TYPE_FUNCTION_CONTROL   0x02  ///< Function Control   (0x21, 4 bytes)
#define KMC_TYPE_ACTION_CONTROL     0x03  ///< Action Control     (0x22, 4 bytes)
#define KMC_TYPE_STABILITY_CONTROL  0x04  ///< Stability Control  (0x23, 4 bytes)
#define KMC_TYPE_VEHICLE_CONTROL    0x05  ///< Vehicle Control    (0x24, 4 bytes)
#define KMC_TYPE_TIME_CONTROL       0x06  ///< Time Control       (0x25, 4 bytes)
#define KMC_TYPE_EVA_MODULE         0x07  ///< EVA Module         (0x26, 4 bytes)
// 0x08 reserved
#define KMC_TYPE_JOYSTICK_ROTATION  0x09  ///< Joystick Rotation  (0x28, 8 bytes)
#define KMC_TYPE_JOYSTICK_TRANS     0x0A  ///< Joystick Trans     (0x29, 8 bytes)
#define KMC_TYPE_GPWS_INPUT         0x0B  ///< GPWS Input Panel   (0x2A, 6 bytes)
#define KMC_TYPE_PRE_WARP_TIME      0x0C  ///< Pre-Warp Time      (0x2B, 6 bytes)
#define KMC_TYPE_THROTTLE           0x0D  ///< Throttle Module    (0x2C, 4 bytes)
#define KMC_TYPE_DUAL_ENCODER       0x0E  ///< Dual Encoder       (0x2D, 4 bytes)
#define KMC_TYPE_SWITCH_PANEL       0x0F  ///< Switch Panel       (0x2E, 4 bytes)
#define KMC_TYPE_INDICATOR          0x10  ///< Indicator Module   (0x2F, 0 bytes)
#define KMC_TYPE_UNKNOWN            0xFF  ///< Unknown / uninitialized

// ============================================================
//  Capability flags (identity response byte 3)
//
//  Bitmask reported in the identity response. The controller reads
//  these at startup to determine module capabilities.
//
//  Previously scattered across KBC_Protocol.h, KJC_Config.h,
//  K7SC_Config.h, and individual standalone Config.h files.
// ============================================================

/** @brief Module supports extended LED states (0x3-0x6). */
#define KMC_CAP_EXTENDED_STATES (1 << 0)

/** @brief Module has an active fault condition. Clear with CMD_ACK_FAULT. */
#define KMC_CAP_FAULT           (1 << 1)

/** @brief Encoder delta data present in response packet (EVA, Dual Encoder). */
#define KMC_CAP_ENCODERS        (1 << 2)

/** @brief Analog joystick axes present in response packet (Joystick modules). */
#define KMC_CAP_JOYSTICK        (1 << 3)

/** @brief 7-segment display and encoder present (GPWS, Pre-Warp). */
#define KMC_CAP_DISPLAY         (1 << 4)

/** @brief Motorized position control (Throttle module). */
#define KMC_CAP_MOTORIZED       (1 << 5)

// ============================================================
//  Identity packet size
//
//  All modules return a 4-byte identity packet in response to
//  CMD_GET_IDENTITY, regardless of module type. Previously
//  defined independently in every Config.h as *_IDENTITY_SIZE.
// ============================================================

/** @brief Identity response packet size in bytes (all module types). */
#define KMC_IDENTITY_SIZE       4

/** @brief LED state command payload size in bytes (nibble-packed, 16 positions). */
#define KMC_LED_PAYLOAD_SIZE    8

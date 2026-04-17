/**
 * @file        LEDs.h
 * @version     2.0
 * @date        2026-04-09
 * @project     Kerbal Controller Mk1 — Indicator Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       NeoPixel LED control for the Indicator Module.
 *
 *              18 SK6812mini-012 RGB LEDs driven from NEOPIX_CMD (PA5).
 *              The controller sends CMD_SET_LED_STATE with a 9-byte
 *              nibble-packed payload (1 nibble per pixel, 2 per byte,
 *              last nibble of byte 8 unused/zero). Each nibble is a
 *              KBC LED state value (0x0-0x6). The module maps each
 *              state to a per-pixel color.
 *
 *              State-to-color mapping:
 *                OFF           (0x0) — unlit
 *                ENABLED       (0x1) — dim white (all pixels)
 *                ACTIVE        (0x2) — per-pixel active color (see below)
 *                WARNING       (0x3) — amber, 500ms/500ms flash
 *                ALERT         (0x4) — red, 150ms/150ms flash
 *                ARMED         (0x5) — cyan, static
 *                PARTIAL_DEPLOY(0x6) — amber, static
 *
 *              All flash timing is handled on the module. The controller
 *              only sends state values — it has no knowledge of timing.
 *
 *              Pixel layout (6 columns × 3 rows, column-major order):
 *                Column:   1         2         3         4         5         6
 *                Row 1:    B0        B3        B6        B9        B12       B15
 *                          THRTL ENA THRTL PRC PREC INPT AUDIO     SCE AUX   AUTO PILOT
 *                Row 2:    B1        B4        B7        B10       B13       B16
 *                          LIGHT ENA BRAKE LCK LNDG GEAR CHUTE ARM RCS       SAS
 *                Row 3:    B2        B5        B8        B11       B14       B17
 *                          CTRL      DEBUG     DEMO      COMM ACTV SWCH ERR  ABORT
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "Config.h"
#include <KerbalModuleCommon.h>

// ============================================================
//  System color constants for shared states
// ============================================================

static const RGBColor IND_OFF       = KMC_OFF;
static const RGBColor IND_WHITE_DIM = scaleColor(KMC_WHITE_COOL, 32);
static const RGBColor IND_AMBER     = KMC_AMBER;
static const RGBColor IND_RED       = KMC_RED;
static const RGBColor IND_CYAN      = KMC_CYAN;

// ============================================================
//  Per-pixel ACTIVE colors (state 0x2)
//
//  Column-major index order: down column 1, then column 2, etc.
//  Matches physical panel layout top-left → bottom → next column.
//
//  Color rationale:
//    GREEN      — system/function enabled states
//    YELLOW     — lighting system
//    CHARTREUSE — precision/modified throttle state
//    AMBER      — caution / armed states
//    RED        — locks, errors, abort
//    SKY        — general control status
//    MINT       — RCS (matches Stability module convention)
//    TEAL       — communications
//    PURPLE     — audio system
//    BLUE       — autopilot
//    MAGENTA    — debug/diagnostic
//    INDIGO     — demo/non-operational
//    ORANGE     — auxiliary/SCE (Apollo SCE reference)
// ============================================================

static const RGBColor IND_ACTIVE_COLORS[IND_NEO_COUNT] = {
    KMC_GREEN,      //  0: THRTL ENA      — throttle enabled
    KMC_YELLOW,     //  1: LIGHT ENA      — lights enabled (matches Vehicle Control)
    KMC_LIME,       //  2: CTRL           — control active (green family, distinct from GREEN)
    KMC_MINT,       //  3: THRTL PREC     — precision throttle mode
    KMC_RED,        //  4: BRAKE LOCK     — brake locked (matches Vehicle Control brakes)
    KMC_ROSE,       //  5: DEBUG          — diagnostic mode (red family, distinct from fault RED)
    KMC_CYAN,       //  6: PREC INPUT     — precision input active
    KMC_GREEN,      //  7: LNDG GEAR LOCK — gear locked (matches Vehicle Control gear)
    KMC_SKY,        //  8: DEMO           — demo / non-operational (blue family)
    KMC_PURPLE,     //  9: AUDIO          — audio system active
    KMC_AMBER,      // 10: CHUTE ARM      — parachute armed (amber = alarming/caution)
    KMC_TEAL,       // 11: COMM ACTIVE    — communications active
    KMC_ORANGE,     // 12: SCE AUX        — auxiliary power
    KMC_MINT,       // 13: RCS            — RCS enabled (matches Stability convention)
    KMC_RED,        // 14: SWITCH ERROR   — switch fault
    KMC_RED,        // 15: ABORT          — abort active (ACTIVE=solid red, ALERT=flashing red)
                    //     Primary consumer of ALERT extended state on this module.
                    //     Controller should send:
                    //       ENABLED       — nominal, abort available
                    //       ARMED         — abort sequence primed
                    //       ALERT (0x4)   — abort in progress (150ms red flash)
                    //       ACTIVE (0x2)  — post-abort confirmation (solid red)
    KMC_GREEN,      // 16: SAS            — SAS enabled (matches Stability Control)
    KMC_BLUE,       // 17: AUTO PILOT     — autopilot engaged
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
 * @brief Set the LED state for all 18 pixels from the
 *        9-byte nibble-packed CMD_SET_LED_STATE payload.
 *        Byte layout: pixels 0-1 in byte 0, 2-3 in byte 1,
 *        ..., pixels 16-17 in byte 8 (high nibble = even index).
 * @param payload  9-byte array from I2C write transaction.
 */
void ledsSetFromPayload(const uint8_t* payload);

/**
 * @brief Set a single pixel state.
 * @param index  Pixel index 0-17.
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

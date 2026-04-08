/**
 * @file        K7SC_Buttons.h
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Button input and SK6812MINI-EA NeoPixel LED control
 *              for the Kerbal7SegmentCore library.
 *
 *              Three NeoPixel buttons (BTN01, BTN02, BTN03) and one
 *              direct GPIO button (BTN_EN — encoder pushbutton).
 *              All inputs are active high with hardware pull-downs.
 *
 *              Button behavior is configured per module via the
 *              ButtonConfig struct passed to buttonsBegin(). This
 *              allows the library to support:
 *                - Cycle buttons (BTN01 on GPWS — multi-state toggle)
 *                - Toggle buttons (BTN02/03 on GPWS — on/off)
 *                - Flash buttons (Pre-Warp — momentary color flash)
 *                - Momentary buttons (BTN_EN — no LED, trigger action)
 *
 *              NeoPixel chain uses SK6812MINI-EA (GRBW, 4 bytes/pixel)
 *              on NEOPIX_CMD (PC2, Port C).
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include "K7SC_Config.h"
#include "K7SC_Colors.h"

// ============================================================
//  Button behavior types
// ============================================================

enum ButtonMode : uint8_t {
    BTN_MODE_CYCLE    = 0,  // Multi-state cycle (e.g. off→green→amber→off)
    BTN_MODE_TOGGLE   = 1,  // Binary on/off toggle
    BTN_MODE_FLASH    = 2,  // Flash color on press, return to ENABLED
    BTN_MODE_MOMENTARY = 3, // No LED — triggers action only
};

// ============================================================
//  Per-button configuration
// ============================================================

struct ButtonConfig {
    ButtonMode      mode;

    /**
     * @brief Colors for each state.
     *        CYCLE:     colors[0]=off, colors[1]=state1, colors[2]=state2
     *        TOGGLE:    colors[0]=off(ENABLED), colors[1]=active
     *        FLASH:     colors[0]=flash color (returns to ENABLED after)
     *        MOMENTARY: not used
     */
    GRBWColor       colors[3];

    /** @brief Number of states for CYCLE mode (2 or 3). */
    uint8_t         numStates;

    /** @brief Flash duration override (0 = use K7SC_FLASH_DURATION_MS). */
    uint16_t        flashMs;
};

// ============================================================
//  Public interface — initialisation
// ============================================================

/**
 * @brief Initialise button GPIO pins and NeoPixel chain.
 * @param configs  Array of 3 ButtonConfig structs [BTN01, BTN02, BTN03].
 */
void buttonsBegin(const ButtonConfig* configs);

// ============================================================
//  Public interface — runtime
// ============================================================

/**
 * @brief Poll all four buttons with debounce.
 *        Handles state transitions, flash timing, and LED updates.
 *        Call every K7SC_POLL_INTERVAL_MS from the main loop.
 * @return true if any button event occurred.
 */
bool buttonsPoll();

/**
 * @brief Returns true if unread button events are pending.
 */
bool buttonsIsIntPending();

/**
 * @brief Get the raw button event bitmask.
 *        bit0=BTN01, bit1=BTN02, bit2=BTN03, bit3=BTN_EN.
 *        Each bit set = button was pressed this cycle.
 */
uint8_t buttonsGetEvents();

/**
 * @brief Get and clear the change mask.
 *        Clears INT pending flag.
 */
uint8_t buttonsGetChangeMask();

/**
 * @brief Get the module state byte for the data packet.
 *        bits 0-1: BTN01 cycle state (0=off, 1=state1, 2=state2)
 *        bit 2:    BTN02 active
 *        bit 3:    BTN03 active
 */
uint8_t buttonsGetStateByte();

/**
 * @brief Returns true if BTN_EN was pressed since last check.
 *        Clears the flag on read.
 */
bool buttonsGetEncoderPress();

/**
 * @brief Clear all button state.
 *        Called on CMD_RESET.
 */
void buttonsClearAll();

/**
 * @brief Clear all LEDs and render.
 *        Called on CMD_SLEEP.
 */
void buttonsClearLEDs();

/**
 * @brief Restore all LEDs to current state and render.
 *        Called on CMD_WAKE.
 */
void buttonsRestoreLEDs();

/**
 * @brief Push current LED state to NeoPixel hardware.
 */
void buttonsRender();

/**
 * @brief Light all NeoPixels full white for bulb test then restore.
 * @param durationMs  Duration in milliseconds.
 */
void buttonsBulbTest(uint16_t durationMs);

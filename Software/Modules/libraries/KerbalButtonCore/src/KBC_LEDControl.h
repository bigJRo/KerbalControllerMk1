/**
 * @file        KBC_LEDControl.h
 * @version     2.0.0
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       LED state machine for the KerbalButtonCore library.
 *              Manages LED output for the 12 NeoPixel RGB buttons
 *              (KBC indices 0-11). Indices 12-15 are panel switch inputs
 *              with no LED hardware on the current boards, so nothing is
 *              driven for them (their colour-array slots are ignored).
 *
 *              Supports the full KBC LED state set:
 *                OFF            — unlit
 *                ENABLED        — dim white backlight
 *                ACTIVE         — full brightness, per-button color
 *                WARNING        — flashing amber (extended modules)
 *                ALERT          — flashing red   (extended modules)
 *                ARMED          — static cyan    (extended modules)
 *                PARTIAL_DEPLOY — static amber   (extended modules)
 *                CUT            — static red     (extended modules)
 *                ACTIVE_ALT     — second per-button active color
 *
 *              All brightness scaling for the ENABLED state is performed
 *              in software via KBC_scaleColor(). The tinyNeoPixel
 *              setBrightness() API is never called at runtime.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the KerbalButtonCore (KBC) library.
 *              Requires: tinyNeoPixel_Static library (megaTinyCore)
 *              Hardware: KC-01-1801/1802 and KC-01-1811/1812
 *              Protocol: I2C_Protocol_Specification.md v2.4
 */

#ifndef KBC_LEDCONTROL_H
#define KBC_LEDCONTROL_H

#include <Arduino.h>
#include <tinyNeoPixel_Static.h>
#include "KBC_Config.h"
#include "KBC_Colors.h"
#include "KBC_Protocol.h"

// ============================================================
//  KBCLEDControl class
// ============================================================

/**
 * @class  KBCLEDControl
 * @brief  Manages all LED output for a KBC button module.
 *
 *         Call begin() once in setup(), providing the per-button
 *         active color array defined in the module sketch.
 *
 *         Call update() regularly from the main loop to service
 *         flash timing for WARNING and ALERT states.
 *
 *         Call setLEDState() to apply a full 16-button state
 *         update from a CMD_SET_LED_STATE payload.
 *
 *         Call render() after any state change to push the
 *         current LED state to hardware.
 */
class KBCLEDControl {
public:

    // --------------------------------------------------------
    //  Construction
    // --------------------------------------------------------

    KBCLEDControl();

    // --------------------------------------------------------
    //  Initialisation
    // --------------------------------------------------------

    /**
     * @brief  Initialise LED hardware and load per-button active colors.
     *
     *         Must be called once in setup() after Wire.begin().
     *         Sets all buttons to OFF state and performs initial render.
     *
     * @param  activeColors  Pointer to array of KBC_BUTTON_COUNT RGBColor
     *                       values defining the ACTIVE color for each button.
     *                       Array must remain valid for the lifetime of this
     *                       object (declare as const in module sketch).
     * @param  altColors     Optional pointer to array of KBC_BUTTON_COUNT
     *                       RGBColor values for the ACTIVE_ALT state. Pass
     *                       nullptr to fall back to activeColors. Must remain
     *                       valid for the lifetime of this object.
     * @param  brightness    Initial ENABLED state brightness (0-255).
     *                       Defaults to KBC_ENABLED_BRIGHTNESS.
     */
    void begin(const RGBColor* activeColors,
               const RGBColor* altColors  = nullptr,
               uint8_t brightness = KBC_ENABLED_BRIGHTNESS);

    // --------------------------------------------------------
    //  State update
    // --------------------------------------------------------

    /**
     * @brief  Apply a full 16-button LED state update from a packed
     *         CMD_SET_LED_STATE payload (8 bytes, nibble-packed).
     *
     *         Unpacks all 16 nibble values and updates internal state.
     *         Does not render — call render() after this to push to hardware.
     *
     * @param  payload  Pointer to 8-byte nibble-packed LED state payload.
     *                  See KBC_ledPackGet() in KBC_Protocol.h for format.
     */
    void setLEDState(const uint8_t* payload);

    /**
     * @brief  Set the LED state for a single button by KBC index.
     *
     *         Does not render — call render() after this to push to hardware.
     *
     * @param  index  KBC button index (0-15).
     * @param  state  LED state nibble value (KBC_LED_*).
     */
    void setButtonState(uint8_t index, uint8_t state);

    /**
     * @brief  Set the ENABLED state brightness for NeoPixel buttons.
     *         Applied via software RGB scaling to the NeoPixel buttons
     *         (indices 0-11); indices 12-15 are switch inputs with no LED.
     *
     *         Does not render — call render() after this to push to hardware.
     *
     * @param  brightness  Brightness value 0-255.
     */
    void setBrightness(uint8_t brightness);

    /**
     * @brief  Set all buttons to OFF state.
     *         Does not render — call render() after this.
     */
    void clearAll();

    // --------------------------------------------------------
    //  Update and render
    // --------------------------------------------------------

    /**
     * @brief  Service flash timing for WARNING and ALERT states.
     *
     *         Must be called regularly from the main loop (or on a
     *         timer interrupt). Uses non-blocking millis() timing.
     *         Calls render() automatically when flash state changes.
     *
     * @return true if any flash state toggled this call (render was called).
     */
    bool update();

    /**
     * @brief  Push current LED state to hardware.
     *
     *         Writes all NeoPixel colors (indices 0-11) to the
     *         tinyNeoPixel buffer and calls show(). Indices 12-15 are
     *         switch inputs with no LED, so nothing is driven for them.
     *
     *         Relatively expensive on the ATtiny816 — only call when
     *         state has actually changed. The update() method calls
     *         this automatically for flash timing.
     */
    void render();

    // --------------------------------------------------------
    //  State query
    // --------------------------------------------------------

    /**
     * @brief  Returns the current LED state nibble for a button.
     * @param  index  KBC button index (0-15).
     * @return LED state nibble value (KBC_LED_*).
     */
    uint8_t getButtonState(uint8_t index) const;

    /**
     * @brief  Returns true if any button is in an extended state
     *         (WARNING, ALERT, ARMED, or PARTIAL_DEPLOY).
     */
    bool hasExtendedState() const;

    // --------------------------------------------------------
    //  Bulb test
    // --------------------------------------------------------

    /**
     * @brief  Illuminate all LEDs at full white for bulb test.
     *         Blocks for durationMs milliseconds then restores
     *         previous state and renders.
     *
     * @param  durationMs  Duration of bulb test in milliseconds.
     */
    void bulbTest(uint16_t durationMs = 2000);

private:

    // --------------------------------------------------------
    //  tinyNeoPixel_Static
    //
    //  Pixel buffer is declared here as a static array so it is
    //  visible at compile time — required by tinyNeoPixel_Static.
    //  3 bytes per pixel × 12 pixels = 36 bytes.
    // --------------------------------------------------------

    uint8_t _pixelBuffer[KBC_NEO_COUNT * 3];
    tinyNeoPixel _pixels;

    // --------------------------------------------------------
    //  Per-button state
    // --------------------------------------------------------

    /** @brief Current LED state nibble for each button (KBC_LED_*). */
    uint8_t _state[KBC_BUTTON_COUNT];

    /** @brief Pointer to per-button active color array (from sketch). */
    const RGBColor* _activeColors;

    /** @brief Pointer to per-button alternate active color array (optional;
     *         used by KBC_LED_ACTIVE_ALT). nullptr falls back to _activeColors. */
    const RGBColor* _altColors;

    /** @brief ENABLED state brightness scalar (0-255). */
    uint8_t _brightness;

    // --------------------------------------------------------
    //  Flash timing state
    // --------------------------------------------------------

    /** @brief Current flash phase: true = ON, false = OFF. */
    bool _flashOn;

    /** @brief millis() timestamp of last flash toggle. */
    uint32_t _flashLastToggle;

    // --------------------------------------------------------
    //  Internal helpers
    // --------------------------------------------------------

    /**
     * @brief  Compute the RGB color to display for a button given its
     *         current state and the flash phase.
     *
     * @param  index    KBC button index (0-15).
     * @param  flashOn  Current flash phase (true = ON).
     * @return RGBColor to write to the LED. {0,0,0} if off.
     */
    RGBColor _resolveColor(uint8_t index, bool flashOn) const;

    /**
     * @brief  Write a resolved color to a NeoPixel button (index 0-11).
     * @param  index  KBC button index (0-11).
     * @param  color  Color to write.
     */
    void _setNeoPixel(uint8_t index, RGBColor color);
};

#endif // KBC_LEDCONTROL_H

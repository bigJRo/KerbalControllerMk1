/**
 * @file        KBC_Config.h
 * @version     1.2
 * @date        2026-04-07
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Hardware pin definitions, timing constants, and default
 *              configuration values for the KerbalButtonCore library.
 *
 *              All values defined here reflect the common KBC PCB hardware
 *              design (KC-01-1822 v1.1) based on the ATtiny816-MNR
 *              microcontroller with megaTinyCore pin mapping, featuring:
 *                - 16 button inputs via two daisy-chained 74HC165 shift
 *                  registers (U14, U15) read via the ShiftIn<2> library
 *                - 12 RGB NeoPixel buttons driven by WS2811 ICs
 *                  (tinyNeoPixel_Static, PIN_PA4, NEO_RGB order)
 *                - 4 discrete LED outputs via 2N3904 NPN transistors
 *                  (buttons 13-16 on PCB, KBC indices 12-15)
 *                - I2C target interface (Wire library, PIN_PB1/PIN_PB0)
 *                - Active-low interrupt output (PIN_PA1)
 *
 *              Button index convention (canonical throughout all library code):
 *                KBC index 0-11  : NeoPixel RGB buttons (BUTTON01-12 on PCB)
 *                KBC index 12-15 : Discrete LED buttons (BUTTON13-16 on PCB)
 *              All protocol packets, LED state arrays, and ShiftIn remapping
 *              use this 0-15 index exclusively.
 *
 *              ATtiny816 no-connect pins (do not use):
 *                PA2 (pin 1), EXTCLK/PA3 (pin 2), PB4 (pin 10),
 *                PB2/TOSC2 (pin 12), PC3 (pin 18)
 *
 *              To override any default value for a specific module, define
 *              the constant before including KerbalButtonCore.h in the sketch:
 *
 *                #define KBC_ENABLED_BRIGHTNESS 64
 *                #include <KerbalButtonCore.h>
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Part of the KerbalButtonCore (KBC) library.
 *              Protocol defined in KBC_Protocol_Spec.md v1.1
 *              Schematic reference: KC-01-1821 v1.1
 */

#ifndef KBC_CONFIG_H
#define KBC_CONFIG_H

// ============================================================
//  ATtiny816 clock speed assertion
//
//  NeoPixel timing requires a minimum of 7.37 MHz.
//  10 MHz or above strongly recommended for timing margin.
//  tinyNeoPixel_Static port must match PIN_PA4 (Port A).
//  Verify: Tools → tinyNeoPixel Port → Port A in the IDE.
// ============================================================

#if F_CPU < 7372800UL
  #error "KBC: CPU clock too slow for NeoPixel timing. Minimum 7.37 MHz required."
#endif

// ============================================================
//  I2C pins (hardware fixed — ATtiny816 with megaTinyCore)
//
//  These are the hardware I2C pins on the ATtiny816. They are
//  managed by the Wire library and must not be used for other
//  purposes. Defined here for reference and documentation only.
//
//  Each module sketch defines its own I2C address via:
//    Wire.begin(KBC_I2C_ADDRESS)
//  before calling KerbalButtonCore::begin().
//  See KBC_Protocol.h for the address range (0x20-0x2E).
// ============================================================

/** @brief I2C SDA pin (ATtiny816 pin 13). Managed by Wire library. */
#define KBC_PIN_SDA             PIN_PB1

/** @brief I2C SCL pin (ATtiny816 pin 14). Managed by Wire library. */
#define KBC_PIN_SCL             PIN_PB0

/** @brief Recommended I2C bus speed in Hz. */
#define KBC_I2C_SPEED           400000UL

// ============================================================
//  Interrupt output pin
//
//  Active-low output. Driven low when the module has unread
//  button state changes. Driven high when the controller
//  completes a read transaction.
//
//  Connect to controller interrupt input with pull-up resistor.
// ============================================================

/** @brief Interrupt output pin, active low (ATtiny816 pin 20). */
#define KBC_PIN_INT             PIN_PA1

// ============================================================
//  Shift register pins (74HC165, U14 + U15 daisy-chained)
//
//  Two SN74HC165PWR ICs are daisy-chained to read 16 button
//  inputs. U15 (BUTTON09-16) feeds into U14's SER input.
//  U14's QH feeds DATA_IN to the ATtiny.
//
//  Read using ShiftIn<2> library (InfectedBytes/ArduinoShiftIn).
//  Constructor: shift.begin(LOAD, CLK_EN, DATA, CLK)
//
//  Read sequence:
//    1. CLK_EN HIGH  — disable clock
//    2. LOAD LOW     — latch all 16 inputs in parallel
//    3. LOAD HIGH    — end latch pulse
//    4. CLK_EN LOW   — enable clock
//    5. Read 16 bits — ShiftIn library handles clocking
//
//  ShiftIn<2> bit-to-KBC-index mapping (see KBC_ShiftReg.cpp):
//    shift.state(15..8) → U15 (A..H) → KBC index 8..15
//    shift.state(7..0)  → U14 (A..H) → KBC index 0..7
//    Note: within each byte, bit order is inverted relative to
//    input pin label (A=MSB, H=LSB per 74HC165 shift order).
//    Full remap handled by KBC_SR_BUTTON_MAP in KBC_ShiftReg.cpp.
// ============================================================

/** @brief Shift register parallel load pin, active low (ATtiny816 pin 8). */
#define KBC_PIN_SR_LOAD         PIN_PA7

/** @brief Shift register clock enable pin, active low (ATtiny816 pin 7). */
#define KBC_PIN_SR_CLK_EN       PIN_PA6

/** @brief Shift register clock input pin (ATtiny816 pin 9). */
#define KBC_PIN_SR_CLK          PIN_PB5

/** @brief Shift register serial data output pin (ATtiny816 pin 6). */
#define KBC_PIN_SR_DATA         PIN_PA5

// ============================================================
//  NeoPixel configuration (WS2811-based RGB buttons, U1-U12)
//
//  The 12 RGB buttons use WS2811 LED driver ICs, NOT WS2812.
//  The WS2811 uses RGB data order (not GRB).
//  NEO_RGB must be used — NEO_GRB will produce incorrect colors.
//
//  Pin must remain on Port A for tinyNeoPixel_Static timing
//  at CPU speeds below 16 MHz.
//  Verify: Tools → tinyNeoPixel Port → Port A in Arduino IDE.
//
//  NeoPixel indices 0-11 map directly to KBC button indices 0-11.
// ============================================================

/** @brief NeoPixel data output pin, Port A required (ATtiny816 pin 5). */
#define KBC_PIN_NEO             PIN_PA4

/** @brief Number of NeoPixels in the chain (RGB buttons KBC index 0-11). */
#define KBC_NEO_COUNT           12

/**
 * @brief NeoPixel color order.
 *        WS2811 ICs use RGB data order. Do not change to NEO_GRB.
 */
#define KBC_NEO_COLOR_ORDER     NEO_RGB

// ============================================================
//  Discrete LED pins (KBC button indices 12-15)
//
//  Four single-color LED outputs for buttons 13-16 (PCB labels).
//  Each LED is driven through a 2N3904 NPN transistor with a
//  470Ω collector resistor. The ATtiny drives the base through
//  a 10kΩ resistor.
//
//  IMPORTANT: These are transistor-switched, ON/OFF only.
//  No PWM brightness control is possible through this circuit.
//  The ENABLED dim state is not available for discrete buttons —
//  they will be fully ON when ENABLED and fully OFF when OFF.
//
//  Logic: ATtiny HIGH → transistor ON → LED ON (active high).
//
//  PCB label → KBC index → ATtiny pin mapping:
//    BUTTON13 / led_13  →  KBC index 12  →  PIN_PB3 (pin 11)
//    BUTTON14 / led_14  →  KBC index 13  →  PIN_PC0 (pin 15)
//    BUTTON15 / led_15  →  KBC index 14  →  PIN_PC2 (pin 17)
//    BUTTON16 / led_16  →  KBC index 15  →  PIN_PC1 (pin 16)
//
//  Note: led_15 (PC2, pin 17) and led_16 (PC1, pin 16) are
//  swapped relative to port pin order. Always use the explicit
//  KBC_DISCRETE_PINS array — never drive by sequential port loop.
// ============================================================

/** @brief Discrete LED pin for KBC button index 12 (PCB: BUTTON13/led_13). */
#define KBC_PIN_LED_12          PIN_PB3

/** @brief Discrete LED pin for KBC button index 13 (PCB: BUTTON14/led_14). */
#define KBC_PIN_LED_13          PIN_PC0

/** @brief Discrete LED pin for KBC button index 14 (PCB: BUTTON15/led_15). */
#define KBC_PIN_LED_14          PIN_PC2

/** @brief Discrete LED pin for KBC button index 15 (PCB: BUTTON16/led_16). */
#define KBC_PIN_LED_15          PIN_PC1

/**
 * @brief Discrete LED pin array indexed by KBC button index.
 *        Usage: digitalWrite(KBC_DISCRETE_PINS[kbcIndex - 12], state)
 *        Note: PC2/PC1 swap is handled here — do not reorder.
 */
#define KBC_DISCRETE_PINS       { KBC_PIN_LED_12, KBC_PIN_LED_13, \
                                  KBC_PIN_LED_14, KBC_PIN_LED_15 }

/** @brief Number of discrete LED buttons. */
#define KBC_DISCRETE_COUNT      4

/** @brief KBC index of the first discrete LED button. */
#define KBC_DISCRETE_INDEX_FIRST  12

// ============================================================
//  Shift register timing
//
//  Minimum pulse widths for reliable 74HC165 operation.
//  The ShiftIn library uses its own internal pulseWidth (default
//  5us). These constants are provided for reference and used
//  only if the ShiftIn pulseWidth is overridden in the sketch.
// ============================================================

/** @brief Minimum LOAD pulse width in microseconds. */
#define KBC_SR_LOAD_PULSE_US    5

/** @brief Minimum clock pulse width in microseconds. */
#define KBC_SR_CLK_PULSE_US     5

// ============================================================
//  Button polling and debounce
//
//  Buttons are polled on a software timer. A state change is
//  registered only after KBC_DEBOUNCE_COUNT consecutive polls
//  read the same new state, eliminating mechanical switch bounce.
//
//  Effective debounce window:
//    KBC_POLL_INTERVAL_MS × KBC_DEBOUNCE_COUNT
//    Default: 5ms × 4 = 20ms debounce window.
// ============================================================

/** @brief Button polling interval in milliseconds. */
#define KBC_POLL_INTERVAL_MS        5

/**
 * @brief Consecutive matching reads required to register a state change.
 *        Increase for noisier mechanical switches.
 */
#define KBC_DEBOUNCE_COUNT          4

// ============================================================
//  Sleep mode polling
//
//  When CMD_SLEEP is received, LEDs turn off and button state
//  changes do not assert INT. Polling continues at a reduced
//  rate so the module remains responsive to I2C commands
//  including CMD_WAKE.
// ============================================================

/** @brief Button polling interval in milliseconds during sleep mode. */
#define KBC_SLEEP_POLL_INTERVAL_MS  50

// ============================================================
//  LED brightness defaults
//
//  KBC_ENABLED_BRIGHTNESS applies to NeoPixel buttons only.
//  Discrete LED buttons are ON/OFF only — this value has no
//  effect on KBC button indices 12-15.
//
//  Scaling is performed in software via KBC_scaleColor() in
//  KBC_LEDControl. The tinyNeoPixel setBrightness() API is
//  never called at runtime to avoid destructive scaling loss.
//
//  Flash timing defaults apply to extended LED states and
//  may be overridden per module sketch before including
//  KerbalButtonCore.h.
// ============================================================

/**
 * @brief Default ENABLED state brightness for NeoPixel buttons (0-255).
 *        32 ≈ 12.5% — visible but non-distracting in a dark cockpit.
 *        Has no effect on discrete LED buttons (indices 12-15).
 *        Override: #define KBC_ENABLED_BRIGHTNESS 64
 */
#ifndef KBC_ENABLED_BRIGHTNESS
  #define KBC_ENABLED_BRIGHTNESS      32
#endif

/** @brief WARNING state flash on-time in milliseconds. */
#ifndef KBC_WARNING_ON_MS
  #define KBC_WARNING_ON_MS           500
#endif

/** @brief WARNING state flash off-time in milliseconds. */
#ifndef KBC_WARNING_OFF_MS
  #define KBC_WARNING_OFF_MS          500
#endif

/** @brief ALERT state flash on-time in milliseconds. */
#ifndef KBC_ALERT_ON_MS
  #define KBC_ALERT_ON_MS             150
#endif

/** @brief ALERT state flash off-time in milliseconds. */
#ifndef KBC_ALERT_OFF_MS
  #define KBC_ALERT_OFF_MS            150
#endif

/**
 * @brief Default bulb test duration in milliseconds.
 *        Applied when CMD_BULB_TEST is received with no payload or
 *        payload 0x01. The test blocks for this duration then restores
 *        the previous LED state. Override per module sketch if needed.
 */
#ifndef KBC_BULB_TEST_MS
  #define KBC_BULB_TEST_MS            2000
#endif

// ============================================================
//  Firmware version
//
//  Reported in the identity response packet via CMD_GET_IDENTITY.
//  Increment MINOR for backward-compatible changes.
//  Increment MAJOR and reset MINOR for breaking protocol changes.
// ============================================================

/** @brief Library firmware version — major. */
#define KBC_FIRMWARE_MAJOR          1

/** @brief Library firmware version — minor. */
#define KBC_FIRMWARE_MINOR          0

// ============================================================
//  Compile-time validation
// ============================================================

#if KBC_DEBOUNCE_COUNT < 1
  #error "KBC: KBC_DEBOUNCE_COUNT must be at least 1."
#endif

#if KBC_POLL_INTERVAL_MS < 1
  #error "KBC: KBC_POLL_INTERVAL_MS must be at least 1."
#endif

#if KBC_ENABLED_BRIGHTNESS > 255
  #error "KBC: KBC_ENABLED_BRIGHTNESS must be in range 0-255."
#endif

#if KBC_NEO_COUNT > 12
  #error "KBC: KBC_NEO_COUNT exceeds hardware maximum of 12 RGB buttons per module."
#endif

#endif // KBC_CONFIG_H

/**
 * @file        KCMk1_Module_Tester.ino
 * @version     1.0.0
 * @date        2026-04-18
 * @project     Kerbal Controller Mk1 — Unified Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Unified field validation tester for all Kerbal Controller Mk1
 *              I2C target modules. Runs on the KC-01-9001 Module Tester board
 *              (Seeed XIAO SAMD21).
 *
 *              On startup, scans I2C addresses 0x20-0x2F, queries identity
 *              from each responding device, and presents a selection menu if
 *              multiple modules are found. Once a target is selected the sketch
 *              loads the appropriate button label table and runs the correct
 *              test suite for that module type.
 *
 *              Serial menu commands (after module selection):
 *                [I] Identity -- print module identity and capability flags
 *                [B] Bulb test -- all LEDs full white for 2 seconds
 *                [L] LED cycle -- step all positions through all LED states
 *                [W] Wake / enable module
 *                [S] Sleep / disable module
 *                [R] Reset module
 *                [M] Monitor -- wait for INT and print button/axis events
 *                    During monitor: pressing a button prints its function
 *                    label and cycles its LED through all states.
 *                    Send [X] to exit monitor mode.
 *                [N] New module -- rescan bus and reselect
 *                [?] Show menu
 *
 * @note        Hardware: KC-01-9001 Module Tester v1.0 (XIAO SAMD21)
 *
 *              XIAO SAMD21 pin assignments (KC-01-9001):
 *                D0  -- SD_CS
 *                D1  -- TFT_CS
 *                D2  -- BACKLITE
 *                D3  -- RESET
 *                D4  -- SDA_3V3 -> I2C level shifter -> SDA_5V -> module
 *                D5  -- SCL_3V3 -> I2C level shifter -> SCL_5V -> module
 *                D6  -- INT_3V3 <- BSS138 level shifter <- INT_5V <- module
 *                D7  -- TFT_DC
 *                D8  -- SCK
 *                D9  -- MISO
 *                D10 -- MOSI
 *
 *              Wire.begin() uses D4/D5 by default on XIAO SAMD21.
 *              LED1 (green) -- power indicator, always on. Not controlled.
 *              LED2 (red)   -- INT indicator, driven by INT_5V via R2.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <Wire.h>

// ============================================================
//  Hardware pins
// ============================================================

#define PIN_INT_IN      6   // D6 -- INT_3V3, active low, pulled up to 3.3V

// ============================================================
//  Protocol constants
// ============================================================

#define CMD_GET_IDENTITY    0x01
#define CMD_SET_LED_STATE   0x02
#define CMD_SET_BRIGHTNESS  0x03
#define CMD_BULB_TEST       0x04
#define CMD_SLEEP           0x05
#define CMD_WAKE            0x06
#define CMD_RESET           0x07
#define CMD_ACK_FAULT       0x08
#define CMD_ENABLE          0x09
#define CMD_DISABLE         0x0A

#define LED_OFF             0x0
#define LED_ENABLED         0x1
#define LED_ACTIVE          0x2
#define LED_WARNING         0x3
#define LED_ALERT           0x4
#define LED_ARMED           0x5
#define LED_PARTIAL         0x6

#define IDENTITY_SIZE       4
#define ADDR_MIN            0x20
#define ADDR_MAX            0x2F

// ============================================================
//  Module Type IDs
// ============================================================

#define TYPE_UI_CONTROL         0x01
#define TYPE_FUNCTION_CONTROL   0x02
#define TYPE_ACTION_CONTROL     0x03
#define TYPE_STABILITY_CONTROL  0x04
#define TYPE_VEHICLE_CONTROL    0x05
#define TYPE_TIME_CONTROL       0x06
#define TYPE_EVA                0x07
#define TYPE_JOYSTICK_ROTATION  0x09
#define TYPE_JOYSTICK_TRANS     0x0A
#define TYPE_GPWS               0x0B
#define TYPE_PREWARP            0x0C
#define TYPE_THROTTLE           0x0D
#define TYPE_DUAL_ENCODER       0x0E
#define TYPE_SWITCH_PANEL       0x0F
#define TYPE_INDICATOR          0x10

// ============================================================
//  Packet and payload sizes by module type
// ============================================================

static uint8_t _packetSize(uint8_t typeId) {
    switch (typeId) {
        case TYPE_JOYSTICK_ROTATION:
        case TYPE_JOYSTICK_TRANS:   return 8;
        case TYPE_GPWS:
        case TYPE_PREWARP:          return 6;
        case TYPE_INDICATOR:        return 0;
        default:                    return 4;
    }
}

static uint8_t _ledPayloadSize(uint8_t typeId) {
    return (typeId == TYPE_INDICATOR) ? 9 : 8;
}

// ============================================================
//  Button / control label tables (PROGMEM)
// ============================================================

// -- UI Control (0x01) --
static const char ui_b00[] PROGMEM = "B0  Screenshot (F2->F1->F2 macro)";
static const char ui_b01[] PROGMEM = "B1  Debug (Alt+F12)";
static const char ui_b02[] PROGMEM = "B2  UI Toggle (F2)";
static const char ui_b03[] PROGMEM = "B3  Nav Ball Toggle (Numpad .)";
static const char ui_b04[] PROGMEM = "B4  Map Reset (apostrophe)";
static const char ui_b05[] PROGMEM = "B5  Navball Reference Cycle (NAVBALLMODE_MESSAGE)";
static const char ui_b06[] PROGMEM = "B6  Map Forward (Tab)";
static const char ui_b07[] PROGMEM = "B7  Ship Forward (])";
static const char ui_b08[] PROGMEM = "B8  Map Back (Shift+Tab)";
static const char ui_b09[] PROGMEM = "B9  Ship Back ([)";
static const char ui_b10[] PROGMEM = "B10 Map Enable (M)";
static const char ui_b11[] PROGMEM = "B11 IVA (C)";
static const char* const UI_LABELS[] PROGMEM = {
    ui_b00, ui_b01, ui_b02, ui_b03,
    ui_b04, ui_b05, ui_b06, ui_b07,
    ui_b08, ui_b09, ui_b10, ui_b11
};
#define UI_LABEL_COUNT 12

// -- Function Control (0x02) --
static const char fc_b00[] PROGMEM = "B0  LES - Launch Escape System (CAG 25)";
static const char fc_b01[] PROGMEM = "B1  Fairing Jettison (CAG 26)";
static const char fc_b02[] PROGMEM = "B2  Engine Alt Mode (CAG 27)";
static const char fc_b03[] PROGMEM = "B3  Science Collect (CAG 28)";
static const char fc_b04[] PROGMEM = "B4  Engine Group 1 (CAG 29)";
static const char fc_b05[] PROGMEM = "B5  Science Group 1 (CAG 30)";
static const char fc_b06[] PROGMEM = "B6  Engine Group 2 (CAG 31)";
static const char fc_b07[] PROGMEM = "B7  Science Group 2 (CAG 32)";
static const char fc_b08[] PROGMEM = "B8  Air Intake (CAG 33)";
static const char fc_b09[] PROGMEM = "B9  Lock Surfaces (CAG 34)";
static const char fc_b10[] PROGMEM = "B10 CP Toggle PRI/ALT (CAG 35/36)";
static const char fc_b11[] PROGMEM = "B11 CP Docking Port (CAG 37)";
static const char* const FC_LABELS[] PROGMEM = {
    fc_b00, fc_b01, fc_b02, fc_b03,
    fc_b04, fc_b05, fc_b06, fc_b07,
    fc_b08, fc_b09, fc_b10, fc_b11
};
#define FC_LABEL_COUNT 12

// -- Action Control (0x03) --
static const char ac_b00[] PROGMEM = "B0  AG6  (CAG6)  Col1 top";
static const char ac_b01[] PROGMEM = "B1  AG12 (CAG12) Col1 bottom";
static const char ac_b02[] PROGMEM = "B2  AG5  (CAG5)  Col2 top";
static const char ac_b03[] PROGMEM = "B3  AG11 (CAG11) Col2 bottom";
static const char ac_b04[] PROGMEM = "B4  AG4  (CAG4)  Col3 top";
static const char ac_b05[] PROGMEM = "B5  AG10 (CAG10) Col3 bottom";
static const char ac_b06[] PROGMEM = "B6  AG3  (CAG3)  Col4 top";
static const char ac_b07[] PROGMEM = "B7  AG9  (CAG9)  Col4 bottom";
static const char ac_b08[] PROGMEM = "B8  AG2  (CAG2)  Col5 top";
static const char ac_b09[] PROGMEM = "B9  AG8  (CAG8)  Col5 bottom";
static const char ac_b10[] PROGMEM = "B10 AG1  (CAG1)  Col6 top  [primary AG]";
static const char ac_b11[] PROGMEM = "B11 AG7  (CAG7)  Col6 bottom";
static const char ac_b12[] PROGMEM = "B12 Spacecraft Mode input (SPC switch)";
static const char ac_b14[] PROGMEM = "B14 Rover Mode input (RVR switch)";
static const char* const AC_LABELS[] PROGMEM = {
    ac_b00, ac_b01, ac_b02, ac_b03,
    ac_b04, ac_b05, ac_b06, ac_b07,
    ac_b08, ac_b09, ac_b10, ac_b11,
    ac_b12, nullptr, ac_b14, nullptr
};
#define AC_LABEL_COUNT 16

// -- Stability Control (0x04) --
static const char sc_b00[] PROGMEM = "B0  SAS Target";
static const char sc_b01[] PROGMEM = "B1  SAS Anti-Target";
static const char sc_b02[] PROGMEM = "B2  SAS Radial In";
static const char sc_b03[] PROGMEM = "B3  SAS Radial Out";
static const char sc_b04[] PROGMEM = "B4  SAS Normal";
static const char sc_b05[] PROGMEM = "B5  SAS Anti-Normal";
static const char sc_b06[] PROGMEM = "B6  SAS Prograde";
static const char sc_b07[] PROGMEM = "B7  SAS Retrograde";
static const char sc_b08[] PROGMEM = "B8  SAS Stability Assist";
static const char sc_b09[] PROGMEM = "B9  SAS Maneuver";
static const char sc_b11[] PROGMEM = "B11 Invert (F key pass-through)";
static const char sc_b12[] PROGMEM = "B12 SAS Enable (latching toggle)";
static const char sc_b13[] PROGMEM = "B13 RCS Enable (latching toggle)";
static const char* const SC_LABELS[] PROGMEM = {
    sc_b00, sc_b01, sc_b02, sc_b03,
    sc_b04, sc_b05, sc_b06, sc_b07,
    sc_b08, sc_b09, nullptr, sc_b11,
    sc_b12, sc_b13, nullptr, nullptr
};
#define SC_LABEL_COUNT 16

// -- Vehicle Control (0x05) --
static const char vc_b00[] PROGMEM = "B0  Brakes (BRAKES_ACTION) R1C2";
static const char vc_b01[] PROGMEM = "B1  Lights (LIGHTS_ACTION) R1C1";
static const char vc_b02[] PROGMEM = "B2  Antenna (CAG13) R2C2";
static const char vc_b03[] PROGMEM = "B3  Gear (GEAR_ACTION) R2C1";
static const char vc_b04[] PROGMEM = "B4  Fuel Cell (CAG14) R3C2";
static const char vc_b05[] PROGMEM = "B5  Solar Array (CAG15) R3C1";
static const char vc_b06[] PROGMEM = "B6  Cargo Door (CAG16) R4C2";
static const char vc_b07[] PROGMEM = "B7  Radiator (CAG17) R4C1";
static const char vc_b08[] PROGMEM = "B8  Heat Shield (CAG19/20) R5C2 [state machine]";
static const char vc_b09[] PROGMEM = "B9  Ladder (CAG18) R5C1";
static const char vc_b10[] PROGMEM = "B10 Main Chute (CAG21/22) R6C2 [state machine+interlock]";
static const char vc_b11[] PROGMEM = "B11 Drogue Chute (CAG23/24) R6C1 [state machine+interlock]";
static const char vc_b12[] PROGMEM = "B12 Parking Brake (latching toggle)";
static const char vc_b13[] PROGMEM = "B13 Parachutes Armed (interlock toggle)";
static const char vc_b14[] PROGMEM = "B14 Lights Lock (latching toggle)";
static const char vc_b15[] PROGMEM = "B15 Gear Lock (latching toggle)";
static const char* const VC_LABELS[] PROGMEM = {
    vc_b00, vc_b01, vc_b02, vc_b03,
    vc_b04, vc_b05, vc_b06, vc_b07,
    vc_b08, vc_b09, vc_b10, vc_b11,
    vc_b12, vc_b13, vc_b14, vc_b15
};
#define VC_LABEL_COUNT 16

// -- Time Control (0x06) --
static const char tc_b00[] PROGMEM = "B0  Warp to Apoapsis";
static const char tc_b01[] PROGMEM = "B1  Warp Stop";
static const char tc_b02[] PROGMEM = "B2  Warp to Periapsis";
static const char tc_b03[] PROGMEM = "B3  Physics Warp modifier (hold + B5/B7)";
static const char tc_b04[] PROGMEM = "B4  Warp to Maneuver";
static const char tc_b05[] PROGMEM = "B5  Warp Forward";
static const char tc_b06[] PROGMEM = "B6  Warp to SOI";
static const char tc_b07[] PROGMEM = "B7  Warp Back";
static const char tc_b08[] PROGMEM = "B8  Warp to Morning";
static const char tc_b09[] PROGMEM = "B9  Quick Load (F9)";
static const char tc_b10[] PROGMEM = "B10 Pause (Escape)";
static const char tc_b11[] PROGMEM = "B11 Quick Save (F5)";
static const char* const TC_LABELS[] PROGMEM = {
    tc_b00, tc_b01, tc_b02, tc_b03,
    tc_b04, tc_b05, tc_b06, tc_b07,
    tc_b08, tc_b09, tc_b10, tc_b11
};
#define TC_LABEL_COUNT 12

// -- EVA Module (0x07) --
static const char ev_b00[] PROGMEM = "B0  EVA Lights (U)";
static const char ev_b01[] PROGMEM = "B1  Jetpack Enable (R)";
static const char ev_b02[] PROGMEM = "B2  Board Craft (B)";
static const char ev_b03[] PROGMEM = "B3  EVA Construction (I)";
static const char ev_b04[] PROGMEM = "B4  Jump / Let Go (Space)";
static const char ev_b05[] PROGMEM = "B5  Grab (F)";
static const char ev_b06[] PROGMEM = "B6  EVA Chute (P)";
static const char ev_b07[] PROGMEM = "B7  Helmet Toggle (O)";
static const char* const EV_LABELS[] PROGMEM = {
    ev_b00, ev_b01, ev_b02, ev_b03,
    ev_b04, ev_b05, ev_b06, ev_b07
};
#define EV_LABEL_COUNT 8

// -- Joystick Rotation (0x09) buttons only --
static const char jr_b00[] PROGMEM = "BTN_JOY  Airbrake Toggle (CAG38)";
static const char jr_b01[] PROGMEM = "BTN01    Reset Trim (Alt+X) [GREEN]";
static const char jr_b02[] PROGMEM = "BTN02    Set Trim [AMBER]";
static const char* const JR_LABELS[] PROGMEM = {
    jr_b00, jr_b01, jr_b02
};
#define JR_LABEL_COUNT 3

// -- Joystick Translation (0x0A) buttons only --
static const char jt_b00[] PROGMEM = "BTN_JOY  Camera Control mode (hold)";
static const char jt_b01[] PROGMEM = "BTN01    Cycle Camera Mode [MAGENTA]";
static const char jt_b02[] PROGMEM = "BTN02    Camera Reset [GREEN]";
static const char* const JT_LABELS[] PROGMEM = {
    jt_b00, jt_b01, jt_b02
};
#define JT_LABEL_COUNT 3

// -- GPWS (0x0B) --
static const char gp_b00[] PROGMEM = "BTN01  GPWS Enable (3-state: OFF/ACTIVE/PROXIMITY)";
static const char gp_b01[] PROGMEM = "BTN02  Proximity Alarm toggle";
static const char gp_b02[] PROGMEM = "BTN03  Rendezvous Radar toggle";
static const char gp_b03[] PROGMEM = "BTN_EN Encoder button (reset threshold to 200m)";
static const char* const GP_LABELS[] PROGMEM = {
    gp_b00, gp_b01, gp_b02, gp_b03
};
#define GP_LABEL_COUNT 4

// -- Pre-Warp Time (0x0C) --
static const char pw_b00[] PROGMEM = "BTN01  5-minute preset";
static const char pw_b01[] PROGMEM = "BTN02  1-hour preset (60 min)";
static const char pw_b02[] PROGMEM = "BTN03  1-day preset (1440 min)";
static const char pw_b03[] PROGMEM = "BTN_EN Encoder button (reset to 0)";
static const char* const PW_LABELS[] PROGMEM = {
    pw_b00, pw_b01, pw_b02, pw_b03
};
#define PW_LABEL_COUNT 4

// -- Throttle (0x0D) --
static const char th_b00[] PROGMEM = "THRTL_100  Set throttle 100%";
static const char th_b01[] PROGMEM = "THRTL_UP   Step throttle up 5%";
static const char th_b02[] PROGMEM = "THRTL_DOWN Step throttle down 5%";
static const char th_b03[] PROGMEM = "THRTL_00   Set throttle 0%";
static const char* const TH_LABELS[] PROGMEM = {
    th_b00, th_b01, th_b02, th_b03
};
#define TH_LABEL_COUNT 4

// -- Dual Encoder (0x0E) --
static const char de_b00[] PROGMEM = "ENC1_SW  Docked flag toggle (controller-side sync)";
static const char de_b01[] PROGMEM = "ENC2_SW  Camera reset (Hullcam VDS Backspace)";
static const char* const DE_LABELS[] PROGMEM = {
    de_b00, de_b01
};
#define DE_LABEL_COUNT 2

// -- Switch Panel (0x0F) --
static const char sw_b00[] PROGMEM = "SW1  Bit0  MODE-CTRL (3-pos latching w/SW2)";
static const char sw_b01[] PROGMEM = "SW2  Bit1  MODE-DEMO (both low=DBG)";
static const char sw_b02[] PROGMEM = "SW3  Bit2  MSTR Master Reset (rising edge only)";
static const char sw_b03[] PROGMEM = "SW4  Bit3  DISPLAY Reset (rising edge only)";
static const char sw_b04[] PROGMEM = "SW5  Bit4  SCE to Auxiliary";
static const char sw_b05[] PROGMEM = "SW6  Bit5  LTG Bulb Test (rise=start fall=stop)";
static const char sw_b06[] PROGMEM = "SW7  Bit6  AUDIO On";
static const char sw_b07[] PROGMEM = "SW8  Bit7  INPUT Precision";
static const char sw_b08[] PROGMEM = "SW9  Bit8  ENGINE Arm";
static const char sw_b09[] PROGMEM = "SW10 Bit9  THRTL Fine Control";
static const char* const SW_LABELS[] PROGMEM = {
    sw_b00, sw_b01, sw_b02, sw_b03,
    sw_b04, sw_b05, sw_b06, sw_b07,
    sw_b08, sw_b09
};
#define SW_LABEL_COUNT 10

// -- Indicator (0x10) --
static const char in_b00[] PROGMEM = "B0  THRTL ENA    GREEN  SW9+throttle enabled";
static const char in_b01[] PROGMEM = "B1  LIGHT ENA    YELLOW LIGHTS_ACTION";
static const char in_b02[] PROGMEM = "B2  CTRL         LIME   MODE=CTRL";
static const char in_b03[] PROGMEM = "B3  THRTL PREC   MINT   SW10 ON";
static const char in_b04[] PROGMEM = "B4  BRAKE LOCK   RED    Parking Brake HIGH";
static const char in_b05[] PROGMEM = "B5  DEBUG        ROSE   MODE=DBG";
static const char in_b06[] PROGMEM = "B6  PREC INPUT   CYAN   SW8 ON";
static const char in_b07[] PROGMEM = "B7  LNDG GEAR    GREEN  Gear Lock HIGH";
static const char in_b08[] PROGMEM = "B8  DEMO         SKY    MODE=DEMO";
static const char in_b09[] PROGMEM = "B9  AUDIO        PURPLE SW7 ON";
static const char in_b10[] PROGMEM = "B10 CHUTE ARM    AMBER  Parachutes Armed HIGH";
static const char in_b11[] PROGMEM = "B11 COMM ACTIVE  TEAL   Simpit connected";
static const char in_b12[] PROGMEM = "B12 SCE AUX      ORANGE SW5 ON";
static const char in_b13[] PROGMEM = "B13 RCS          MINT   RCS_ACTION";
static const char in_b14[] PROGMEM = "B14 SWITCH ERROR RED    Toggle/game mismatch";
static const char in_b15[] PROGMEM = "B15 ABORT        RED    ABORT_ACTION (ALERT=flash)";
static const char in_b16[] PROGMEM = "B16 SAS          GREEN  SAS_ACTION";
static const char in_b17[] PROGMEM = "B17 AUTO PILOT   BLUE   TBD future";
static const char* const IN_LABELS[] PROGMEM = {
    in_b00, in_b01, in_b02, in_b03,
    in_b04, in_b05, in_b06, in_b07,
    in_b08, in_b09, in_b10, in_b11,
    in_b12, in_b13, in_b14, in_b15,
    in_b16, in_b17
};
#define IN_LABEL_COUNT 18

// ============================================================
//  Module descriptor
// ============================================================

struct ModuleInfo {
    uint8_t address;
    uint8_t typeId;
    uint8_t fwMajor;
    uint8_t fwMinor;
    uint8_t capFlags;
};

#define MAX_MODULES 16
static ModuleInfo _found[MAX_MODULES];
static uint8_t    _foundCount    = 0;
static ModuleInfo _target;
static bool       _targetSelected = false;

// Per-position LED state for monitor cycling
static uint8_t _ledState[18];

// ============================================================
//  Utility
// ============================================================

static const char* _typeName(uint8_t typeId) {
    switch (typeId) {
        case TYPE_UI_CONTROL:        return "UI Control";
        case TYPE_FUNCTION_CONTROL:  return "Function Control";
        case TYPE_ACTION_CONTROL:    return "Action Control";
        case TYPE_STABILITY_CONTROL: return "Stability Control";
        case TYPE_VEHICLE_CONTROL:   return "Vehicle Control";
        case TYPE_TIME_CONTROL:      return "Time Control";
        case TYPE_EVA:               return "EVA Module";
        case TYPE_JOYSTICK_ROTATION: return "Joystick Rotation";
        case TYPE_JOYSTICK_TRANS:    return "Joystick Translation";
        case TYPE_GPWS:              return "GPWS Input Panel";
        case TYPE_PREWARP:           return "Pre-Warp Time";
        case TYPE_THROTTLE:          return "Throttle Module";
        case TYPE_DUAL_ENCODER:      return "Dual Encoder";
        case TYPE_SWITCH_PANEL:      return "Switch Panel";
        case TYPE_INDICATOR:         return "Indicator Module";
        default:                     return "Unknown";
    }
}

static void _printLabel(const char* const* table,
                         uint8_t idx, uint8_t tableSize) {
    if (idx >= tableSize || table == nullptr) {
        Serial.print(F("B")); Serial.print(idx);
        Serial.print(F("  (no label)"));
        return;
    }
    const char* ptr = (const char*)pgm_read_ptr(&table[idx]);
    if (ptr == nullptr) {
        Serial.print(F("B")); Serial.print(idx);
        Serial.print(F("  (input only / not installed)"));
        return;
    }
    char buf[72];
    strncpy_P(buf, ptr, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    Serial.print(buf);
}

static void _printButtonLabel(uint8_t typeId, uint8_t idx) {
    switch (typeId) {
        case TYPE_UI_CONTROL:
            _printLabel(UI_LABELS, idx, UI_LABEL_COUNT); break;
        case TYPE_FUNCTION_CONTROL:
            _printLabel(FC_LABELS, idx, FC_LABEL_COUNT); break;
        case TYPE_ACTION_CONTROL:
            _printLabel(AC_LABELS, idx, AC_LABEL_COUNT); break;
        case TYPE_STABILITY_CONTROL:
            _printLabel(SC_LABELS, idx, SC_LABEL_COUNT); break;
        case TYPE_VEHICLE_CONTROL:
            _printLabel(VC_LABELS, idx, VC_LABEL_COUNT); break;
        case TYPE_TIME_CONTROL:
            _printLabel(TC_LABELS, idx, TC_LABEL_COUNT); break;
        case TYPE_EVA:
            _printLabel(EV_LABELS, idx, EV_LABEL_COUNT); break;
        case TYPE_JOYSTICK_ROTATION:
            _printLabel(JR_LABELS, idx, JR_LABEL_COUNT); break;
        case TYPE_JOYSTICK_TRANS:
            _printLabel(JT_LABELS, idx, JT_LABEL_COUNT); break;
        case TYPE_GPWS:
            _printLabel(GP_LABELS, idx, GP_LABEL_COUNT); break;
        case TYPE_PREWARP:
            _printLabel(PW_LABELS, idx, PW_LABEL_COUNT); break;
        case TYPE_THROTTLE:
            _printLabel(TH_LABELS, idx, TH_LABEL_COUNT); break;
        case TYPE_DUAL_ENCODER:
            _printLabel(DE_LABELS, idx, DE_LABEL_COUNT); break;
        case TYPE_SWITCH_PANEL:
            _printLabel(SW_LABELS, idx, SW_LABEL_COUNT); break;
        case TYPE_INDICATOR:
            _printLabel(IN_LABELS, idx, IN_LABEL_COUNT); break;
        default:
            Serial.print(F("B")); Serial.print(idx); break;
    }
}

// ============================================================
//  I2C helpers
// ============================================================

static void _send(uint8_t addr, uint8_t cmd) {
    Wire.beginTransmission(addr);
    Wire.write(cmd);
    Wire.endTransmission();
}

static void _send(uint8_t addr, uint8_t cmd, uint8_t payload) {
    Wire.beginTransmission(addr);
    Wire.write(cmd);
    Wire.write(payload);
    Wire.endTransmission();
}

static void _send(uint8_t addr, uint8_t cmd,
                  const uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(addr);
    Wire.write(cmd);
    Wire.write(buf, len);
    Wire.endTransmission();
}

static bool _readIdentity(uint8_t addr, uint8_t* buf) {
    _send(addr, CMD_GET_IDENTITY);
    delay(2);
    uint8_t n = Wire.requestFrom(addr, (uint8_t)IDENTITY_SIZE);
    if (n != IDENTITY_SIZE) return false;
    for (uint8_t i = 0; i < IDENTITY_SIZE; i++) buf[i] = Wire.read();
    return true;
}

static bool _intAsserted() {
    return digitalRead(PIN_INT_IN) == LOW;
}

// ============================================================
//  LED payload helpers
// ============================================================

static void _payloadAll(uint8_t* buf, uint8_t len, uint8_t state) {
    uint8_t packed = (state << 4) | state;
    for (uint8_t i = 0; i < len; i++) buf[i] = packed;
}

static void _payloadOne(uint8_t* buf, uint8_t len,
                         uint8_t idx, uint8_t state) {
    memset(buf, 0, len);
    uint8_t byte = idx / 2;
    if (byte >= len) return;
    if (idx % 2 == 0)
        buf[byte] = (uint8_t)(state << 4);
    else
        buf[byte] = state;
}

// ============================================================
//  I2C scan
// ============================================================

static void _scanBus() {
    Serial.println(F("\nScanning I2C bus (0x20-0x2F)..."));
    _foundCount = 0;

    for (uint8_t addr = ADDR_MIN; addr <= ADDR_MAX; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() != 0) { delay(5); continue; }

        uint8_t id[IDENTITY_SIZE];
        if (_readIdentity(addr, id)) {
            ModuleInfo m;
            m.address  = addr;
            m.typeId   = id[0];
            m.fwMajor  = id[1];
            m.fwMinor  = id[2];
            m.capFlags = id[3];
            if (_foundCount < MAX_MODULES) _found[_foundCount++] = m;

            Serial.print(F("  ["));
            Serial.print(_foundCount);
            Serial.print(F("] 0x"));
            if (addr < 0x10) Serial.print('0');
            Serial.print(addr, HEX);
            Serial.print(F(" -- "));
            Serial.print(_typeName(id[0]));
            Serial.print(F("  FW "));
            Serial.print(id[1]); Serial.print('.'); Serial.println(id[2]);
        } else {
            Serial.print(F("  0x"));
            if (addr < 0x10) Serial.print('0');
            Serial.print(addr, HEX);
            Serial.println(F(" -- responded but identity failed"));
        }
        delay(5);
    }

    if (_foundCount == 0)
        Serial.println(F("  No modules found. Check wiring and power."));
}

// ============================================================
//  Module selection
// ============================================================

static void _selectModule() {
    if (_foundCount == 0) return;

    if (_foundCount == 1) {
        _target = _found[0];
        _targetSelected = true;
        Serial.print(F("\nAuto-selected: "));
        Serial.println(_typeName(_target.typeId));
        return;
    }

    Serial.println(F("\nMultiple modules found. Enter number to select:"));
    Serial.print(F("> "));

    while (true) {
        if (!Serial.available()) continue;
        char c = Serial.read();
        while (Serial.available()) Serial.read();
        uint8_t sel = (uint8_t)(c - '1');
        if (sel < _foundCount) {
            _target = _found[sel];
            _targetSelected = true;
            Serial.print(F("Selected: "));
            Serial.println(_typeName(_target.typeId));
            return;
        }
        Serial.println(F("Invalid. Try again:"));
        Serial.print(F("> "));
    }
}

// ============================================================
//  Tests
// ============================================================

static void _testIdentity() {
    Serial.println(F("\n-- Identity --"));
    uint8_t id[IDENTITY_SIZE];
    if (!_readIdentity(_target.address, id)) {
        Serial.println(F("  ERROR: read failed")); return;
    }
    Serial.print(F("  Address:  0x"));
    if (_target.address < 0x10) Serial.print('0');
    Serial.println(_target.address, HEX);
    Serial.print(F("  Type:     0x"));
    if (id[0] < 0x10) Serial.print('0');
    Serial.print(id[0], HEX);
    Serial.print(F("  (")); Serial.print(_typeName(id[0])); Serial.println(')');
    Serial.print(F("  Firmware: ")); Serial.print(id[1]); Serial.print('.');
    Serial.println(id[2]);
    Serial.print(F("  CapFlags: 0x"));
    if (id[3] < 0x10) Serial.print('0');
    Serial.println(id[3], HEX);
    if (id[3] & 0x01) Serial.println(F("    bit0: Extended LED states"));
    if (id[3] & 0x02) Serial.println(F("    bit1: Fault active"));
    if (id[3] & 0x04) Serial.println(F("    bit2: Encoder data in packet"));
    if (id[3] & 0x08) Serial.println(F("    bit3: Joystick axes in packet"));
    if (id[3] & 0x10) Serial.println(F("    bit4: 7-segment display"));
    if (id[3] & 0x20) Serial.println(F("    bit5: Motorized position control"));
    Serial.print(F("  Match:    "));
    Serial.println(id[0] == _target.typeId ? F("OK") : F("MISMATCH"));
}

static void _testBulbTest() {
    Serial.println(F("\n-- Bulb Test --"));
    Serial.println(F("  Sending CMD_BULB_TEST -- all LEDs white for 2s..."));
    _send(_target.address, CMD_BULB_TEST);
    delay(2200);
    Serial.println(F("  Done."));
}

static void _testLEDCycle() {
    Serial.println(F("\n-- LED State Cycle --"));
    uint8_t payloadLen = _ledPayloadSize(_target.typeId);
    uint8_t payload[9];
    const uint8_t states[]     = { LED_OFF, LED_ENABLED, LED_ACTIVE,
                                   LED_WARNING, LED_ALERT, LED_ARMED, LED_PARTIAL };
    const char*   stateNames[] = { "OFF", "ENABLED", "ACTIVE",
                                   "WARNING", "ALERT", "ARMED", "PARTIAL" };
    _send(_target.address, CMD_ENABLE);
    delay(50);
    for (uint8_t s = 0; s < 7; s++) {
        Serial.print(F("  -> ")); Serial.println(stateNames[s]);
        _payloadAll(payload, payloadLen, states[s]);
        _send(_target.address, CMD_SET_LED_STATE, payload, payloadLen);
        delay(900);
    }
    _payloadAll(payload, payloadLen, LED_OFF);
    _send(_target.address, CMD_SET_LED_STATE, payload, payloadLen);
    Serial.println(F("  Cycle complete."));
}

static void _testReset() {
    Serial.println(F("\n-- Reset --"));
    _send(_target.address, CMD_RESET);
    delay(20);
    bool intClear = (digitalRead(PIN_INT_IN) == HIGH);
    Serial.print(F("  INT: "));
    Serial.println(intClear ? F("HIGH (OK)") : F("LOW (unexpected)"));
    Serial.println(F("  Visual: all LEDs should be OFF."));
    delay(100);
    _send(_target.address, CMD_ENABLE);
    Serial.println(F("  CMD_ENABLE sent to restore operation."));
}

// ============================================================
//  Monitor modes
// ============================================================

// Shared exit check
static bool _checkExit() {
    if (!Serial.available()) return false;
    char c = Serial.read();
    while (Serial.available()) Serial.read();
    return (c == 'X' || c == 'x');
}

// Standard 4-byte packet modules
static void _monitorStandard() {
    Serial.println(F("  Waiting for button events (press buttons on module)..."));
    Serial.println(F("  Each press cycles that button LED through all states."));
    Serial.println(F("  Send [X] to exit.\n"));

    uint8_t payloadLen = _ledPayloadSize(_target.typeId);
    uint8_t payload[9];
    memset(_ledState, LED_ENABLED, sizeof(_ledState));
    _payloadAll(payload, payloadLen, LED_ENABLED);
    _send(_target.address, CMD_SET_LED_STATE, payload, payloadLen);

    while (true) {
        if (_checkExit()) break;
        if (!_intAsserted()) { delay(5); continue; }

        uint8_t pkt[4];
        if (Wire.requestFrom(_target.address, (uint8_t)4) != 4) {
            delay(10); continue;
        }
        for (uint8_t i = 0; i < 4; i++) pkt[i] = Wire.read();

        uint16_t state  = ((uint16_t)pkt[0] << 8) | pkt[1];
        uint16_t change = ((uint16_t)pkt[2] << 8) | pkt[3];

        for (uint8_t bit = 0; bit < 16; bit++) {
            if (!(change & (1u << bit))) continue;
            bool pressed = (state >> bit) & 1;
            Serial.print(pressed ? F("  [ON]  ") : F("  [OFF] "));
            _printButtonLabel(_target.typeId, bit);
            Serial.println();

            if (pressed) {
                _ledState[bit] = (_ledState[bit] + 1) % 7;
                _payloadOne(payload, payloadLen, bit, _ledState[bit]);
                _send(_target.address, CMD_SET_LED_STATE, payload, payloadLen);
            }
        }
        delay(5);
    }

    _payloadAll(payload, payloadLen, LED_OFF);
    _send(_target.address, CMD_SET_LED_STATE, payload, payloadLen);
    Serial.println(F("  Monitor exited."));
}

// Joystick 8-byte packet
static void _monitorJoystick() {
    bool isRotation = (_target.typeId == TYPE_JOYSTICK_ROTATION);
    Serial.println(F("  Move axes and press buttons. Send [X] to exit.\n"));
    if (isRotation) {
        Serial.println(F("  Spacecraft: AXIS1=Yaw  AXIS2=Pitch  AXIS3=Roll"));
        Serial.println(F("  Airplane:   AXIS1=Roll AXIS2=Pitch  AXIS3=Yaw"));
        Serial.println(F("  Rover:      AXIS1=Steer AXIS2=WhlThrottle AXIS3=N/A"));
    } else {
        Serial.println(F("  Translation: AXIS1=L/R  AXIS2=U/D(inverted fw)  AXIS3=Fwd/Rev(CW)"));
        Serial.println(F("  Camera(hold BTN_JOY): AXIS1=Yaw  AXIS2=Pitch  AXIS3=Zoom"));
    }
    Serial.println();

    uint8_t payload[8];
    memset(_ledState, LED_ENABLED, 3);
    _payloadAll(payload, 8, LED_ENABLED);
    _send(_target.address, CMD_SET_LED_STATE, payload, 8);

    int16_t lastAxis[3] = {0, 0, 0};

    while (true) {
        if (_checkExit()) break;
        if (!_intAsserted()) { delay(10); continue; }

        uint8_t pkt[8];
        if (Wire.requestFrom(_target.address, (uint8_t)8) != 8) {
            delay(10); continue;
        }
        for (uint8_t i = 0; i < 8; i++) pkt[i] = Wire.read();

        uint8_t btnState  = pkt[0];
        uint8_t btnChange = pkt[1];
        int16_t axis[3];
        for (uint8_t i = 0; i < 3; i++)
            axis[i] = (int16_t)(((uint16_t)pkt[2+i*2] << 8) | pkt[3+i*2]);

        // Buttons
        for (uint8_t bit = 0; bit < 3; bit++) {
            if (!(btnChange & (1u << bit))) continue;
            bool pressed = (btnState >> bit) & 1;
            Serial.print(pressed ? F("  [PRESS] ") : F("  [REL]   "));
            _printButtonLabel(_target.typeId, bit);
            Serial.println();
            if (pressed) {
                _ledState[bit] = (_ledState[bit] + 1) % 7;
                _payloadOne(payload, 8, bit, _ledState[bit]);
                _send(_target.address, CMD_SET_LED_STATE, payload, 8);
            }
        }

        // Axes (print when change > threshold)
        bool anyChanged = false;
        for (uint8_t i = 0; i < 3; i++)
            if (abs(axis[i] - lastAxis[i]) > 300) anyChanged = true;
        if (anyChanged) {
            Serial.print(F("  AXIS1=")); Serial.print(axis[0]);
            Serial.print(F("  AXIS2=")); Serial.print(axis[1]);
            Serial.print(F("  AXIS3=")); Serial.println(axis[2]);
            for (uint8_t i = 0; i < 3; i++) lastAxis[i] = axis[i];
        }
        delay(20);
    }

    _payloadAll(payload, 8, LED_OFF);
    _send(_target.address, CMD_SET_LED_STATE, payload, 8);
    Serial.println(F("  Monitor exited."));
}

// Display 6-byte packet (GPWS / Pre-Warp)
static void _monitorDisplay() {
    Serial.println(F("  Turn encoder or press buttons. Send [X] to exit.\n"));

    uint8_t payload[8];
    _payloadAll(payload, 8, LED_ENABLED);
    _send(_target.address, CMD_SET_LED_STATE, payload, 8);

    uint16_t lastValue = 0xFFFF;

    while (true) {
        if (_checkExit()) break;
        if (!_intAsserted()) { delay(5); continue; }

        uint8_t pkt[6];
        if (Wire.requestFrom(_target.address, (uint8_t)6) != 6) {
            delay(10); continue;
        }
        for (uint8_t i = 0; i < 6; i++) pkt[i] = Wire.read();

        uint8_t  btnEvts = pkt[0];
        uint8_t  btnChg  = pkt[1];
        uint16_t value   = ((uint16_t)pkt[4] << 8) | pkt[5];

        for (uint8_t bit = 0; bit < 4; bit++) {
            if (!(btnChg & (1u << bit))) continue;
            if (!((btnEvts >> bit) & 1)) continue;
            Serial.print(F("  [PRESS] "));
            _printButtonLabel(_target.typeId, bit);
            Serial.println();
        }

        if (value != lastValue) {
            Serial.print(F("  Value: ")); Serial.println(value);
            lastValue = value;
        }
        delay(10);
    }

    _payloadAll(payload, 8, LED_OFF);
    _send(_target.address, CMD_SET_LED_STATE, payload, 8);
    Serial.println(F("  Monitor exited."));
}

// Throttle 4-byte packet
static void _monitorThrottle() {
    Serial.println(F("  Move slider or press buttons. Send [X] to exit.\n"));
    _send(_target.address, CMD_ENABLE);
    delay(100);

    uint16_t lastThrottle = 0xFFFF;

    while (true) {
        if (_checkExit()) break;
        if (!_intAsserted()) { delay(5); continue; }

        uint8_t pkt[4];
        if (Wire.requestFrom(_target.address, (uint8_t)4) != 4) {
            delay(10); continue;
        }
        for (uint8_t i = 0; i < 4; i++) pkt[i] = Wire.read();

        uint8_t  flags    = pkt[0];
        uint8_t  btnEvts  = pkt[1];
        uint16_t throttle = ((uint16_t)pkt[2] << 8) | pkt[3];

        // Status flags
        Serial.print(F("  Flags:"));
        if (flags & 0x01) Serial.print(F(" ENABLED"));
        if (flags & 0x02) Serial.print(F(" PRECISION"));
        if (flags & 0x04) Serial.print(F(" PILOT_TOUCH"));
        if (flags & 0x08) Serial.print(F(" MOTOR_MOVING"));
        Serial.println();

        // Buttons (rising edge)
        for (uint8_t bit = 0; bit < 4; bit++) {
            if (!((btnEvts >> bit) & 1)) continue;
            Serial.print(F("  [PRESS] "));
            _printButtonLabel(TYPE_THROTTLE, bit);
            Serial.println();
        }

        // Throttle value
        if ((uint16_t)abs((int32_t)throttle - (int32_t)lastThrottle) > 100) {
            uint8_t pct = (uint8_t)((uint32_t)throttle * 100UL / 32767UL);
            Serial.print(F("  Throttle: "));
            Serial.print(throttle);
            Serial.print(F(" (")); Serial.print(pct); Serial.println(F("%)"));
            lastThrottle = throttle;
        }
        delay(20);
    }
    Serial.println(F("  Monitor exited."));
}

// Dual encoder 4-byte packet
static void _monitorDualEncoder() {
    Serial.println(F("  Turn or press encoders. Send [X] to exit.\n"));
    Serial.println(F("  ENC1: AG block selector (CW=next, CCW=prev)"));
    Serial.println(F("  ENC2: Camera (CW=next '-', CCW=prev '=')"));
    Serial.println();

    while (true) {
        if (_checkExit()) break;
        if (!_intAsserted()) { delay(5); continue; }

        uint8_t pkt[4];
        if (Wire.requestFrom(_target.address, (uint8_t)4) != 4) {
            delay(10); continue;
        }
        for (uint8_t i = 0; i < 4; i++) pkt[i] = Wire.read();

        uint8_t btnEvts   = pkt[0];
        uint8_t btnChg    = pkt[1];
        int8_t  enc1Delta = (int8_t)pkt[2];
        int8_t  enc2Delta = (int8_t)pkt[3];

        for (uint8_t bit = 0; bit < 2; bit++) {
            if (!(btnChg & (1u << bit))) continue;
            bool pressed = (btnEvts >> bit) & 1;
            Serial.print(pressed ? F("  [PRESS] ") : F("  [REL]   "));
            _printButtonLabel(TYPE_DUAL_ENCODER, bit);
            Serial.println();
        }

        if (enc1Delta != 0) {
            Serial.print(F("  ENC1 delta: "));
            if (enc1Delta > 0) Serial.print('+');
            Serial.print(enc1Delta);
            Serial.println(F("  (AG Block)"));
        }
        if (enc2Delta != 0) {
            Serial.print(F("  ENC2 delta: "));
            if (enc2Delta > 0) Serial.print('+');
            Serial.print(enc2Delta);
            Serial.println(enc2Delta > 0 ? F("  (next cam '-')") : F("  (prev cam '=')"));
        }
        delay(10);
    }
    Serial.println(F("  Monitor exited."));
}

// Switch panel 4-byte packet
static void _monitorSwitchPanel() {
    Serial.println(F("  Flip switches. Send [X] to exit."));
    Serial.println(F("  MODE: CTRL=SW1hi, DBG=both lo, DEMO=SW2hi\n"));

    while (true) {
        if (_checkExit()) break;
        if (!_intAsserted()) { delay(5); continue; }

        uint8_t pkt[4];
        if (Wire.requestFrom(_target.address, (uint8_t)4) != 4) {
            delay(10); continue;
        }
        for (uint8_t i = 0; i < 4; i++) pkt[i] = Wire.read();

        uint16_t state  = ((uint16_t)pkt[0] << 8) | pkt[1];
        uint16_t change = ((uint16_t)pkt[2] << 8) | pkt[3];

        for (uint8_t bit = 0; bit < 10; bit++) {
            if (!(change & (1u << bit))) continue;
            bool on = (state >> bit) & 1;
            Serial.print(on ? F("  [ON]  ") : F("  [OFF] "));
            _printButtonLabel(TYPE_SWITCH_PANEL, bit);
            Serial.println();
        }

        uint8_t mode = state & 0x03;
        Serial.print(F("  MODE: "));
        switch (mode) {
            case 0: Serial.println(F("DBG (center)")); break;
            case 1: Serial.println(F("CTRL (up)")); break;
            case 2: Serial.println(F("DEMO (down)")); break;
            case 3: Serial.println(F("INVALID (both high)")); break;
        }
        delay(10);
    }
    Serial.println(F("  Monitor exited."));
}

// Indicator — output only, cycle pixels and print reference
static void _monitorIndicator() {
    Serial.println(F("  Indicator is output-only. Cycling all pixel states...\n"));

    uint8_t payload[9];
    const uint8_t states[]     = { LED_OFF, LED_ENABLED, LED_ACTIVE,
                                   LED_WARNING, LED_ALERT, LED_ARMED, LED_PARTIAL };
    const char*   stateNames[] = { "OFF", "ENABLED", "ACTIVE",
                                   "WARNING", "ALERT", "ARMED", "PARTIAL" };

    for (uint8_t s = 0; s < 7; s++) {
        Serial.print(F("  All pixels -> ")); Serial.println(stateNames[s]);
        _payloadAll(payload, 9, states[s]);
        _send(_target.address, CMD_SET_LED_STATE, payload, 9);
        delay(1100);
    }

    _payloadAll(payload, 9, LED_OFF);
    _send(_target.address, CMD_SET_LED_STATE, payload, 9);

    Serial.println(F("\n  Pixel reference:"));
    for (uint8_t i = 0; i < 18; i++) {
        Serial.print(F("  "));
        _printButtonLabel(TYPE_INDICATOR, i);
        Serial.println();
    }
}

// Dispatch
static void _startMonitor() {
    Serial.println(F("\n-- Monitor --"));
    switch (_target.typeId) {
        case TYPE_JOYSTICK_ROTATION:
        case TYPE_JOYSTICK_TRANS:    _monitorJoystick();     break;
        case TYPE_GPWS:
        case TYPE_PREWARP:           _monitorDisplay();      break;
        case TYPE_THROTTLE:          _monitorThrottle();     break;
        case TYPE_DUAL_ENCODER:      _monitorDualEncoder();  break;
        case TYPE_SWITCH_PANEL:      _monitorSwitchPanel();  break;
        case TYPE_INDICATOR:         _monitorIndicator();    break;
        default:                     _monitorStandard();     break;
    }
}

// ============================================================
//  Menu
// ============================================================

static void _printMenu() {
    Serial.println();
    Serial.println(F("+==============================================+"));
    Serial.println(F("|   Kerbal Controller Mk1 - Module Tester     |"));
    Serial.println(F("|==============================================|"));
    Serial.print  (F("|  Module: "));
    Serial.print(_typeName(_target.typeId));
    Serial.print(F(" @ 0x"));
    if (_target.address < 0x10) Serial.print('0');
    Serial.println(_target.address, HEX);
    Serial.println(F("|----------------------------------------------|"));
    Serial.println(F("|  [I] Identity                                |"));
    Serial.println(F("|  [B] Bulb test                               |"));
    Serial.println(F("|  [L] LED state cycle                         |"));
    Serial.println(F("|  [W] Wake / enable                           |"));
    Serial.println(F("|  [S] Sleep / disable                         |"));
    Serial.println(F("|  [R] Reset                                   |"));
    Serial.println(F("|  [M] Monitor inputs / cycle LEDs             |"));
    Serial.println(F("|  [N] New module (rescan bus)                 |"));
    Serial.println(F("|  [?] Show this menu                          |"));
    Serial.println(F("+==============================================+"));
    Serial.print(F("> "));
}

// ============================================================
//  setup()
// ============================================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    Wire.begin();
    Wire.setClock(400000);
    pinMode(PIN_INT_IN, INPUT);
    delay(200);

    Serial.println(F("\nKerbal Controller Mk1 -- Module Tester v1.0"));
    Serial.println(F("KC-01-9001 / XIAO SAMD21"));
    Serial.println(F("============================================"));

    _scanBus();
    _selectModule();

    if (_targetSelected) {
        memset(_ledState, LED_ENABLED, sizeof(_ledState));
        _send(_target.address, CMD_RESET);
        delay(20);
        _send(_target.address, CMD_ENABLE);
        delay(50);
        _printMenu();
    }
}

// ============================================================
//  loop()
// ============================================================

void loop() {
    if (!_targetSelected || !Serial.available()) return;

    char cmd = Serial.read();
    while (Serial.available()) Serial.read();

    switch (cmd) {
        case 'I': case 'i': _testIdentity();  break;
        case 'B': case 'b': _testBulbTest();  break;
        case 'L': case 'l': _testLEDCycle();  break;
        case 'R': case 'r': _testReset();     break;
        case 'M': case 'm': _startMonitor();  break;
        case 'W': case 'w':
            Serial.println(F("\n-- Wake / Enable --"));
            _send(_target.address, CMD_WAKE);
            _send(_target.address, CMD_ENABLE);
            Serial.println(F("  CMD_WAKE + CMD_ENABLE sent."));
            break;
        case 'S': case 's':
            Serial.println(F("\n-- Sleep / Disable --"));
            _send(_target.address, CMD_SLEEP);
            Serial.println(F("  CMD_SLEEP sent."));
            break;
        case 'N': case 'n':
            _targetSelected = false;
            _foundCount = 0;
            _scanBus();
            _selectModule();
            if (_targetSelected) {
                memset(_ledState, LED_ENABLED, sizeof(_ledState));
                _send(_target.address, CMD_RESET);
                delay(20);
                _send(_target.address, CMD_ENABLE);
                delay(50);
            }
            break;
        case '?':
            _printMenu(); return;
        default:
            Serial.println(F("Unknown command -- send [?] for menu"));
            break;
    }

    if (_targetSelected) {
        Serial.println();
        Serial.print(F("> "));
    }
}

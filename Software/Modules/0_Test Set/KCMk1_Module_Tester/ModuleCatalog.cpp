/**
 * @file        ModuleCatalog.cpp
 * @version     2.0.0
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Per-module metadata tables for the tester. Implements the
 *              ModuleCatalog.h contract: type-ID lookup of name, expected
 *              address, rendering kind, and per-bit input labels, plus the
 *              packet-size mapping for each rendering kind.
 *
 *              Labels are indexed by INPUT BIT POSITION: labels[i] is the
 *              name reported for event/change bit i. Unused / no-connect bit
 *              positions carry "" so that indexing stays aligned with the
 *              hardware. inputCount equals the label array length.
 *
 *              RA4M1 is an ARM target with memory-mapped flash, so the const
 *              tables are addressed directly — no PROGMEM.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */

#include "ModuleCatalog.h"

// ============================================================
//  Total I2C packet size (header + payload) per rendering kind.
// ============================================================
uint8_t kindPacketSize(ModuleKind kind) {
    switch (kind) {
        case MK_BUTTON12:     return KMC_BUTTON_PACKET_SIZE;
        case MK_BUTTON24:     return KMC_BUTTON24_PACKET_SIZE;
        case MK_JOYSTICK:     return KMC_JOYSTICK_PACKET_SIZE;
        case MK_DISPLAY:      return KMC_DISPLAY_PACKET_SIZE;
        case MK_THROTTLE:     return KMC_THROTTLE_PACKET_SIZE;
        case MK_DUAL_ENCODER: return KMC_BUTTON_PACKET_SIZE;
    }
    return 0;
}

// ============================================================
//  RGB565 ACTIVE colors — derived once from the canonical RGB
//  values in KerbalModuleCommon.h so the tester's chip colors
//  cannot drift from the modules' actual NeoPixel colors.
//  0 in a color array = input has no LED (neutral pressed style).
// ============================================================
static inline uint16_t rgb565(const RGBColor& c) {
    return (uint16_t)(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
}

static const uint16_t CLR_GREEN      = rgb565(KMC_GREEN);
static const uint16_t CLR_RED        = rgb565(KMC_RED);
static const uint16_t CLR_AMBER      = rgb565(KMC_AMBER);
static const uint16_t CLR_YELLOW     = rgb565(KMC_YELLOW);
static const uint16_t CLR_GOLD       = rgb565(KMC_GOLD);
static const uint16_t CLR_ORANGE     = rgb565(KMC_ORANGE);
static const uint16_t CLR_CHARTREUSE = rgb565(KMC_CHARTREUSE);
static const uint16_t CLR_LIME       = rgb565(KMC_LIME);
static const uint16_t CLR_MINT       = rgb565(KMC_MINT);
static const uint16_t CLR_SEAFOAM    = rgb565(KMC_SEAFOAM);
static const uint16_t CLR_BLUE       = rgb565(KMC_BLUE);
static const uint16_t CLR_SKY        = rgb565(KMC_SKY);
static const uint16_t CLR_TEAL       = rgb565(KMC_TEAL);
static const uint16_t CLR_CYAN       = rgb565(KMC_CYAN);
static const uint16_t CLR_PURPLE     = rgb565(KMC_PURPLE);
static const uint16_t CLR_INDIGO     = rgb565(KMC_INDIGO);
static const uint16_t CLR_CORAL      = rgb565(KMC_CORAL);
static const uint16_t CLR_ROSE       = rgb565(KMC_ROSE);
static const uint16_t CLR_PINK       = rgb565(KMC_PINK);
static const uint16_t CLR_MAGENTA    = rgb565(KMC_MAGENTA);
static const uint16_t CLR_WHITE_WARM = rgb565(KMC_WHITE_WARM);

// ============================================================
//  Per-module input labels — indexed by input bit position.
//  "" marks unused / no-connect positions to keep alignment.
//  Each labels array has a parallel colors array (per the
//  canonical Module UI Reference).
// ============================================================

static const char* const labels_UI[] = {
    "Screenshot", "Debug", "UI Toggle", "Nav Ball",
    "Map Reset", "Navball Cyc", "Map Fwd", "Ship Fwd",
    "Map Back", "Ship Back", "Map Enable", "IVA"
};
static const uint16_t colors_UI[] = {
    CLR_GREEN, CLR_RED,   CLR_GREEN, CLR_AMBER,
    CLR_SKY,   CLR_GREEN, CLR_SKY,   CLR_TEAL,
    CLR_SKY,   CLR_TEAL,  CLR_SKY,   CLR_CORAL
};

static const char* const labels_Function[] = {
    "LES", "Fairing Jett", "Air Intake", "Lock Surf",
    "Airbrake", "RW Disable", "Eng Alt Mode", "Sci Collect",
    "Eng Group 1", "Sci Group 1", "Eng Group 2", "Sci Group 2",
    "", "", "", "",
    "SG1 MSTR", "SG1 DISPL", "SG1 ENGINE", "SG1 THROTTLE",
    "SG1 SCE", "SG1 UPTLM", "SG1 LTG", "SG1 THRTL"
};
static const uint16_t colors_Function[] = {
    CLR_RED,    CLR_AMBER,  CLR_TEAL,       CLR_SKY,
    CLR_CYAN,   CLR_AMBER,  CLR_ORANGE,     CLR_PURPLE,
    CLR_YELLOW, CLR_INDIGO, CLR_CHARTREUSE, CLR_BLUE,
    0, 0, 0, 0,                    // no-connect
    0, 0, 0, 0, 0, 0, 0, 0        // Switch Group 1 — no LEDs
};

static const char* const labels_Action[] = {
    "AG6", "AG12", "AG5", "AG11",
    "AG4", "AG10", "AG3", "AG9",
    "AG2", "AG8", "AG1", "AG7"
};
static const uint16_t colors_Action[] = {
    CLR_GREEN, CLR_GREEN, CLR_GREEN, CLR_GREEN,
    CLR_GREEN, CLR_GREEN, CLR_GREEN, CLR_GREEN,
    CLR_GREEN, CLR_GREEN, CLR_GREEN, CLR_GREEN
};

static const char* const labels_Stability[] = {
    "SAS Target", "SAS A-Target", "SAS Rad In", "SAS Rad Out",
    "SAS Normal", "SAS A-Normal", "SAS Prograde", "SAS Retro",
    "SAS Stab", "SAS Maneuver", "RCS", "Invert",
    "", "", "Stage", "Stage Ena"
};
static const uint16_t colors_Stability[] = {
    CLR_GREEN, CLR_GREEN, CLR_GREEN, CLR_GREEN,
    CLR_GREEN, CLR_GREEN, CLR_GREEN, CLR_GREEN,
    CLR_GREEN, CLR_GREEN,           // 10x SAS
    CLR_GREEN,                      // RCS
    CLR_AMBER,                      // Invert
    0, 0,                           // no-connect
    0,                              // Stage (discrete LED)
    0                               // Stage Ena (discrete LED)
};

static const char* const labels_Vehicle[] = {
    "Brakes", "Lights", "Antenna", "Gear",
    "Fuel Cell", "Solar Array", "Cargo Door", "Radiator",
    "Heat Shield", "Ladder", "Main Chute", "Drogue Chute",
    "", "", "", "",
    "SG2 CHUTE", "SG2 GEAR", "SG2 BRAKE", "SG2 EXT LT",
    "SG2 SAS", "SG2 RCS", "SG2 THC/RHC", "SG2 AUDIO"
};
static const uint16_t colors_Vehicle[] = {
    CLR_RED,   CLR_YELLOW, CLR_PINK,  CLR_GREEN,
    CLR_CYAN,  CLR_GOLD,   CLR_TEAL,  CLR_ORANGE,
    CLR_GREEN,                       // Heat Shield
    CLR_LIME,
    CLR_GREEN,                       // Main Chute
    CLR_GREEN,                       // Drogue Chute
    0, 0, 0, 0,                      // no-connect
    0, 0, 0, 0, 0, 0, 0, 0          // Switch Group 2 — no LEDs
};

static const char* const labels_Time[] = {
    "Warp ApA", "Warp Stop", "Warp PeA", "Phys Warp",
    "Warp Mnvr", "Warp Fwd", "Warp SOI", "Warp Back",
    "Warp Morn", "Quick Load", "Pause", "Quick Save"
};
static const uint16_t colors_Time[] = {
    CLR_YELLOW, CLR_AMBER,      CLR_YELLOW, CLR_CHARTREUSE,
    CLR_YELLOW, CLR_WHITE_WARM, CLR_YELLOW, CLR_WHITE_WARM,
    CLR_YELLOW, CLR_MINT,       CLR_AMBER,  CLR_LIME
};

static const char* const labels_Aux[] = {
    "EVA Lights", "Jetpack", "Board Craft", "EVA Constr",
    "Jump/LetGo", "Grab", "EVA Chute", "Helmet",
    "Cruise Ctrl", "Plant Flag", "CP Toggle", "CP Dock"
};
static const uint16_t colors_Aux[] = {
    CLR_MINT,       CLR_LIME,    CLR_GREEN, CLR_TEAL,
    CLR_CHARTREUSE, CLR_SEAFOAM, CLR_AMBER, CLR_SKY,
    CLR_GOLD,       CLR_SEAFOAM, CLR_ROSE,  CLR_PINK
};

static const char* const labels_JoyRotation[] = {
    "BTN_JOY", "Reset Trim", "Set Trim"
};
static const uint16_t colors_JoyRotation[] = {
    0,                               // BTN_JOY — no LED
    CLR_GREEN, CLR_AMBER
};

static const char* const labels_JoyTranslation[] = {
    "BTN_JOY", "Cycle Cam", "Cam Reset"
};
static const uint16_t colors_JoyTranslation[] = {
    0,                               // BTN_JOY — no LED
    CLR_MAGENTA, CLR_GREEN
};

static const char* const labels_Gpws[] = {
    "GPWS Enable", "Proximity", "Rndzv Radar", "Reset (Enc)"
};
static const uint16_t colors_Gpws[] = {
    CLR_GREEN, CLR_GREEN, CLR_GREEN,
    0                                // encoder button — no LED
};

static const char* const labels_PreWarp[] = {
    "5 min", "1 hour", "1 day", "Reset (Enc)"
};
static const uint16_t colors_PreWarp[] = {
    CLR_GOLD, CLR_GOLD, CLR_GOLD,
    0                                // encoder button — no LED
};

static const char* const labels_Throttle[] = {
    "THRTL 100%", "THRTL Up", "THRTL Down", "THRTL 0%"
};
static const uint16_t colors_Throttle[] = {
    0, 0, 0, 0                       // discrete white LEDs — no color
};

static const char* const labels_DualEncoder[] = {
    "ENC1 Btn", "ENC2 Btn"
};
static const uint16_t colors_DualEncoder[] = {
    0, 0                             // no LEDs
};

// ============================================================
//  Module catalog — one entry per known module type.
// ============================================================
static const ModuleInfo kCatalog[] = {
    { KMC_TYPE_UI_CONTROL,        "UI Control",      0x20, MK_BUTTON12,     12, labels_UI,             colors_UI },
    { KMC_TYPE_FUNCTION_CONTROL,  "Function Ctrl",   0x21, MK_BUTTON24,     24, labels_Function,       colors_Function },
    { KMC_TYPE_ACTION_CONTROL,    "Action Ctrl",     0x22, MK_BUTTON12,     12, labels_Action,         colors_Action },
    { KMC_TYPE_STABILITY_CONTROL, "Stability Ctrl",  0x23, MK_BUTTON12,     16, labels_Stability,      colors_Stability },
    { KMC_TYPE_VEHICLE_CONTROL,   "Vehicle Ctrl",    0x24, MK_BUTTON24,     24, labels_Vehicle,        colors_Vehicle },
    { KMC_TYPE_TIME_CONTROL,      "Time Control",    0x25, MK_BUTTON12,     12, labels_Time,           colors_Time },
    { KMC_TYPE_AUX_CTRL,          "Aux Control",     0x26, MK_BUTTON12,     12, labels_Aux,            colors_Aux },
    { KMC_TYPE_JOYSTICK_ROTATION, "Joy Rotation",    0x28, MK_JOYSTICK,      3, labels_JoyRotation,    colors_JoyRotation },
    { KMC_TYPE_JOYSTICK_TRANS,    "Joy Translation", 0x29, MK_JOYSTICK,      3, labels_JoyTranslation, colors_JoyTranslation },
    { KMC_TYPE_GPWS_INPUT,        "GPWS Input",      0x2A, MK_DISPLAY,       4, labels_Gpws,           colors_Gpws },
    { KMC_TYPE_PRE_WARP_TIME,     "Pre-Warp Time",   0x2B, MK_DISPLAY,       4, labels_PreWarp,        colors_PreWarp },
    { KMC_TYPE_THROTTLE,          "Throttle",        0x2C, MK_THROTTLE,      4, labels_Throttle,       colors_Throttle },
    { KMC_TYPE_DUAL_ENCODER,      "Dual Encoder",    0x2D, MK_DUAL_ENCODER,  2, labels_DualEncoder,    colors_DualEncoder }
};

static const uint8_t kCatalogCount = sizeof(kCatalog) / sizeof(kCatalog[0]);

// ============================================================
//  Lookup by Type ID (identity byte 0). Linear search; returns
//  nullptr if the type is not in the catalog.
// ============================================================
const ModuleInfo* catalogByType(uint8_t typeId) {
    for (uint8_t i = 0; i < kCatalogCount; ++i) {
        if (kCatalog[i].typeId == typeId) {
            return &kCatalog[i];
        }
    }
    return nullptr;
}

#ifndef _CUSTOM_ACTION_GRP_DEF_H
#define _CUSTOM_ACTION_GRP_DEF_H

/***************************************************************************************
   Custom Action Group (CAG) base numbers — Kerbal Controller Mk1

   Authoritative source: Module UI Reference v5.4, "Custom AG Assignment Table".
   These are the BASE CAG numbers for control Group 1. The AGX number actually
   sent to KSP (via Action Groups Extended) for the active control group is:

       AGX = base + ((controlGroup - 1) * CTRL_GRP_STRIDE)

   with controlGroup in 1..6 (Default, Lander #1, Lander #2, Custom, Re-Entry,
   Space Station) and CTRL_GRP_STRIDE = 40. Maximum AGX used is 239 (base 39,
   group 6), within the AGX 250 limit.

   The 1-slot gap left by the 39 bases within each 40-wide block is intentional
   headroom for future additions without renumbering.
****************************************************************************************/

/** @brief Per-control-group stride applied to every base CAG. */
const uint8_t CTRL_GRP_STRIDE = 40;

// --- Action Control module (0x22): AG1–AG12 ---
const uint8_t ag1_base  = 1;
const uint8_t ag2_base  = 2;
const uint8_t ag3_base  = 3;
const uint8_t ag4_base  = 4;
const uint8_t ag5_base  = 5;
const uint8_t ag6_base  = 6;
const uint8_t ag7_base  = 7;
const uint8_t ag8_base  = 8;
const uint8_t ag9_base  = 9;
const uint8_t ag10_base = 10;
const uint8_t ag11_base = 11;
const uint8_t ag12_base = 12;

// --- Vehicle Control module (0x24) ---
const uint8_t antenna_base             = 13;  // B2  Antenna
const uint8_t fuel_cell_base           = 14;  // B4  Fuel Cell
const uint8_t solar_array_base         = 15;  // B5  Solar Array
const uint8_t cargo_door_base          = 16;  // B6  Cargo Door
const uint8_t radiator_base            = 17;  // B7  Radiator
const uint8_t ladder_base              = 18;  // B9  Ladder
const uint8_t heat_shield_deploy_base  = 19;  // B8  Heat Shield Deploy
const uint8_t heat_shield_release_base = 20;  // B8  Heat Shield Release
const uint8_t parachute_base           = 21;  // B10 Main Chute Deploy
const uint8_t main_chute_cut_base      = 22;  // B10 Main Chute Cut
const uint8_t drogue_base              = 23;  // B11 Drogue Chute Deploy
const uint8_t drogue_cut_base          = 24;  // B11 Drogue Chute Cut

// --- Function Control module (0x21) ---
const uint8_t les_base           = 25;  // B0  LES
const uint8_t fairing_base       = 26;  // B1  Fairing Jettison
const uint8_t engineMode_base    = 27;  // B6  Engine Alt Mode
const uint8_t collectSci_base    = 28;  // B7  Science Collect
const uint8_t engine1_base       = 29;  // B8  Engine Group 1
const uint8_t science1_base      = 30;  // B9  Science Group 1
const uint8_t engine2_base       = 31;  // B10 Engine Group 2
const uint8_t science2_base      = 32;  // B11 Science Group 2
const uint8_t intake_base        = 33;  // B2  Air Intake
const uint8_t lock_surfaces_base = 34;  // B3  Lock Surfaces

// --- AUX CTRL module (0x26): Control Points ---
const uint8_t cp_primary_base = 35;  // B10 CP Primary
const uint8_t cp_alternate_base = 36; // B10 CP Alternate
const uint8_t cp_docking_base = 37;  // B11 CP Docking Port

// --- Shared / aerodynamic ---
const uint8_t airbrake_base   = 38;  // Function B4 / Rotation BTN_JOY (intentional duplicate)
const uint8_t rw_disable_base = 39;  // Function B5  Reaction Wheel Disable

#endif  // _CUSTOM_ACTION_GRP_DEF_H

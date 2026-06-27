/***************************************************************************************
   ScreenMain.ino -- Main screen for Kerbal Controller Mk1 Annunciator
   Layout constants, static chrome, C&W panel helpers, and per-frame update pass.
   Alarm condition tracking (ALARM_* bits, alarmActiveMask, updateAlarmMask) lives
   in Audio.ino alongside the rest of the audio wiring.

   LAYOUT SUMMARY (800x480 display)
   ┌──────────────────────────────────────────────────────────────────────────────────┐
   │  MASTER ALARM        │  C&W GRID (5x5, 98x73 each)          │DOCK│  SIT COL   │
   │  240x160 @ (0,0)     │  490x365 @ (245,6)                    │60x │  60x47 x8  │
   │  font: 48pt          │  font: 20pt                           │104 │  @ (740,   │
   │                      │                                       │@   │    104)    │
   │  SOI LABEL           │                                       │(740│  font: 12pt│
   │  240x48 @ (0,160)    ├───────────────────────────────────────│ ,0)│            │
   │  font: 24pt          │  PANEL COND (5x2, 72x52)  │  FLIGHT   │    │            │
   │                      │  360x104 @ (245,376)       │  COND     │    │            │
   │  SOI GLOBE           │  font: 16pt                │  2x2,63x52│    │            │
   │  240x168 @ (0,208)   │                            │  @ (609,  │    │            │
   ├──────────────────────┤                            │    376)   │    │            │
   │  CtrlGrp 240x52@376  │                            │  font:12pt│    │            │
   │  TW      240x52@428  │                            │           │    │            │
   │  font: 24pt          │                            │           │    │            │
   └──────────────────────┴────────────────────────────┴───────────┴────┴────────────┘

   Zone borders are filled with TFT_SILVER (5px gutters) to visually separate regions.
   Button borders use TFT_GREY.

   TWO-TIER BUTTON COLOUR CONVENTION
   CW_PE_LOW, CW_PROP_LOW, CW_LIFE_SUPPORT each have yellow and red severity tiers
   sharing a single C&W bit. The C&W bit is set only for the red (alarm) condition.
   CautionWarning.ino also sets companion bools (peLowYellow, propLowYellow, lsYellow)
   for the yellow tier. updateCautWarnPanel() reads these to override the button colour
   for those three specific buttons, giving yellow when the companion bool is true and
   the C&W bit is not set.

   CHUTE ENV COLOUR CONVENTION
   CW_CHUTE_ENV uses chuteEnvState (off/red/yellow/green). The C&W bit is set whenever
   chuteEnvState is not chute_Off. The button colour is driven by chuteEnvState directly.
   prevChuteEnvState is used for dirty detection so colour-only changes (e.g. yellow->green)
   trigger a redraw even when the C&W bit itself does not change.

   DEMO/CTRL/DEBUG and SPCFT/PLN/RVR BUTTON CONVENTION
   These two panel condition buttons always show on=false (black background) with
   coloured text only. DEMO/CTRL/DEBUG cycles green(CTRL) / blue(DEMO) / purple(DEBUG).
   SPCFT/PLN/RVR shows green text when control mode matches vessel type, red when not.

   CONTACT BUTTON CONVENTION
   CNTCT (situation column row 0) is driven by VSIT_LANDED or VSIT_SPLASH, not VSIT_DOCKED.
   VSIT_DOCKED drives only the separate DOCK vertical text indicator above the column.
   forceContactState() and forceDockState() are provided for TestMode walk-through use.
****************************************************************************************/
#include "KCMk1_Annunciator.h"

// Extern companion bools from CautionWarning.ino
extern bool peLowYellow;
extern bool propLowYellow;
extern bool lsYellow;


/***************************************************************************************
   LAYOUT CONSTANTS -- MAIN SCREEN
   All values in pixels. Origins are top-left of each zone.

   LAYOUT SUMMARY (800x480 display)
   ┌──────────────────────────────────────────────────────────────────────────────────┐
   │  MASTER ALARM        │  C&W GRID (5x5, 98x73 each)              │DOCK│ SIT COL │
   │  240x160 @ (0,0)     │  490x365 @ (245,6)                        │60x │ 60x47x8 │
   │                      │                                           │104 │ @ (740, │
   │  SOI LABEL           │                                           │@740│   104)  │
   │  240x48 @ (0,160)    ├───────────────────────────────────────────┤    │         │
   │                      │  PANEL COND (5x2, 72x52)  │  FLIGHT COND │    │         │
   │  SOI GLOBE           │  360x104 @ (245,376)       │  126x104     │    │         │
   │  240x168 @ (0,208)   │                            │  @ (609,376) │    │         │
   ├──────────────────────┤                            │  2x2, 63x52  │    │         │
   │  CtrlGrp 240x52@376  │                            │              │    │         │
   │  TW      240x52@428  │                            │              │    │         │
   └──────────────────────┴────────────────────────────┴──────────────┴────┴─────────┘
****************************************************************************************/

// Master Alarm
static const uint16_t MASTER_X      =   0;
static const uint16_t MASTER_Y      =   0;
static const uint16_t MASTER_W      = 240;
static const uint16_t MASTER_H      = 160;

// SOI display (label + globe)
static const uint16_t SOI_LABEL_X   =   0;
static const uint16_t SOI_LABEL_Y   = 160;
static const uint16_t SOI_LABEL_W   = 240;
static const uint16_t SOI_LABEL_H   =  48;
static const uint16_t SOI_GLOBE_X   =   0;
static const uint16_t SOI_GLOBE_Y   = 208;
static const uint16_t SOI_GLOBE_W   = 240;
static const uint16_t SOI_GLOBE_H   = 168;

// Data strip (CtrlGrp and TW)
static const uint16_t DATA_X        =   0;
static const uint16_t DATA_Y        = 376;
static const uint16_t DATA_W        = 240;
static const uint16_t DATA_H        =  52;

// C&W grid: 5 columns x 5 rows, row-major order
static const uint16_t CW_X0         = 245;
static const uint16_t CW_Y0         =   6;
static const uint16_t CW_BTN_W      =  98;
static const uint16_t CW_BTN_H      =  73;
static const uint16_t CW_COLS       =   5;
static const uint16_t CW_ROWS       =   5;

// DOCKED indicator: sits above situation column
static const uint16_t DOCKED_X      = 740;
static const uint16_t DOCKED_Y      =   0;
static const uint16_t DOCKED_W      =  60;
static const uint16_t DOCKED_H      = 104;

// Vessel situation column: 8 buttons, below DOCK indicator
static const uint16_t SIT_X0        = 740;
static const uint16_t SIT_Y0        = 104;
static const uint16_t SIT_BTN_W     =  60;
static const uint16_t SIT_BTN_H     =  47;
static const uint16_t SIT_COUNT     =   8;

// Panel condition strip: 5 columns x 2 rows
static const uint16_t PS_X0         = 245;
static const uint16_t PS_Y0         = 376;
static const uint16_t PS_BTN_W      =  72;
static const uint16_t PS_BTN_H      =  52;
static const uint16_t PS_COLS       =   5;
static const uint16_t PS_ROWS       =   2;

// Flight condition block: 2 columns x 2 rows
static const uint16_t FC_X0         = 609;
static const uint16_t FC_Y0         = 376;
static const uint16_t FC_BTN_W      =  63;
static const uint16_t FC_BTN_H      =  52;


/***************************************************************************************
   PRINT STATE -- KDC v2 flicker-free rendering
   One PrintState per printDisp / printValue call site.
****************************************************************************************/
PrintState psSOILabel;   // SOI label
PrintState psCtrlGrp;    // CtrlGrp data row
PrintState psTW;         // TW data row


/***************************************************************************************
   C&W BUTTON LABEL DEFINITIONS
   Row-major order: index = row * CW_COLS + col
   Matches the CW_* bit indices defined in KCMk1_Annunciator.h.

   ButtonLabel fields: { text, fontColorOff, fontColorOn,
                         bgColorOff, bgColorOn, borderColorOff, borderColorOn }

   Two-tier buttons (PE_LOW, PROP_LOW, LIFE_SUPPORT): bgColorOn shown here is the RED
   (alarm) colour. The yellow (caution) colour is applied dynamically in
   updateCautWarnPanel() by checking companion bools from CautionWarning.ino.

   CHUTE_ENV: bgColorOn is a placeholder; actual colour is driven by chuteEnvState.
   SRB_ACTIVE / EVA_ACTIVE: orange (TFT_ORANGE).
   ORBIT_STABLE / ELEC_GEN: green (TFT_DARK_GREEN).
   O2_PRESENT: blue (TFT_BLUE).
   All WARNING items: red (TFT_RED).
   All CAUTION items: yellow (TFT_YELLOW).
****************************************************************************************/
static const ButtonLabel cautWarn[CW_COUNT] = {
  // Row 0 -- WARNING (red, master alarm)
  { "LOW \x94V",     TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 0  CW_LOW_DV
  { "HIGH G",        TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 1  CW_HIGH_G
  { "HIGH TEMP",     TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 2  CW_HIGH_TEMP
  { "BUS VOLTAGE",   TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 3  CW_BUS_VOLTAGE
  { "ABORT",         TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 4  CW_ABORT

  // Row 1 -- mixed: WARNING + two-tier + informational
  { "GROUND PROX",   TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 5  CW_GROUND_PROX
  { "Pe LOW",        TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 6  CW_PE_LOW (two-tier)
  { "PROP LOW",      TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 7  CW_PROP_LOW (two-tier)
  { "LIFE SUPP",   TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 8  CW_LIFE_SUPPORT (two-tier)
  { "O2 PRESENT",    TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_BLUE,       TFT_GREY, TFT_GREY }, // 9  CW_O2_PRESENT

  // Row 2 -- CAUTION (yellow)
  { "IMPACT IMM",    TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 10 CW_IMPACT_IMM
  { "ALT",           TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 11 CW_ALT
  { "DESCENT",       TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 12 CW_DESCENT
  { "GEAR UP",       TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 13 CW_GEAR_UP
  { "ATMO",          TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 14 CW_ATMO

  // Row 3 -- CAUTION (yellow)
  { "RCS LOW",       TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 15 CW_RCS_LOW
  { "PROP RATIO",    TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 16 CW_PROP_IMBAL
  { "COMM LOST",     TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 17 CW_COMM_LOST
  { "Ap LOW",        TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 18 CW_Ap_LOW
  { "HIGH Q",        TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 19 CW_HIGH_Q

  // Row 4 -- POSITIVE / STATE indicators
  { "ORBIT STABLE",  TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // 20 CW_ORBIT_STABLE
  { "ELEC GEN",      TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // 21 CW_ELEC_GEN
  { "CHUTE ENV",     TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 22 CW_CHUTE_ENV (dynamic)
  { "SRB ACTIVE",    TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_ORANGE,     TFT_GREY, TFT_GREY }, // 23 CW_SRB_ACTIVE
  { "EVA ACTIVE",    TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_ORANGE,     TFT_GREY, TFT_GREY }, // 24 CW_EVA_ACTIVE
};


/***************************************************************************************
   VESSEL SITUATION COLUMN LABELS
   8 buttons top-to-bottom, matching VSIT_* bit indices 0-7.
   CNTCT (index 0) maps to VSIT_DOCKED bit in the array but is drawn separately
   in updateVesselSitPanel -- it illuminates when LANDED or SPLASH is set, not
   when DOCKED is set. VSIT_DOCKED drives only the separate DOCK indicator.
   Color: CNTCT=blue (surface contact), SPLASH=blue, all others=green.
****************************************************************************************/
static const ButtonLabel vesselSituation[SIT_COUNT] = {
  { "CNTCT",      TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_BLUE,   TFT_GREY, TFT_GREY }, // 0 special: LANDED|SPLASH
  { "PRE- LNCH",  TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY }, // 1 VSIT_PRELAUNCH
  { "FLIGHT",     TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY }, // 2 VSIT_FLIGHT
  { "SUB- ORBIT", TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY }, // 3 VSIT_SUBORBIT
  { "ORBIT",      TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY }, // 4 VSIT_ORBIT
  { "ESCAPE",     TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY }, // 5 VSIT_ESCAPE
  { "SPLASH",     TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_NAVY,   TFT_GREY, TFT_GREY }, // 6 VSIT_SPLASH
  { "LANDED",     TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY }, // 7 VSIT_LANDED
};


/***************************************************************************************
   PANEL STATUS STRIP LABELS
   10 buttons: 5 columns x 2 rows, row-major.
   Row 0: DEMO, WARP, AUDIO, THRTL ENA, TRIM SET
   Row 1: SPCFT, SWITCH ERR, SIMPIT LST, THRTL PREC, PREC INPUT
   Colours are set dynamically in updatePanelStatus() -- these templates define labels only.
   fontColorOn and bgColorOn are overridden at draw time.
****************************************************************************************/
static const ButtonLabel panelStatus[PS_COLS * PS_ROWS] = {
  // Row 0
  { "DEMO",       TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_BLUE,       TFT_GREY, TFT_GREY }, // 0
  { "WARP",       TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 1
  { "AUDIO",      TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE,     TFT_GREY, TFT_GREY }, // 2
  { "THRTL ENA",  TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE,     TFT_GREY, TFT_GREY }, // 3
  { "TRIM SET",   TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_AQUA,       TFT_GREY, TFT_GREY }, // 4
  // Row 1
  { "SPCFT",      TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE,     TFT_GREY, TFT_GREY }, // 5
  { "SWITCH ERR", TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 6
  { "SIMPIT\nLOST", TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 7
  { "THRTL PREC", TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE,     TFT_GREY, TFT_GREY }, // 8
  { "PREC INPUT", TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE,     TFT_GREY, TFT_GREY }, // 9
};


/***************************************************************************************
   FLIGHT CONDITION BLOCK LABELS
   4 buttons: 2 columns x 2 rows, row-major.
   Row 0: FLYING LOW, LOW SPACE
   Row 1: FLYING HIGH, HIGH SPACE
   These are informational state indicators -- only one illuminates at a time.
   The C&W bit logic for these is NOT part of the 25-button C&W grid;
   they are driven separately from vesselSituationState via inAtmo/inFlight globals.
****************************************************************************************/
static const ButtonLabel flightCond[4] = {
  { "FLYING LOW",  TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // 0
  { "LOW SPACE",   TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // 1
  { "FLYING HIGH", TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // 2
  { "HIGH SPACE",  TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // 3
};


/***************************************************************************************
   MASTER ALARM BUTTON LABEL
****************************************************************************************/
static const ButtonLabel masterAlarmLabel = {
  "MASTER\nALARM", TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED, TFT_GREY, TFT_GREY
};


/***************************************************************************************
   CTRL MODE HELPER
   Returns display text and on-colour for the current vehicle control mode.
   Red if mode doesn't match vessel type; green if it does.
****************************************************************************************/
static const char *ctrlModeText(CtrlMode mode) {
  switch (mode) {
    case ctrl_Rover:      return "RVR";
    case ctrl_Plane:      return "PLN";
    case ctrl_Spacecraft: return "SPCFT";
    default:              return "ERR";
  }
}

static uint16_t ctrlModeColor(CtrlMode mode, VesselType type) {
  bool shouldBeSpacecraft = (type == type_Probe || type == type_Relay ||
                             type == type_Lander || type == type_Ship ||
                             type == type_Station);
  if (mode != ctrl_Spacecraft && shouldBeSpacecraft) return TFT_RED;
  if (mode != ctrl_Plane      && type == type_Plane)  return TFT_RED;
  if (mode != ctrl_Rover      && type == type_Rover)  return TFT_RED;
  return TFT_DARK_GREEN;
}


/***************************************************************************************
   FLIGHT CONDITION STATE
   Computes which of the four altitude-band indicators should be lit
   from current vessel situation and atmosphere state.
   Returns 0-3 matching flightCond[] index, or -1 for none lit.
   0 = FLYING LOW  (in atmo, below fly-high alt)
   1 = LOW SPACE   (above atmo, below high-space threshold)
   2 = FLYING HIGH (in atmo, above fly-high alt)
   3 = HIGH SPACE  (above atmo, above high-space threshold)
****************************************************************************************/
static int8_t flightCondIndex() {
  bool isAloft = bitRead(state.vesselSituationState, VSIT_FLIGHT)   ||
                 bitRead(state.vesselSituationState, VSIT_SUBORBIT)  ||
                 bitRead(state.vesselSituationState, VSIT_ORBIT)     ||
                 bitRead(state.vesselSituationState, VSIT_ESCAPE);
  if (!isAloft) return -1;
  if (inAtmo) {
    // In atmosphere -- FLYING LOW below flyHigh boundary, FLYING HIGH above it
    bool aboveFlyHigh = (currentBody.flyHigh > 0 && state.alt_sl > currentBody.flyHigh);
    return aboveFlyHigh ? 2 : 0;
  } else {
    // In space -- LOW SPACE below highSpace boundary, HIGH SPACE above it
    bool aboveHighSpace = (currentBody.highSpace > 0 && state.alt_sl > currentBody.highSpace);
    return aboveHighSpace ? 3 : 1;
  }
}


/***************************************************************************************
   C&W PANEL UPDATE
   Redraws only buttons whose bit has changed, handling two-tier and chute env
   colour overrides for the three special buttons.
****************************************************************************************/
void updateCautWarnPanel(RA8875 &tft, uint32_t prevCW, uint32_t newCW) {
  // Check for changes in the standard C&W bits
  uint32_t changed = prevCW ^ newCW;

  // Also force redraw of two-tier and chute env buttons when their companion
  // state may have changed even if the C&W bit itself did not toggle.
  // We do this by always re-evaluating those buttons when cautionWarningState changes.
  if (changed != 0 || chuteEnvState != prevChuteEnvState) {
    // Force redraw of the three two-tier buttons and chute env on any C&W change
    // so their colour stays in sync with companion bools and chuteEnvState.
    bitSet(changed, CW_PE_LOW);
    bitSet(changed, CW_PROP_LOW);
    bitSet(changed, CW_LIFE_SUPPORT);
    bitSet(changed, CW_CHUTE_ENV);
  }

  if (changed == 0) return;

  for (uint8_t i = 0; i < CW_COUNT; i++) {
    if (!bitRead(changed, i)) continue;

    uint8_t col = i % CW_COLS;
    uint8_t row = i / CW_COLS;
    int16_t x   = CW_X0 + col * CW_BTN_W;
    int16_t y   = CW_Y0 + row * CW_BTN_H;
    bool    on  = bitRead(newCW, i);

    // Default draw
    ButtonLabel btn = cautWarn[i];

    // Two-tier colour overrides: yellow companion bool takes priority over off state
    if (i == CW_PE_LOW && !on && peLowYellow) {
      btn.backgroundColorOff = TFT_YELLOW;
      btn.fontColorOff       = TFT_DARK_GREY;
      drawButton(tft, x, y, CW_BTN_W, CW_BTN_H, btn, &Roboto_Black_20, false);
      continue;
    }
    if (i == CW_PROP_LOW && !on && propLowYellow) {
      btn.backgroundColorOff = TFT_YELLOW;
      btn.fontColorOff       = TFT_DARK_GREY;
      drawButton(tft, x, y, CW_BTN_W, CW_BTN_H, btn, &Roboto_Black_20, false);
      continue;
    }
    if (i == CW_LIFE_SUPPORT && !on && lsYellow) {
      btn.backgroundColorOff = TFT_YELLOW;
      btn.fontColorOff       = TFT_DARK_GREY;
      drawButton(tft, x, y, CW_BTN_W, CW_BTN_H, btn, &Roboto_Black_20, false);
      continue;
    }

    // Chute env colour override
    if (i == CW_CHUTE_ENV) {
      switch (chuteEnvState) {
        case chute_Red:    btn.backgroundColorOn = TFT_RED;        btn.fontColorOn = TFT_WHITE; break;
        case chute_Yellow: btn.backgroundColorOn = TFT_YELLOW;     btn.fontColorOn = TFT_DARK_GREY; break;
        case chute_Green:  btn.backgroundColorOn = TFT_DARK_GREEN; btn.fontColorOn = TFT_WHITE; break;
        default:           break;
      }
      drawButton(tft, x, y, CW_BTN_W, CW_BTN_H, btn, &Roboto_Black_20,
                 chuteEnvState != chute_Off);
      continue;
    }

    // Standard button
    drawButton(tft, x, y, CW_BTN_W, CW_BTN_H, btn, &Roboto_Black_20, on);
  }

  prevChuteEnvState = chuteEnvState;
}


/***************************************************************************************
   VESSEL SITUATION PANEL UPDATE
   CNTCT (row 0) is driven by VSIT_LANDED or VSIT_SPLASH -- not VSIT_DOCKED.
   VSIT_DOCKED drives only the separate DOCK indicator.
   All other rows map directly to their VSIT_* bit index.
****************************************************************************************/
static bool _prevContact = false;  // separate dirty tracker for CONTACT button
static uint8_t prevPanelStatusMask = 0xFF;  // force redraw on first pass

void updateVesselSitPanel(RA8875 &tft, uint8_t prevSit, uint8_t newSit) {
  // CONTACT is on when LANDED or SPLASH is set
  bool newContact = bitRead(newSit, VSIT_LANDED) || bitRead(newSit, VSIT_SPLASH);

  // Draw CONTACT button (row 0) if its effective state changed
  if (newContact != _prevContact) {
    drawButton(tft, SIT_X0, SIT_Y0,
               SIT_BTN_W, SIT_BTN_H,
               vesselSituation[0], &Roboto_Black_12, newContact);
    _prevContact = newContact;
  }

  // Draw rows 1-7 directly from their VSIT_* bits (skip row 0 / VSIT_DOCKED)
  uint8_t changed = prevSit ^ newSit;
  for (uint8_t row = 1; row < SIT_COUNT; row++) {
    if (!bitRead(changed, row)) continue;
    int16_t y = SIT_Y0 + row * SIT_BTN_H;
    drawButton(tft, SIT_X0, y, SIT_BTN_W, SIT_BTN_H,
               vesselSituation[row], &Roboto_Black_12, bitRead(newSit, row));
  }
}


/***************************************************************************************
   FLIGHT CONDITION BLOCK UPDATE
   Only one indicator is lit at a time. Redraws all four on any change.
****************************************************************************************/
static int8_t prevFlightCondIdx = -2;  // sentinel: force draw on first pass

void updateFlightCondBlock(RA8875 &tft) {
  int8_t idx = flightCondIndex();
  if (idx == prevFlightCondIdx) return;
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t col = i % 2;
    uint8_t row = i / 2;
    int16_t x   = FC_X0 + col * FC_BTN_W;
    int16_t y   = FC_Y0 + row * FC_BTN_H;
    drawButton(tft, x, y, FC_BTN_W, FC_BTN_H,
               flightCond[i], &Roboto_Black_12, (i == (uint8_t)idx));
  }
  prevFlightCondIdx = idx;
}


/***************************************************************************************
   PANEL STATUS STRIP UPDATE
   Panel status items are driven by local panel state (demoMode, audioEnabled, etc.)
   and simpit connection state, not by game telemetry.
   Most items are read-only indicators of panel configuration.

   DEMO button: blue when demoMode, purple when debugMode, dark when neither.
   WARP: yellow when twIndex > 0.
   AUDIO: green when audioEnabled.
   THRTL ENA: driven by I2C panel state (not yet wired -- shows green as placeholder).
   TRIM SET: cyan when trim is engaged (not yet wired -- shows off as placeholder).
   SPCFT/PLN/RVR: green showing current ctrl mode.
   SWITCH ERR: red when panel switch positions mismatch game state (not yet wired).
   SIMPIT LST: red when simpit connection is lost.
   THRTL PREC: green when throttle precision mode active (not yet wired).
   PREC INPUT: green when precision input mode active (not yet wired).

   Items marked "not yet wired" are drawn from their template defaults until
   the I2C command interface exposes the required panel state bits.
****************************************************************************************/
void updatePanelStatus(RA8875 &tft) {
  // Build a simple bitmask of panel status conditions for dirty detection
  uint8_t mask = 0;
  if (demoMode)               bitSet(mask, 0);
  if (debugMode)              bitSet(mask, 1);
  if (state.twIndex > 0)      bitSet(mask, 2);
  if (audioEnabled)           bitSet(mask, 3);
  if (!simpitConnected)       bitSet(mask, 4);
  if (testPsForceOn)          bitSet(mask, 5);
  // vehCtrlMode and vesselType affect SPCFT button colour/text
  if (state.vehCtrlMode != prev.vehCtrlMode ||
      state.vesselType  != prev.vesselType)   bitSet(mask, 6);

  if (mask == prevPanelStatusMask) return;
  prevPanelStatusMask = mask;

  // Draw all 10 panel status buttons
  for (uint8_t i = 0; i < PS_COLS * PS_ROWS; i++) {
    uint8_t col = i % PS_COLS;
    uint8_t row = i / PS_COLS;
    int16_t x   = PS_X0 + col * PS_BTN_W;
    int16_t y   = PS_Y0 + row * PS_BTN_H;
    ButtonLabel btn = panelStatus[i];
    bool on = false;

    switch (i) {
      case 0:  // DEMO/DEBUG/CTRL -- black background, colored text always
        if (debugMode) {
          btn.text         = "DEBUG";
          btn.fontColorOff = TFT_PURPLE;
        } else if (demoMode) {
          btn.fontColorOff = TFT_BLUE;
        } else {
          btn.text         = "CTRL";
          btn.fontColorOff = TFT_DARK_GREEN;
        }
        on = false;  // always dark background
        break;
      case 1:  // WARP
        on = (state.twIndex > 0);
        break;
      case 2:  // AUDIO
        on = audioEnabled;
        break;
      case 3:  // THRTL ENA -- not yet wired; testPsForceOn overrides for display test
        on = bitRead(testPsForceOn, 3);
        break;
      case 4:  // TRIM SET -- not yet wired; testPsForceOn overrides for display test
        on = bitRead(testPsForceOn, 4);
        break;
      case 5:  // SPCFT/PLN/RVR -- black background, text colour = green(match) or red(mismatch)
        btn.text         = ctrlModeText(state.vehCtrlMode);
        btn.fontColorOff = ctrlModeColor(state.vehCtrlMode, state.vesselType);
        on = false;  // always dark background
        break;
      case 6:  // SWITCH ERR -- placeholder, not yet wired
        on = false;
        break;
      case 7:  // SIMPIT LOST
        on = !simpitConnected;
        break;
      case 8:  // THRTL PREC -- not yet wired; testPsForceOn overrides for display test
        on = bitRead(testPsForceOn, 8);
        break;
      case 9:  // PREC INPUT -- not yet wired; testPsForceOn overrides for display test
        on = bitRead(testPsForceOn, 9);
        break;
    }
    drawButton(tft, x, y, PS_BTN_W, PS_BTN_H, btn, &Roboto_Black_16, on);
  }
}


/***************************************************************************************
   DOCKED INDICATOR UPDATE
   Double-height button to the right of panel status strip.
   Uses drawVerticalText so "DOCKED" reads top-to-bottom within the tall narrow button.
   Lit green when vessel is docked, dark grey when not.
****************************************************************************************/
static bool prevDockedState = false;

void updateDockedIndicator(RA8875 &tft) {
  bool isDocked = bitRead(state.vesselSituationState, VSIT_DOCKED);
  if (isDocked == prevDockedState) return;
  prevDockedState = isDocked;
  uint16_t bgColor   = isDocked ? TFT_JUNGLE    : TFT_OFF_BLACK;
  uint16_t textColor = isDocked ? TFT_WHITE      : TFT_DARK_GREY;
  drawVerticalText(tft, DOCKED_X, DOCKED_Y, DOCKED_W, DOCKED_H,
                   &Roboto_Black_16, "DOCK", textColor, bgColor);
  tft.drawRect(DOCKED_X, DOCKED_Y, DOCKED_W, DOCKED_H, TFT_GREY);
}


/***************************************************************************************
   RESET SITUATION AND PANEL STATE SENTINELS
   Forces full redraws of situation column, panel status, flight condition block,
   and DOCK indicator on the next update pass. Called by drawStaticMain() and by
   TestMode's display walk-through between steps.
****************************************************************************************/
void resetSitAndPanelState() {
  _prevContact        = false;
  prevPanelStatusMask = 0xFF;
  prevFlightCondIdx   = -2;
  // prevDockedState left as-is -- forceDockState() handles it per-step in walk-through
  // drawStaticMain sets it explicitly before calling updateDockedIndicator
}

// Force the CONTACT button dirty tracker to a specific value.
void forceContactState(bool newContact) {
  _prevContact = !newContact;
}

// Force the DOCK indicator dirty tracker.
void forceDockState(bool isDocked) {
  prevDockedState = !isDocked;
}


/***************************************************************************************
   STATIC CHROME -- draw once on screen entry
   Draws all fixed elements and syncs prev fields to prevent immediate redraw.
   Also draws the 1px separator line at y=0 above the C&W grid.
****************************************************************************************/
void drawStaticMain(RA8875 &tft) {
  tft.setXY(0, 0);
  tft.fillScreen(TFT_BLACK);

  // --- Zone gap fills in TFT_DARK_GREY to visually separate all regions ---
  // Left gutter: between left column right edge and C&W/panel strip left edge
  tft.fillRect(240,                            0,   5,   480,     TFT_SILVER);
  // Top gutter: above C&W grid
  tft.fillRect(CW_X0,                          0,   CW_COLS * CW_BTN_W, CW_Y0, TFT_SILVER);
  // Right gutter: between C&W grid right edge and situation column
  tft.fillRect(CW_X0 + CW_COLS * CW_BTN_W,    0,   5,   480,     TFT_SILVER);
  // Bottom gutter: below C&W grid, above panel strip
  tft.fillRect(CW_X0, CW_Y0 + CW_ROWS * CW_BTN_H,
               CW_COLS * CW_BTN_W,
               PS_Y0 - (CW_Y0 + CW_ROWS * CW_BTN_H),              TFT_SILVER);
  // Gap between panel condition right edge and flight condition left edge
  tft.fillRect(PS_X0 + PS_COLS * PS_BTN_W,    PS_Y0,
               FC_X0 - (PS_X0 + PS_COLS * PS_BTN_W), 104,          TFT_SILVER);
  // Separator between DOCK and situation column
  tft.fillRect(SIT_X0, DOCKED_H,              SIT_BTN_W, 2,        TFT_SILVER);
  // Separators inside left column
  tft.drawLine(0, SOI_LABEL_Y - 1, 239, SOI_LABEL_Y - 1, TFT_SILVER);
  tft.drawLine(0, DATA_Y      - 1, 239, DATA_Y      - 1, TFT_SILVER);

  // Master Alarm
  drawButton(tft, MASTER_X, MASTER_Y, MASTER_W, MASTER_H,
             masterAlarmLabel, &Roboto_Black_48, state.masterAlarmOn);
  prev.masterAlarmOn = state.masterAlarmOn;

  // C&W panel (full redraw)
  updateCautWarnPanel(tft, ~state.cautionWarningState, state.cautionWarningState);
  prev.cautionWarningState = state.cautionWarningState;

  // Vessel situation column -- force all 8 buttons to draw by using 0xFF as prevSit
  // so every bit differs from current state.
  resetSitAndPanelState();
  forceContactState( bitRead(state.vesselSituationState, VSIT_LANDED) ||
                     bitRead(state.vesselSituationState, VSIT_SPLASH) );
  updateVesselSitPanel(tft, 0xFF, state.vesselSituationState);
  prev.vesselSituationState = state.vesselSituationState;

  // Flight condition block
  prevFlightCondIdx = -2;
  updateFlightCondBlock(tft);

  // Panel status strip
  prevPanelStatusMask = 0xFF;
  updatePanelStatus(tft);

  // DOCKED indicator
  prevDockedState = !bitRead(state.vesselSituationState, VSIT_DOCKED);
  updateDockedIndicator(tft);

  // SOI label chrome
  printDispChrome(tft, &Roboto_Black_24, SOI_LABEL_X, SOI_LABEL_Y,
                  SOI_LABEL_W, SOI_LABEL_H, "SOI:", TFT_WHITE, TFT_BLACK, TFT_GREY);

  // Globe border
  tft.drawRect(SOI_GLOBE_X, SOI_GLOBE_Y, SOI_GLOBE_W, SOI_GLOBE_H, TFT_GREY);

  // Data strip chrome
  printDispChrome(tft, &Roboto_Black_24, DATA_X, DATA_Y,
                  DATA_W, DATA_H, "CtrlGrp:", TFT_WHITE, TFT_BLACK, TFT_GREY);
  printDispChrome(tft, &Roboto_Black_24, DATA_X, DATA_Y + DATA_H,
                  DATA_W, DATA_H, "TW:", TFT_WHITE, TFT_BLACK, TFT_GREY);
}


/***************************************************************************************
   UPDATE PASS -- redraws only widgets whose state has changed
****************************************************************************************/
void updateScreenMain(RA8875 &tft) {

  // --- MASTER ALARM ---
  if (state.masterAlarmOn != prev.masterAlarmOn) {
    drawButton(tft, MASTER_X, MASTER_Y, MASTER_W, MASTER_H,
               masterAlarmLabel, &Roboto_Black_48, state.masterAlarmOn);
    prev.masterAlarmOn = state.masterAlarmOn;
  }

  // --- C&W PANEL ---
  // Redraw on C&W bit change OR chute env state change (colour-only change).
  if (state.cautionWarningState != prev.cautionWarningState ||
      chuteEnvState != prevChuteEnvState) {

    uint32_t newBits = state.cautionWarningState & ~prev.cautionWarningState;
    uint32_t clrBits = prev.cautionWarningState  & ~state.cautionWarningState;

    if (audioEnabled) {
      // Master alarm conditions
      if (newBits & (1ul << CW_LOW_DV))       updateAlarmMask(ALARM_LOW_DV,      true);
      if (clrBits & (1ul << CW_LOW_DV))       updateAlarmMask(ALARM_LOW_DV,      false);
      if (newBits & (1ul << CW_HIGH_G))        updateAlarmMask(ALARM_HIGH_G,      true);
      if (clrBits & (1ul << CW_HIGH_G))        updateAlarmMask(ALARM_HIGH_G,      false);
      if (newBits & (1ul << CW_HIGH_TEMP))     updateAlarmMask(ALARM_HIGH_TEMP,   true);
      if (clrBits & (1ul << CW_HIGH_TEMP))     updateAlarmMask(ALARM_HIGH_TEMP,   false);
      if (newBits & (1ul << CW_BUS_VOLTAGE))   updateAlarmMask(ALARM_BUS_VOLTAGE, true);
      if (clrBits & (1ul << CW_BUS_VOLTAGE))   updateAlarmMask(ALARM_BUS_VOLTAGE, false);
      if (newBits & (1ul << CW_ABORT))         updateAlarmMask(ALARM_ABORT,       true);
      if (clrBits & (1ul << CW_ABORT))         updateAlarmMask(ALARM_ABORT,       false);
      if (newBits & (1ul << CW_GROUND_PROX))   updateAlarmMask(ALARM_GROUND_PROX, true);
      if (clrBits & (1ul << CW_GROUND_PROX))   updateAlarmMask(ALARM_GROUND_PROX, false);
      if (newBits & (1ul << CW_PE_LOW))        updateAlarmMask(ALARM_PE_LOW,      true);
      if (clrBits & (1ul << CW_PE_LOW))        updateAlarmMask(ALARM_PE_LOW,      false);
      if (newBits & (1ul << CW_PROP_LOW))      updateAlarmMask(ALARM_PROP_LOW,    true);
      if (clrBits & (1ul << CW_PROP_LOW))      updateAlarmMask(ALARM_PROP_LOW,    false);
      if (newBits & (1ul << CW_LIFE_SUPPORT))  updateAlarmMask(ALARM_LIFE_SUPPORT,true);
      if (clrBits & (1ul << CW_LIFE_SUPPORT))  updateAlarmMask(ALARM_LIFE_SUPPORT,false);

      // Caution tones on transition to active
      if (newBits & (1ul << CW_ALT))           audioCautionTone();
      if (newBits & (1ul << CW_IMPACT_IMM))    audioCautionTone();
      if (newBits & ((1ul << CW_DESCENT) |
                     (1ul << CW_ATMO)   |
                     (1ul << CW_GEAR_UP)))      audioCautionChirp();
    }

    updateCautWarnPanel(tft, prev.cautionWarningState, state.cautionWarningState);
    prev.cautionWarningState = state.cautionWarningState;
  }

  // --- VESSEL SITUATION ---
  if (state.vesselSituationState != prev.vesselSituationState) {
    updateVesselSitPanel(tft, prev.vesselSituationState, state.vesselSituationState);
    prev.vesselSituationState = state.vesselSituationState;
  }

  // --- FLIGHT CONDITION BLOCK ---
  // Depends on vesselSituationState, inAtmo, alt_sl, currentBody -- update each pass.
  updateFlightCondBlock(tft);

  // --- PANEL STATUS ---
  updatePanelStatus(tft);

  // --- DOCKED INDICATOR ---
  updateDockedIndicator(tft);

  // --- SOI LABEL AND GLOBE ---
  if (state.gameSOI != prev.gameSOI) {
    printDisp(tft, &Roboto_Black_24, SOI_LABEL_X, SOI_LABEL_Y,
              SOI_LABEL_W, SOI_LABEL_H,
              "SOI:", currentBody.dispName,
              TFT_WHITE, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, TFT_GREY, psSOILabel);
    tft.fillRect(SOI_GLOBE_X + 1, SOI_GLOBE_Y + 1,
                 SOI_GLOBE_W - 2, SOI_GLOBE_H - 2, TFT_BLACK);
    drawBMP(tft, currentBody.image, SOI_GLOBE_X + 2, SOI_GLOBE_Y + 2);
    tft.drawRect(SOI_GLOBE_X, SOI_GLOBE_Y, SOI_GLOBE_W, SOI_GLOBE_H, TFT_GREY);
    prev.gameSOI = state.gameSOI;
  }

  // --- DATA STRIP: CtrlGrp ---
  if (state.ctrlGrp != prev.ctrlGrp) {
    printValue(tft, &Roboto_Black_24, DATA_X, DATA_Y, DATA_W, DATA_H,
               "CtrlGrp:", formatInt(state.ctrlGrp),
               TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, psCtrlGrp);
    prev.ctrlGrp = state.ctrlGrp;
  }

  // --- DATA STRIP: TW ---
  if (state.twIndex != prev.twIndex) {
    printValue(tft, &Roboto_Black_24, DATA_X, DATA_Y + DATA_H, DATA_W, DATA_H,
               "TW:", twString(state.twIndex, physTW),
               TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, psTW);
    prev.twIndex = state.twIndex;
  }

  // --- THRESHOLD CROSSING ALERT CHIRPS ---
  // Suppressed on first pass after entering main screen to avoid spurious triggers.
  if (audioEnabled && !firstPassOnMain) {
    if (state.alt_sl >= ALERT_ALT_THRESHOLD && prev.alt_sl < ALERT_ALT_THRESHOLD)
      audioAlertChirp();
    if (state.vel_surf >= ALERT_VEL_THRESHOLD && prev.vel_surf < ALERT_VEL_THRESHOLD)
      audioAlertChirp();
    if (currentBody.minSafe > 0 &&
        state.apoapsis >= currentBody.minSafe && prev.apoapsis < currentBody.minSafe)
      audioAlertChirp();
    if ((state.vesselSituationState & (1 << VSIT_ORBIT)) &&
        !(prev.vesselSituationState & (1 << VSIT_ORBIT)))
      audioAlertChirp();
  }

  prev.alt_sl   = state.alt_sl;
  prev.vel_surf = state.vel_surf;
  prev.apoapsis = state.apoapsis;

  firstPassOnMain = false;
}

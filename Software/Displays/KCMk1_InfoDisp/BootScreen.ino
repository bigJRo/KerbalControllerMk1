/***************************************************************************************
   BootScreen.ino -- Boot simulation screen for KCMk1 Info Display
   Three KSP-themed sequences selected at random each boot, each with internal
   variation drawn from item pools so the sequence differs run to run.

   Uses tft.setFont / setCursor / print (GFX graphics mode calls) exclusively.
   The RA8875 setFontScale/print API puts the chip into internal text mode and the
   PaulStoffregen v0.7.11 library does not expose a public graphics-mode restore call,
   so we avoid text mode entirely and stay in graphics mode throughout.

   TO CHANGE THE BOOT TEXT: edit the pool arrays in the three _boot_* functions below.
   Do not edit the rendering helpers (_bs_line, _bs_big, etc.).

   Call bootSimText() from setup() after setupDisplay() and setupI2CSlave().
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


/***************************************************************************************
   CONSTANTS
****************************************************************************************/
static const uint16_t BS_HOLD  = 300;
static const uint16_t BS_ROW_H = 18;
static const uint16_t BS_COL_X = 4;
static const tFont   *BS_FONT  = &TerminalFont_16;
static const tFont   *BS_BIG   = &TerminalFont_32;


/***************************************************************************************
   RENDERING HELPERS (#5E — delegate to KDC library)
   Signatures unchanged so _boot_B/C/E call sites are unmodified.
****************************************************************************************/
static void _bs_wait(uint16_t ms) { delay(ms); }

static void _bs_print(RA8875 &tft, const tFont *font, uint16_t x, uint16_t y,
                      const char *text, uint16_t col) {
  bsPrint(tft, font, x, y, text, col);
}

static uint16_t _bs_line(RA8875 &tft, uint16_t y, const char *text, uint16_t col) {
  return bsLine(tft, BS_FONT, BS_COL_X, y, BS_ROW_H, text, col);
}

static uint16_t _bs_blank(uint16_t y) { return bsBlank(y, BS_ROW_H); }

static uint16_t _bs_big(RA8875 &tft, uint16_t y, const char *text, uint16_t col) {
  return bsBig(tft, BS_BIG, BS_COL_X, y, text, col);
}

static uint16_t _bs_wrap(RA8875 &tft, uint16_t y, const char *text,
                          uint16_t col, uint16_t maxW) {
  return bsWrap(tft, BS_FONT, BS_COL_X, y, BS_ROW_H, text, col, maxW);
}

// Standard header bar used by all three sequences
static uint16_t _bs_header(RA8875 &tft, uint16_t y, const char *title) {
  tft.fillRect(0, y, 800, 2, TFT_GREY); y += 4;
  char buf[128];   // #4B enlarged for full version string
  snprintf(buf, sizeof(buf),
           "KCMk1-INFODISP  //  Jeb's Controller Works  //  v%d.%d.%d"
           " / KDC %d.%d.%d",
           SKETCH_VERSION_MAJOR, SKETCH_VERSION_MINOR, SKETCH_VERSION_PATCH,
           KDC_VERSION_MAJOR,    KDC_VERSION_MINOR,    KDC_VERSION_PATCH);
  _bs_print(tft, BS_FONT, BS_COL_X, y, buf, TFT_GREY); y += BS_ROW_H;
  tft.fillRect(0, y, 800, 2, TFT_GREY); y += 4;
  _bs_wait(BS_HOLD);
  y = _bs_big(tft, y, title, TFT_WHITE);
  _bs_wait(BS_HOLD);
  return y;
}

// _bs_shuffle delegates to KDC bsShuffle (#5E)
static void _bs_shuffle(uint8_t *arr, uint8_t n) {
  bsShuffle(arr, n);
}


/***************************************************************************************
   SEQUENCE B -- JEB'S MISSION LOG
   Pool of 15 day entries; 6 shown per boot in random order.
****************************************************************************************/
static void _boot_B(RA8875 &tft) {
  static const char * const days[] = {
    "1",  "2",  "3",  "5",  "7",
    "9",  "12", "14", "17", "19",
    "23", "28", "33", "41", "47"
  };
  static const char * const entries[] = {
    /* 0  */ "Gene said this would be a simple orbital mission.",
    /* 1  */ "I have already lost the launch clamps. Both of them.",
    /* 2  */ "Circularization burn complete. Mostly circular.",
    /* 3  */ "Val says 'mostly circular' is not a technical term. Filed for future reference.",
    /* 4  */ "Attempted Mun transfer. Overshot. Now heading to Minmus.",
    /* 5  */ "Gene says this is fine. Gene does not look fine.",
    /* 6  */ "Minmus landing successful. It is very slippery.",
    /* 7  */ "The rover is somewhere on the surface. Will find it eventually.",
    /* 8  */ "Found the rover. It found me first.",
    /* 9  */ "Low on fuel. Kerbal Space Program math says I have exactly enough dV to get home.",
    /* 10 */ "Kerbal Space Program math has been wrong before.",
    /* 11 */ "Attempted aerobrake. Kerbin atmosphere said no.",
    /* 12 */ "Parachutes deployed. Only two were upside down. Still counts.",
    /* 13 */ "Gene is no longer taking my calls. His loss.",
    /* 14 */ "Splashdown confirmed. Slight overshoot. This was the plan."
  };
  static const uint8_t POOL = 15, SHOW = 6;

  uint8_t idx[POOL];
  for (uint8_t i = 0; i < POOL; i++) idx[i] = i;
  _bs_shuffle(idx, POOL);

  uint16_t y = 0, wrapW = 792;
  y = _bs_header(tft, y, "MISSION LOG -- KSC FLIGHT 247");

  for (uint8_t i = 0; i < SHOW; i++) {
    char buf[16];
    snprintf(buf, sizeof(buf), "Day %s.", days[idx[i]]);
    y = _bs_line(tft, y, buf, TFT_CYAN);
    y = _bs_wrap(tft, y, entries[idx[i]], TFT_WHITE, wrapW);
    _bs_wait(BS_HOLD);
  }

  y = _bs_blank(y);
  y = _bs_line(tft, y, "TELEMETRY RESTORED.", TFT_GREEN);
  _bs_big(tft, y + 4, "Welcome back, Jeb.", TFT_WHITE);
  _bs_wait(2000);
}


/***************************************************************************************
   SEQUENCE C -- LOADING SCREEN TIPS
   Pool of 15 tips; 6 shown per boot in random order.
   3 closing quips; 1 selected at random.
****************************************************************************************/
static void _boot_C(RA8875 &tft) {
  static const char * const tips_head[] = {
    /* 0  */ "TIP: The Mun has no atmosphere.",
    /* 1  */ "TIP: Delta-V is the currency of space travel.",
    /* 2  */ "TIP: Parachutes work best when deployed before landing.",
    /* 3  */ "TIP: The navball is your friend.",
    /* 4  */ "TIP: Thrust-to-weight ratio matters.",
    /* 5  */ "TIP: Check your staging sequence.",
    /* 6  */ "TIP: Aerobraking is free delta-V.",
    /* 7  */ "TIP: Landing legs are optional.",
    /* 8  */ "TIP: The Kraken is not a myth.",
    /* 9  */ "TIP: Kerbals are surprisingly resilient.",
    /* 10 */ "TIP: Maneuver nodes are your friend.",
    /* 11 */ "TIP: Fuel math is important.",
    /* 12 */ "TIP: Communication antennas have limited range.",
    /* 13 */ "TIP: Gravity assists are essentially free.",
    /* 14 */ "TIP: Quick save exists for a reason."
  };
  static const char * const tips_body[] = {
    /* 0  */ "This will not stop Jeb from trying to aerobrake.",
    /* 1  */ "You are probably already bankrupt.",
    /* 2  */ "Technically any time before reaching the ground is still valid.",
    /* 3  */ "You needed it three minutes ago.",
    /* 4  */ "A ratio below 1.0 is called a 'ground feature.'",
    /* 5  */ "Check it twice. Check it again. Then check it one more time.",
    /* 6  */ "Alternatively, exploding is also free.",
    /* 7  */ "Craters are a valid landing zone.",
    /* 8  */ "It has claimed ships far more competent than yours.",
    /* 9  */ "Whether this is a feature or a bug remains under investigation.",
    /* 10 */ "Place them. Use them. Do not ignore them like last time.",
    /* 11 */ "Specifically whether you have enough of it to also get home.",
    /* 12 */ "Jeb is currently out of contact. Gene is aware and has moved on.",
    /* 13 */ "Kerbal scientists remain baffled by how Jeb keeps finding them.",
    /* 14 */ "It does not prevent consequences. It merely postpones them."
  };
  static const char * const closings[] = {
    "LOADING COMPLETE. Please ensure all crew have signed their waivers.",
    "LOADING COMPLETE. Good luck. You will probably need it.",
    "LOADING COMPLETE. Jeb is ready. Gene is not. Launching anyway."
  };
  static const uint8_t POOL = 15, SHOW = 6;

  uint8_t idx[POOL];
  for (uint8_t i = 0; i < POOL; i++) idx[i] = i;
  _bs_shuffle(idx, POOL);

  uint16_t y = 0, wrapW = 792;
  y = _bs_header(tft, y, "LOADING KERBAL FLIGHT SYSTEMS...");

  for (uint8_t i = 0; i < SHOW; i++) {
    y = _bs_line(tft, y, tips_head[idx[i]], TFT_YELLOW);
    y = _bs_wrap(tft, y, tips_body[idx[i]], TFT_WHITE, wrapW);
    y = _bs_blank(y);
    _bs_wait(BS_HOLD);
  }

  y = _bs_wrap(tft, y, closings[random(3)], TFT_GREEN, wrapW);
  _bs_wait(2000);
}


/***************************************************************************************
   SEQUENCE E -- GENE'S PRE-FLIGHT CHECKLIST
   Pool of 15 items; 7 shown per boot in random order.
   Crew manifest is always last (fixed).
****************************************************************************************/
static void _boot_E(RA8875 &tft) {
  static const char * const items[] = {
    /* 0  */ "Thrust-to-weight ratio ..........",
    /* 1  */ "Staging sequence .................",
    /* 2  */ "Fairings .........................",
    /* 3  */ "Landing legs .....................",
    /* 4  */ "Parachutes .......................",
    /* 5  */ "Return fuel ......................",
    /* 6  */ "Science experiments ..............",
    /* 7  */ "Communications antenna ...........",
    /* 8  */ "Solar panels .....................",
    /* 9  */ "Heat shields .....................",
    /* 10 */ "Reaction wheels ..................",
    /* 11 */ "RCS propellant ...................",
    /* 12 */ "Abort system .....................",
    /* 13 */ "Launch clamps ....................",
    /* 14 */ "Flight plan ......................"
  };
  static const char * const statuses[] = {
    /* 0  */ "CHECK  (barely)",
    /* 1  */ "CHECK",
    /* 2  */ "CHECK  (Val added three more)",
    /* 3  */ "CHECK  (added 4 legs, Jeb asked why 4)",
    /* 4  */ "CHECK",
    /* 5  */ "CHECK  (optimistic)",
    /* 6  */ "CHECK  (Val added 7 more. Nobody asked Val.)",
    /* 7  */ "CHECK  (range: optimistic)",
    /* 8  */ "CHECK  (most of them)",
    /* 9  */ "CHECK  (Jeb says he won't need it)",
    /* 10 */ "CHECK",
    /* 11 */ "CHECK  (enough, probably)",
    /* 12 */ "...    (Gene is still looking)",
    /* 13 */ "CHECK  (remembering to detach: pending)",
    /* 14 */ "CHECK  (Jeb has not read it)"
  };
  static const uint16_t status_cols[] = {
    TFT_DARK_GREEN, TFT_DARK_GREEN, TFT_YELLOW,
    TFT_YELLOW,     TFT_DARK_GREEN, TFT_YELLOW,
    TFT_YELLOW,     TFT_YELLOW,     TFT_YELLOW,
    TFT_YELLOW,     TFT_DARK_GREEN, TFT_YELLOW,
    TFT_RED,        TFT_YELLOW,     TFT_RED
  };
  static const uint8_t POOL = 15, SHOW = 7;

  uint8_t idx[POOL];
  for (uint8_t i = 0; i < POOL; i++) idx[i] = i;
  _bs_shuffle(idx, POOL);

  uint16_t y = 0;
  y = _bs_header(tft, y, "KSC LAUNCH CONTROL -- PRE-FLIGHT");
  y = _bs_line(tft, y, "Flight Director: Gene Kerman", TFT_GREY);
  y = _bs_blank(y);
  _bs_wait(BS_HOLD);

  for (uint8_t i = 0; i < SHOW; i++) {
    uint8_t n = idx[i];
    char buf[80];
    snprintf(buf, sizeof(buf), "%s %s", items[n], statuses[n]);
    y = _bs_line(tft, y, buf, status_cols[n]);
    _bs_wait(BS_HOLD / 2);
  }

  // Crew manifest always last
  y = _bs_line(tft, y,
    "Crew manifest .................... CHECK  (Jeb: READY)",
    TFT_DARK_GREEN);
  _bs_wait(BS_HOLD);

  y = _bs_blank(y);
  y = _bs_line(tft, y, "BOARD IS GREEN.", TFT_GREEN);
  y = _bs_line(tft, y, "Godspeed, Jebediah.", TFT_WHITE);
  _bs_big(tft, y + 4, "Try to come back this time.", TFT_CYAN);
  _bs_wait(2000);
}


/***************************************************************************************
   BOOT SIM TEXT -- entry point
   Randomly selects one of three sequences each boot.
   Teensy 4.0 hardware RNG provides entropy without needing an analog pin.
****************************************************************************************/
void bootSimText(RA8875 &tft) {
  tft.fillScreen(TFT_BLACK);

  // Enable ARM cycle counter and seed random() from it.
  // The counter has genuine jitter from hardware init timing differences boot to boot.
  ARM_DEMCR |= ARM_DEMCR_TRCENA;
  ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
  randomSeed(ARM_DWT_CYCCNT ^ (micros() << 13) ^ (micros() * 31337UL));

  switch (random(3)) {
    case 0: _boot_B(tft); break;
    case 1: _boot_C(tft); break;
    case 2: _boot_E(tft); break;
  }
}

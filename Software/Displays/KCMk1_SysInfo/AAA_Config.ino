/***************************************************************************************
   AAA_Config.ino -- pins, timings, thresholds, names and icons

   Pins from the KC-01-1901 V2.0 schematic; GPIO numbers from Seeed's XIAO RP2350
   pinout (design §1.2, confirmed at the first build, bench check #1).
****************************************************************************************/

/*--------------------------------------------------------------------------------------
  Pins
--------------------------------------------------------------------------------------*/
static const uint8_t PIN_BL     = 26;   // D0  BACKLITE -> panel BLK (PWM, high = on)
static const uint8_t PIN_TFT_CS = 27;   // D1  TFT_CS (10 k pull-up)
static const uint8_t PIN_TFT_DC = 28;   // D2  TFT_DC
static const uint8_t PIN_TFT_RST = 5;   // D3  RESET
static const uint8_t PIN_SDA    = 6;    // D4  SDA_BUS -- I2C1, so Wire1
static const uint8_t PIN_SCL    = 7;    // D5  SCL_BUS
static const uint8_t PIN_INT    = 0;    // D6  INT_BUS -> master INT_2; unused, held high
static const uint8_t PIN_SD_CS  = 1;    // D7  SD_CS (10 k pull-up)
static const uint8_t PIN_SCK    = 2;    // D8  SPI0 SCK
static const uint8_t PIN_MISO   = 4;    // D9  SPI0 RX
static const uint8_t PIN_MOSI   = 3;    // D10 SPI0 TX

static const uint32_t TFT_SPI_HZ   = 62500000;   // bench check #2: 40 MHz if it tears
static const uint8_t  TFT_ROTATION = 0;          // portrait; 2 if upside down as mounted
static const uint32_t BL_PWM_HZ    = 20000;

/*--------------------------------------------------------------------------------------
  Timings
--------------------------------------------------------------------------------------*/
static const uint32_t SILENT_MS        = 5000;     // no frame for this long: MASTER SILENT
static const uint32_t MSG_MS           = 2500;     // each message-area entry
static const uint32_t SWERR_PAGE_MS    = 3000;     // SWITCH ERR pages
static const uint32_t SHIFT_MS         = 600000;   // anti-sticking shift, every 10 min
static const uint32_t SPLASH_MIN_MS    = 2000;     // splash: at least this long ...
static const uint32_t SPLASH_MAX_MS    = 5000;     // ... and no longer, line or not
static const uint32_t I2C_RISE_MS      = 60000;    // I2C errors "rising": within this
static const uint32_t WDT_MS           = 2000;

/*--------------------------------------------------------------------------------------
  Thresholds (design §4.2)
--------------------------------------------------------------------------------------*/
static const uint16_t V_CAUTION_CV  = 1150;   // below 11.50 V: yellow
static const uint16_t V_WARNING_CV  = 1100;   // below 11.00 V: red (the master's POWER_LOW_V)
static const int16_t  A_CAUTION_MA  = 7000;   // above 7 A: yellow (D7)
static const int16_t  A_WARNING_MA  = 8000;   // above 8 A: red (D7)
static const int8_t   T_CAUTION_C   = 50;     // from 50 C: yellow (D8)
static const int8_t   T_WARNING_C   = 60;     // from 60 C: red (D8)

/*--------------------------------------------------------------------------------------
  Names, in the master's order
--------------------------------------------------------------------------------------*/
// STATUS links / known bits 0..4 (sysinfo.h SI_LINK_*).
static const char *const LINK_NAME[SI_LINK_COUNT] = {
  "MASTER", "ANNUNCIATOR", "INFO DISP 1", "INFO DISP 2", "RESOURCE"
};
// STATUS present / FAULT mask bits (module_bus.h ModId order).
static const char *const MOD_NAME[] = {
  "UI", "FUNCTION", "ACTION", "STABILITY", "VEHICLE", "TIME", "AUX", "ROTATION",
  "TRANSLATION", "GPWS", "PRE-WARP", "THROTTLE", "DUAL ENC"
};
static const uint8_t MOD_NAMED = sizeof MOD_NAME / sizeof MOD_NAME[0];

// SWITCH_ERR ids 1..16: name and the two positions, as kSw[] in the master's switches.cpp.
struct SwitchName { const char *name, *pos0, *pos1; };
static const SwitchName SW_NAME[] = {
  { "-", "", "" },
  { "MSTR", "OPER", "RESET" },  { "DISP", "OPER", "CLR" },    { "SCE", "NORM", "AUX" },
  { "UPTLM", "INHIB", "XMIT" }, { "LTG", "NORM", "TEST" },    { "A/P", "INHIB", "ARM" },
  { "ENGINE", "SAFE", "ARM" },  { "THRTL", "STD", "FINE" },   { "CHUTE", "SAFE", "ARM" },
  { "GEAR", "UP", "DOWN" },     { "BRAKE", "REL", "LOCK" },   { "EXT LT", "OFF", "ILLUM" },
  { "SAS", "OFF", "ENAB" },     { "RCS", "OFF", "ENAB" },     { "THC/RHC", "NORM", "PREC" },
  { "AUDIO", "MUTE", "LIVE" },
};
static const uint8_t SW_NAMED = sizeof SW_NAME / sizeof SW_NAME[0];

/*--------------------------------------------------------------------------------------
  Icons: 16 x 16, '#' lit, drawn at 2x (32 px) in the colour of the value beside them
--------------------------------------------------------------------------------------*/
static Icon ICON_BOLT = {
  ".........#####..", "........#####...", ".......#####....", "......#####.....",
  ".....#####......", "....#####.......", "...###########..", "..###########...",
  ".......#####....", "......#####.....", ".....#####......", "....####........",
  "...####.........", "..###...........", "..##............", ".#..............",
};
static Icon ICON_THERM = {
  "......###.......", ".....##.##......", ".....#...#..##..", ".....#.#.#......",
  ".....#.#.#..##..", ".....#.#.#......", ".....#.#.#..##..", ".....#.#.#......",
  ".....#.#.#......", "....##.#.##.....", "...##.###.##....", "...#.#####.#....",
  "...#.#####.#....", "...##.###.##....", "....##...##.....", ".....#####......",
};
static Icon ICON_FAN = {
  "......####......", ".....######.....", ".....######.....", "......####......",
  "##....####....##", "###...####...###", "####..####..####", "######.##.######",
  "######.##.######", "####..####..####", "###...####...###", "##....####....##",
  "......####......", ".....######.....", ".....######.....", "......####......",
};
static Icon ICON_LINK = {
  "................", "....#####.......", "...##...##......", "..##.....##.....",
  "..#.......#.....", "..#...#####.....", "..##.##...##....", "...###.....##...",
  "....##.....###..", "....##...##.##..", ".....#####...#..", ".....#.......#..",
  ".....##.....##..", "......##...##...", ".......#####....", "................",
};
static Icon ICON_CHIP = {
  "...#..#..#..#...", "...#..#..#..#...", "..############..", "..#..........#..",
  "###..........###", "..#..######..#..", "..#..#....#..#..", "###..#....#..###",
  "..#..#....#..#..", "..#..######..#..", "###..........###", "..#..........#..",
  "..############..", "...#..#..#..#...", "...#..#..#..#...", "................",
};
static Icon ICON_OK = {
  "................", "..............##", ".............###", "............###.",
  "...........###..", "..........###...", ".........###....", "##......###.....",
  "###....###......", ".###..###.......", "..######........", "...####.........",
  "....##..........", "................", "................", "................",
};
static Icon ICON_WARN = {
  ".......##.......", "......####......", "......####......", ".....##..##.....",
  ".....##..##.....", "....##.##.##....", "....##.##.##....", "...##..##..##...",
  "...##..##..##...", "..##...##...##..", "..##........##..", ".##.....##....##",
  ".##.....##....##", "##............##", "################", "################",
};
static Icon ICON_ERR = {
  "................", ".##..........##.", ".###........###.", "..###......###..",
  "...###....###...", "....###..###....", ".....######.....", "......####......",
  "......####......", ".....######.....", "....###..###....", "...###....###...",
  "..###......###..", ".###........###.", ".##..........##.", "................",
};

/*--------------------------------------------------------------------------------------
  Pins, watchdog
--------------------------------------------------------------------------------------*/
void pinsBegin() {
  pinMode(PIN_INT, OUTPUT);
  digitalWrite(PIN_INT, HIGH);         // idle: this protocol never asserts it
  SPI.setSCK(PIN_SCK);
  SPI.setTX(PIN_MOSI);
  SPI.setRX(PIN_MISO);
}

void wdtBegin() { rp2040.wdt_begin(WDT_MS); }
void wdtFeed()  { rp2040.wdt_reset(); }

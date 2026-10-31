/***************************************************************************************
   InputTest.ino -- the input test screen (Information Display 1 only).

   A hidden full-panel screen showing what the master reads from every module, raw
   (Hardware Inventory §8.11): each module's presence, lifecycle, FAULT flag, INT line
   and inputs (the button modules' state words as sent), both sticks, the lever, the
   mode selector, the safe lines, the control group, and what the Annunciator, both
   Information Displays and the Resource Display report in their status packets.

   Opened and closed by a two-finger diagonal pull down from the top-right corner:
   both fingers land in the corner zone (IT_ZONE_W x IT_ZONE_H) and their midpoint
   then travels at least IT_PULL_PX left AND down within IT_PULL_MS. Available any time
   (flight, standby or demo). The tracker runs ahead of the single-finger filter in
   processTouchEvents(); a touch it has claimed never reaches the tap handling, even as
   the fingers lift one at a time.

   While open, this panel reports screen INPUT_TEST_SCREEN (18) in byte 2 of its status
   packet; the master then pushes two frames (sync 0xAB, I2C spec §15.3; layout in
   KCMk1_Master/input_test.h): page 0, the inputs, every 100 ms; page 1 (the INT lines,
   the display modules' values and every module's lifecycle) and page 2 (the four 7"
   displays' status) every 500 ms. Frames are taken in demo too. The
   overlay is repainted whole (its own page, then a flip) when a frame arrives and at
   least every IT_REDRAW_MS. Closing it repaints the screen underneath, or the standby
   splash.

   Licensed under the GNU General Public License v3.0 (GPL-3.0).
   Written for Jeb's Controller Works.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

static const uint8_t  IT_SCREEN_ID   = 18;     // status byte 2 while open (master INPUT_TEST_SCREEN)
static const uint8_t  IT_SYNC        = 0xAB;   // both pages
static const uint8_t  IT_INPUTS_LEN  = 40;
static const uint8_t  IT_MODULES_LEN = 24;
static const uint8_t  IT_DISPLAYS_LEN = 24;
static const uint16_t IT_ZONE_W      = 320;    // the top-right corner zone the two fingers start in
static const uint16_t IT_ZONE_H      = 220;
static const uint16_t IT_PULL_PX     = 140;    // midpoint travel, left and down
static const uint32_t IT_PULL_MS     = 1500;
static const uint32_t IT_REDRAW_MS   = 250;
static const uint32_t IT_STALE_MS    = 1500;   // no page 0 for this long: NO DATA

static bool     _itOpen = false;
static uint8_t  _itPhase = 0;                  // 0 idle, 1 tracking, 2 done: wait for every finger up
static int32_t  _itX0 = 0, _itY0 = 0;
static uint32_t _itT0 = 0;
static uint8_t  _itIn[IT_INPUTS_LEN];
static uint8_t  _itMod[IT_MODULES_LEN];
static uint8_t  _itDsp[IT_DISPLAYS_LEN];
static bool     _itHaveIn = false, _itHaveMod = false, _itHaveDsp = false, _itDirty = false;
static uint32_t _itInMs = 0, _itDspMs = 0, _itDrawMs = 0;

bool    inputTestIsOpen()   { return _itOpen; }
uint8_t inputTestScreenId() { return IT_SCREEN_ID; }

static void inputTestToggle() {
  _itOpen = !_itOpen;
  if (debugMode) Serial.println(_itOpen ? F("InfoDisp: input test screen open") : F("InfoDisp: input test screen closed"));
  if (_itOpen) {
    _itDirty = true;
    _itDrawMs = 0;
  } else if (flightScene || demoMode) {
    prevScreen = screen_COUNT;                 // the loop repaints the screen underneath in full
  } else {
    drawStandbyScreen(infoDisp);
  }
}

/***************************************************************************************
   GESTURE TRACKER -- called with every touch read (count 0 when nothing touches).
   Returns true when the touch belongs to the gesture, so the caller drops it.
****************************************************************************************/
bool inputTestTouch(uint8_t count, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  if (INFO_DISP_UNIT != 1) return false;
  if (count == 0) { _itPhase = 0; return false; }
  uint32_t now = millis();
  if (count == 2) {
    int32_t cx = ((int32_t)x0 + x1) / 2, cy = ((int32_t)y0 + y1) / 2;
    if (_itPhase == 0) {
      bool inZone = x0 >= SCREEN_W - IT_ZONE_W && x1 >= SCREEN_W - IT_ZONE_W && y0 <= IT_ZONE_H && y1 <= IT_ZONE_H;
      if (!inZone) return false;
      _itPhase = 1; _itX0 = cx; _itY0 = cy; _itT0 = now;
      return true;
    }
    if (_itPhase == 1) {
      if (now - _itT0 > IT_PULL_MS) { _itPhase = 2; return true; }
      if (_itX0 - cx >= IT_PULL_PX && cy - _itY0 >= IT_PULL_PX) { _itPhase = 2; inputTestToggle(); }
    }
    return true;
  }
  // One finger, or three or more: part of a gesture already under way (fingers lifting
  // one at a time) is swallowed; otherwise it is not ours.
  if (_itPhase != 0) { _itPhase = 2; return true; }
  return false;
}

/***************************************************************************************
   FRAMES (from updateI2CState(), demo or not)
****************************************************************************************/
void inputTestFrame(const uint8_t *buf, uint8_t len) {
  if (buf[0] != IT_SYNC || len < 2) return;
  if (buf[1] == 0 && len == IT_INPUTS_LEN) {
    memcpy(_itIn, buf, IT_INPUTS_LEN); _itHaveIn = true; _itInMs = millis(); _itDirty = true;
  } else if (buf[1] == 1 && len == IT_MODULES_LEN) {
    memcpy(_itMod, buf, IT_MODULES_LEN); _itHaveMod = true; _itDirty = true;
  } else if (buf[1] == 2 && len == IT_DISPLAYS_LEN) {
    memcpy(_itDsp, buf, IT_DISPLAYS_LEN); _itHaveDsp = true; _itDspMs = millis(); _itDirty = true;
  }
}

/***************************************************************************************
   DRAWING
****************************************************************************************/
struct ItModule { const char *name; uint8_t addr; uint8_t kind; uint8_t levelOff; uint8_t inputs; };
enum : uint8_t { IT_BTN = 0, IT_JOY, IT_DISP, IT_THR, IT_ENC };
// ModId order (KCMk1_Master module_bus.h). levelOff: the module's first byte in page 0.
static const ItModule IT_MODS[13] = {
  { "UI",          0x20, IT_BTN,  8, 16 }, { "FUNCTION",  0x21, IT_BTN, 10, 24 },
  { "ACTION",      0x22, IT_BTN, 13, 16 }, { "STABILITY", 0x23, IT_BTN, 15, 16 },
  { "VEHICLE",     0x24, IT_BTN, 17, 24 }, { "TIME",      0x25, IT_BTN, 20, 16 },
  { "AUX",         0x26, IT_BTN, 22, 16 }, { "ROTATION",  0x28, IT_JOY, 24,  3 },
  { "TRANSLATION", 0x29, IT_JOY, 28,  3 }, { "GPWS",      0x2A, IT_DISP, 32, 4 },
  { "PRE-WARP",    0x2B, IT_DISP, 33, 4 }, { "THROTTLE",  0x2C, IT_THR, 35,  4 },
  { "DUAL ENC",    0x2D, IT_ENC,  36,  2 },
};

static const uint16_t IT_TITLE_H = 44;
static const uint16_t IT_ROW_Y0  = 76;         // first module row
static const uint16_t IT_ROW_H   = 40;
static const uint16_t IT_COL_NAME = 6, IT_COL_ADDR = 150, IT_COL_LINE = 206, IT_COL_STATE = 272,
                      IT_COL_IN = 362;
static const uint16_t IT_BOX = 10, IT_BOX_GAP = 1;
static const uint16_t IT_RIGHT_X = 676;         // right-hand column (sticks, lever, mode, safe lines)

static void itBoxes(KCM_TFT &tft, uint16_t x, uint16_t y, uint32_t bits, uint8_t n, uint16_t onColor) {
  for (uint8_t i = 0; i < n; i++) {
    uint16_t bx = (uint16_t)(x + i * (IT_BOX + IT_BOX_GAP) + (i >= 16 ? 8 : 0));   // a gap before switches 16–23
    bool on = (bits >> i) & 1;
    tft.fillRect(bx, y, IT_BOX, IT_BOX + 6, on ? onColor : TFT_OFF_BLACK);
    tft.drawRect(bx, y, IT_BOX, IT_BOX + 6, TFT_DARK_GREY);
  }
}

static void itLineName(uint8_t line, char *b, size_t n) {
  if (line < 12)      snprintf(b, n, "INT %u", line + 1);
  else if (line < 24) snprintf(b, n, "B %u", line - 11);
  else                snprintf(b, n, "--");
}

static void itStick(KCM_TFT &tft, uint16_t x, uint16_t y, const char *name, const uint8_t *in) {
  const uint16_t S = 150;
  textLeft(tft, &Roboto_Black_16, x, y - 24, S, 22, name, TFT_LIGHT_GREY, TFT_BLACK);
  tft.drawRect(x, y, S, S, TFT_GREY);
  tft.drawLine(x + S / 2, y, x + S / 2, y + S - 1, TFT_DARK_GREY);
  tft.drawLine(x, y + S / 2, x + S - 1, y + S / 2, TFT_DARK_GREY);
  int16_t ax = (int8_t)in[1], ay = (int8_t)in[2], az = (int8_t)in[3];
  int16_t px = (int16_t)(x + S / 2 + ax * (S / 2 - 8) / 100);
  int16_t py = (int16_t)(y + S / 2 - ay * (S / 2 - 8) / 100);
  tft.fillCircle(px, py, 7, TFT_GREEN);
  // AXIS3 (twist / Z) as a bar under the box, centred
  uint16_t by = y + S + 6;
  tft.drawRect(x, by, S, 12, TFT_GREY);
  int16_t w = (int16_t)(az * (S / 2 - 2) / 100);
  if (w > 0) tft.fillRect(x + S / 2, by + 2, w, 8, TFT_GREEN);
  else if (w < 0) tft.fillRect(x + S / 2 + w, by + 2, -w, 8, TFT_GREEN);
  char b[24];
  snprintf(b, sizeof b, "X:%+d  Y:%+d  Z:%+d", ax, ay, az);
  textLeft(tft, &Roboto_Black_12, x, by + 16, S, 16, b, TFT_LIGHT_GREY, TFT_BLACK);
  static const char *const kBtn[3] = { "STK", "B1", "B2" };
  for (uint8_t i = 0; i < 3; i++) {
    bool on = (in[0] >> i) & 1;
    uint16_t cx = (uint16_t)(x + i * 50);
    tft.fillRect(cx, by + 34, 46, 22, on ? TFT_GREEN : TFT_OFF_BLACK);
    tft.drawRect(cx, by + 34, 46, 22, TFT_DARK_GREY);
    textCenter(tft, &Roboto_Black_12, cx, by + 34, 46, 22, kBtn[i], on ? TFT_BLACK : TFT_GREY, on ? TFT_GREEN : TFT_OFF_BLACK);
  }
}

static void itChip(KCM_TFT &tft, uint16_t x, uint16_t y, uint16_t w, const char *t, bool on, uint16_t onColor) {
  tft.fillRect(x, y, w, 30, on ? onColor : TFT_OFF_BLACK);
  tft.drawRect(x, y, w, 30, TFT_DARK_GREY);
  textCenter(tft, &Roboto_Black_16, x, y, w, 30, t, on ? TFT_BLACK : TFT_GREY, on ? onColor : TFT_OFF_BLACK);
}

// The four 7" displays, as their status packets report them (page 2).
static const char *itScreenName(uint8_t s) {
  static const char *const kNames[] = { "LNCH", "ORB", "SCFT", "MNVR", "TGT", "DOCK", "LNDG", "VEH", "ACFT",
                                        "ROVR", "ORB ADV", "RE-ENTRY", "ASC AP", "NAV", "ACFT AP", "ROVR AP",
                                        "ORBT AP", "LNDG AP", "INPUT" };
  return s < sizeof kNames / sizeof kNames[0] ? kNames[s] : "?";
}

static void inputTestDrawDisplays(KCM_TFT &tft, uint16_t y0) {
  textLeft(tft, &Roboto_Black_16, IT_RIGHT_X + 4, y0, 200, 22, "DISPLAYS", TFT_LIGHT_GREY, TFT_BLACK);
  static const char *const kName[4] = { "ANNUNC", "INFO 1", "INFO 2", "RESOURCE" };
  const uint8_t *d = _itDsp;
  bool fresh = _itHaveDsp && millis() - _itDspMs < IT_STALE_MS;   // page 2 stopped: show "--", not old status
  for (uint8_t c = 0; c < 4; c++) {
    uint16_t y = (uint16_t)(y0 + 24 + c * 30);
    textLeft(tft, &Roboto_Black_16, IT_RIGHT_X + 4, y, 92, 28, kName[c], TFT_WHITE, TFT_BLACK);
    if (!fresh) { textLeft(tft, &Roboto_Black_12, IT_RIGHT_X + 98, y, 240, 28, "--", TFT_GREY, TFT_BLACK); continue; }
    if (!((d[2] >> c) & 1)) {
      textLeft(tft, &Roboto_Black_12, IT_RIGHT_X + 98, y, 240, 28, "NO ANSWER", TFT_RED, TFT_BLACK);
      continue;
    }
    // flags and the per-display extras (I2C spec §15.2; KCMk1_Master input_test.h page 2)
    uint8_t fl = c == 0 ? d[3] : c == 1 ? d[8] : c == 2 ? d[10] : d[12];
    textLeft(tft, &Roboto_Black_12, IT_RIGHT_X + 98, y, 62, 28, (fl & 1) ? "LINK" : "NO LINK", (fl & 1) ? TFT_GREEN : TFT_RED, TFT_BLACK);
    textLeft(tft, &Roboto_Black_12, IT_RIGHT_X + 160, y, 56, 28, (fl & 2) ? "FLIGHT" : "STBY", (fl & 2) ? TFT_WHITE : TFT_GREY, TFT_BLACK);
    char e[32] = "";
    uint16_t ec = TFT_LIGHT_GREY;
    if (c == 0) {
      uint32_t cw = (uint32_t)(d[4] | d[5] << 8 | (uint32_t)d[6] << 16 | (uint32_t)d[7] << 24) & 0x01FFFFFFUL;
      uint8_t n = 0;
      for (uint8_t i = 0; i < 25; i++) n += (cw >> i) & 1;
      snprintf(e, sizeof e, "%sC&W %u", (fl & 4) ? "ALARM  " : "", n);
      if (fl & 4) ec = TFT_RED;
    } else if (c == 1 || c == 2) {
      snprintf(e, sizeof e, "%s%s", (fl & 4) ? "DEMO  " : "", itScreenName(c == 1 ? d[9] : d[11]));
    } else {
      snprintf(e, sizeof e, "%s%s%u bars", (fl & 4) ? "DEMO  " : "",
               (fl & 16) ? "ALARM  " : (fl & 8) ? "CAUTION  " : "", d[13]);
      if (fl & 16) ec = TFT_RED; else if (fl & 8) ec = TFT_YELLOW;
    }
    textLeft(tft, &Roboto_Black_12, IT_RIGHT_X + 218, y, 128, 28, e, ec, TFT_BLACK);
  }
}

static void inputTestDraw(KCM_TFT &tft) {
  uint32_t now = millis();
  bool live = _itHaveIn && now - _itInMs < IT_STALE_MS;
  tft.fillScreen(TFT_BLACK);

  // Title bar
  textCenter(tft, &Roboto_Black_24, 0, 0, SCREEN_W, IT_TITLE_H, "INPUT TEST", TFT_WHITE, TFT_BLACK);
  textLeft(tft, &Roboto_Black_16, 8, 0, 160, IT_TITLE_H, live ? "LIVE" : "NO DATA", live ? TFT_GREEN : TFT_RED, TFT_BLACK);
  textRight(tft, &Roboto_Black_12, SCREEN_W - 330, 0, 322, IT_TITLE_H, "two-finger pull from top-right to close",
            TFT_GREY, TFT_BLACK);
  tft.drawLine(0, IT_TITLE_H, SCREEN_W - 1, IT_TITLE_H, TFT_DARK_GREY);

  // Module table header
  uint16_t hy = IT_TITLE_H + 6;
  textLeft(tft, &Roboto_Black_12, IT_COL_NAME, hy, 140, 20, "MODULE", TFT_GREY, TFT_BLACK);
  textLeft(tft, &Roboto_Black_12, IT_COL_ADDR, hy, 50, 20, "ADDR", TFT_GREY, TFT_BLACK);
  textLeft(tft, &Roboto_Black_12, IT_COL_LINE, hy, 60, 20, "LINE", TFT_GREY, TFT_BLACK);
  textLeft(tft, &Roboto_Black_12, IT_COL_STATE, hy, 86, 20, "STATE", TFT_GREY, TFT_BLACK);
  textLeft(tft, &Roboto_Black_12, IT_COL_IN, hy, 300, 20, "INPUTS (lit = bit set)     red STATE = FAULT", TFT_GREY, TFT_BLACK);

  uint16_t present = live ? (uint16_t)(_itIn[2] | _itIn[3] << 8) : 0;
  uint16_t fault   = live ? (uint16_t)(_itIn[4] | _itIn[5] << 8) : 0;
  uint16_t known   = live ? (uint16_t)(_itIn[6] | _itIn[7] << 8) : 0;
  uint32_t life    = _itHaveMod ? (uint32_t)(_itMod[19] | _itMod[20] << 8 | (uint32_t)_itMod[21] << 16 |
                                             (uint32_t)_itMod[22] << 24) : 0;
  for (uint8_t m = 0; m < 13; m++) {
    const ItModule &d = IT_MODS[m];
    uint16_t y = (uint16_t)(IT_ROW_Y0 + m * IT_ROW_H);
    if (m & 1) tft.fillRect(0, y, IT_RIGHT_X - 10, IT_ROW_H, TFT_OFF_BLACK);
    uint16_t bg = (m & 1) ? TFT_OFF_BLACK : TFT_BLACK;
    bool pr = (present >> m) & 1, fl = (fault >> m) & 1, kn = (known >> m) & 1;
    char b[24];
    textLeft(tft, &Roboto_Black_16, IT_COL_NAME, y, 140, IT_ROW_H, d.name, pr ? TFT_WHITE : TFT_GREY, bg);
    snprintf(b, sizeof b, "%02X", d.addr);
    textLeft(tft, &Roboto_Black_16, IT_COL_ADDR, y, 50, IT_ROW_H, b, TFT_LIGHT_GREY, bg);
    itLineName(_itHaveMod ? _itMod[2 + m] : 0xFF, b, sizeof b);
    textLeft(tft, &Roboto_Black_16, IT_COL_LINE, y, 64, IT_ROW_H, b, TFT_LIGHT_GREY, bg);
    // Lifecycle as the module reports it (KMC_STATUS_*: 0 ACTIVE, 1 SLEEPING, 2 DISABLED,
    // 3 BOOT_READY); a FAULT bit turns the cell red.
    static const char *const kLife[4] = { "ACTIVE", "SLEEPING", "DISABLED", "BOOT RDY" };
    static const uint16_t kLifeCol[4] = { TFT_GREEN, TFT_SKY, TFT_ORANGE, TFT_MAGENTA };   // DISABLED amber, apart from CAUTION's yellow
    uint8_t lc = (uint8_t)((life >> (2 * m)) & 3);
    const char *st = !live ? "--" : !pr ? "MISSING" : !_itHaveMod ? "..." : kLife[lc];
    uint16_t sc = !live ? TFT_GREY : !pr ? TFT_RED : !_itHaveMod ? TFT_GREY : kLifeCol[lc];
    if (live && pr && fl) {
      tft.fillRect(IT_COL_STATE - 4, y + 4, 90, IT_ROW_H - 8, TFT_RED);
      textLeft(tft, &Roboto_Black_16, IT_COL_STATE, y + 4, 84, IT_ROW_H - 8, st, TFT_WHITE, TFT_RED);
    } else {
      textLeft(tft, &Roboto_Black_16, IT_COL_STATE, y, 88, IT_ROW_H, st, sc, bg);
    }
    if (!live || !pr || !kn) continue;
    uint16_t by = (uint16_t)(y + (IT_ROW_H - IT_BOX - 6) / 2);
    switch (d.kind) {
      case IT_BTN: {
        uint32_t lv = 0;
        uint8_t nb = d.inputs / 8;
        for (uint8_t i = 0; i < nb; i++) lv |= (uint32_t)_itIn[d.levelOff + i] << (8 * i);
        itBoxes(tft, IT_COL_IN, by, lv, d.inputs, TFT_GREEN);
        break;
      }
      case IT_JOY: {
        itBoxes(tft, IT_COL_IN, by, _itIn[d.levelOff], 3, TFT_GREEN);
        snprintf(b, sizeof b, "%+4d %+4d %+4d %%", (int8_t)_itIn[d.levelOff + 1], (int8_t)_itIn[d.levelOff + 2],
                 (int8_t)_itIn[d.levelOff + 3]);
        textLeft(tft, &Roboto_Black_16, IT_COL_IN + 44, y, 200, IT_ROW_H, b, TFT_LIGHT_GREY, bg);
        break;
      }
      case IT_DISP: {
        uint8_t v8 = _itIn[d.levelOff];
        itBoxes(tft, IT_COL_IN, by, v8 & 0x0F, 4, TFT_SKY);              // recent presses
        uint16_t val = _itHaveMod ? (uint16_t)(_itMod[15 + 2 * (m - 9)] | _itMod[16 + 2 * (m - 9)] << 8) : 0;
        snprintf(b, sizeof b, "state %X  value %u", v8 >> 4, val);
        textLeft(tft, &Roboto_Black_16, IT_COL_IN + 56, y, 240, IT_ROW_H, b, TFT_LIGHT_GREY, bg);
        break;
      }
      case IT_THR: {
        uint8_t fl = _itIn[35];
        itBoxes(tft, IT_COL_IN, by, fl >> 4, 4, TFT_SKY);                // recent presses
        snprintf(b, sizeof b, "%s%s%s", (fl & 1) ? "TOUCH " : "", (fl & 2) ? "FINE " : "", (fl & 4) ? "" : "NOT ACTIVE");
        textLeft(tft, &Roboto_Black_16, IT_COL_IN + 56, y, 240, IT_ROW_H, b, TFT_LIGHT_GREY, bg);
        break;
      }
      case IT_ENC: {
        uint8_t e = _itIn[36];
        itBoxes(tft, IT_COL_IN, by, e & 0x03, 2, TFT_SKY);
        snprintf(b, sizeof b, "group %u   ENC2 %+d", e >> 4, (int8_t)_itIn[37]);
        textLeft(tft, &Roboto_Black_16, IT_COL_IN + 34, y, 260, IT_ROW_H, b, TFT_LIGHT_GREY, bg);
        break;
      }
    }
  }

  // Right-hand column: the sticks, then the lever with the mode selector, safe lines and
  // control group, then the four displays.
  tft.drawLine(IT_RIGHT_X - 6, IT_TITLE_H + 2, IT_RIGHT_X - 6, SCREEN_H - 1, TFT_DARK_GREY);
  static const uint8_t ZERO4[4] = { 0, 0, 0, 0 };
  itStick(tft, IT_RIGHT_X + 4, IT_TITLE_H + 32, "TRANSLATION", live ? &_itIn[28] : ZERO4);
  itStick(tft, IT_RIGHT_X + 176, IT_TITLE_H + 32, "ROTATION", live ? &_itIn[24] : ZERO4);
  tft.drawLine(IT_RIGHT_X, 296, SCREEN_W - 1, 296, TFT_DARK_GREY);

  const uint16_t ly = 324, LH = 120;
  textLeft(tft, &Roboto_Black_16, IT_RIGHT_X + 4, ly - 24, 120, 22, "LEVER", TFT_LIGHT_GREY, TFT_BLACK);
  tft.drawRect(IT_RIGHT_X + 4, ly, 44, LH, TFT_GREY);
  uint8_t lever = live ? _itIn[34] : 0xFF;
  if (lever <= 100) {
    uint16_t h = (uint16_t)(lever * (LH - 4) / 100);
    tft.fillRect(IT_RIGHT_X + 6, ly + LH - 2 - h, 40, h, (_itIn[35] & 1) ? TFT_BLUE : TFT_GREEN);   // blue while touched
  }
  char b[24];
  if (lever <= 100) snprintf(b, sizeof b, "%u %%", lever); else snprintf(b, sizeof b, "--");
  textLeft(tft, &Roboto_Black_16, IT_RIGHT_X + 52, ly + LH - 24, 70, 24, b, TFT_LIGHT_GREY, TFT_BLACK);

  uint16_t cx = IT_RIGHT_X + 130;
  textLeft(tft, &Roboto_Black_16, cx, ly - 24, 200, 22, "MODE SELECTOR", TFT_LIGHT_GREY, TFT_BLACK);
  uint8_t mode = live ? _itIn[38] : 0xFF;                  // CTRL_MODE_RVR 0, PLN 1, SPC 2
  itChip(tft, cx,       ly, 66, "SPC", mode == 2, TFT_GREEN);
  itChip(tft, cx + 70,  ly, 66, "PLN", mode == 1, TFT_GREEN);
  itChip(tft, cx + 140, ly, 66, "RVR", mode == 0, TFT_GREEN);
  textLeft(tft, &Roboto_Black_16, cx, ly + 34, 200, 22, "SAFE LINES", TFT_LIGHT_GREY, TFT_BLACK);
  uint8_t safe = live ? _itIn[39] : 0;
  itChip(tft, cx,       ly + 58, 100, "ABORT", safe & 1, TFT_RED);
  itChip(tft, cx + 106, ly + 58, 100, "STAGE", safe & 2, TFT_ORANGE);
  snprintf(b, sizeof b, "CONTROL GROUP  %u", live ? (_itIn[36] >> 4) : 0);
  textLeft(tft, &Roboto_Black_16, cx, ly + 94, 210, 26, live ? b : "CONTROL GROUP  --", TFT_LIGHT_GREY, TFT_BLACK);
  tft.drawLine(IT_RIGHT_X, 452, SCREEN_W - 1, 452, TFT_DARK_GREY);

  inputTestDrawDisplays(tft, 456);
}

/***************************************************************************************
   SERVICE -- from loop() while open: repaint on a new frame, and at least every
   IT_REDRAW_MS so NO DATA appears when the frames stop.
****************************************************************************************/
void inputTestService(KCM_TFT &tft) {
  uint32_t now = millis();
  if (!_itDirty && now - _itDrawMs < IT_REDRAW_MS) return;
  _itDirty = false;
  _itDrawMs = now;
  infoDB.beginFrame(tft);
  canvasPanelRegion(tft);      // the full panel, sidebar included
  inputTestDraw(tft);
  infoDB.flip(tft);
}

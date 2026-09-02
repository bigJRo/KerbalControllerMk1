/***************************************************************************************
   ScreenEVA.ino -- EVA layout of the Main screen for KCMk1 Resource Display

   While a Kerbal is on EVA the Main screen hands over to this renderer. The slot set
   is the fixed EVA five (EC, EVA Propellant, O2, Food, Water) loaded by loadEvaSlots();
   the sidebar and the alert strip are unchanged, and the gestures are the tape
   screen's, mapped onto rings.

   Layout (1024x600): one large GAUGE for EVA Propellant, the resource a jetpack is
   about, with four smaller gauges to its right for the rest. Each gauge is a 270-
   degree arc, open at the bottom, filling clockwise from the lower left in the
   resource colour on a half-brightness track -- a dial, not a progress ring, so the
   start is empty and the end is full the way a fuel gauge reads. The resource label
   sits in the gap. Inside the ring: the percent counter (state coloured, white-on-red
   in alarm) with the trend arrow beside it, and beneath it the time to empty, which
   on this screen is always shown, since for a jetpack it IS the number. The big
   gauge also shows raw units.

   Around the track: the limit bands as thin red/yellow arcs just outside it, at the
   low end of the sweep; a reserve bug as a cyan dot outside the bands. A hold on the
   ring sets a bug at the angle touched; a hold on a bug clears it; a drag moves it
   around the arc; a tap anywhere on a gauge opens Detail. The Main screen's touch
   code calls evaHitTest / evaLevelAt for the geometry.

   Drawing: the driver has no arc primitive but a fast filled circle. An arc is a
   chain of filled dots of the ring's thickness on a FIXED angular grid, spaced
   closely enough that the edge reads smooth (see the grid notes below), which gives
   rounded ends for free. Because every dot, drawn or erased, sits on the same grid,
   an erase covers exactly what a draw painted and no scallop is left behind. The
   full track and bands draw once at chrome time; a level change draws only the grid
   dots between the old and new end, colour going up and track colour coming down,
   then restores the rounded cap. The bands are thick-line arcs, not dots.
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


/***************************************************************************************
   GEOMETRY
   Angles in degrees, clockwise from 3 o'clock (screen y is down, so the usual
   sin/cos gives that). The sweep starts at EVA_ARC_START (lower left) and runs
   EVA_ARC_SWEEP clockwise through the top to the lower right.
****************************************************************************************/
static const float    EVA_ARC_START = 135.0f;
static const float    EVA_ARC_SWEEP = 270.0f;
static const uint16_t EVA_BAND_GAP  = 6;    // band arc centreline sits this far outside the track
static const uint16_t EVA_BAND_W    = 5;    // band arc stroke width
static const uint16_t EVA_BUG_OUT   = 16;   // bug dot centre sits this far outside the track
static const uint16_t EVA_BUG_R     = 5;
static const uint16_t EVA_BUG_TXT   = 14;   // bug percent text centre sits this far beyond the dot

struct EvaGauge {
  ResourceType type;
  int16_t      cx, cy;
  uint16_t     r;          // ring centreline radius
  uint16_t     thick;      // ring thickness
  const tFont *percFont;
  const tFont *timeFont;
  const tFont *labelFont;
  const tFont *bugFont;
  bool         big;        // the EVA propellant gauge: units line, larger counter
};

// Sidebar 84 px and the alert strip 24 px are the Main screen's; the gauges sit in
// the content area below the strip. Each gauge owns a disc out to its bug text
// (r + thick/2 + EVA_BUG_OUT + EVA_BUG_R + EVA_BUG_TXT + ~10 for the figures):
// 257 px for the big gauge, 86 px for the small ones. Centres are placed so those
// discs clear each other, the strip, the sidebar and the screen edge.
static const uint8_t EVA_GAUGE_COUNT = 5;
static const EvaGauge EVA_GAUGES[EVA_GAUGE_COUNT] = {
  { RES_EVA_PROP,    342, 312, 190, 40, &Roboto_Black_72, &Roboto_Black_24, &Roboto_Black_36, &Roboto_Black_16, true  },
  { RES_ELEC_CHARGE, 715, 170,  56, 16, &Roboto_Black_24, &Roboto_Black_16, &Roboto_Black_24, &Roboto_Black_12, false },
  { RES_LS_OXYGEN,   905, 170,  56, 16, &Roboto_Black_24, &Roboto_Black_16, &Roboto_Black_24, &Roboto_Black_12, false },
  { RES_LS_FOOD,     715, 440,  56, 16, &Roboto_Black_24, &Roboto_Black_16, &Roboto_Black_24, &Roboto_Black_12, false },
  { RES_LS_WATER,    905, 440,  56, 16, &Roboto_Black_24, &Roboto_Black_16, &Roboto_Black_24, &Roboto_Black_12, false },
};

// Explicit prototypes: the IDE generates one for every function above the tabs,
// where EvaGauge is not yet visible, unless the sketch declares it itself.
static inline void arcPoint(const EvaGauge &g, float radius, float angDeg, int16_t &x, int16_t &y);
static uint16_t gridSteps(const EvaGauge &g);
static float    gridStepDeg(const EvaGauge &g);
static int16_t  gridIndex(const EvaGauge &g, float level);
static void  arcGridDots(KCM_TFT &tft, const EvaGauge &g, int16_t i0, int16_t i1, uint16_t color);
static void  bandArc(KCM_TFT &tft, const EvaGauge &g, float f0, float f1, uint16_t color);
static void  drawBands(KCM_TFT &tft, const EvaGauge &g);
static void  drawBug(KCM_TFT &tft, const EvaGauge &g, float bug, bool erase);
static void  drawEvaCentre(KCM_TFT &tft, const EvaGauge &g, bool hasData, uint8_t state,
                           const char *perc, int8_t trend, const char *tte, const char *units);
static void  drawEvaLabel(KCM_TFT &tft, const EvaGauge &g);
static float evaLevelAtGauge(const EvaGauge &g, uint16_t x, uint16_t y);

static inline float evaAngle(float level) { return EVA_ARC_START + EVA_ARC_SWEEP * level; }

static inline void arcPoint(const EvaGauge &g, float radius, float angDeg, int16_t &x, int16_t &y) {
  float a = angDeg * DEG_TO_RAD;
  x = (int16_t)lroundf(g.cx + radius * cosf(a));
  y = (int16_t)lroundf(g.cy + radius * sinf(a));
}

// The dot grid. Neighbouring dots of radius R spaced s apart leave an edge dip of
// R - sqrt(R^2 - s^2/4), so s = 2*sqrt(R) keeps it under half a pixel and the arc
// reads as a smooth band rather than a string of beads. The sweep is divided into a
// whole number of such steps, so grid index 0 is the start and index gridSteps() is
// exactly the end, and every draw and erase uses the same centres.
static uint16_t gridSteps(const EvaGauge &g) {
  float R = 0.5f * g.thick;
  float sDeg = (2.0f * sqrtf(R) / (float)g.r) * RAD_TO_DEG;
  uint16_t n = (uint16_t)ceilf(EVA_ARC_SWEEP / sDeg);
  return n < 8 ? 8 : n;
}
static float gridStepDeg(const EvaGauge &g) { return EVA_ARC_SWEEP / gridSteps(g); }

// Highest grid index at or below a level, so the arc never overstates it.
static int16_t gridIndex(const EvaGauge &g, float level) {
  int16_t i = (int16_t)floorf(level * gridSteps(g) + 0.0001f);
  return constrain(i, (int16_t)0, (int16_t)gridSteps(g));
}

// Grid dots i0..i1 inclusive, dot radius half the ring thickness.
static void arcGridDots(KCM_TFT &tft, const EvaGauge &g, int16_t i0, int16_t i1, uint16_t color) {
  if (i1 < i0) return;
  float step = gridStepDeg(g);
  int16_t x, y;
  for (int16_t i = i0; i <= i1; i++) {
    arcPoint(g, g.r, EVA_ARC_START + i * step, x, y);
    tft.fillCircle(x, y, g.thick / 2, color);
  }
}

// Limit band: a thick-line arc just outside the track, drawn as short chords with
// round caps so it reads as one solid stroke. A chord every 3 degrees at these
// radii sags well under a pixel.
static void bandArc(KCM_TFT &tft, const EvaGauge &g, float f0, float f1, uint16_t color) {
  float radius = g.r + g.thick / 2 + EVA_BAND_GAP;
  float a0 = evaAngle(f0), a1 = evaAngle(f1);
  int16_t px, py, x, y;
  arcPoint(g, radius, a0, px, py);
  for (float a = a0 + 3.0f; a < a1 + 2.99f; a += 3.0f) {
    if (a > a1) a = a1;
    arcPoint(g, radius, a, x, y);
    drawThickLine(tft, px, py, x, y, EVA_BAND_W, color, true);
    px = x; py = y;
    if (a >= a1) break;
  }
}

static void drawBands(KCM_TFT &tft, const EvaGauge &g) {
  ResLimits lim = resLimits(g.type);
  if (!lim.enabled) return;
  if (lim.highIsBad) {
    bandArc(tft, g, lim.warn, lim.alarm, TFT_YELLOW);
    bandArc(tft, g, lim.alarm, 1.0f, TFT_RED);
  } else {
    bandArc(tft, g, lim.alarm, lim.warn, TFT_YELLOW);
    bandArc(tft, g, 0.0f, lim.alarm, TFT_RED);
  }
}

// Reserve bug: a cyan dot outside the bands at the bug's angle, with its percent in
// cyan figures further outboard along the same radial. erase=true paints the same
// footprint black; nothing else lives out there, so no repair is needed.
static void drawBug(KCM_TFT &tft, const EvaGauge &g, float bug, bool erase) {
  uint16_t col = erase ? TFT_BLACK : TFT_CYAN;
  float ang = evaAngle(bug);
  int16_t x, y;
  arcPoint(g, g.r + g.thick / 2 + EVA_BUG_OUT, ang, x, y);
  tft.fillCircle(x, y, EVA_BUG_R, col);

  char pct[5];
  snprintf(pct, sizeof(pct), "%d", (int)roundf(bug * 100.0f));
  int16_t tw = getFontStringWidth(g.bugFont, pct);
  int16_t th = g.bugFont->cap_height;
  int16_t tx, ty;
  arcPoint(g, g.r + g.thick / 2 + EVA_BUG_OUT + EVA_BUG_R + EVA_BUG_TXT + tw / 2, ang, tx, ty);
  if (erase) tft.fillRect(tx - tw / 2 - 2, ty - th / 2 - 2, tw + 4, th + 4, TFT_BLACK);
  else       textCenter(tft, g.bugFont, tx - tw / 2, ty - th / 2, tw, th, String(pct), col, TFT_BLACK);
}


/***************************************************************************************
   PER-GAUGE CACHE
****************************************************************************************/
struct EvaCache {
  int16_t endIdx;      // grid index of the drawn arc end; -1 = force (track only, nothing lit)
  uint8_t state;       // 255 = force
  bool    hasData;
  bool    timeFlag;
  int8_t  trend;
  float   bug;         // -2 = force
  char    perc[8];     // "\x01" = force
  char    tte[8];
  char    units[12];
};
static EvaCache _ec[EVA_GAUGE_COUNT];

static void resetEvaCaches() {
  for (uint8_t k = 0; k < EVA_GAUGE_COUNT; k++) {
    _ec[k].endIdx = -1; _ec[k].state = 255; _ec[k].hasData = false; _ec[k].timeFlag = false;
    _ec[k].trend = 127;   _ec[k].bug = -2.0f;
    _ec[k].perc[0] = 1;   _ec[k].perc[1] = '\0';
    _ec[k].tte[0]  = 1;   _ec[k].tte[1]  = '\0';
    _ec[k].units[0] = 1;  _ec[k].units[1] = '\0';
  }
}

static int8_t evaSlotOf(ResourceType t) {
  for (uint8_t i = 0; i < slotCount; i++) if (slots[i].type == t) return (int8_t)i;
  return -1;
}


/***************************************************************************************
   CENTRE TEXT
   Percent with the trend arrow two spaces to its right, centred as a group; the time
   line beneath; the units line beneath that on the big gauge. Everything inside the
   ring's inner circle, so the cleared rectangle is the inscribed square.
****************************************************************************************/
static uint16_t evaStateColor(uint8_t state) {
  switch (state) {
    case ALERT_ALARM:   return TFT_RED;
    case ALERT_CAUTION: return TFT_YELLOW;
    case ALERT_BUG:     return TFT_CYAN;
    default:            return TFT_WHITE;
  }
}

static void drawEvaCentre(KCM_TFT &tft, const EvaGauge &g, bool hasData, uint8_t state,
                          const char *perc, int8_t trend, const char *tte, const char *units) {
  uint16_t inner = g.r - g.thick / 2 - 2;            // inner radius
  uint16_t half  = (uint16_t)(inner * 0.707f);       // inscribed square half side
  tft.fillRect(g.cx - half, g.cy - half, half * 2, half * 2, TFT_BLACK);

  bool     alarm = hasData && state == ALERT_ALARM;
  uint16_t fore  = !hasData ? TFT_GREY : alarm ? TFT_WHITE : evaStateColor(state);
  uint16_t back  = alarm ? TFT_RED : TFT_BLACK;
  int16_t  capH  = g.percFont->cap_height;
  int16_t  timeH = g.timeFont->cap_height;
  int16_t  arrowW = g.big ? 18 : 9, arrowH = g.big ? 11 : 5;
  int16_t  gapW  = getFontStringWidth(g.percFont, "  ");
  int16_t  textW = getFontStringWidth(g.percFont, perc);
  int16_t  groupW = textW + gapW + arrowW;

  // Rows: percent, time, (units) stacked and centred on cy
  int16_t rows = g.big ? 3 : 2;
  int16_t rowGap = g.big ? 10 : 4;
  int16_t totalH = capH + rowGap + timeH + (g.big ? rowGap + g.timeFont->cap_height : 0);
  int16_t y = g.cy - totalH / 2;
  int16_t gx = g.cx - groupW / 2;
  if (alarm) tft.fillRect(gx - 4, y - 3, textW + 8, capH + 6, back);
  textCenter(tft, g.percFont, gx, y, textW, capH, String(perc), fore, back);
  if (hasData && trend != 0) {
    int16_t cx = gx + textW + gapW + arrowW / 2;
    int16_t cy = y + capH / 2;
    uint16_t col = alarm ? TFT_RED : fore;
    if (trend > 0) tft.fillTriangle(cx, cy - arrowH, cx - arrowW / 2, cy + arrowH, cx + arrowW / 2, cy + arrowH, col);
    else           tft.fillTriangle(cx, cy + arrowH, cx - arrowW / 2, cy - arrowH, cx + arrowW / 2, cy - arrowH, col);
  }
  y += capH + rowGap;
  textCenter(tft, g.timeFont, g.cx - half, y, half * 2, timeH, String(tte), hasData ? fore : TFT_GREY, TFT_BLACK);
  if (g.big) {
    y += timeH + rowGap;
    textCenter(tft, g.timeFont, g.cx - half, y, half * 2, timeH, String(units), TFT_LIGHT_GREY, TFT_BLACK);
  }
  (void)rows;
}

// Resource label in the gap at the bottom of the arc.
static void drawEvaLabel(KCM_TFT &tft, const EvaGauge &g) {
  int16_t h = g.labelFont->cap_height;
  int16_t y = g.cy + g.r - h / 2;
  textCenter(tft, g.labelFont, g.cx - g.r, y, g.r * 2, h, resLabel(g.type), resColor(g.type), TFT_BLACK);
}


/***************************************************************************************
   CHROME
****************************************************************************************/
void drawStaticEVA(KCM_TFT &tft) {
  resetEvaCaches();
  stripReset();
  for (uint8_t k = 0; k < EVA_GAUGE_COUNT; k++) {
    const EvaGauge &g = EVA_GAUGES[k];
    arcGridDots(tft, g, 0, gridSteps(g), dimColor(resColor(g.type)));
    drawBands(tft, g);
    drawEvaLabel(tft, g);
  }
}


/***************************************************************************************
   UPDATE PASS
****************************************************************************************/
void updateScreenEVA(KCM_TFT &tft) {
  for (uint8_t k = 0; k < EVA_GAUGE_COUNT; k++) {
    const EvaGauge &g = EVA_GAUGES[k];
    EvaCache &c = _ec[k];
    int8_t si = evaSlotOf(g.type);
    if (si < 0) continue;
    ResourceSlot &s = slots[si];
    uint8_t i = (uint8_t)si;

    float cur = s.current, max = s.maxVal;
    bool  hasData = (max > 0.0f);
    float level   = hasData ? constrain(cur / max, 0.0f, 1.0f) : 0.0f;
    bool  awaiting = !hasData && slotAwaiting(i);
    uint8_t state = hasData ? alertState(s.type, level, s.bug, c.state) : 0;
    const SlotSample &smp = sampleTot[i];
    float tteS = hasData ? sampleTteSeconds(smp, s.type, cur, max) : -1.0f;
    bool  timeFlag = false;
    if (hasData) {
      ResTimeLimits tl = resTimeLimits(s.type);
      if (tl.warn > 0.0f && tteS >= 0.0f) {
        float alarmT = tl.alarm * ((c.timeFlag && c.state == ALERT_ALARM)   ? 1.0f + TIME_HYST_FRAC : 1.0f);
        float warnT  = tl.warn  * ((c.timeFlag && c.state == ALERT_CAUTION) ? 1.0f + TIME_HYST_FRAC : 1.0f);
        if (tteS < alarmT) { timeFlag = (state != ALERT_ALARM); state = ALERT_ALARM; }
        else if (tteS < warnT && state != ALERT_ALARM && state != ALERT_CAUTION) { timeFlag = true; state = ALERT_CAUTION; }
      }
    }
    uint8_t code = (!hasData || state == ALERT_NOMINAL) ? STRIP_NONE
                 : (state == ALERT_ALARM) ? STRIP_ALARM
                 : (state == ALERT_BUG)   ? STRIP_BUG : STRIP_CAUTION;
    stripSetSlot(i, code, hasData && timeFlag);

    char perc[8], tte[8], units[12];
    if (!hasData) { strlcpy(perc, awaiting ? "..." : "---", sizeof(perc)); strlcpy(tte, "", sizeof(tte)); units[0] = '\0'; }
    else {
      snprintf(perc, sizeof(perc), "%d%%", (int)(level * 100.0f));
      fmtTte(tteS, tte, sizeof(tte));
      fmtUnits(cur, units, sizeof(units));
    }

    int16_t endIdx = hasData ? gridIndex(g, level) : 0;
    bool levelChanged = (endIdx != c.endIdx);
    bool stateChanged = (state != c.state) || (hasData != c.hasData) || (timeFlag != c.timeFlag);
    bool textChanged  = stateChanged || strcmp(perc, c.perc) != 0 || strcmp(tte, c.tte) != 0 ||
                        strcmp(units, c.units) != 0 || smp.trend != c.trend;
    bool bugChanged   = (c.bug < -1.5f) || ((s.bug < 0.0f) != (c.bug < 0.0f)) ||
                        (s.bug >= 0.0f && fabsf(s.bug - c.bug) > 0.0001f);

    if (bugChanged) {
      if (c.bug >= 0.0f) drawBug(tft, g, c.bug, true);
      if (s.bug >= 0.0f) drawBug(tft, g, s.bug, false);
      c.bug = s.bug;
    }
    if (levelChanged) {
      // Lit dots are grid 1..endIdx (index 0 is the start cap, lit only when the
      // arc has any length). Going up paints the new ones in colour; going down
      // repaints the dropped ones in track colour, then relights the new end dot so
      // its rounded cap is whole again where the erasing neighbour overlapped it.
      int16_t prev = (c.endIdx < 0) ? 0 : c.endIdx;
      uint16_t col = resColor(s.type), dimc = dimColor(col);
      if (endIdx > prev) {
        arcGridDots(tft, g, (prev == 0) ? 0 : prev + 1, endIdx, col);
      } else if (endIdx < prev) {
        arcGridDots(tft, g, endIdx + 1, prev, dimc);
        if (endIdx > 0) arcGridDots(tft, g, endIdx, endIdx, col);
        else            arcGridDots(tft, g, 0, 0, dimc);
      }
      c.endIdx = endIdx;
    }
    if (textChanged) {
      c.state = state; c.hasData = hasData; c.timeFlag = timeFlag; c.trend = smp.trend;
      strlcpy(c.perc, perc, sizeof(c.perc)); strlcpy(c.tte, tte, sizeof(c.tte)); strlcpy(c.units, units, sizeof(c.units));
      drawEvaCentre(tft, g, hasData, state, perc, smp.trend, tte, units);
    }
  }
  stripUpdate(tft);
}


/***************************************************************************************
   TOUCH GEOMETRY
   evaHitTest: which gauge is under (x,y). onRing is true on the ring itself and the
   band/bug margin outside it (level = the arc position of the touch); false inside
   the ring, where a tap opens Detail.
****************************************************************************************/
static float evaLevelAtGauge(const EvaGauge &g, uint16_t x, uint16_t y) {
  float ang = atan2f((float)y - g.cy, (float)x - g.cx) * RAD_TO_DEG;   // -180..180 from 3 o'clock
  if (ang < 0.0f) ang += 360.0f;
  float rel = ang - EVA_ARC_START;
  if (rel < 0.0f) rel += 360.0f;                       // 0 at the start of the sweep
  if (rel > EVA_ARC_SWEEP) rel = (rel > EVA_ARC_SWEEP + (360.0f - EVA_ARC_SWEEP) / 2) ? 0.0f : EVA_ARC_SWEEP;
  return constrain(rel / EVA_ARC_SWEEP, 0.0f, 1.0f);
}

int8_t evaHitTest(uint16_t x, uint16_t y, bool &onRing, float &level) {
  for (uint8_t k = 0; k < EVA_GAUGE_COUNT; k++) {
    const EvaGauge &g = EVA_GAUGES[k];
    float dx = (float)x - g.cx, dy = (float)y - g.cy;
    float d  = sqrtf(dx * dx + dy * dy);
    float outer = g.r + g.thick / 2 + EVA_BUG_OUT + EVA_BUG_R + EVA_BUG_TXT;
    float innerRing = g.r - g.thick / 2 - 8;
    if (d > outer) continue;
    int8_t si = evaSlotOf(g.type);
    if (si < 0) return -1;
    onRing = (d >= innerRing);
    level  = onRing ? evaLevelAtGauge(g, x, y) : 0.0f;
    return si;
  }
  return -1;
}

float evaLevelAt(uint8_t slot, uint16_t x, uint16_t y) {
  for (uint8_t k = 0; k < EVA_GAUGE_COUNT; k++) {
    if (slot < slotCount && EVA_GAUGES[k].type == slots[slot].type) return evaLevelAtGauge(EVA_GAUGES[k], x, y);
  }
  return 0.0f;
}

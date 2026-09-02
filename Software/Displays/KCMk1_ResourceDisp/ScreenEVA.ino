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
   chain of filled dots of the ring's thickness spaced half a thickness apart, which
   gives rounded ends for free. The full track and bands draw once at chrome time; a
   level change draws only the dots between the old and new end angles, colour going
   up and track colour coming down, then restores the rounded cap.
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
static const uint16_t EVA_BAND_GAP  = 6;    // band arc sits this far outside the track
static const uint16_t EVA_BAND_R    = 2;    // band dot radius
static const uint16_t EVA_BUG_OUT   = 16;   // bug dot sits this far outside the track
static const uint16_t EVA_BUG_R     = 5;

struct EvaGauge {
  ResourceType type;
  int16_t      cx, cy;
  uint16_t     r;          // ring centreline radius
  uint16_t     thick;      // ring thickness
  const tFont *percFont;
  const tFont *timeFont;
  const tFont *labelFont;
  bool         big;        // the EVA propellant gauge: units line, larger counter
};

// Sidebar 84 px and the alert strip 24 px are the Main screen's; the gauges sit in
// the content area below the strip.
static const uint8_t EVA_GAUGE_COUNT = 5;
static const EvaGauge EVA_GAUGES[EVA_GAUGE_COUNT] = {
  { RES_EVA_PROP,    384, 318, 222, 44, &Roboto_Black_72, &Roboto_Black_24, &Roboto_Black_28, true  },
  { RES_ELEC_CHARGE, 760, 160,  72, 20, &Roboto_Black_28, &Roboto_Black_16, &Roboto_Black_20, false },
  { RES_LS_OXYGEN,   930, 160,  72, 20, &Roboto_Black_28, &Roboto_Black_16, &Roboto_Black_20, false },
  { RES_LS_FOOD,     760, 430,  72, 20, &Roboto_Black_28, &Roboto_Black_16, &Roboto_Black_20, false },
  { RES_LS_WATER,    930, 430,  72, 20, &Roboto_Black_28, &Roboto_Black_16, &Roboto_Black_20, false },
};

// Explicit prototypes: the IDE generates one for every function above the tabs,
// where EvaGauge is not yet visible, unless the sketch declares it itself.
static inline void arcPoint(const EvaGauge &g, float radius, float angDeg, int16_t &x, int16_t &y);
static void  arcDots(KCM_TFT &tft, const EvaGauge &g, float a0, float a1, uint16_t color);
static void  bandDots(KCM_TFT &tft, const EvaGauge &g, float f0, float f1, uint16_t color);
static void  drawBands(KCM_TFT &tft, const EvaGauge &g);
static void  drawBugDot(KCM_TFT &tft, const EvaGauge &g, float bug, uint16_t color);
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

// Dots along the centreline from a0 to a1 (degrees, a1 >= a0), dot radius half the
// ring thickness, spaced half a thickness so the chain is gap-free.
static void arcDots(KCM_TFT &tft, const EvaGauge &g, float a0, float a1, uint16_t color) {
  if (a1 < a0) return;
  float stepDeg = (0.5f * g.thick / (float)g.r) * RAD_TO_DEG;
  if (stepDeg < 1.0f) stepDeg = 1.0f;
  int16_t x, y;
  for (float a = a0; a < a1; a += stepDeg) {
    arcPoint(g, g.r, a, x, y);
    tft.fillCircle(x, y, g.thick / 2, color);
  }
  arcPoint(g, g.r, a1, x, y);
  tft.fillCircle(x, y, g.thick / 2, color);
}

// Thin dotted arc for the limit bands, just outside the track.
static void bandDots(KCM_TFT &tft, const EvaGauge &g, float f0, float f1, uint16_t color) {
  float radius = g.r + g.thick / 2 + EVA_BAND_GAP;
  int16_t x, y;
  for (float a = evaAngle(f0); a <= evaAngle(f1); a += 1.0f) {
    arcPoint(g, radius, a, x, y);
    tft.fillCircle(x, y, EVA_BAND_R, color);
  }
}

static void drawBands(KCM_TFT &tft, const EvaGauge &g) {
  ResLimits lim = resLimits(g.type);
  if (!lim.enabled) return;
  if (lim.highIsBad) {
    bandDots(tft, g, lim.warn, lim.alarm, TFT_YELLOW);
    bandDots(tft, g, lim.alarm, 1.0f, TFT_RED);
  } else {
    bandDots(tft, g, lim.alarm, lim.warn, TFT_YELLOW);
    bandDots(tft, g, 0.0f, lim.alarm, TFT_RED);
  }
}

static void drawBugDot(KCM_TFT &tft, const EvaGauge &g, float bug, uint16_t color) {
  int16_t x, y;
  arcPoint(g, g.r + g.thick / 2 + EVA_BUG_OUT, evaAngle(bug), x, y);
  tft.fillCircle(x, y, EVA_BUG_R, color);
}


/***************************************************************************************
   PER-GAUGE CACHE
****************************************************************************************/
struct EvaCache {
  float   level;       // last drawn level; -1 = force (track only)
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
    _ec[k].level = -1.0f; _ec[k].state = 255; _ec[k].hasData = false; _ec[k].timeFlag = false;
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
  int16_t  arrowW = g.big ? 18 : 10, arrowH = g.big ? 11 : 6;
  int16_t  gapW  = getFontStringWidth(g.percFont, "  ");
  int16_t  textW = getFontStringWidth(g.percFont, perc);
  int16_t  groupW = textW + gapW + arrowW;

  // Rows: percent, time, (units) stacked and centred on cy
  int16_t rows = g.big ? 3 : 2;
  int16_t rowGap = g.big ? 10 : 6;
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
  textCenter(tft, g.labelFont, g.cx - g.r / 2, y, g.r, h, resLabel(g.type), resColor(g.type), TFT_BLACK);
}


/***************************************************************************************
   CHROME
****************************************************************************************/
void drawStaticEVA(KCM_TFT &tft) {
  resetEvaCaches();
  stripReset();
  for (uint8_t k = 0; k < EVA_GAUGE_COUNT; k++) {
    const EvaGauge &g = EVA_GAUGES[k];
    arcDots(tft, g, EVA_ARC_START, EVA_ARC_START + EVA_ARC_SWEEP, dimColor(resColor(g.type)));
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

    bool levelChanged = fabsf(level - c.level) >= BAR_LEVEL_HYSTERESIS;
    bool stateChanged = (state != c.state) || (hasData != c.hasData) || (timeFlag != c.timeFlag);
    bool textChanged  = stateChanged || strcmp(perc, c.perc) != 0 || strcmp(tte, c.tte) != 0 ||
                        strcmp(units, c.units) != 0 || smp.trend != c.trend;
    bool bugChanged   = (c.bug < -1.5f) || ((s.bug < 0.0f) != (c.bug < 0.0f)) ||
                        (s.bug >= 0.0f && fabsf(s.bug - c.bug) > 0.0001f);

    if (bugChanged) {
      if (c.bug >= 0.0f) drawBugDot(tft, g, c.bug, TFT_BLACK);
      if (s.bug >= 0.0f) drawBugDot(tft, g, s.bug, TFT_CYAN);
      c.bug = s.bug;
    }
    if (levelChanged) {
      float prev = (c.level < 0.0f) ? 0.0f : c.level;
      uint16_t col = resColor(s.type), dimc = dimColor(col);
      if (level > prev) {
        arcDots(tft, g, evaAngle(prev), evaAngle(level), col);
      } else if (level < prev) {
        arcDots(tft, g, evaAngle(level), evaAngle(prev), dimc);
        if (level > 0.0f) arcDots(tft, g, evaAngle(level), evaAngle(level), col);   // restore the cap
      }
      c.level = level;
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
    float outer = g.r + g.thick / 2 + EVA_BUG_OUT + EVA_BUG_R + 4;
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

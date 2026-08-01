/***************************************************************************************
   Screen_LNDG_Reentry.ino -- Re-entry mode (one of two modes of the LNDG screen).

   rev-2 Phase-2 redesign: RE-ENTRY is now a graphical instrument screen, a sibling of
   POWERED DESCENT. The left/centre zone carries the graphics; a condensed text board
   fills the right ~360px panel.

   Widgets (left/centre graphics zone, x < RE_DIV_X):
     - Corridor tape      : full-height altitude tape whose coloured zones are the
                            re-entry corridor (NO RE-ENTRY / AEROBRAKE / SAFE / DANGER)
                            with the current altitude and periapsis markers. Zone
                            boundaries are anchored on the body table (flyHigh /
                            reentryAlt / lowSpace) and shift up with entry velocity.
     - Alignment ball     : heat-shield / retrograde alignment. Plots the surface
                            retrograde vector relative to the nose (boresight); when
                            centred the shield is square into the airflow. AoA readout.
     - Parachute envelope : speed-vs-altitude safe-deploy chart with coloured speed
                            bands for drogue/main and a live vessel marker.
     - VSI / G / Thermal   : vertical bar gauges (descent rate, load, skin+core temp).

   Text panel (right, x >= RE_TXT_X): T+Grnd/T+Atm, Alt, V.Srf, PeA, Mach, G, and the
   chute / gear / SAS status board.

   The graphics widgets are fully repainted every frame (the hardware double buffer
   BTE-copies the front page to the back each frame, so a full widget repaint simply
   overwrites the copy — no incremental erase bookkeeping needed). The text panel keeps
   the row-cache so values redraw only on change.

   Public-to-the-sketch entry points (called by the LNDG dispatcher):
     - _lndgChromeReentry(tft) — divider + static text-panel labels
     - _lndgDrawReentry(tft)   — per-frame widgets + panel values

   Re-entry row-toggle state (_lndgReentryRow0TPe, _lndgReentryRow1SL) and parachute
   deploy/cut state (_drogueDeployed etc.) are defined in Screen_LNDG.ino as externs.
****************************************************************************************/

/***************************************************************************************
   LAYOUT
****************************************************************************************/
static const uint16_t RE_DIV_X   = 578;                 // graphics | panel divider
static const uint16_t RE_TXT_X   = 580;                 // text panel left edge
static const uint16_t RE_TXT_W   = CONTENT_W - RE_TXT_X;// 360

// ── Corridor tape (far left) ──
static const uint16_t RE_TAPE_X   = 44;
static const uint16_t RE_TAPE_W   = 44;
static const uint16_t RE_TAPE_Y   = TITLE_TOP + 8;                  // 70
static const uint16_t RE_TAPE_H   = SCREEN_H - RE_TAPE_Y - 3;       // 527
static const uint16_t RE_TAPE_BOT = RE_TAPE_Y + RE_TAPE_H;         // 597
static const uint16_t RE_TAPE_GUT = 58;   // right-side marker gutter (erased each frame)

// ── Alignment ball (centre-top) ──
static const int16_t  RE_ATT_CX = 300;
static const int16_t  RE_ATT_CY = 194;
static const int16_t  RE_ATT_R  = 104;
static const float    RE_ATT_FS = (float)RE_ATT_R / 40.0f;  // px per degree (outer ring = 40°)

// ── Parachute deploy envelope (centre-bottom) ──
static const uint16_t RE_ENV_X    = 190;
static const uint16_t RE_ENV_Y    = 350;
static const uint16_t RE_ENV_W    = 248;
static const uint16_t RE_ENV_H    = 248;
static const float    RE_ENV_VMAX = 1000.0f;   // x-axis: surface speed (m/s)
static const float    RE_ENV_AMAX = 10000.0f;  // y-axis: altitude AGL (m)

// ── Vertical bar gauges (right of the graphics zone) ──
static const uint16_t RE_GA_Y  = TITLE_TOP + 20;                 // 82 — gauge top
static const uint16_t RE_GA_H  = SCREEN_H - RE_GA_Y - 34;       // leave a label row
static const uint16_t RE_GA_BOT= RE_GA_Y + RE_GA_H;
static const uint16_t RE_VSI_X = 452;   static const uint16_t RE_VSI_W = 34;
static const uint16_t RE_GF_X  = 498;   static const uint16_t RE_GF_W  = 34;
static const uint16_t RE_TS_X  = 544;   static const uint16_t RE_TS_W  = 14;  // skin
static const uint16_t RE_TC_X  = 560;   static const uint16_t RE_TC_W  = 14;  // core

/***************************************************************************************
   HELPERS
****************************************************************************************/
static inline float _reWrap180(float a) { while (a > 180.0f) a -= 360.0f; while (a < -180.0f) a += 360.0f; return a; }

// Re-entry corridor boundaries (metres, ASL), velocity-adjusted. ReCorridor is
// declared in KCMk1_InfoDisp.h so the auto-prototype for this helper sees the type.
static ReCorridor _reCorridor() {
  ReCorridor c; c.valid = false; c.dangerLine = c.safeTop = c.atmoTop = 0.0f;
  float atmoTop = currentBody.lowSpace;
  if (!currentBody.hasAtmo || atmoTop <= 0.0f) return c;
  float flyHigh    = currentBody.flyHigh;
  float reentryAlt = currentBody.reentryAlt;
  float escV       = currentBody.escapeVelocity;
  float vCirc      = (escV > 1.0f) ? escV / 1.41421356f : 1.0f;   // ~circular low-orbit speed
  float vFac       = constrain(state.surfaceVel / vCirc, 1.0f, 1.7f);
  c.dangerLine = flyHigh * vFac;
  c.safeTop    = min(reentryAlt * vFac, 0.92f * atmoTop);
  if (c.safeTop < c.dangerLine) c.safeTop = c.dangerLine;
  c.atmoTop = atmoTop;
  c.valid   = true;
  return c;
}

// Classify a periapsis (ASL) into a corridor regime: 0 danger, 1 safe, 2 aerobrake,
// 3 no-reentry, -1 n/a. Colours chosen to match the tape zones.
static int8_t _rePeRegime(const ReCorridor &c, float pe) {
  if (!c.valid) return -1;
  if (pe >= c.atmoTop)    return 3;
  if (pe >= c.safeTop)    return 2;
  if (pe >= c.dangerLine) return 1;
  return 0;
}
static uint16_t _reRegimeColor(int8_t r) {
  switch (r) {
    case 3:  return TFT_LIGHT_GREY;   // no re-entry
    case 2:  return TFT_CYAN;         // aerobrake
    case 1:  return TFT_DARK_GREEN;   // safe
    case 0:  return TFT_RED;          // dangerous
    default: return TFT_DARK_GREY;
  }
}

// "Nice" label step (metres) for the tape scale — aims for ~5 labelled divisions.
static float _reNiceStep(float span) {
  float raw = span / 5.0f;
  static const float steps[] = { 5000, 10000, 20000, 25000, 50000, 100000, 200000, 500000 };
  for (uint8_t i = 0; i < 8; i++) if (steps[i] >= raw) return steps[i];
  return 500000.0f;
}

/***************************************************************************************
   WIDGET: CORRIDOR TAPE (altitude ladder + periapsis regime)
   Powered-descent tape style: km scale labels + ticks on the left, corridor zone
   fills in the bar, and the current-altitude (white) + periapsis (magenta) markers
   on the right. The whole footprint is repainted every frame so moving markers can't
   leave streaks.
****************************************************************************************/
static void _reDrawTape(KCM_TFT &tft) {
  ReCorridor c = _reCorridor();
  float atmoTop  = (c.valid ? c.atmoTop : (currentBody.lowSpace > 0 ? currentBody.lowSpace : 70000.0f));
  float scaleTop = atmoTop * 1.3f;
  if (scaleTop < 1.0f) scaleTop = 70000.0f;

  // The drawn bar is inset from the full footprint by the marker half-height so the
  // markers can reach the true 0 / scaleTop ends without their triangles overhanging
  // (no clamping — clamping would misreport the value at the extremes).
  const int16_t RE_TAPE_M = 12;                       // marker half-height inset
  const int16_t yTop = RE_TAPE_Y + RE_TAPE_M;
  const int16_t yBot = RE_TAPE_BOT - RE_TAPE_M;
  const int16_t usableH = yBot - yTop;
  auto altToY = [&](float alt) -> int16_t {
    float f = alt / scaleTop; if (f < 0) f = 0; if (f > 1) f = 1;
    return (int16_t)(yBot - f * usableH);
  };
  const uint16_t ix   = RE_TAPE_X + 1, iw = RE_TAPE_W - 2;
  const uint16_t barR = RE_TAPE_X + RE_TAPE_W;      // bar right edge (88)

  // Full-footprint erase (label gutter + bar + marker gutter) — kills streaks.
  tft.fillRect(14, RE_TAPE_Y, (barR + RE_TAPE_GUT) - 14, RE_TAPE_H + 2, TFT_BLACK);

  // Corridor zone fills (dim) inside the bar
  if (c.valid) {
    int16_t yDanger = altToY(c.dangerLine);
    int16_t ySafe   = altToY(c.safeTop);
    int16_t yAtmo   = altToY(c.atmoTop);
    tft.fillRect(ix, yDanger, iw, yBot - yDanger, TFT_DARK_RED);
    tft.fillRect(ix, ySafe,   iw, yDanger - ySafe, TFT_JUNGLE);
    tft.fillRect(ix, yAtmo,   iw, ySafe   - yAtmo, TFT_AQUA);
    tft.fillRect(ix, yTop,    iw, yAtmo   - yTop,  TFT_OFF_BLACK);
    tft.drawLine(ix, yAtmo, ix + iw - 1, yAtmo, TFT_LIGHT_GREY);
  } else {
    tft.fillRect(ix, yTop, iw, usableH, TFT_OFF_BLACK);
  }

  // Scale: km labels (left, right-aligned to the bar) + major ticks both edges
  float step = _reNiceStep(scaleTop);
  tft.setFont(Roboto_Black_12);
  tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
  for (float m = 0.0f; m <= scaleTop + 1.0f; m += step) {
    int16_t ty = altToY(m);
    tft.drawLine(RE_TAPE_X + 1, ty, RE_TAPE_X + 8, ty, TFT_LIGHT_GREY);
    tft.drawLine(barR - 9,      ty, barR - 2,      ty, TFT_LIGHT_GREY);
    char buf[6]; snprintf(buf, sizeof(buf), "%d", (int)(m / 1000.0f + 0.5f));
    int16_t lw = getFontStringWidth(&Roboto_Black_12, buf);
    int16_t ly = (int16_t)max((int)RE_TAPE_Y, min((int)(RE_TAPE_BOT - 14), (int)ty - 7));
    tft.setCursor(RE_TAPE_X - 3 - lw, ly);
    tft.print(buf);
  }
  // Minor ticks — 3 per major division (quarter-steps), skipping the major lines.
  float minor = step * 0.25f;
  int   nTicks = (int)(scaleTop / minor + 0.5f);
  for (int i = 1; i <= nTicks; i++) {
    if (i % 4 == 0) continue;            // coincides with a major tick
    int16_t ty = altToY(i * minor);
    tft.drawLine(RE_TAPE_X + 1, ty, RE_TAPE_X + 4, ty, TFT_GREY);
    tft.drawLine(barR - 5,      ty, barR - 2,      ty, TFT_GREY);
  }

  tft.drawRect(RE_TAPE_X, yTop, RE_TAPE_W, usableH, TFT_GREY);

  // ── Markers on the right ──
  // Markers use altToY directly (no clamp) so they read the true value at the ends;
  // the inset mapping keeps their triangles within the erased footprint. Periapsis is
  // drawn first so the current-altitude marker always sits on top of it.
  if (currentBody.hasAtmo) {
    int16_t yt = altToY(state.periapsis);
    tft.fillRect(ix, yt - 2, iw, 5, TFT_MAGENTA);
    tft.fillTriangle(barR + 1, yt, barR + 17, yt - 11, barR + 17, yt + 11, TFT_MAGENTA);
    tft.setFont(Roboto_Black_20);
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.setCursor(barR + 20, yt - 12);
    tft.print("Pe");
  }
  {
    int16_t yt = altToY(state.altitude);
    tft.fillRect(ix, yt - 1, iw, 3, TFT_WHITE);
    tft.fillTriangle(barR + 1, yt, barR + 17, yt - 11, barR + 17, yt + 11, TFT_WHITE);
  }
}

/***************************************************************************************
   WIDGET: HEAT-SHIELD / RETROGRADE ALIGNMENT BALL
****************************************************************************************/
// Navball-style retrograde symbol: a ringed circle with an internal X and three
// short spokes radiating out at 12 / 4 / 8 o'clock.
static void _reRetroSymbol(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t col) {
  tft.drawCircle(cx, cy, r,     col);
  tft.drawCircle(cx, cy, r - 1, col);
  int16_t d = (int16_t)(r * 0.7071f);
  tft.drawLine(cx - d, cy - d, cx + d, cy + d, col);
  tft.drawLine(cx - d, cy + d, cx + d, cy - d, col);
  static const int16_t spoke[3] = { -90, 30, 150 };   // screen degrees: up, lower-right, lower-left
  for (uint8_t i = 0; i < 3; i++) {
    float a = spoke[i] * (float)DEG_TO_RAD;
    tft.drawLine(cx + (int16_t)(r * cosf(a)),       cy + (int16_t)(r * sinf(a)),
                 cx + (int16_t)((r + 5) * cosf(a)), cy + (int16_t)((r + 5) * sinf(a)), col);
  }
}

static void _reDrawBall(KCM_TFT &tft) {
  const int16_t cx = RE_ATT_CX, cy = RE_ATT_CY, R = RE_ATT_R;

  // Shared reticle chrome (disc, good-zone ring, rings, cardinals, nose crosshair,
  // 30° ticks, bezel) — identical style to the MNVR / TGT / DOCK reticles.
  reticleDrawBase(tft, cx, cy, R, 12, 9);

  // Ring degree labels (NE quadrant, just inside each ring) — 10/20/30/40° scale.
  tft.setFont(Roboto_Black_12);
  tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
  static const char *ringL[4] = { "10", "20", "30", "40" };
  const int16_t ringR[4] = { R / 4, R / 2, (R * 3) / 4, R };
  for (uint8_t i = 0; i < 4; i++) { tft.setCursor(cx + 3, cy - ringR[i] + 2); tft.print(ringL[i]); }

  // Title
  const char *cap = "RETRO ALIGNMENT";
  int16_t cw = getFontStringWidth(&Roboto_Black_12, cap);
  tft.setCursor(cx - cw / 2, cy - R - 16);
  tft.print(cap);

  // Bank (roll) indicator — fixed scale at 0/±30/±60 + a moving sky-pointer, drawn
  // inside the top of the reticle (the disc fill erases it each frame, so it needs no
  // extra space). The pointer sits at the current bank angle on the fixed scale.
  {
    static const int16_t bankMarks[] = { 0, -30, 30, -60, 60 };
    for (uint8_t i = 0; i < 5; i++) {
      float ma = (-90.0f + bankMarks[i]) * (float)DEG_TO_RAD;
      int16_t r0 = R - 2, r1 = R - (bankMarks[i] == 0 ? 13 : 9);
      tft.drawLine(cx + (int16_t)(r0 * cosf(ma)), cy + (int16_t)(r0 * sinf(ma)),
                   cx + (int16_t)(r1 * cosf(ma)), cy + (int16_t)(r1 * sinf(ma)), TFT_LIGHT_GREY);
    }
    float ra = (-90.0f - state.roll) * (float)DEG_TO_RAD;
    int16_t px = cx + (int16_t)((R - 4)  * cosf(ra)), py = cy + (int16_t)((R - 4)  * sinf(ra));
    int16_t bx = cx + (int16_t)((R - 14) * cosf(ra)), by = cy + (int16_t)((R - 14) * sinf(ra));
    float pp = ra + 1.5708f;
    tft.fillTriangle(px, py,
                     bx + (int16_t)(6 * cosf(pp)), by + (int16_t)(6 * sinf(pp)),
                     bx - (int16_t)(6 * cosf(pp)), by - (int16_t)(6 * sinf(pp)), TFT_YELLOW);
  }

  // Retrograde marker — where the surface-retrograde vector sits relative to the nose
  // (boresight = reticle centre). Roll-rotated into the cockpit frame.
  bool  moving = (state.surfaceVel > 1.0f);
  float yawErr = _reWrap180((state.srfVelHeading + 180.0f) - state.heading);
  float pitErr = (-state.srfVelPitch) - state.pitch;
  float aoa    = sqrtf(yawErr * yawErr + pitErr * pitErr);
  uint16_t mc  = (aoa < 10.0f) ? TFT_NEON_GREEN : (aoa < 30.0f) ? TFT_YELLOW : TFT_RED;

  if (moving) {
    float a  = state.roll * (float)DEG_TO_RAD;
    float dx = yawErr * RE_ATT_FS, dy = -pitErr * RE_ATT_FS;
    float rx = dx * cosf(a) - dy * sinf(a);
    float ry = dx * sinf(a) + dy * cosf(a);
    float mag = sqrtf(rx * rx + ry * ry);
    float lim = R - 15;                          // keep the whole symbol inside the disc
    if (mag > lim && mag > 0.0f) { float k = lim / mag; rx *= k; ry *= k; }
    _reRetroSymbol(tft, cx + (int16_t)rx, cy + (int16_t)ry, 9, mc);
  }

  // AoA (nose-to-airflow angle) readout below the reticle
  char buf[8];
  if (moving) snprintf(buf, sizeof(buf), "%d\xB0", (int)(aoa + 0.5f));
  else        snprintf(buf, sizeof(buf), "---");
  tft.setFont(Roboto_Black_20);
  tft.setTextColor(moving ? mc : TFT_DARK_GREY, TFT_BLACK);
  int16_t tw = getFontStringWidth(&Roboto_Black_20, buf);
  tft.fillRect(cx - 40, cy + R + 4, 80, 24, TFT_BLACK);
  tft.setCursor(cx - tw / 2, cy + R + 6);
  tft.print(buf);
}

/***************************************************************************************
   WIDGET: PARACHUTE DEPLOY ENVELOPE (speed vs altitude)
****************************************************************************************/
static void _reDrawEnvelope(KCM_TFT &tft) {
  const uint16_t x0 = RE_ENV_X, y0 = RE_ENV_Y, w = RE_ENV_W, h = RE_ENV_H;
  auto spdToX = [&](float v) -> int16_t {
    float f = v / RE_ENV_VMAX; if (f < 0) f = 0; if (f > 1) f = 1; return (int16_t)(x0 + f * w);
  };
  auto altToY = [&](float alt) -> int16_t {
    float f = alt / RE_ENV_AMAX; if (f < 0) f = 0; if (f > 1) f = 1; return (int16_t)(y0 + h - f * h);
  };

  // Speed bands (vertical). main safe/risky < drogue safe/risky.
  int16_t xMs = spdToX(LNDG_MAIN_SAFE_MS),   xMr = spdToX(LNDG_MAIN_RISKY_MS);
  int16_t xDs = spdToX(LNDG_DROGUE_SAFE_MS), xDr = spdToX(LNDG_DROGUE_RISKY_MS);
  tft.fillRect(x0,  y0, xMs - x0,          h, TFT_JUNGLE);    // both safe
  tft.fillRect(xMs, y0, xMr - xMs,         h, TFT_OLIVE);     // main caution
  tft.fillRect(xMr, y0, xDs - xMr,         h, TFT_AQUA);      // drogue-only
  tft.fillRect(xDs, y0, xDr - xDs,         h, TFT_OLIVE);     // drogue caution
  tft.fillRect(xDr, y0, (x0 + w) - xDr,    h, TFT_DARK_RED);  // both rip

  // Main full-open altitude reference line (below it a main chute fully blooms)
  int16_t yFull = altToY(LNDG_MAIN_FULL_ALT);
  for (int16_t x = x0; x < x0 + w; x += 8) tft.drawLine(x, yFull, x + 3, yFull, TFT_LIGHT_GREY);

  tft.drawRect(x0, y0, w, h, TFT_GREY);

  // Vessel marker (speed, AGL altitude), coloured by main-chute safety
  float spd = state.surfaceVel;
  uint16_t vc = (spd < LNDG_MAIN_SAFE_MS) ? TFT_NEON_GREEN :
                (spd < LNDG_DROGUE_RISKY_MS) ? TFT_YELLOW : TFT_RED;
  int16_t vx = spdToX(spd), vy = altToY(state.radarAlt);
  tft.drawLine(vx - 7, vy, vx + 7, vy, vc);
  tft.drawLine(vx, vy - 7, vx, vy + 7, vc);
  tft.fillCircle(vx, vy, 3, vc);

  // Caption
  tft.setFont(Roboto_Black_12);
  tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
  tft.setCursor(x0, y0 - 15);
  tft.print("CHUTE DEPLOY  (spd / alt)");
}

/***************************************************************************************
   WIDGET: VERTICAL BAR GAUGES (VSI / G / Thermal)
****************************************************************************************/
// One coloured vertical fill bar with a border. `frac` 0..1 fills from the bottom.
static void _reBar(KCM_TFT &tft, uint16_t x, uint16_t w, float frac, uint16_t col, uint16_t dim) {
  if (frac < 0) frac = 0; if (frac > 1) frac = 1;
  uint16_t fh = (uint16_t)(frac * RE_GA_H);
  tft.fillRect(x + 1, RE_GA_Y, w - 2, RE_GA_H - fh, dim);         // empty (top)
  tft.fillRect(x + 1, RE_GA_BOT - fh, w - 2, fh, col);            // filled (bottom)
  tft.drawRect(x, RE_GA_Y, w, RE_GA_H, TFT_GREY);
}
static void _reLabelUnder(KCM_TFT &tft, uint16_t xc, const char *s, uint16_t col) {
  tft.setFont(Roboto_Black_12);
  tft.setTextColor(col, TFT_BLACK);
  int16_t tw = getFontStringWidth(&Roboto_Black_12, s);
  tft.setCursor(xc - tw / 2, RE_GA_BOT + 4);
  tft.print(s);
}

static void _reDrawGauges(KCM_TFT &tft) {
  // VSI — descent rate, 0..150 m/s (clamped). Green normally; near ground use the
  // landing rate thresholds (yellow < -5, red < -8 m/s).
  {
    float dr = -state.verticalVel;                 // positive = descending
    float frac = dr / 150.0f;
    uint16_t col = TFT_NEON_GREEN;
    if (state.radarAlt < 3000.0f) {
      if (state.verticalVel < LNDG_VVRT_ALARM_MS)      col = TFT_RED;
      else if (state.verticalVel < LNDG_VVRT_WARN_MS)  col = TFT_YELLOW;
    }
    _reBar(tft, RE_VSI_X, RE_VSI_W, frac, col, TFT_OFF_BLACK);
    _reLabelUnder(tft, RE_VSI_X + RE_VSI_W / 2, "VSI", TFT_LIGHT_GREY);
  }
  // G-load — 0..8 g, warn/alarm zones.
  {
    float g = fabsf(state.gForce);
    float frac = g / 8.0f;
    uint16_t col = (state.gForce > G_ALARM_POS || state.gForce < G_ALARM_NEG) ? TFT_RED :
                   (state.gForce > G_WARN_POS  || state.gForce < G_WARN_NEG)  ? TFT_YELLOW : TFT_NEON_GREEN;
    _reBar(tft, RE_GF_X, RE_GF_W, frac, col, TFT_OFF_BLACK);
    _reLabelUnder(tft, RE_GF_X + RE_GF_W / 2, "G", TFT_LIGHT_GREY);
  }
  // Thermal — skin + core temp, % of limit.
  {
    auto tcol = [&](uint8_t pct) -> uint16_t {
      return (pct >= TEMP_ALARM_PCT) ? TFT_RED : (pct >= TEMP_WARN_PCT) ? TFT_YELLOW : TFT_NEON_GREEN;
    };
    _reBar(tft, RE_TS_X, RE_TS_W, state.skinTempPct / 100.0f, tcol(state.skinTempPct), TFT_OFF_BLACK);
    _reBar(tft, RE_TC_X, RE_TC_W, state.coreTempPct / 100.0f, tcol(state.coreTempPct), TFT_OFF_BLACK);
    _reLabelUnder(tft, (RE_TS_X + RE_TC_X + RE_TC_W) / 2, "TEMP", TFT_LIGHT_GREY);
  }
}

/***************************************************************************************
   CHROME: RE-ENTRY  (divider + static text-panel labels)
****************************************************************************************/
static const tFont   *RE_PF  = &Roboto_Black_24;   // panel label/value font
static const uint8_t  RE_NR  = 8;

static void _lndgChromeReentry(KCM_TFT &tft) {
  // Divider between graphics zone and text panel
  tft.drawLine(RE_DIV_X, TITLE_TOP, RE_DIV_X, SCREEN_H - 1, TFT_GREY);

  // Vertical "ALTITUDE" label alongside the corridor tape
  drawVerticalText(tft, 0, RE_TAPE_Y, 14, RE_TAPE_H, &Roboto_Black_12,
                   "ALTITUDE", TFT_LIGHT_GREY, TFT_BLACK);

  const uint16_t RHW = RE_TXT_W / 2;
  // Panel row labels (values filled by the draw pass). Rows 0-5 full width.
  const char *r0 = _lndgReentryRow0TPe ? "T+Atm:" : "T+Grnd:";
  const char *r1 = _lndgReentryRow1SL  ? "Alt.SL:" : "Alt.Rdr:";
  printDispChrome(tft, RE_PF, RE_TXT_X, rowYFor(0, RE_NR), RE_TXT_W, rowHFor(RE_NR), r0, COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, RE_PF, RE_TXT_X, rowYFor(1, RE_NR), RE_TXT_W, rowHFor(RE_NR), r1, COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, RE_PF, RE_TXT_X, rowYFor(2, RE_NR), RE_TXT_W, rowHFor(RE_NR), "V.Srf:", COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, RE_PF, RE_TXT_X, rowYFor(3, RE_NR), RE_TXT_W, rowHFor(RE_NR), "PeA:",   COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, RE_PF, RE_TXT_X, rowYFor(4, RE_NR), RE_TXT_W, rowHFor(RE_NR), "Mach:",  COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, RE_PF, RE_TXT_X, rowYFor(5, RE_NR), RE_TXT_W, rowHFor(RE_NR), "G:",     COL_LABEL, COL_BACK, COL_NO_BDR);

  // Rows 6 (Drogue|Main) and 7 (Gear|SAS) — split, with dividers
  auto splitLabels = [&](uint8_t row, const char *lbl, const char *rbl) {
    uint16_t y = rowYFor(row, RE_NR), h = rowHFor(RE_NR);
    printDispChrome(tft, RE_PF, RE_TXT_X,            y, RHW - ROW_PAD, h, lbl, COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, RE_PF, RE_TXT_X + RHW + ROW_PAD, y, RHW - ROW_PAD, h, rbl, COL_LABEL, COL_BACK, COL_NO_BDR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(RE_TXT_X + RHW + dx, y, RE_TXT_X + RHW + dx, rowYFor(row + 1, RE_NR) - 1, TFT_GREY);
  };
  splitLabels(6, "Drogue:", "Main:");
  // Row 7 buttons are drawn in the update pass; only draw the divider here.
  {
    uint16_t y = rowYFor(7, RE_NR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(RE_TXT_X + RHW + dx, y, RE_TXT_X + RHW + dx, SCREEN_H - 1, TFT_GREY);
  }
}

/***************************************************************************************
   DRAW: RE-ENTRY
****************************************************************************************/
static void _lndgDrawReentry(KCM_TFT &tft) {
  const uint16_t RHW = RE_TXT_W / 2;
  uint16_t fg, bg;
  char buf[16];

  auto reVal = [&](uint8_t row, const char *label, const String &val, uint16_t fgc, uint16_t bgc) {
    drawValue(tft, screen_LNDGRE, row, RE_TXT_X, RE_TXT_W, label, val, fgc, bgc, RE_PF, RE_NR);
  };

  // Advance the shared vertical-accel filter once per frame before the estimates.
  ttgAdvanceAccel();
  float tGround = estimateTimeToGround();
  float atmoAlt = (currentBody.lowSpace > 0.0f) ? currentBody.lowSpace : 70000.0f;
  bool  aboveAtmo   = !state.inAtmo;
  bool  peaBelowAtm = (state.periapsis < atmoAlt);
  bool  wantTPe     = (aboveAtmo && peaBelowAtm);

  // ── Graphics widgets (full repaint each frame) ──
  _reDrawTape(tft);
  _reDrawBall(tft);
  _reDrawEnvelope(tft);
  _reDrawGauges(tft);

  // ── Text panel row-label swaps (force a chrome redraw when a label changes) ──
  if (wantTPe != _lndgReentryRow0TPe) {
    _lndgReentryRow0TPe = wantTPe; rowCache[screen_LNDGRE][0].value = "\x01";
    switchToScreen(screen_LNDGRE); return;
  }
  if (aboveAtmo != _lndgReentryRow1SL) {
    _lndgReentryRow1SL = aboveAtmo; rowCache[screen_LNDGRE][1].value = "\x01";
    switchToScreen(screen_LNDGRE); return;
  }

  // Row 0: T+Atm (above atmo, Pe below atmo) or T+Grnd
  if (wantTPe) {
    float tAtmo = estimateTimeToAtmosphere();
    if      (tAtmo >= 0.0f)             reVal(0, "T+Atm:", formatTimeCompact(tAtmo), TFT_DARK_GREEN, TFT_BLACK);
    else                               reVal(0, "T+Atm:", "---", TFT_DARK_GREY, TFT_BLACK);
  } else if (!aboveAtmo) {
    lndgTGroundColors(tGround, fg, bg);
    if (tGround >= 0.0f) reVal(0, "T+Grnd:", formatTimeCompact(tGround), fg, bg);
    else                 reVal(0, "T+Grnd:", "---", fg, bg);
  } else {
    lndgTGroundColors(-1.0f, fg, bg);
    reVal(0, "T+Grnd:", "---", fg, bg);
  }

  // Row 1: Alt.SL (above atmo) or Alt.Rdr (in atmo, coloured by proximity)
  if (aboveAtmo) {
    reVal(1, "Alt.SL:", formatAlt(state.altitude), TFT_DARK_GREEN, TFT_BLACK);
  } else {
    fg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_WHITE  :
         (state.radarAlt < ALT_RDR_WARN_M)  ? TFT_YELLOW : TFT_DARK_GREEN;
    bg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_RED    : TFT_BLACK;
    reVal(1, "Alt.Rdr:", formatAlt(state.radarAlt), fg, bg);
  }

  // Row 2: V.Srf
  reVal(2, "V.Srf:", fmtMs(state.surfaceVel), (state.surfaceVel < 0) ? TFT_RED : TFT_DARK_GREEN, TFT_BLACK);

  // Row 3: PeA — coloured by re-entry corridor regime
  {
    ReCorridor c = _reCorridor();
    int8_t reg = _rePeRegime(c, state.periapsis);
    uint16_t pfg = _reRegimeColor(reg);
    if (reg == 0) { pfg = TFT_WHITE; bg = TFT_RED; } else bg = TFT_BLACK;
    reVal(3, "PeA:", formatAlt(state.periapsis), pfg, bg);
  }

  // Row 4: Mach (transonic band highlighted)
  {
    float m = state.machNumber; snprintf(buf, sizeof(buf), "%.2f", m);
    bool transonic = (m >= 0.85f && m <= 1.2f);
    reVal(4, "Mach:", String(buf), transonic ? TFT_YELLOW : TFT_DARK_GREEN, TFT_BLACK);
  }

  // Row 5: G
  {
    float g = state.gForce; snprintf(buf, sizeof(buf), "%.2f", g);
    fg = (g > G_ALARM_POS || g < G_ALARM_NEG) ? TFT_WHITE  :
         (g > G_WARN_POS  || g < G_WARN_NEG)  ? TFT_YELLOW : TFT_DARK_GREEN;
    bg = (g > G_ALARM_POS || g < G_ALARM_NEG) ? TFT_RED    : TFT_BLACK;
    reVal(5, "G:", String(buf), fg, bg);
  }

  // ── Chute latch bookkeeping (unchanged logic) ──
  float spd = state.surfaceVel;
  if (state.drogueDeploy && !_drogueDeployed) { _drogueDeployed = true; _drogueArmedSafe = (!state.inAtmo || spd <= LNDG_DROGUE_RISKY_MS); }
  if (state.drogueCut    && !_drogueCut)      { _drogueCut = true; _drogueDeployed = false; _drogueArmedSafe = false; }
  if (state.mainDeploy   && !_mainDeployed)   { _mainDeployed = true; _mainArmedSafe = (!state.inAtmo || spd <= LNDG_MAIN_RISKY_MS); }
  if (state.mainCut      && !_mainCut)        { _mainCut = true; _mainDeployed = false; _mainArmedSafe = false; }

  auto chuteState = [&](bool dep, bool cut, bool safe, float safeSpd, float riskySpd, float fullAlt,
                        const char *&lbl, uint16_t &cfg, uint16_t &cbg) {
    if (cut) { lbl = "CUT"; cfg = TFT_RED; cbg = TFT_BLACK; return; }
    if (dep) {
      if (!safe && state.inAtmo && spd > riskySpd) { lbl = "OPEN"; cfg = TFT_WHITE; cbg = TFT_RED; return; }
      if (state.airDensity < LNDG_CHUTE_SEMI_DENSITY) { lbl = "ARMED"; cfg = TFT_SKY; cbg = TFT_BLACK; return; }
      lbl = "OPEN"; cfg = (state.radarAlt > fullAlt) ? TFT_YELLOW : TFT_DARK_GREEN; cbg = TFT_BLACK; return;
    }
    lbl = "STOWED";
    if (!state.inAtmo)          { cfg = TFT_DARK_GREEN; cbg = TFT_BLACK; }
    else if (spd > riskySpd)    { cfg = TFT_WHITE;      cbg = TFT_RED; }
    else if (spd > safeSpd)     { cfg = TFT_YELLOW;     cbg = TFT_BLACK; }
    else                        { cfg = TFT_DARK_GREEN; cbg = TFT_BLACK; }
  };

  // Row 6: Drogue | Main (split values, cached)
  {
    uint16_t xL = RE_TXT_X, wL = RHW - ROW_PAD, xR = RE_TXT_X + RHW + ROW_PAD, wR = RHW - ROW_PAD;
    uint16_t y6 = rowYFor(6, RE_NR), h6 = rowHFor(RE_NR);
    const char *dv; uint16_t dfg, dbg;
    chuteState(_drogueDeployed, _drogueCut, _drogueArmedSafe, LNDG_DROGUE_SAFE_MS, LNDG_DROGUE_RISKY_MS, LNDG_DROGUE_FULL_ALT, dv, dfg, dbg);
    { String ds = dv; RowCache &dc = rowCache[screen_LNDGRE][6];
      if (dc.value != ds || dc.fg != dfg || dc.bg != dbg) {
        printValue(tft, RE_PF, xL, y6, wL, h6, "Drogue:", ds, dfg, dbg, COL_BACK, printState[screen_LNDGRE][6]);
        dc.value = ds; dc.fg = dfg; dc.bg = dbg; } }
    const char *mv; uint16_t mfg, mbg;
    chuteState(_mainDeployed, _mainCut, _mainArmedSafe, LNDG_MAIN_SAFE_MS, LNDG_MAIN_RISKY_MS, LNDG_MAIN_FULL_ALT, mv, mfg, mbg);
    { String ms = mv; RowCache &mc = rowCache[screen_LNDGRE][11];
      if (mc.value != ms || mc.fg != mfg || mc.bg != mbg) {
        printValue(tft, RE_PF, xR, y6, wR, h6, "Main:", ms, mfg, mbg, COL_BACK, printState[screen_LNDGRE][11]);
        mc.value = ms; mc.fg = mfg; mc.bg = mbg; } }
  }

  // Row 7: Gear | SAS buttons
  {
    uint16_t y = rowYFor(7, RE_NR);
    uint16_t rh = SCREEN_H - y;
    uint16_t xL = RE_TXT_X - 2, wL = RHW + 2;
    uint16_t xR = RE_TXT_X + RHW, wR = CONTENT_W - xR;
    // Gear
    {
      bool gearDown = state.gear_on;
      String gv = gearDown ? "DOWN" : "UP";
      RowCache &gc = rowCache[screen_LNDGRE][13];
      if (gc.value != gv) {
        ButtonLabel btn = gearDown
            ? ButtonLabel{ "GEAR", TFT_WHITE,     TFT_WHITE,     TFT_DARK_GREEN, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }
            : ButtonLabel{ "GEAR", TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK,  TFT_OFF_BLACK,  TFT_GREY, TFT_GREY };
        drawButton(tft, xL, y, wL, rh, btn, RE_PF, false);
        gc.value = gv;
      }
    }
    // SAS (re-entry context: STAB/RETRO good; OFF alarm above aero-stable Mach)
    {
      const char *ss; uint16_t sfg, sbg;
      if (state.sasMode == 255) {
        ss = "SAS OFF";
        bool aeroUnstable = (state.machNumber > REENTRY_SAS_AERO_STABLE_MACH);
        sfg = aeroUnstable ? TFT_WHITE : TFT_DARK_GREY;
        sbg = aeroUnstable ? TFT_RED   : TFT_OFF_BLACK;
      } else {
        switch (state.sasMode) {
          case 0:  ss = "STABILITY";  sfg = TFT_WHITE; sbg = TFT_DARK_GREEN; break;
          case 2:  ss = "RETROGRADE"; sfg = TFT_WHITE; sbg = TFT_DARK_GREEN; break;
          case 1:  ss = "PROGRADE";   sfg = TFT_WHITE; sbg = TFT_RED;        break;
          case 3:  ss = "NORMAL";     sfg = TFT_WHITE; sbg = TFT_RED;        break;
          case 4:  ss = "ANTI-NRM";   sfg = TFT_WHITE; sbg = TFT_RED;        break;
          case 5:  ss = "RADIAL+";    sfg = TFT_WHITE; sbg = TFT_RED;        break;
          case 6:  ss = "RADIAL-";    sfg = TFT_WHITE; sbg = TFT_RED;        break;
          case 7:  ss = "TARGET";     sfg = TFT_WHITE; sbg = TFT_RED;        break;
          case 8:  ss = "ANTI-TGT";   sfg = TFT_WHITE; sbg = TFT_RED;        break;
          case 9:  ss = "MANEUVER";   sfg = TFT_WHITE; sbg = TFT_RED;        break;
          default: ss = "SAS OFF";    sfg = TFT_DARK_GREY; sbg = TFT_OFF_BLACK; break;
        }
      }
      RowCache &sc = rowCache[screen_LNDGRE][14];
      String ssv = ss;
      if (sc.value != ssv || sc.fg != sfg || sc.bg != sbg) {
        ButtonLabel btn = { ss, sfg, sfg, sbg, sbg, TFT_GREY, TFT_GREY };
        drawButton(tft, xR, y, wR, rh, btn, RE_PF, false);
        sc.value = ssv; sc.fg = sfg; sc.bg = sbg;
      }
    }
  }
}

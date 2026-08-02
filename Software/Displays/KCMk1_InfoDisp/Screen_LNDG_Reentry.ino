/***************************************************************************************
   Screen_LNDG_Reentry.ino -- Re-entry mode (one of two modes of the LNDG screen).

   rev-2 Phase-2 redesign: RE-ENTRY is now a graphical instrument screen, a sibling of
   POWERED DESCENT. The left/centre zone carries the graphics; a condensed text board
   fills the right ~360px panel.

   Widgets (left/centre graphics zone, x < RE_DIV_X):
     Left cluster (three vertical bars):
     - Corridor tape      : full-height altitude tape whose coloured zones are the
                            re-entry corridor (NO RE-ENTRY / AEROBRAKE / SAFE / DANGER)
                            with the current altitude and periapsis markers. Zone
                            boundaries are anchored on the body table (flyHigh /
                            reentryAlt / lowSpace) and shift up with entry velocity.
     - ATMOSPHERE bar     : atmospheric density fraction (matches the ascent screen).
     - G meter            : zone-background load gauge with a numbered axis.
     Right cluster (centred, sized as large as the space allows):
     - Alignment ball     : heat-shield / retrograde alignment. Plots the surface
                            retrograde vector relative to the nose (boresight); when
                            centred the shield is square into the airflow. AoA readout.
     - CHUTE DEPLOY       : horizontal airspeed bar with live safe-deploy limits.
     - Skin / core temp   : two horizontal bars beneath the chute bar.

   Text panel (right, x >= RE_TXT_X): T+Grnd/T+Atm, Alt, V.Srf, PeA, Mach, V.Vrt, and the
   chute / gear / SAS status board. (G load is shown by the graphical G meter.)

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
static const uint16_t RE_DIV_X   = 578;                 // graphics | panel divider (standard 360px panel)
static const uint16_t RE_TXT_X   = 580;                 // text panel left edge (matches the other screens)
static const uint16_t RE_TXT_W   = CONTENT_W - RE_TXT_X;// 360

// ── Corridor tape (far left) ──
static const uint16_t RE_TAPE_X   = 44;
static const uint16_t RE_TAPE_W   = 35;   // ~80% of the original 44 to free horizontal room
static const uint16_t RE_TAPE_Y   = TITLE_TOP + 8;                  // 70
static const uint16_t RE_TAPE_H   = SCREEN_H - RE_TAPE_Y - 3;       // 527
static const uint16_t RE_TAPE_BOT = RE_TAPE_Y + RE_TAPE_H;         // 597
static const uint16_t RE_TAPE_GUT = 34;   // right-side marker gutter (erased each frame)
static const int16_t  RE_TAPE_M   = 8;    // marker half-height inset (= triangle half-height + 1);
                                          // the tape's VISIBLE rectangle is RE_TAPE_Y+M .. RE_TAPE_BOT-M

// ── ATMO density bar — matches the altitude tape's VISIBLE rectangle (same width and
//    same inset top/bottom) and sits just right of it, with a matching vertical label. ──
static const uint16_t RE_ATMO_LBL_X = 114;                          // vertical label column
static const uint16_t RE_ATMO_X   = 130;
static const uint16_t RE_ATMO_W   = RE_TAPE_W;                      // same width as the tape
static const uint16_t RE_ATMO_Y   = RE_TAPE_Y + RE_TAPE_M;          // 82 — same top as the tape's border
static const uint16_t RE_ATMO_BOT = RE_TAPE_BOT - RE_TAPE_M;        // 585 — same bottom as the tape's border
static const uint16_t RE_ATMO_H   = RE_ATMO_BOT - RE_ATMO_Y;

// ── G METER (left cluster — a third vertical bar just right of the ATMOSPHERE bar) ──
static const uint16_t RE_GA_Y   = RE_ATMO_Y;                // 82 — match the ATMO/altitude top
static const uint16_t RE_GA_H   = RE_ATMO_BOT - RE_GA_Y;    // 503 — match the ATMO height
static const uint16_t RE_GA_BOT = RE_GA_Y + RE_GA_H;        // 585
static const uint16_t RE_GF_X   = 218;  static const uint16_t RE_GF_W = 34;  // bar (left gutter carries axis numbers)
static const uint16_t RE_GLBL_X = RE_GF_X - 36;             // 182 — "G METER" vertical label column

// ── Right graphical cluster: alignment reticle (top) + CHUTE DEPLOY (mid) + TEMP
//    (bottom), all centred on RE_RCX and sized as large as the space allows. ──
static const int16_t  RE_RCX    = 428;                      // shared horizontal centre (nudged left for the 360px panel)

// Alignment ball
static const int16_t  RE_ATT_CX = RE_RCX;
static const int16_t  RE_ATT_CY = 212;
static const int16_t  RE_ATT_R  = 122;
static const float    RE_ATT_FS = (float)RE_ATT_R / 40.0f;  // px per degree (outer ring = 40°)

// Parachute deploy SPEED BAR (horizontal). The green/yellow/red zone boundaries are the
// altitude-corrected safe-deploy speeds derived from live dynamic pressure, so they
// slide left as you descend into denser air. Airspeed axis 0 (left) .. VMAX (right).
static const uint16_t RE_ENV_X    = 280;   // widget footprint (erased each frame)
static const uint16_t RE_ENV_Y    = 390;
static const uint16_t RE_ENV_W    = 296;
static const uint16_t RE_ENV_H    = 140;
static const uint16_t RE_CT_X     = 297;                // bar left edge
static const uint16_t RE_CT_W     = 262;                // bar width (airspeed axis)
static const uint16_t RE_CT_Y     = 436;                // bar top
static const uint16_t RE_CT_H     = 54;                 // bar height
static const uint16_t RE_CT_RIGHT = RE_CT_X + RE_CT_W;  // 570 — bar right edge
static const float    RE_CT_VMAX  = 1000.0f;            // airspeed axis max (m/s)

// Skin / core temperature — two horizontal bars beneath the chute bar.
static const uint16_t RE_TMP_LX = 282;                  // SKIN/CORE label column (right-aligned)
static const uint16_t RE_TMP_X  = 330;                  // bar left edge
static const uint16_t RE_TMP_W  = 229;                  // bar width -> right edge 559 (aligned with chute)
static const uint16_t RE_TMP_Y  = 552;                  // first (skin) bar top
static const uint16_t RE_TMP_BH = 18;                   // bar height
static const uint16_t RE_TMP_VS = 22;                   // row pitch (skin -> core)

/***************************************************************************************
   HELPERS
****************************************************************************************/
static inline float _reWrap180(float a) { while (a > 180.0f) a -= 360.0f; while (a < -180.0f) a += 360.0f; return a; }

// ── Chute deployment (dynamic pressure) ──────────────────────────────────────────────
// Current dynamic pressure q = 0.5*rho*v^2 (Pa); 0 outside the atmosphere.
static inline float _reDynPressure() { return 0.5f * state.airDensity * state.surfaceVel * state.surfaceVel; }
// Altitude-corrected safe-deploy airspeed (m/s) for a chute with the given max q, at
// the current air density. Huge (effectively "any speed") in near-vacuum.
static inline float _reChuteSafeSpeed(float maxQ) {
  return (state.airDensity > 1e-6f) ? sqrtf(2.0f * maxQ / state.airDensity) : 1.0e6f;
}
// Deploy-safety tier for a chute: 0 = safe (green), 1 = caution (yellow), 2 = rip (red).
static inline uint8_t _reChuteTier(float maxQ) {
  float q = _reDynPressure();
  if (q >= maxQ)         return 2;
  if (q >= 0.85f * maxQ) return 1;
  return 0;
}

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
    tft.fillTriangle(barR + 1, yt, barR + 13, yt - 7, barR + 13, yt + 7, TFT_MAGENTA);
    tft.setFont(Roboto_Black_12);
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.setCursor(barR + 16, yt - 7);
    tft.print("Pe");
  }
  {
    int16_t yt = altToY(state.altitude);
    tft.fillRect(ix, yt - 1, iw, 3, TFT_WHITE);
    tft.fillTriangle(barR + 1, yt, barR + 13, yt - 7, barR + 13, yt + 7, TFT_WHITE);
  }
}

/***************************************************************************************
   WIDGET: ATMO DENSITY BAR (same as the ascent screen — density fraction with SKY/
   FRENCH_BLUE/NAVY zones, a no-atmosphere parking segment, 10% ticks, and a marker).
****************************************************************************************/
// Body sea-level density (kg/m^3); 0 = airless. Mirrors the ascent-screen table.
static float _reBodySurfaceDensity() {
  if (currentBody.cond && strcmp(currentBody.cond, "Vacuum") == 0) return 0.0f;
  const char *n = currentBody.soiName;
  if (!n || n[0] == '\0') return 0.0f;
  if (!strcmp(n, "Kerbin")) return 1.225f;
  if (!strcmp(n, "Eve"))    return 6.150f;
  if (!strcmp(n, "Duna"))   return 0.0678f;
  if (!strcmp(n, "Jool"))   return 14.00f;
  if (!strcmp(n, "Laythe")) return 1.700f;
  return 0.0f;
}
// Atmospheric depth fraction (0 = vacuum, 1 = sea level); -1 if airless. ratio^0.25
// so the thin upper atmosphere still reads on the scale (matches ascent).
static float _reAtmoFraction() {
  float maxD = _reBodySurfaceDensity();
  if (maxD <= 0.0f) return -1.0f;
  float r = state.airDensity / maxD;
  if (r <= 0.0f) return 0.0f;
  if (r >= 1.0f) return 1.0f;
  return sqrtf(sqrtf(r));
}

static void _reDrawAtmo(KCM_TFT &tft) {
  const int16_t x0 = RE_ATMO_X, w = RE_ATMO_W;
  const int16_t innerX0 = x0 + 1, innerW = w - 2;
  const int16_t innerY0 = RE_ATMO_Y + 1, innerY1 = RE_ATMO_BOT - 1;
  const int16_t innerH  = innerY1 - innerY0 + 1;
  const int16_t noAtmH  = (int16_t)roundf(0.10f * innerH);   // bottom 10% = parking
  const int16_t atmYBot = innerY1 - noAtmH;
  const int16_t atmH    = atmYBot - innerY0 + 1;
  auto fracToY = [&](float f) -> int16_t {
    if (f < 0) f = 0; if (f > 1) f = 1;
    return atmYBot - (int16_t)roundf(f * (float)(atmH - 1));
  };

  // Erase the bar + right-side marker gutter each frame (the vertical "ATMO" label
  // lives left of the bar and is chrome, so it is not touched here).
  tft.fillRect(x0, RE_ATMO_Y, RE_ATMO_W + 14, RE_ATMO_H + 2, TFT_BLACK);

  int16_t y75 = fracToY(0.75f), y35 = fracToY(0.35f);
  tft.fillRect(innerX0, innerY0, innerW, y75 - innerY0,     TFT_SKY);          // dense (top)
  tft.fillRect(innerX0, y75,     innerW, y35 - y75,         TFT_FRENCH_BLUE);  // medium
  tft.fillRect(innerX0, y35,     innerW, atmYBot - y35 + 1, TFT_NAVY);         // near vacuum
  if (atmYBot + 1 <= innerY1)
    tft.fillRect(innerX0, atmYBot + 1, innerW, innerY1 - atmYBot, TFT_OFF_BLACK);  // parking

  int16_t tMaj = (int16_t)roundf(0.40f * innerW), tMin = (int16_t)roundf(0.20f * innerW);
  if (tMaj < 1) tMaj = 1; if (tMin < 1) tMin = 1;
  for (int16_t i = 0; i < 10; i++) {
    int16_t ty = fracToY((float)i / 10.0f);
    tft.drawLine(innerX0, ty, innerX0 + tMaj - 1, ty, (ty < y75) ? TFT_DARK_GREY : TFT_LIGHT_GREY);
  }
  for (int16_t i = 0; i < 10; i++) {
    int16_t ty = fracToY(((float)i + 0.5f) / 10.0f);
    tft.drawLine(innerX0, ty, innerX0 + tMin - 1, ty, (ty < y75) ? TFT_DARK_GREY : TFT_LIGHT_GREY);
  }

  tft.drawRect(x0, RE_ATMO_Y, w, RE_ATMO_BOT - RE_ATMO_Y, TFT_LIGHT_GREY);

  // Marker — right-side triangle pointing left at the current density fraction; parks
  // in the OFF_BLACK segment in vacuum / on airless bodies.
  float frac = _reAtmoFraction();
  bool  vac  = (frac <= 0.0f);
  int16_t triY = vac ? (int16_t)((atmYBot + 1 + innerY1) / 2) : fracToY(frac);
  int16_t tipX = x0 + w + 1, baseX = tipX + 12;
  tft.fillTriangle(tipX, triY, baseX, triY - 7, baseX, triY + 7, TFT_WHITE);
}

/***************************************************************************************
   WIDGET: HEAT-SHIELD / RETROGRADE ALIGNMENT BALL
****************************************************************************************/
// KSP retrograde symbol: a ringed circle with an internal X and three short spokes
// pointing up, left and right. Stroke width and spoke overshoot scale from `r`.
static void _reRetroSymbol(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t col) {
  int16_t w   = (r >= 16) ? 3 : 2;
  int16_t ext = (r / 2 > 4) ? r / 2 : 4;
  for (int16_t i = 0; i < w; i++) tft.drawCircle(cx, cy, r - i, col);
  int16_t d = (int16_t)(r * 0.7071f);
  drawThickLine(tft, cx - d, cy - d, cx + d, cy + d, w, col);
  drawThickLine(tft, cx - d, cy + d, cx + d, cy - d, w, col);
  static const int16_t spoke[3] = { -90, 0, 180 };    // screen degrees: up, right, left
  for (uint8_t i = 0; i < 3; i++) {
    float a = spoke[i] * (float)DEG_TO_RAD;
    drawThickLine(tft, cx + (int16_t)(r         * cosf(a)), cy + (int16_t)(r         * sinf(a)),
                       cx + (int16_t)((r + ext) * cosf(a)), cy + (int16_t)((r + ext) * sinf(a)), w, col);
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
    float lim = R - 23;                          // keep the whole (r=14) symbol inside the disc
    if (mag > lim && mag > 0.0f) { float k = lim / mag; rx *= k; ry *= k; }
    _reRetroSymbol(tft, cx + (int16_t)rx, cy + (int16_t)ry, 14, mc);
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
   WIDGET: CHUTE DEPLOY SPEED TAPE (airspeed vs live safe-deploy limits)
   Horizontal airspeed bar. The green/yellow/red boundaries are the altitude-corrected
   safe-deploy speeds (from live dynamic pressure), so they slide left as the air
   thickens. The white marker is the current airspeed.
     green  : q < main limit        (main + drogue safe)
     yellow : main <= q < drogue     (drogue only)
     red    : q >= drogue limit      (nothing safe)
****************************************************************************************/
static void _reDrawEnvelope(KCM_TFT &tft) {
  const uint16_t iy = RE_CT_Y + 1, ih = RE_CT_H - 2;
  const uint16_t barBot = RE_CT_Y + RE_CT_H;

  // Live altitude-corrected safe-deploy speeds
  float vMain   = _reChuteSafeSpeed(LNDG_CHUTE_MAIN_MAX_Q);
  float vDrogue = _reChuteSafeSpeed(LNDG_CHUTE_DROGUE_MAX_Q);

  // Auto-range the airspeed axis so the current speed AND the drogue boundary always
  // fit. With a fixed max the safe speeds run off-scale at altitude (thin air), the
  // bar reads all-green, yet the q-based marker can be red — a contradiction. Grow the
  // axis 1000 -> 3000 m/s in 500 steps so marker position and colour always agree.
  float need    = fmaxf(state.surfaceVel, vDrogue) * 1.08f;
  float axisMax = RE_CT_VMAX;
  while (axisMax < need && axisMax < 3000.0f) axisMax += 500.0f;

  // Map speed to the bar INTERIOR [RE_CT_X+1 .. RE_CT_RIGHT-2] so ticks, boundaries
  // and the marker never draw on/over the border rectangle.
  auto spdToX = [&](float v) -> int16_t {
    float f = v / axisMax; if (f < 0) f = 0; if (f > 1) f = 1;
    return (int16_t)((RE_CT_X + 1) + f * (RE_CT_W - 3));
  };

  // Erase the widget footprint each frame (title + tags + bar + labels) — no streaks.
  tft.fillRect(RE_ENV_X, RE_ENV_Y - 16, RE_ENV_W, RE_ENV_H + 16, TFT_BLACK);

  int16_t xMain   = spdToX(vMain);
  int16_t xDrogue = spdToX(vDrogue);

  // Zone fills (dim): green 0..vMain (left), yellow vMain..vDrogue, red vDrogue..max
  int16_t gx0 = RE_CT_X + 1, rEnd = RE_CT_RIGHT - 1;
  if (xMain   > gx0)     tft.fillRect(gx0,     iy, xMain - gx0,     ih, TFT_JUNGLE);
  if (xDrogue > xMain)   tft.fillRect(xMain,   iy, xDrogue - xMain, ih, TFT_DARK_YELLOW);
  if (rEnd    > xDrogue) tft.fillRect(xDrogue, iy, rEnd - xDrogue,  ih, TFT_DARK_RED);

  // Boundary lines + tags above the bar
  tft.setFont(Roboto_Black_12);
  if (xMain > RE_CT_X && xMain < RE_CT_RIGHT) {
    tft.drawLine(xMain, iy, xMain, iy + ih - 1, TFT_NEON_GREEN);
    tft.setTextColor(TFT_NEON_GREEN, TFT_BLACK);
    tft.setCursor(xMain - 14, RE_CT_Y - 28); tft.print("MAIN");
  }
  if (xDrogue > RE_CT_X && xDrogue < RE_CT_RIGHT) {
    tft.drawLine(xDrogue, iy, xDrogue, iy + ih - 1, TFT_YELLOW);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(xDrogue - 12, RE_CT_Y - 28); tft.print("DROG");
  }

  // Ticks INSIDE the bar on both edges (like the altitude tape) + centred numeric
  // labels below. Major ticks at each axisMax/5 step, minor at the half-steps.
  int step = (int)(axisMax / 5.0f + 0.5f);
  tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
  for (int v = 0; v <= (int)axisMax + 1; v += step) {
    int16_t tx = spdToX((float)v);
    tft.drawLine(tx, iy,             tx, iy + 7,        TFT_LIGHT_GREY);   // top edge, inward
    tft.drawLine(tx, barBot - 1 - 7, tx, barBot - 1,    TFT_LIGHT_GREY);   // bottom edge, inward
    char b[6]; snprintf(b, sizeof(b), "%d", v);
    int16_t lw = getFontStringWidth(&Roboto_Black_12, b);
    int16_t lx = (int16_t)constrain((int)(tx - lw / 2), (int)RE_ENV_X, (int)(RE_ENV_X + RE_ENV_W - lw));
    tft.setCursor(lx, barBot + 6); tft.print(b);
  }
  for (int v = step / 2; v <= (int)axisMax; v += step) {   // minor ticks (both edges)
    int16_t tx = spdToX((float)v);
    tft.drawLine(tx, iy,             tx, iy + 4,        TFT_GREY);
    tft.drawLine(tx, barBot - 1 - 4, tx, barBot - 1,    TFT_GREY);
  }

  tft.drawRect(RE_CT_X, RE_CT_Y, RE_CT_W, RE_CT_H, TFT_GREY);

  // Current-airspeed marker: white vertical line through the bar + a tier-coloured
  // triangle above it pointing down.
  float q = _reDynPressure();
  uint16_t vc = (q < LNDG_CHUTE_MAIN_MAX_Q) ? TFT_NEON_GREEN :
                (q < LNDG_CHUTE_DROGUE_MAX_Q) ? TFT_YELLOW : TFT_RED;
  int16_t xm = constrain(spdToX(state.surfaceVel), (int16_t)(RE_CT_X + 8), (int16_t)(RE_CT_RIGHT - 8));
  tft.fillRect(xm - 1, iy, 3, ih, TFT_WHITE);
  tft.fillTriangle(xm, RE_CT_Y - 1, xm - 8, RE_CT_Y - 11, xm + 8, RE_CT_Y - 11, vc);

  // Title + axis label, both centred on the bar (textCenter centres within the box).
  textCenter(tft, &Roboto_Black_12, RE_CT_X, RE_CT_Y - 44, RE_CT_W, 16, "CHUTE DEPLOY",   TFT_LIGHT_GREY, TFT_BLACK);
  textCenter(tft, &Roboto_Black_12, RE_CT_X, barBot + 22,   RE_CT_W, 16, "AIRSPEED (m/s)", TFT_LIGHT_GREY, TFT_BLACK);
}

/***************************************************************************************
   WIDGET: G METER (left cluster) — zone-background gauge with a numbered axis
   Green/yellow/red bands come from this controller's G thresholds (aligned to the
   annunciator, G_WARN/G_ALARM, ±); a neutral dark-grey band marks the -1..+2 g nominal
   range. Live white marker + numbered axis in the left gutter.
****************************************************************************************/
static void _reDrawGauges(KCM_TFT &tft) {
  const uint16_t gx = RE_GF_X, gw = RE_GF_W;
  const uint16_t gix = gx + 1, giw = gw - 2;
  const float gMin = -6.0f, gMax = 12.0f;
  auto gToY = [&](float g) -> int16_t {
    float f = (g - gMin) / (gMax - gMin); if (f < 0) f = 0; if (f > 1) f = 1;
    return (int16_t)(RE_GA_BOT - f * RE_GA_H);
  };
  tft.fillRect(gx - 20, RE_GA_Y, gw + 34, RE_GA_H + 1, TFT_BLACK); // erase axis numbers + bar + marker gutter
  int16_t yAN = gToY(G_ALARM_NEG), yWN = gToY(G_WARN_NEG), yWP = gToY(G_WARN_POS), yAP = gToY(G_ALARM_POS);
  tft.fillRect(gix, yAN,     giw, RE_GA_BOT - yAN, TFT_DARK_RED);     // g < alarm-neg
  tft.fillRect(gix, yWN,     giw, yAN - yWN,       TFT_DARK_YELLOW);  // warn-neg .. alarm-neg
  tft.fillRect(gix, yWP,     giw, yWN - yWP,       TFT_JUNGLE);       // caution-safe band
  tft.fillRect(gix, yAP,     giw, yWP - yAP,       TFT_DARK_YELLOW);  // warn-pos .. alarm-pos
  tft.fillRect(gix, RE_GA_Y, giw, yAP - RE_GA_Y,   TFT_DARK_RED);     // g > alarm-pos
  // Nominal band (-1 .. +2 g) — neutral dark-grey background over the green safe band.
  int16_t yP2 = gToY(2.0f), yN1 = gToY(-1.0f);
  tft.fillRect(gix, yP2, giw, yN1 - yP2, TFT_DARK_GREY);
  int16_t y0 = gToY(0.0f);
  tft.drawLine(gix, y0, gix + giw - 1, y0, TFT_LIGHT_GREY);          // 0-g reference
  // Numbered vertical axis: major tick + right-aligned value every 3 g in the left gutter.
  tft.setFont(Roboto_Black_12);
  for (int gv = (int)gMin; gv <= (int)gMax; gv += 3) {
    int16_t ty = gToY((float)gv);
    tft.drawLine(gx + 1, ty, gx + 5, ty, TFT_LIGHT_GREY);
    char nb[6]; snprintf(nb, sizeof(nb), "%d", gv);
    int16_t nw = getFontStringWidth(&Roboto_Black_12, nb);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(gx - 4 - nw, ty - 7);
    tft.print(nb);
  }
  for (int gv = (int)gMin + 1; gv < (int)gMax; gv++) {               // minor integer ticks
    if (((gv - (int)gMin) % 3) == 0) continue;
    int16_t ty = gToY((float)gv);
    tft.drawLine(gx + 1, ty, gx + 3, ty, TFT_GREY);
  }
  tft.drawRect(gx, RE_GA_Y, gw, RE_GA_H, TFT_GREY);
  float g = state.gForce;
  int16_t ym = constrain(gToY(g), (int16_t)(RE_GA_Y + 7), (int16_t)(RE_GA_BOT - 7));
  tft.fillRect(gix, ym - 1, giw, 3, TFT_WHITE);                      // white index line
  tft.fillTriangle(gx + gw + 1, ym, gx + gw + 12, ym - 7, gx + gw + 12, ym + 7, TFT_WHITE);
}

/***************************************************************************************
   WIDGET: SKIN / CORE TEMPERATURE (horizontal bars beneath the chute deploy bar)
****************************************************************************************/
// One horizontal temp bar: left tag, dim background, coloured fill, warn/alarm ticks.
static void _reHTempBar(KCM_TFT &tft, uint16_t y, const char *tag, uint8_t pct) {
  uint16_t col = (pct >= TEMP_ALARM_PCT) ? TFT_RED :
                 (pct >= TEMP_WARN_PCT)  ? TFT_YELLOW : TFT_NEON_GREEN;
  const uint16_t ix = RE_TMP_X + 1, iw = RE_TMP_W - 2;
  float f = pct / 100.0f; if (f < 0) f = 0; if (f > 1) f = 1;
  tft.setFont(Roboto_Black_12);
  tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
  int16_t lw = getFontStringWidth(&Roboto_Black_12, tag);
  tft.setCursor(RE_TMP_X - 8 - lw, y + (RE_TMP_BH - 14) / 2);
  tft.print(tag);
  tft.fillRect(ix, y + 1, iw, RE_TMP_BH - 2, TFT_OFF_BLACK);           // empty
  tft.fillRect(ix, y + 1, (uint16_t)(f * iw), RE_TMP_BH - 2, col);     // fill from the left
  int16_t xw = ix + (int16_t)(TEMP_WARN_PCT  / 100.0f * iw);
  int16_t xa = ix + (int16_t)(TEMP_ALARM_PCT / 100.0f * iw);
  tft.drawLine(xw, y + 1, xw, y + RE_TMP_BH - 2, TFT_YELLOW);          // warn threshold
  tft.drawLine(xa, y + 1, xa, y + RE_TMP_BH - 2, TFT_RED);             // alarm threshold
  tft.drawRect(RE_TMP_X, y, RE_TMP_W, RE_TMP_BH, TFT_GREY);
}
static void _reDrawTemp(KCM_TFT &tft) {
  tft.fillRect(RE_TMP_LX, RE_TMP_Y - 2, (RE_TMP_X + RE_TMP_W) - RE_TMP_LX + 2,
               RE_TMP_VS + RE_TMP_BH + 4, TFT_BLACK);
  _reHTempBar(tft, RE_TMP_Y,             "SKIN", state.skinTempPct);
  _reHTempBar(tft, RE_TMP_Y + RE_TMP_VS, "CORE", state.coreTempPct);
}

/***************************************************************************************
   CHROME: RE-ENTRY  (divider + static text-panel labels)
****************************************************************************************/
static const tFont   *RE_LF  = &Roboto_Black_20;   // panel label font (LNDG 20pt tier)
static const tFont   *RE_BTN = &Roboto_Black_28;   // SAS/Gear button font — matches the 28pt button labels on the other screens
static const tFont   *RE_VF  = &Roboto_Black_32;   // panel value font — enlarged; fits the 360px standard panel
static const uint8_t  RE_NR  = 8;

static void _lndgChromeReentry(KCM_TFT &tft) {
  // Divider between graphics zone and text panel
  tft.drawLine(RE_DIV_X, TITLE_TOP, RE_DIV_X, SCREEN_H - 1, TFT_GREY);

  // Vertical labels for the two side-by-side bars — same format for both.
  drawVerticalText(tft, 0, RE_TAPE_Y, 14, RE_TAPE_H, &Roboto_Black_12,
                   "ALTITUDE", TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, RE_ATMO_LBL_X, RE_ATMO_Y, 14, RE_ATMO_H, &Roboto_Black_12,
                   "ATMOSPHERE", TFT_LIGHT_GREY, TFT_BLACK);

  // G METER vertical label (left cluster, just right of the ATMOSPHERE bar).
  drawVerticalText(tft, RE_GLBL_X, RE_GA_Y, 14, RE_GA_H, &Roboto_Black_12,
                   "G METER", TFT_LIGHT_GREY, TFT_BLACK);

  const uint16_t RHW = RE_TXT_W / 2;
  // Panel row labels (values filled by the draw pass). Rows 0-5 full width.
  const char *r0 = _lndgReentryRow0TPe ? "T+Atm:" : "T+Grnd:";
  const char *r1 = _lndgReentryRow1SL  ? "Alt.SL:" : "Alt.Rdr:";
  printDispChrome(tft, RE_LF, RE_TXT_X, rowYFor(0, RE_NR), RE_TXT_W, rowHFor(RE_NR), r0, COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, RE_LF, RE_TXT_X, rowYFor(1, RE_NR), RE_TXT_W, rowHFor(RE_NR), r1, COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, RE_LF, RE_TXT_X, rowYFor(2, RE_NR), RE_TXT_W, rowHFor(RE_NR), "V.Srf:", COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, RE_LF, RE_TXT_X, rowYFor(3, RE_NR), RE_TXT_W, rowHFor(RE_NR), "V.Vrt:", COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, RE_LF, RE_TXT_X, rowYFor(4, RE_NR), RE_TXT_W, rowHFor(RE_NR), "PeA:",   COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, RE_LF, RE_TXT_X, rowYFor(5, RE_NR), RE_TXT_W, rowHFor(RE_NR), "Mach:",  COL_LABEL, COL_BACK, COL_NO_BDR);

  // Horizontal separator above the Drogue/Main status board.
  tft.drawLine(RE_DIV_X, rowYFor(6, RE_NR) - ROW_PAD, CONTENT_W - 1, rowYFor(6, RE_NR) - ROW_PAD, TFT_GREY);

  // Rows 6 (Drogue|Main) and 7 (Gear|SAS) — split, with dividers
  auto splitLabels = [&](uint8_t row, const char *lbl, const char *rbl) {
    uint16_t y = rowYFor(row, RE_NR), h = rowHFor(RE_NR);
    printDispChrome(tft, RE_LF, RE_TXT_X,            y, RHW - ROW_PAD, h, lbl, COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, RE_LF, RE_TXT_X + RHW + ROW_PAD, y, RHW - ROW_PAD, h, rbl, COL_LABEL, COL_BACK, COL_NO_BDR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(RE_TXT_X + RHW + dx, y, RE_TXT_X + RHW + dx, rowYFor(row + 1, RE_NR) - 1, TFT_GREY);
  };
  splitLabels(6, "Drog:", "Main:");
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
    drawValue(tft, screen_LNDGRE, row, RE_TXT_X, RE_TXT_W, label, val, fgc, bgc, RE_VF, RE_NR);
  };

  // Advance the shared vertical-accel filter once per frame before the estimates.
  ttgAdvanceAccel();
  float tGround = estimateTimeToGround();
  float atmoAlt = (currentBody.lowSpace > 0.0f) ? currentBody.lowSpace : 70000.0f;
  bool  aboveAtmo   = !state.inAtmo;
  bool  peaBelowAtm = (state.periapsis < atmoAlt);
  bool  wantTPe     = (aboveAtmo && peaBelowAtm);

  // ── Graphics widgets (full repaint each frame) ──
  // Left cluster: altitude tape, ATMOSPHERE bar, G meter.
  _reDrawTape(tft);
  _reDrawAtmo(tft);
  _reDrawGauges(tft);          // G meter, beside the ATMOSPHERE bar
  // Right cluster (top -> bottom): reticle, chute deploy, skin/core temp.
  _reDrawBall(tft);
  _reDrawEnvelope(tft);
  _reDrawTemp(tft);

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
  // surfaceVel is a magnitude, always >= 0
  reVal(2, "V.Srf:", fmtMs(state.surfaceVel), TFT_DARK_GREEN, TFT_BLACK);

  // Row 3: V.Vrt (vertical descent rate). The G load now lives in the graphical G meter.
  // Hazard-coloured (landing thresholds) only near the ground; at altitude a high descent
  // rate is normal, so it reads neutral there.
  {
    float vv = state.verticalVel;
    if (state.inAtmo && state.radarAlt < ALT_RDR_WARN_M) {
      fg = (vv < LNDG_VVRT_ALARM_MS) ? TFT_WHITE  :
           (vv < LNDG_VVRT_WARN_MS)  ? TFT_YELLOW : TFT_DARK_GREEN;
      bg = (vv < LNDG_VVRT_ALARM_MS) ? TFT_RED    : TFT_BLACK;
    } else {
      fg = TFT_DARK_GREEN; bg = TFT_BLACK;
    }
    reVal(3, "V.Vrt:", fmtMs(vv), fg, bg);
  }

  // Row 4: PeA — coloured by re-entry corridor regime
  {
    ReCorridor c = _reCorridor();
    int8_t reg = _rePeRegime(c, state.periapsis);
    uint16_t pfg = _reRegimeColor(reg);
    if (reg == 0) { pfg = TFT_WHITE; bg = TFT_RED; } else bg = TFT_BLACK;
    reVal(4, "PeA:", formatAlt(state.periapsis), pfg, bg);
  }

  // Row 5: Mach (transonic band highlighted)
  {
    float m = state.machNumber; snprintf(buf, sizeof(buf), "%.2f", m);
    bool transonic = (m >= 0.85f && m <= 1.2f);
    reVal(5, "Mach:", String(buf), transonic ? TFT_YELLOW : TFT_DARK_GREEN, TFT_BLACK);
  }

  // ── Chute latch bookkeeping — armed-safe when deployed below the rip q ──
  if (state.drogueDeploy && !_drogueDeployed) { _drogueDeployed = true; _drogueArmedSafe = (!state.inAtmo || _reDynPressure() < LNDG_CHUTE_DROGUE_MAX_Q); }
  if (state.drogueCut    && !_drogueCut)      { _drogueCut = true; _drogueDeployed = false; _drogueArmedSafe = false; }
  if (state.mainDeploy   && !_mainDeployed)   { _mainDeployed = true; _mainArmedSafe = (!state.inAtmo || _reDynPressure() < LNDG_CHUTE_MAIN_MAX_Q); }
  if (state.mainCut      && !_mainCut)        { _mainCut = true; _mainDeployed = false; _mainArmedSafe = false; }

  // Chute status by dynamic-pressure tier vs the chute's rip limit (maxQ).
  auto chuteState = [&](bool dep, bool cut, bool safe, float maxQ, float fullAlt,
                        const char *&lbl, uint16_t &cfg, uint16_t &cbg) {
    uint8_t tier = _reChuteTier(maxQ);   // 0 safe, 1 caution, 2 rip
    if (cut) { lbl = "CUT"; cfg = TFT_RED; cbg = TFT_BLACK; return; }
    if (dep) {
      if (!safe && state.inAtmo && tier == 2) { lbl = "OPEN"; cfg = TFT_WHITE; cbg = TFT_RED; return; }
      if (state.airDensity < LNDG_CHUTE_SEMI_DENSITY) { lbl = "ARM"; cfg = TFT_SKY; cbg = TFT_BLACK; return; }
      lbl = "OPEN"; cfg = (state.radarAlt > fullAlt) ? TFT_YELLOW : TFT_DARK_GREEN; cbg = TFT_BLACK; return;
    }
    lbl = "STOW";
    if (!state.inAtmo)      { cfg = TFT_DARK_GREEN; cbg = TFT_BLACK; }
    else if (tier == 2)     { cfg = TFT_WHITE;      cbg = TFT_RED; }
    else if (tier == 1)     { cfg = TFT_YELLOW;     cbg = TFT_BLACK; }
    else                    { cfg = TFT_DARK_GREEN; cbg = TFT_BLACK; }
  };

  // Row 6: Drogue | Main (split values, cached). The "Drog:"/"Main:" labels are chrome;
  // the status right-aligns in a value sub-cell to the right of the label (empty param —
  // no redundant reservation), so the short states fit at the full value font.
  {
    uint16_t xL = RE_TXT_X, xR = RE_TXT_X + RHW + ROW_PAD;
    uint16_t wL = RHW - ROW_PAD, wR = RHW - ROW_PAD;
    uint16_t xDV = xL + 80, wDV = wL - 80;   // drogue value sub-cell (right of "Drog:")
    uint16_t xMV = xR + 76, wMV = wR - 76;   // main value sub-cell (right of "Main:")
    uint16_t y6 = rowYFor(6, RE_NR), h6 = rowHFor(RE_NR);
    const char *dv; uint16_t dfg, dbg;
    chuteState(_drogueDeployed, _drogueCut, _drogueArmedSafe, LNDG_CHUTE_DROGUE_MAX_Q, LNDG_DROGUE_FULL_ALT, dv, dfg, dbg);
    { String ds = dv; RowCache &dc = rowCache[screen_LNDGRE][6];
      if (dc.value != ds || dc.fg != dfg || dc.bg != dbg) {
        printValue(tft, RE_VF, xDV, y6, wDV, h6, "", ds, dfg, dbg, COL_BACK, printState[screen_LNDGRE][6]);
        dc.value = ds; dc.fg = dfg; dc.bg = dbg; } }
    const char *mv; uint16_t mfg, mbg;
    chuteState(_mainDeployed, _mainCut, _mainArmedSafe, LNDG_CHUTE_MAIN_MAX_Q, LNDG_MAIN_FULL_ALT, mv, mfg, mbg);
    { String ms = mv; RowCache &mc = rowCache[screen_LNDGRE][11];
      if (mc.value != ms || mc.fg != mfg || mc.bg != mbg) {
        printValue(tft, RE_VF, xMV, y6, wMV, h6, "", ms, mfg, mbg, COL_BACK, printState[screen_LNDGRE][11]);
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
        drawButton(tft, xL, y, wL, rh, btn, RE_BTN, false);
        gc.value = gv;
      }
    }
    // SAS (re-entry context: STAB/RETRO good; OFF alarm above aero-stable Mach)
    {
      const char *ss; uint16_t sfg, sbg;
      if (state.sasMode == 255) {
        ss = "SAS";
        bool aeroUnstable = (state.machNumber > REENTRY_SAS_AERO_STABLE_MACH);
        sfg = aeroUnstable ? TFT_WHITE : TFT_DARK_GREY;
        sbg = aeroUnstable ? TFT_RED   : TFT_OFF_BLACK;
      } else {
        switch (state.sasMode) {
          case 0:  ss = "STAB"; sfg = TFT_WHITE; sbg = TFT_DARK_GREEN; break;
          case 2:  ss = "RETR"; sfg = TFT_WHITE; sbg = TFT_DARK_GREEN; break;
          case 1:  ss = "PRO";  sfg = TFT_WHITE; sbg = TFT_RED;        break;
          case 3:  ss = "NRM";  sfg = TFT_WHITE; sbg = TFT_RED;        break;
          case 4:  ss = "ANRM"; sfg = TFT_WHITE; sbg = TFT_RED;        break;
          case 5:  ss = "RAD+"; sfg = TFT_WHITE; sbg = TFT_RED;        break;
          case 6:  ss = "RAD-"; sfg = TFT_WHITE; sbg = TFT_RED;        break;
          case 7:  ss = "TGT";  sfg = TFT_WHITE; sbg = TFT_RED;        break;
          case 8:  ss = "ATGT"; sfg = TFT_WHITE; sbg = TFT_RED;        break;
          case 9:  ss = "MNVR"; sfg = TFT_WHITE; sbg = TFT_RED;        break;
          default: ss = "SAS";  sfg = TFT_DARK_GREY; sbg = TFT_OFF_BLACK; break;
        }
      }
      RowCache &sc = rowCache[screen_LNDGRE][14];
      String ssv = ss;
      if (sc.value != ssv || sc.fg != sfg || sc.bg != sbg) {
        ButtonLabel btn = { ss, sfg, sfg, sbg, sbg, TFT_GREY, TFT_GREY };
        drawButton(tft, xR, y, wR, rh, btn, RE_BTN, false);
        sc.value = ssv; sc.fg = sfg; sc.bg = sbg;
      }
    }
  }
}

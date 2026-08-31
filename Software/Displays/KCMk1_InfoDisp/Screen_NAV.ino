/***************************************************************************************
   Screen_NAV.ino -- Navigation display (mission panel, atmospheric flight)

   The other half of a glass-cockpit pair. Info Display 1 holds the PFD (attitude,
   altitude, speed); this is the plan view beside it — where the vessel is pointed,
   where it is actually going, and where the target is. That pairing is the standard
   airliner arrangement, and it is the one thing the mission panel had nothing to offer
   during atmospheric flight: its fallback was ORBIT, whose apsides and period mean
   nothing to an aircraft.

   Routed to when the vessel is in the atmosphere and its apoapsis is below the top of
   that atmosphere — i.e. flying in the air rather than climbing out of it. A spaceplane
   building apoapsis keeps ORBIT, which is what it wants; see missionContextScreen().

   Layout (940 x 538 content area):

     Compass card, centred        rotating rose, nose fixed at 12 o'clock
       nose triangle              vessel heading, always straight up
       green marker               ground track — where the vessel is actually moving
       violet marker              target bearing (matches the target colour elsewhere)
     Heading box, above the card  HDG, three digits
     Left column                  TRK (ground track), DRIFT (track minus heading)
     Right column                 DIST, V.CLS (closure) and T+INT, when a target is set

   Drift is the one number here that appears nowhere else on either panel: the angle
   between where the nose points and where the vessel is going. In an atmosphere that
   is the crab angle, and it is the difference between a heading that is holding and a
   heading that is quietly sliding off.

   Everything is derived from telemetry the panel already receives — heading,
   srfVelHeading, target bearing and distance, closure. No new Simpit channels.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


// ── Geometry ──────────────────────────────────────────────────────────────────────────
// ROVER's card at 1.12x. Every proportion is ROVER's, scaled by one factor, so the two
// screens still read as one instrument family and the shared renderer still drives both
// -- but this screen has no bottom strip to clear, so the card spends the height ROVER
// spends on target distance.
//
// The scale is not a preference, it is what the budget allows. The heading box has to
// clear the title rule (y=62) by 5 px and the ring has to clear the bottom edge, and the
// box hangs off the nose base, which scales with the radius:
//
//   box top    = cy - noseRBase - 6 - NAV_HDG_BOX_H = 370 - 245 - 58 = 67   (rule + 5)
//   ring bottom= cy + rRing                         = 370 + 224      = 594  (edge - 5)
//
// Sizing this card by eye rather than from that budget is how an earlier attempt put the
// heading box at y=57, four pixels into the title rule. Horizontally the ring spans
// 246..694, clearing both 190 px side columns by 56 px.
static const int16_t NAV_CX = CONTENT_W / 2;   // 470
static const int16_t NAV_CY = 370;
static const int16_t NAV_R  = 224;

static const CompassGeom NAV_GEOM = {
  NAV_CX, NAV_CY,
  NAV_R,
  219,          // tick outer  (inside the ring, so tick-erase never touches it)
  199,          // major tick inner
  207,          // minor tick inner
  176, 176,     // letter / numeric label centres
  228, 245, 13, // nose: tip just outside the ring, base beyond it
  143, 123, 13  // bearing markers: inside the label boxes (which reach in to ~152)
};

static const float NAV_HDG_THRESH_DEG = 0.5f;   // card rotates
static const float NAV_MK_THRESH_DEG  = 1.0f;   // marker moved

// Heading readout, above the nose triangle.
static const int16_t NAV_HDG_BOX_W = 110;
static const int16_t NAV_HDG_BOX_H = 52;
static const int16_t NAV_HDG_BOX_X = NAV_CX - (NAV_HDG_BOX_W / 2);
static const int16_t NAV_HDG_BOX_Y = NAV_CY - NAV_GEOM.noseRBase - 6 - NAV_HDG_BOX_H;

// Side columns, same widths as ROVER's so the compass centres between them.
static const int16_t NAV_COL_W    = 190;
static const int16_t NAV_LCOL_X   = 0;
static const int16_t NAV_RCOL_X   = CONTENT_W - NAV_COL_W;   // 750
static const int16_t NAV_COL_Y    = TITLE_TOP;
static const int16_t NAV_BLOCK_H  = 120;
static const int16_t NAV_LBL_H    = 32;
static const int16_t NAV_VAL_H    = 48;

// No bottom strip. It carried V.Srf and Alt.Rdr, and this screen's partner is always
// AIRCRAFT -- the routing rule is the same condition on both panels -- so both numbers
// were on the other display in every phase NAV can appear in, at the same size, while
// the instrument they sat under was 48 px smaller than it needed to be. There is no
// always-available number NAV can show that AIRCRAFT does not: the target group in the
// right column is the whole of what this screen knows and that panel does not.

// ── State ─────────────────────────────────────────────────────────────────────────────
static CompassCache       _navCard;
static CompassMarkerCache _navTrkMk;    // ground-track marker
static CompassMarkerCache _navTgtMk;    // target-bearing marker
static int16_t _navPrevHdg      = -9999;
static int16_t _navPrevTrk      = -9999;
static int16_t _navPrevDrift    = -9999;
static int32_t _navPrevDist     = -1;
static bool    _navPrevDistAvail = false;
static int16_t _navPrevClose    = -9999;
static int32_t _navPrevTInt     = -1;     // whole seconds; -1 = not closing / no target

// ── Ground track ──────────────────────────────────────────────────────────────────────
// Where the vessel is actually moving, as opposed to where it is pointed. Only
// meaningful once there is enough surface speed for the velocity vector's heading to be
// stable; below that KSP's reported heading wanders and the marker would spin.
static const float NAV_TRK_MIN_MS = 5.0f;

static inline bool _navHasTrack() { return state.surfaceVel >= NAV_TRK_MIN_MS; }


/***************************************************************************************
   CHROME — drawn once on screen entry
****************************************************************************************/
void chromeScreen_NAV(KCM_TFT &tft) {
  compassDrawRing(tft, NAV_GEOM);
  compassDrawNose(tft, NAV_GEOM);

  // Heading box: border is chrome, the value redraws on change.
  tft.drawRect(NAV_HDG_BOX_X, NAV_HDG_BOX_Y, NAV_HDG_BOX_W, NAV_HDG_BOX_H, TFT_GREY);

  // Column labels.
  textCenter(tft, &Roboto_Black_24, NAV_LCOL_X, NAV_COL_Y, NAV_COL_W, NAV_LBL_H,
             "TRK", TFT_WHITE, TFT_BLACK);
  textCenter(tft, &Roboto_Black_24, NAV_LCOL_X, NAV_COL_Y + NAV_BLOCK_H, NAV_COL_W, NAV_LBL_H,
             "DRIFT", TFT_WHITE, TFT_BLACK);
  textCenter(tft, &Roboto_Black_24, NAV_RCOL_X, NAV_COL_Y, NAV_COL_W, NAV_LBL_H,
             "DIST", TFT_WHITE, TFT_BLACK);
  textCenter(tft, &Roboto_Black_24, NAV_RCOL_X, NAV_COL_Y + NAV_BLOCK_H, NAV_COL_W, NAV_LBL_H,
             "V.CLS", TFT_WHITE, TFT_BLACK);
  // Third block completes the target group: how far, how fast, how long.
  textCenter(tft, &Roboto_Black_24, NAV_RCOL_X, NAV_COL_Y + 2 * NAV_BLOCK_H, NAV_COL_W, NAV_LBL_H,
             "T+INT", TFT_WHITE, TFT_BLACK);

  // Force every value to repaint on the first frame after entry.
  _navCard = CompassCache();
  _navTrkMk = CompassMarkerCache();
  _navTgtMk = CompassMarkerCache();
  _navPrevHdg = _navPrevTrk = _navPrevDrift = -9999;
  _navPrevClose = -9999;
  _navPrevDist = -1;
  _navPrevTInt = -1;
  _navPrevDistAvail = false;
}


/***************************************************************************************
   PER-FRAME UPDATE
****************************************************************************************/
void drawScreen_NAV(KCM_TFT &tft) {
  const float hdg = state.heading;

  // ── Compass card ───────────────────────────────────────────────────────────────────
  // The card rotation erases and redraws ticks and labels, which pass through the band
  // the markers occupy, so both markers are re-stamped whenever the card moves.
  bool cardMoved = compassUpdateCard(tft, NAV_GEOM, _navCard, hdg, NAV_HDG_THRESH_DEG);
  if (cardMoved) {
    _navTrkMk.prevAvail = false;
    _navTgtMk.prevAvail = false;
  }

  // ── Ground-track marker ────────────────────────────────────────────────────────────
  const bool  hasTrk = _navHasTrack();
  const float trkScreenDeg = hasTrk ? eadiHdgDelta(state.srfVelHeading, hdg) : 0.0f;
  compassUpdateMarker(tft, NAV_GEOM, _navTrkMk, hasTrk, trkScreenDeg,
                      TFT_NEON_GREEN, NAV_MK_THRESH_DEG);

  // ── Target-bearing marker ──────────────────────────────────────────────────────────
  const bool  hasTgt = state.targetAvailable;
  const float tgtScreenDeg = hasTgt ? eadiHdgDelta(state.tgtHeading, hdg) : 0.0f;
  compassUpdateMarker(tft, NAV_GEOM, _navTgtMk, hasTgt, tgtScreenDeg,
                      TFT_VIOLET, NAV_MK_THRESH_DEG);

  // ── Heading readout ────────────────────────────────────────────────────────────────
  {
    int16_t h = (int16_t)lroundf(hdg) % 360;
    if (h < 0) h += 360;
    if (h != _navPrevHdg) {
      _navPrevHdg = h;
      char buf[8];
      snprintf(buf, sizeof(buf), "%03d\xB0", h);
      tft.fillRect(NAV_HDG_BOX_X + 2, NAV_HDG_BOX_Y + 2,
                   NAV_HDG_BOX_W - 4, NAV_HDG_BOX_H - 4, TFT_BLACK);
      textCenter(tft, &Roboto_Black_36, NAV_HDG_BOX_X, NAV_HDG_BOX_Y,
                 NAV_HDG_BOX_W, NAV_HDG_BOX_H, buf, TFT_WHITE, TFT_BLACK);
    }
  }

  // ── TRK — ground track, or dashes when too slow to be meaningful ───────────────────
  {
    int16_t t = -1;
    if (hasTrk) {
      t = (int16_t)lroundf(state.srfVelHeading) % 360;
      if (t < 0) t += 360;
    }
    if (t != _navPrevTrk) {
      _navPrevTrk = t;
      char buf[8];
      if (t < 0) snprintf(buf, sizeof(buf), "---");
      else       snprintf(buf, sizeof(buf), "%03d\xB0", t);
      tft.fillRect(NAV_LCOL_X, NAV_COL_Y + NAV_LBL_H, NAV_COL_W, NAV_VAL_H, TFT_BLACK);
      textCenter(tft, &Roboto_Black_36, NAV_LCOL_X, NAV_COL_Y + NAV_LBL_H,
                 NAV_COL_W, NAV_VAL_H, buf,
                 (t < 0) ? TFT_DARK_GREY : TFT_NEON_GREEN, TFT_BLACK);
    }
  }

  // ── DRIFT — track minus heading, the crab angle ────────────────────────────────────
  // Yellow past NAV_DRIFT_WARN_DEG: in an atmosphere a large crab angle is either a
  // strong crosswind-equivalent or a vessel that is not going where it is pointed.
  {
    int16_t d = -9999;
    if (hasTrk) d = (int16_t)lroundf(eadiHdgDelta(state.srfVelHeading, hdg));
    if (d != _navPrevDrift) {
      _navPrevDrift = d;
      char buf[10];
      if (d == -9999) snprintf(buf, sizeof(buf), "---");
      else            snprintf(buf, sizeof(buf), "%+d\xB0", d);
      uint16_t fg = (d == -9999)             ? TFT_DARK_GREY
                  : (abs(d) >= NAV_DRIFT_WARN_DEG) ? TFT_YELLOW
                                             : TFT_DARK_GREEN;
      tft.fillRect(NAV_LCOL_X, NAV_COL_Y + NAV_BLOCK_H + NAV_LBL_H,
                   NAV_COL_W, NAV_VAL_H, TFT_BLACK);
      textCenter(tft, &Roboto_Black_36, NAV_LCOL_X, NAV_COL_Y + NAV_BLOCK_H + NAV_LBL_H,
                 NAV_COL_W, NAV_VAL_H, buf, fg, TFT_BLACK);
    }
  }

  // ── DIST — range to target ─────────────────────────────────────────────────────────
  {
    int32_t dist = hasTgt ? (int32_t)lroundf(state.tgtDistance) : -1;
    if (dist != _navPrevDist || hasTgt != _navPrevDistAvail) {
      _navPrevDist = dist;
      _navPrevDistAvail = hasTgt;
      tft.fillRect(NAV_RCOL_X, NAV_COL_Y + NAV_LBL_H, NAV_COL_W, NAV_VAL_H, TFT_BLACK);
      textCenter(tft, &Roboto_Black_36, NAV_RCOL_X, NAV_COL_Y + NAV_LBL_H,
                 NAV_COL_W, NAV_VAL_H,
                 hasTgt ? formatAlt(state.tgtDistance) : String("---"),
                 hasTgt ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);
    }
  }

  // ── V.CLS — closure rate, negative closing (same sign convention as TGT/DOCK) ──────
  {
    int16_t c = hasTgt ? (int16_t)lroundf(state.tgtVelocity) : -9999;
    if (c != _navPrevClose) {
      _navPrevClose = c;
      tft.fillRect(NAV_RCOL_X, NAV_COL_Y + NAV_BLOCK_H + NAV_LBL_H,
                   NAV_COL_W, NAV_VAL_H, TFT_BLACK);
      textCenter(tft, &Roboto_Black_36, NAV_RCOL_X, NAV_COL_Y + NAV_BLOCK_H + NAV_LBL_H,
                 NAV_COL_W, NAV_VAL_H,
                 hasTgt ? fmtMs(state.tgtVelocity) : String("---"),
                 hasTgt ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);
    }
  }

  // ── T+INT — time to intercept, distance over closure rate ──────────────────────────
  // The same quantity, the same formula and the same label as TARGET's T+Int row: only
  // meaningful while actually closing, dashed otherwise. DIST and V.CLS were already
  // here and neither answers "how long"; on an approach that is the number being flown
  // to, and it is nowhere on this panel's partner.
  {
    const bool closing = hasTgt && (state.tgtVelocity < -0.5f);
    int32_t t = closing ? (int32_t)(state.tgtDistance / fabsf(state.tgtVelocity)) : -1;
    if (t != _navPrevTInt) {
      _navPrevTInt = t;
      const int16_t y = NAV_COL_Y + 2 * NAV_BLOCK_H + NAV_LBL_H;
      tft.fillRect(NAV_RCOL_X, y, NAV_COL_W, NAV_VAL_H, TFT_BLACK);
      textCenter(tft, &Roboto_Black_36, NAV_RCOL_X, y, NAV_COL_W, NAV_VAL_H,
                 closing ? formatTimeCompact((float)t) : String("---"),
                 closing ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);
    }
  }

}

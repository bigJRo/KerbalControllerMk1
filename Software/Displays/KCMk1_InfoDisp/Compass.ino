/***************************************************************************************
   Compass.ino -- Shared compass-rose renderer (ROVER and NAV)

   A rotating compass card: outer ring, 5° ticks with 30° majors, cardinal letters and
   numeric labels, a fixed nose triangle at 12 o'clock, and bearing markers placed by
   world heading. The card rotates under the nose, so the vessel's own heading is
   always straight up.

   Extracted verbatim from Screen_ROVR.ino when the NAV screen was added, rather than
   copied: this project has consolidated the reticle layer (MNVR/TGT/DOCK) and the EADI
   tape (SCFT/ACFT) the same way, and a second hand-maintained compass would be the
   third place to fix any bug found in the first.

   The split follows the same rule those two use. Everything here is geometry and
   pixels — it reads no telemetry and owns no state. The caller supplies a CompassGeom
   (its own radii) and a CompassCache (its own prev-drawn values, so the redraw gating
   stays per-screen), and decides what a marker means.

   Screen angle convention: measured from 12 o'clock, increasing clockwise. A world
   bearing appears on the card at (worldDeg - headingDeg), so with the card rotated to
   the vessel's heading, north sits at its true relative bearing.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


/***************************************************************************************
   POLAR -> SCREEN
   x = cx + r*sin(screenDeg), y = cy - r*cos(screenDeg): clockwise from 12 o'clock,
   with y inverted for screen coordinates.
****************************************************************************************/
void compassPolar(const CompassGeom &g, float screenDeg, int16_t r, int16_t &x, int16_t &y) {
  float rad = screenDeg * (float)DEG_TO_RAD;
  x = (int16_t)((float)g.cx + (float)r * sinf(rad));
  y = (int16_t)((float)g.cy - (float)r * cosf(rad));
}


/***************************************************************************************
   OUTER RING
   Stationary chrome. Ticks stop short of it (rTickOuter < rRing) so a tick erase can
   never reach the ring, which is what lets the ring be drawn once and left alone.

   Light grey, one step brighter than the minor ticks it encloses -- ROVER's shipped
   value, inherited here so the two navigation screens read the same.
****************************************************************************************/
void compassDrawRing(KCM_TFT &tft, const CompassGeom &g) {
  tft.drawCircle(g.cx, g.cy, g.rRing, TFT_LIGHT_GREY);
}


/***************************************************************************************
   TICKS — every 5°, with a longer lighter tick every 30°
****************************************************************************************/
void compassDrawTicks(KCM_TFT &tft, const CompassGeom &g, float headingDeg, bool erase) {
  for (int16_t worldDeg = 0; worldDeg < 360; worldDeg += 5) {
    float screenDeg = (float)worldDeg - headingDeg;
    int16_t x0, y0, x1, y1;
    if (worldDeg % 30 == 0) {
      compassPolar(g, screenDeg, g.rTickOuter,      x1, y1);
      compassPolar(g, screenDeg, g.rTickMajorInner, x0, y0);
      tft.drawLine(x0, y0, x1, y1, erase ? TFT_BLACK : TFT_LIGHT_GREY);
    } else {
      compassPolar(g, screenDeg, g.rTickOuter,      x1, y1);
      compassPolar(g, screenDeg, g.rTickMinorInner, x0, y0);
      tft.drawLine(x0, y0, x1, y1, erase ? TFT_BLACK : TFT_GREY);
    }
  }
}


/***************************************************************************************
   LABELS — N/E/S/W at the cardinals, two-digit tens elsewhere
   Erase fills each glyph box with a hardware fillRect rather than re-rendering the
   glyph in black: text rasterisation is the most expensive software primitive in this
   driver, and the fill covers the same box for a fraction of the cost.
****************************************************************************************/
void compassDrawLabels(KCM_TFT &tft, const CompassGeom &g, float headingDeg, bool erase) {
  tft.setFont(Roboto_Black_28);
  const int16_t capH = (int16_t)Roboto_Black_28.cap_height;

  struct LabelSpec { int16_t worldDeg; const char *text; uint16_t color; bool letter; };
  static const LabelSpec labels[] = {
    {   0, "N",  TFT_YELLOW,     true  },
    {  30, "03", TFT_LIGHT_GREY, false },
    {  60, "06", TFT_LIGHT_GREY, false },
    {  90, "E",  TFT_WHITE,      true  },
    { 120, "12", TFT_LIGHT_GREY, false },
    { 150, "15", TFT_LIGHT_GREY, false },
    { 180, "S",  TFT_WHITE,      true  },
    { 210, "21", TFT_LIGHT_GREY, false },
    { 240, "24", TFT_LIGHT_GREY, false },
    { 270, "W",  TFT_WHITE,      true  },
    { 300, "30", TFT_LIGHT_GREY, false },
    { 330, "33", TFT_LIGHT_GREY, false },
  };

  for (uint8_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
    float screenDeg = (float)labels[i].worldDeg - headingDeg;
    int16_t x, y;
    compassPolar(g, screenDeg, labels[i].letter ? g.rLetter : g.rNumLabel, x, y);

    // Roboto_Black_28 metrics (empirical): glyph ~16 px wide, ~24 px tall.
    uint8_t textLen = strlen(labels[i].text);
    int16_t cursorX = x - (int16_t)(textLen * 16) / 2;
    int16_t cursorY = y - 24 / 2;

    if (erase) {
      int16_t realW = getFontStringWidth(&Roboto_Black_28, labels[i].text);
      tft.fillRect(cursorX - 1, cursorY, realW + 2, capH, TFT_BLACK);
    } else {
      tft.setTextColor(labels[i].color, TFT_BLACK);
      tft.setCursor(cursorX, cursorY);
      tft.print(labels[i].text);
    }
  }
}


/***************************************************************************************
   NOSE — fixed triangle outside the ring at 12 o'clock, pointing inward.
   Stationary: the card rotates beneath it, so it always marks vessel heading.
****************************************************************************************/
void compassDrawNose(KCM_TFT &tft, const CompassGeom &g) {
  int16_t tipX, tipY, blX, blY, brX, brY;
  compassPolar(g, 0.0f, g.noseRTip, tipX, tipY);
  float angOffset = (float)g.noseHalfW / (float)g.noseRBase * (180.0f / (float)PI);
  compassPolar(g, -angOffset, g.noseRBase, blX, blY);
  compassPolar(g,  angOffset, g.noseRBase, brX, brY);
  tft.fillTriangle(tipX, tipY, blX, blY, brX, brY, TFT_WHITE);
}


/***************************************************************************************
   BEARING MARKER — triangle inside the ring at a given screen bearing, tip outward.
   What it means is the caller's business: ROVER uses one for the target, NAV uses two
   (target and ground track).

   Erase fills the triangle's bounding rectangle rather than drawing a black triangle.
   A triangle-based erase left trails at certain angles, because the rasteriser's edge
   rules differ between the drawn and erased passes; a padded bounding rect cannot.
****************************************************************************************/
void compassDrawMarker(KCM_TFT &tft, const CompassGeom &g, float screenDeg,
                       uint16_t colour, bool erase) {
  int16_t tipX, tipY, blX, blY, brX, brY;
  compassPolar(g, screenDeg, g.mkRTip, tipX, tipY);
  float angOffset = (float)g.mkHalfW / (float)g.mkRBase * (180.0f / (float)PI);
  compassPolar(g, screenDeg - angOffset, g.mkRBase, blX, blY);
  compassPolar(g, screenDeg + angOffset, g.mkRBase, brX, brY);

  if (erase) {
    int16_t xMin = tipX, xMax = tipX, yMin = tipY, yMax = tipY;
    if (blX < xMin) xMin = blX;
    if (brX < xMin) xMin = brX;
    if (blX > xMax) xMax = blX;
    if (brX > xMax) xMax = brX;
    if (blY < yMin) yMin = blY;
    if (brY < yMin) yMin = brY;
    if (blY > yMax) yMax = blY;
    if (brY > yMax) yMax = brY;
    const int16_t pad = 3;
    tft.fillRect(xMin - pad, yMin - pad,
                 (xMax - xMin) + 2 * pad + 1, (yMax - yMin) + 2 * pad + 1, TFT_BLACK);
  } else {
    tft.fillTriangle(tipX, tipY, blX, blY, brX, brY, colour);
  }
}


/***************************************************************************************
   CARD UPDATE — rotate the ticks and labels when the heading has moved enough.
   Erase at the old heading, draw at the new one. The ring and nose are stationary and
   are not touched. Returns true if the card was redrawn, which the caller may need in
   order to re-stamp anything that overlaps.
****************************************************************************************/
bool compassUpdateCard(KCM_TFT &tft, const CompassGeom &g, CompassCache &c,
                       float headingDeg, float threshDeg) {
  if (c.prevHeading > -9000.0f &&
      fabsf(eadiHdgDelta(headingDeg, c.prevHeading)) < threshDeg) return false;

  if (c.prevHeading > -9000.0f) {
    compassDrawTicks(tft, g, c.prevHeading, true);
    compassDrawLabels(tft, g, c.prevHeading, true);
  }
  compassDrawTicks(tft, g, headingDeg, false);
  compassDrawLabels(tft, g, headingDeg, false);
  c.prevHeading = headingDeg;
  return true;
}


/***************************************************************************************
   MARKER UPDATE — erase the old marker and draw the new one when it has moved or its
   availability has changed. Screen bearing is the world bearing minus vessel heading,
   wrapped to +/-180.
****************************************************************************************/
/***************************************************************************************
   MARKER PAIR UPDATE — two markers that share one annulus.

   compassUpdateMarker() is correct on its own but not in company: each marker's erase is
   a bounding box a few pixels larger than the triangle, and two markers on the same ring
   pass through each other as the card turns. Whichever moves erases the other and does
   not put it back, so on NAVIGATION the moving target-bearing marker would sweep the
   ring once and take the stationary ground-track marker with it.

   The fix is the one the ascent ladder's two altitude markers use: when either has
   changed, erase both, then draw both. A proximity test asking "are they close enough to
   interfere?" is the version that leaves half a marker on the screen, because the answer
   depends on the erase pad, the triangle's bounding box and the angle between them.
   Two triangles per frame cost nothing.
****************************************************************************************/
bool compassUpdateMarkerPair(KCM_TFT &tft, const CompassGeom &g,
                             CompassMarkerCache &ca, bool availA, float degA, uint16_t colA,
                             CompassMarkerCache &cb, bool availB, float degB, uint16_t colB,
                             float threshDeg) {
  const bool chgA = (availA != ca.prevAvail) ||
                    (availA && fabsf(eadiHdgDelta(degA, ca.prevScreenDeg)) >= threshDeg);
  const bool chgB = (availB != cb.prevAvail) ||
                    (availB && fabsf(eadiHdgDelta(degB, cb.prevScreenDeg)) >= threshDeg);
  if (!chgA && !chgB) return false;

  if (ca.prevAvail) compassDrawMarker(tft, g, ca.prevScreenDeg, colA, true);
  if (cb.prevAvail) compassDrawMarker(tft, g, cb.prevScreenDeg, colB, true);

  if (availA) { compassDrawMarker(tft, g, degA, colA, false); ca.prevScreenDeg = degA; }
  if (availB) { compassDrawMarker(tft, g, degB, colB, false); cb.prevScreenDeg = degB; }
  ca.prevAvail = availA;
  cb.prevAvail = availB;
  // The caller may have its own artwork attached to one of these markers -- NAVIGATION's
  // track stalk runs into the ground-track triangle's base -- and a marker's erase box is
  // padded 3 px beyond the triangle, so a redraw here can take a bite out of it. Say so,
  // rather than leaving the caller to guess from its own change test.
  return true;
}


void compassUpdateMarker(KCM_TFT &tft, const CompassGeom &g, CompassMarkerCache &c,
                         bool available, float screenDeg, uint16_t colour,
                         float threshDeg) {
  bool moved = (c.prevAvail && fabsf(eadiHdgDelta(screenDeg, c.prevScreenDeg)) >= threshDeg);

  if (c.prevAvail && (!available || moved))
    compassDrawMarker(tft, g, c.prevScreenDeg, colour, true);

  if (available && (!c.prevAvail || moved)) {
    compassDrawMarker(tft, g, screenDeg, colour, false);
    c.prevScreenDeg = screenDeg;
  }
  c.prevAvail = available;
}

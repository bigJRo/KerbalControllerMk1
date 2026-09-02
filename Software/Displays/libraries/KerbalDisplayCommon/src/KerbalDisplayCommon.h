#ifndef KERBAL_DISPLAY_COMMON_H
#define KERBAL_DISPLAY_COMMON_H

#define KDC_VERSION_MAJOR 3
#define KDC_VERSION_MINOR 8
#define KDC_VERSION_PATCH 0

/***************************************************************************************
   KerbalDisplayCommon Library
   A UI toolkit for the RA8876-based 7" touchscreen displays (hardware rev 2) used
   in Kerbal Controller Mk1. Provides button drawing, text rendering, value
   formatting, and threshold coloring.

   v3.8.0 — three palette entries for the ResourceDisp meter fills, each filling a gap
            the existing palette could not: TFT_BRICK, a rust red for Solid Fuel that
            stays clearly off the TFT_RED alarm colour beside a red limit band;
            TFT_PLUM, a muted magenta for CO2 that is neither a red (it alerts HIGH
            against a red band) nor already used in the life-support family; and
            TFT_STRAW, a pale straw yellow for Liquid Waste that reads as what it is,
            distinct from the TFT_GOLD used for Electric Charge.

   v3.7.2 — formatSep/formatSepI64 fill their buffer backwards, one digit at a time. The
            previous version built the string front-to-back with a sprintf + strcpy of
            the whole accumulated result per three-digit group -- quadratic in the digit
            count, two 64-byte buffers, and a printf call per group -- on what is the
            hottest formatting path on the panel, since every formatAlt() on every
            visible distance readout lands there every frame. ~7x faster on the same
            workload with byte-identical output over 40,269 differential test values.
            Also removes the INT64_MIN caveat: the old `value = -value` was undefined
            there and was documented as unreachable rather than fixed; negating into an
            unsigned accumulator is well defined over the whole range.

   v3.7.1 — glyph data moved out of DTCM into flash. On Teensy 4 a plain `const` array
            lands in .data, which shares the 512 KB of FlexRAM with ITCM; seventeen fonts
            is ~189 KB of it, and a build measured .data at 195,264 against a DTCM
            allowance of 196,608 -- nothing left for the stack, and teensy_size failing
            outright rather than reporting it. A KCM_FONT_FLASH section attribute on each
            font's _data[] and _index[] moves them to .progmem. Flash is memory-mapped on
            this part, so the renderer is unchanged; the macro is a no-op off-target so
            the host harnesses still build the same files.

   v3.7.0 — kcmRateUpdate/kcmRateReset: body angular rates for the Shuttle-style rate
            pointers, derived from the relative rotation of two successive kspBodyAxes
            frames rather than by differentiating the Euler angles, so they stay well
            conditioned at the +/-90 deg pitch singularity a launch sits in. Also
            promotes drawRoundRectOutline in from KCMk1_InfoDisp, as its own note there
            anticipated, now that the reference chips are its second and third callers.

   v3.6.0 — U+00B7 MIDDLE DOT added to every Roboto Black size.

   v3.5.0 — off-scale markers are now visibly pinned. reticleClampDot returns whether it
            clamped, ReticleDotCache carries primaryPinned/velPinned, and reticleUpdateDots
            draws a clamped marker in a new half-brightness shade (TFT_DIM_VIOLET /
            TFT_DIM_NEON_GRN) so it cannot be read as a live value. reticleEraseDot gained
            a `restyled` argument: the transition can move the marker as little as 1 px,
            which the >1 px movement gate would otherwise swallow, leaving a bright marker
            drawn while clamped.

   v3.4.0 — ReticleAngles::appBrg/appElv become appRight/appUp: the relative velocity
            decomposed about the TARGET axis rather than horizon-frame heading/pitch
            differences, so the approach-path readouts are the exact 3D angle and agree
            with the on-screen marker gap. (3.4.0 briefly named an unrelated marker glyph
            that was prototyped and reverted before release; that version never left the
            branch, so the number is reused rather than skipped.)

   v3.3.1 — documented the project-wide rule that every boresight display builds its
            axes with the vessel roll; the horizon-referenced path (rollDeg = 0) now has
            no caller but is kept for a possible world-referenced scope.

   v3.3.0 — true boresight projection. kspCockpitOffset (flat heading/pitch offsets
            rotated by roll) is REPLACED by kspBodyAxes + kspBoresightAngles, which
            resolve the vessel attitude into 3D body axes and project a world
            direction azimuthal-equidistant about the boresight. The displayed radius
            is now the TRUE angular separation at any attitude; the old scheme
            stretched the bearing axis by ~1/cos(pitch) and inverted past ~80 deg.
            Roll is carried by the axes, so ReticleGeom::rollRef is gone — a caller
            picks the frame by passing roll (body-referenced) or 0 (horizon) when it
            builds the axes. ReticleAngles now carries right/up pairs rather than
            bearing/elevation. Superseding 3.2.0's kspCockpitOffset outright is safe:
            it existed only on this branch and had no other consumers.

   v3.2.0 — the shared reticle marker layer moved in from KCMk1_InfoDisp: ReticleGeom /
            ReticleDotCache / ReticleAngles plus reticleProject / reticleClampDot /
            reticleEraseDot / reticleRepairDotChrome / reticleUpdateDots now sit beside
            the reticleDrawBase + reticleRepair chrome they already called, so MNVR /
            DOCK / TGT run one implementation instead of three. Added kspCockpitOffset,
            the SINGLE definition of roll handedness for every body-referenced display
            (EADI ball markers, the DOCK reticle, the re-entry retro ball), and
            eadiHdgDelta (heading wrap), previously sketch-local.

   v3.1.1 — marker polish: drawThickLine gained a caps arg so free-ended spokes
            draw without round end-caps; shrank the level-indicator nose dot.
   v3.1.0 — added the full KSP navball marker set: retrograde, normal, anti-normal,
   radial-in, radial-out, anti-target and the level indicator draw functions (joining
   prograde/target/maneuver), all selectable via the extended KspMarkerKind enum.

   v3.0.0 — hardware rev 2 migration: display type RA8875 -> KCM_TFT (RA8876_t41_p
   via KCM_Display), fonts sumotoy tFont -> ILI9341_t3 (fonts_ili/), BMP blit via
   writeRect(), SD via Teensy 4.1 BUILTIN_SDCARD, touch moved out to KCM_Touch.

   Dependencies (install on the build machine — see PORTING_7inch_TFT.md):
    - wwatson4506/TeensyRA8876-8080 (RA8876_t41_p) + TeensyRA8876-GFX-Common
    - PaulStoffregen/ILI9341_fonts (ILI9341_t3 font format)
    - KCM_Display, KCMk1_SystemConfig, KCM_Touch (this repo)

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
  Version: 3.8.0
****************************************************************************************/
#include <Arduino.h>
#include <SD.h>
#include <Wire.h>
#include <cfloat>   // DBL_MAX -- used in BodyParams soiAlt for Kerbol sentinel
#include <KCM_Display.h>                  // KCM_TFT (RA8876_t41_p) + pins/resolution
#include "fonts_ili/kcm_ili9341_font.h"   // ILI9341_t3_font_t (used in signatures)

/***************************************************************************************
   DEFINES
   Display pins (CS/RS/RESET), resolution, and the SD chip select (BUILTIN_SDCARD)
   now come from KCMk1_SystemConfig.h (pulled in via KCM_Display.h). The RA8875_*
   and SD_CS_PIN defines from the rev-1 SPI stack have been removed.
****************************************************************************************/

// Font includes -- all sizes, ILI9341_t3 format, located in src/fonts_ili/
#include "fonts_ili/Roboto_Black_12.c"
#include "fonts_ili/Roboto_Black_16.c"
#include "fonts_ili/Roboto_Black_20.c"
#include "fonts_ili/Roboto_Black_24.c"
#include "fonts_ili/Roboto_Black_28.c"
#include "fonts_ili/Roboto_Black_32.c"
#include "fonts_ili/Roboto_Black_36.c"
#include "fonts_ili/Roboto_Black_40.c"
#include "fonts_ili/Roboto_Black_48.c"
#include "fonts_ili/Roboto_Black_72.c"
// KcmTerm -- monospace terminal font, glyph bitmaps from Terminus Font 4.49
// (SIL OFL 1.1). True bitmaps, so every native size (16/20/24/28/32) is pixel-exact;
// 36/40 are 2x doubles of the 18/20 strikes. Replaces the old IBM VGA TerminalFont.
// See fonts_ili/OFL.txt and fonts_ili/README.md.
#include "fonts_ili/KcmTerm_16.c"
#include "fonts_ili/KcmTerm_20.c"
#include "fonts_ili/KcmTerm_24.c"
#include "fonts_ili/KcmTerm_28.c"
#include "fonts_ili/KcmTerm_32.c"
#include "fonts_ili/KcmTerm_36.c"   // 2x native 18px strike
#include "fonts_ili/KcmTerm_40.c"   // 2x native 20px strike — clean heading size


/***************************************************************************************
   NO_BORDER SENTINEL
   Use as borderColor parameter in any function to skip drawing a border.
   Value 0x0001 is not used as a real UI color (near-black, not in color palette).
****************************************************************************************/
#define NO_BORDER 0x0001


/***************************************************************************************
   COLOR DEFINITIONS - RGB565 format
   Bits 0..4   -> Blue  0..4
   Bits 5..10  -> Green 0..5
   Bits 11..15 -> Red   0..4
   Ref: https://rgbcolorpicker.com/565
   Ref: https://github.com/newdigate/rgb565_colors
****************************************************************************************/
// Screen dimensions — used internally by drawBMP/drawButton active-window restore.
// KCMk1_SystemConfig.h defines the same values; these guards prevent redefinition.
// Defined in KCMk1_SystemConfig.h (1024x600). Guards kept as a documentation
// fallback only — the real values arrive via KCM_Display.h above.
#ifndef KCM_SCREEN_W
#define KCM_SCREEN_W 1024
#endif
#ifndef KCM_SCREEN_H
#define KCM_SCREEN_H 600
#endif

#define TFT_BLACK        0x0000  /*   0,   0,   0 */
#define TFT_OFF_BLACK    0x2104  /*   4,   8,   4 */
#define TFT_DARK_GREY    0x39E7  /*   7,  15,   7 */
#define TFT_GREY         0x8410  /*  16,  16,  16 */
#define TFT_LIGHT_GREY   0xBDF7  /*  23,  47,  23 */
#define TFT_WHITE        0xFFFF  /*  31,  63,  31 */
#define TFT_GREEN        0x07E0  /*   0,  63,   0 */
#define TFT_DARK_GREEN   0x03E0  /*   0,  31,   0 */
#define TFT_JUNGLE       0x01E0  /*   0,  15,   0 */
#define TFT_RED          0xF800  /*  31,   0,   0 */
#define TFT_MAROON       0x7800  /*  15,   0,   0 */
#define TFT_CORNELL      0xB0E3  /*  22,   7,   3 */
#define TFT_DARK_RED     0x6000  /*  12,   0,   0 */
#define TFT_BLUE         0x001F  /*   0,   0,  31 */
#define TFT_SKY          0x761F  /*  14,  48,  31 */
#define TFT_ROYAL        0x010C  /*   0,   8,  12 */
#define TFT_AQUA         0x5D1C  /*  11,  40,  28 */
#define TFT_NAVY         0x000F  /*   0,   0,  15 */
#define TFT_CYAN         0x07FF  /*   0,  63,  31 */
#define TFT_FRENCH_BLUE  0x347C  /*   6,  35,  28 */
#define TFT_MAGENTA      0xF81F  /*  31,   0,  31 */
#define TFT_PURPLE       0x8010  /*  16,   0,  16 */
#define TFT_VIOLET       0x901A  /*  18,   0,  26 */
#define TFT_DIM_VIOLET   0x480D  /*   9,   0,  13 -- half TFT_VIOLET */
#define TFT_YELLOW       0xFDC2  /*  31,  46,   2 */
#define TFT_DULL_YELLOW  0xEEEB  /*  29,  55,  11 */
#define TFT_DARK_YELLOW  0xA500  /*  20,  40,   0 */
#define TFT_OLIVE        0x8400  /*  16,  32,   0 */
#define TFT_BROWN        0x8200  /*  16,  16,  40 */
#define TFT_SILVER       0xC618  /*  24,  48,  24 */
#define TFT_GOLD         0xD566  /*  26,  43,   6 */
#define TFT_ORANGE       0xFBE0  /*  31,  31,   0 */
#define TFT_AIR_SUP_BLUE 0x7517  /*  14,  40,  23 */
#define TFT_NEON_GREEN   0x3FE2  /*   7,  63,   2 */
#define TFT_DIM_NEON_GRN 0x1BE1  /*   3,  31,   1 -- half TFT_NEON_GREEN */
#define TFT_SAP_GREEN    0x53E5  /*  10,  31,   5 */
#define TFT_INT_ORANGE   0xFA80  /*  31,  20,   0 */
#define TFT_UPS_BROWN    0x6203  /*  12,  16,   3 */
#define TFT_MINT         0xA6F6  /*  20,  55,  22 */
#define TFT_MED_GREEN    0x0507  /*   0,  40,   7 */
#define TFT_TAN          0xB46A  /*  22,  35,  10 */
#define TFT_ROSE         0xF3CF  /*  30,  30,  15 */
#define TFT_CRIMSON      0xD8A7  /*  27,   5,   7 */
#define TFT_OCEAN        0x01F1  /*   0,  15,  17 */
#define TFT_BRICK        0xC285  /*  24,  20,   5 -- rust red, off the TFT_RED alarm colour */
#define TFT_PLUM         0x91F0  /*  18,  15,  16 -- muted magenta */
#define TFT_STRAW        0xEE2E  /*  29,  49,  14 -- pale straw yellow */


/***************************************************************************************
   BUTTON LABEL STRUCT
   Defines text and colors for a button in both off and on states.
   Note: Δ is stored at 0x94 in Roboto_Black fonts (non-standard encoding) — works
   correctly with these fonts but may break if fonts are regenerated.
****************************************************************************************/
struct ButtonLabel {
  const char *text;
  uint16_t fontColorOff;
  uint16_t fontColorOn;
  uint16_t backgroundColorOff;
  uint16_t backgroundColorOn;
  uint16_t borderColorOff;   // use NO_BORDER to skip border
  uint16_t borderColorOn;    // use NO_BORDER to skip border
};


/***************************************************************************************
   TEXT PRIMITIVE CONSTANTS
****************************************************************************************/
extern const byte TEXT_BORDER;  // horizontal padding from edge, default 8


/***************************************************************************************
   FUNCTION DECLARATIONS
****************************************************************************************/

// --- Setup helper ---
void setupDisplay(KCM_TFT &tft, uint16_t backColor);

// --- Font measurement ---
int16_t getFontCharWidth(const ILI9341_t3_font_t *font, char c);
int16_t getFontStringWidth(const ILI9341_t3_font_t *font, const char *str);

// --- Button ---
void drawButton(KCM_TFT &tft, int16_t x, int16_t y, int16_t w, int16_t h,
                const ButtonLabel &label, const ILI9341_t3_font_t *font, bool isOn);

// --- Text primitives ---
//
// (#A12) Encoding caveat: the text-rendering family (textLeft, textRight,
// textCenter, printDisp, printValue, printName, printTitle) operates on
// single bytes — there is no UTF-8 decoding. Each byte in the input string
// is looked up directly in the font's character table. This has two
// implications:
//   1. UTF-8 multibyte sequences (e.g. 'é' = 0xC3 0xA9) are NOT decoded;
//      each byte is treated as a separate character lookup. Such inputs
//      will render as two unrelated glyphs (or zero-width glyphs per #A4)
//      rather than the intended character.
//   2. Byte-length truncation in printName() can split a multibyte sequence,
//      leaving an orphaned UTF-8 continuation byte at the end.
// Sketches receiving UTF-8 input (e.g. KSP vessel names) should pre-filter
// non-ASCII bytes or transliterate before passing the string to these
// functions. The Roboto_Black fonts include a few non-ASCII glyphs at custom
// single-byte code points (e.g. Δ at 0x94) — those work only when the caller
// passes the single byte directly, not as the corresponding UTF-8 sequence.

void textLeft(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
              const String &value, uint16_t foreColor, uint16_t backColor);
void textRight(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               const String &value, uint16_t foreColor, uint16_t backColor);
void textCenter(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                const String &value, uint16_t foreColor, uint16_t backColor);

// Erase a previously-drawn textCenter value with a hardware fillRect over exactly
// the box textCenter would have drawn into (same centring math). Behaviourally
// identical to re-rendering the old string black-on-black, but avoids an expensive
// software glyph raster each change. `bg` is the fill colour (usually TFT_BLACK,
// or the previous background for threshold cells).
void eraseCenteredValue(KCM_TFT &tft, const ILI9341_t3_font_t *font,
                        int16_t x0, int16_t y0, int16_t w, int16_t h,
                        const char *oldStr, uint16_t bg);

// Solid diamond marker, centred at (cx,cy), `half` px from centre to each tip.
// Drawn as two triangles that share the horizontal waist scanline, so the seam has
// no raster gaps. General utility — the reticle screens now use drawProgradeMarker()
// (velocity/maneuver) and drawTargetMarker() (target) for the KSP-style markers.
void drawDiamondMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t half, uint16_t color);

// Straight line of stroke width `w` px (w<=1 falls back to drawLine). Width is applied
// symmetrically about the ideal line via a unit-perpendicular offset, so it thickens
// cleanly at any angle. Used by the KSP navball markers for their spokes/prongs/X.
void drawThickLine(KCM_TFT &tft, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   int16_t w, uint16_t color, bool caps = true);

// KSP prograde marker: ring + centre dot + three spokes pointing up/right/left. Used
// for the velocity/prograde marker (green) and the maneuver marker (blue). `r` is the
// ring radius; all sub-elements (stroke width, dot radius, spoke length) scale from `r`
// so the marker stays proportional at any size.
void drawProgradeMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color);

// KSP target marker: ring drawn as four arc segments with gaps at top/bottom/left/
// right (a "+" cut through the circle) plus a centre dot. Used for the target /
// docking-port marker (magenta). `r` is the ring radius; stroke/dot scale from `r`.
void drawTargetMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color);

// KSP maneuver-node marker: centre dot + three prongs (up, lower-left, lower-right)
// each ending in a short perpendicular crossbar, no ring. Used for the maneuver
// marker (blue). `r` is the prong length; stroke/dot/crossbar scale from `r`.
void drawManeuverMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color);

// KSP retrograde marker: ring + X + three spokes (up, lower-right, lower-left); no dot.
// Same green as prograde. `r` is the ring radius.
void drawRetrogradeMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color);

// KSP normal marker: hollow upward triangle + centre dot (magenta). `r` scales it.
void drawNormalMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color);

// KSP anti-normal marker: hollow downward triangle + centre dot + a spoke off the
// midpoint of each face (magenta). `r` scales it.
void drawAntiNormalMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color);

// KSP radial-in marker: ring + four diagonal spokes pointing inward; no dot (cyan).
void drawRadialInMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color);

// KSP radial-out marker: ring + centre dot + four short diagonal spokes pointing
// outward (cyan).
void drawRadialOutMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color);

// KSP anti-target marker: centre dot + three spokes (upper-left, upper-right, down),
// each gapped from the dot; no ring (magenta).
void drawAntiTargetMarker(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color);

// KSP level indicator (nose/waterline reticle): two horizontal wings with a centre dip
// and a dot on the wing line (yellow-gold). `r` sets the overall size.
void drawLevelIndicator(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r, uint16_t color);

// Which KSP navball symbol an ADI-ball marker should draw. Existing values keep their
// ordinals (0/1/2); new kinds are appended.
enum KspMarkerKind {
  KSP_MK_PROGRADE, KSP_MK_TARGET, KSP_MK_MANEUVER,
  KSP_MK_RETROGRADE, KSP_MK_NORMAL, KSP_MK_ANTINORMAL,
  KSP_MK_RADIAL_IN, KSP_MK_RADIAL_OUT, KSP_MK_ANTITARGET, KSP_MK_LEVEL
};

// Shared attitude-reticle chrome for the MNVR / DOCK / TGT screens. All three draw
// an identical black disc with four concentric rings (r/4, r/2, 3r/4, r coloured
// dark-green / dark-grey / dark-grey / grey), cardinal cross with a centre gap, a
// small nose crosshair, 30° minor ticks, and a two-px bezel. All three pass the same
// cardinal `gap` of 18 and minor-tick length of 14, so their chrome is identical; the
// re-entry retro ball reuses this base at 12/9 for its smaller disc. Ring degree
// LABELS, the legend, and the bottom bar remain per-screen (drawn after this base).
void reticleDrawBase(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r,
                     int16_t gap, int16_t tickLen);

// Repair the reticle chrome inside the box [bx,by]..[bx+2*bh, by+2*bh] after a
// marker at that location was erased to black. Redraws only the rings / cardinals /
// crosshair / good-zone that the box overlaps. `gap` matches the value passed to
// reticleDrawBase for the same screen.
// Returns the erase box's nearest distance to (cx,cy) so the caller can tell when
// the good-zone refill covered the innermost ring label (radius ≈ r/4).
float reticleRepair(KCM_TFT &tft, int16_t cx, int16_t cy, int16_t r,
                    int16_t gap, int16_t bx, int16_t by, uint8_t bh);

/***************************************************************************************
   BORESIGHT PROJECTION
   Every boresight-centred display in this project -- the EADI ball markers, the MNVR /
   TGT / DOCK reticles, the re-entry retro ball -- answers one question: given a world
   direction (navball heading/pitch), where does it sit relative to where the craft is
   POINTING? Telemetry arrives in the horizon frame, the screen is in the craft's frame,
   and the conversion is a genuine 3D rotation, not a pair of angle subtractions.

   The old approach subtracted heading and pitch and scaled the differences. That is
   only valid near zero pitch: heading lines converge toward the poles, so it stretched
   the horizontal axis by roughly 1/cos(pitch) -- a 10 deg error reads 14 deg at 45 deg
   pitch, 20 deg at 60 deg -- and inverted entirely past about 80 deg, which is exactly
   where a radial-in/out maneuver node puts you.

   PROJECTION. Azimuthal equidistant about the boresight: the radius of the plotted
   offset IS the true angular separation from the nose, in degrees, at any attitude and
   out to 180 deg. That is the property a reticle with degree-labelled rings needs --
   a marker on the 10 deg ring is 10 deg off the nose, always. (The obvious
   alternative, an atan2 pair per axis, is gnomonic: it reads 71.8 deg for a true
   60 deg offset at a 45 deg clock angle, which would make TGT's outer rings lie.)

   ROLL HANDEDNESS lives in kspBodyAxes and nowhere else. If markers rotate the wrong
   way in KSP, flip the sign of rollDeg there and every screen follows.
****************************************************************************************/

// Vessel body axes as unit vectors in ENU (East, North, Up) -- the frame navball
// heading/pitch already use.
struct KspBodyAxes {
  float fwd[3];     // out the nose
  float right[3];   // out the starboard side
  float up[3];      // out the roof
};

// Resolve a navball attitude into body axes.
//   rollDeg = state.roll  -> body/cockpit frame: screen up is the craft's roof, which
//                            is what a hand-flown display wants (RCS translation and
//                            pitch/yaw are both in these axes). Every boresight display
//                            in this project passes the vessel roll -- there are no
//                            exceptions, so one instinct serves them all.
//   rollDeg = 0           -> horizon-referenced frame: screen up is the local vertical.
//                            No current caller wants this; kept because it costs
//                            nothing and a world-referenced scope may want it.
// Well conditioned everywhere except exactly at the +/-90 deg pitch singularity of the
// heading/pitch/roll parameterisation itself, where heading and roll trade off.
KspBodyAxes kspBodyAxes(float headingDeg, float pitchDeg, float rollDeg);

// Unit vector for a world direction given as navball heading/pitch, in ENU.
void kspDirUnit(float headingDeg, float pitchDeg, float out[3]);

// Project a world direction onto a boresight-centred display.
//   degRight  + = right of the nose
//   degUp     + = above the nose
// hypot(degRight, degUp) is the true angular separation from the boresight (0..180).
// A direction exactly on the boresight axis returns (0,0) ahead or (0,180) behind,
// where the clock angle is genuinely undefined.
void kspBoresightAngles(const KspBodyAxes &ax, float dirHeadingDeg, float dirPitchDeg,
                        float &degRight, float &degUp);

// Rounded-rectangle OUTLINE. The display driver has no round-rect primitive, so the four
// straight edges are drawn inset by the corner radius and the corners are a Bresenham
// quarter-arc mirrored into all four -- only drawLine and drawPixel, both of which the
// driver provides. Used for status badges that must read as state rather than as
// something to press: the AUTO/MAN mode chip and the reference chips beside the balls.
void drawRoundRectOutline(KCM_TFT &tft, int16_t x, int16_t y,
                          int16_t w, int16_t h, int16_t r, uint16_t col);

// Shortest-arc delta between two headings, result in [-180, 180]. Pass b = 0 to wrap a
// single already-differenced angle into range.
float eadiHdgDelta(float a, float b);


/***************************************************************************************
   BODY ANGULAR RATES  (the Shuttle ADI / Apollo FDAI rate pointers)

   Real spacecraft attitude balls carry rate pointers as well as attitude, because a
   spacecraft is flown on rates: an RCS pulse is judged by the rate it produces, not by
   where the ball settles ten seconds later.

   WHY NOT DIFFERENTIATE THE EULER ANGLES. Heading/pitch/roll rates blow up near +/-90
   deg pitch, where heading and roll trade off against each other -- and a rocket on
   ascent sits there for the first minutes of every flight. But that is a defect of the
   PARAMETERISATION, not of the orientation: if heading jumps +10 deg and roll jumps
   -10 deg at pitch 90, the craft has not moved and the basis built from the pair is
   unchanged. So this differentiates the ROTATION instead of the angles, and is well
   conditioned at every attitude.

   HOW. Build the body basis at two successive samples and take the relative rotation
   dR = R1^T R2, whose skew-symmetric part is the rotation vector:

       w_i = (dR[k][j] - dR[j][k]) / 2      for (i,j,k) cyclic in a right-handed triad

   exact to second order in the angle, so at telemetry cadence (per-sample angles of a
   few degrees) it is far inside its validity and needs no acos -- which would be
   ill-conditioned near zero rate, i.e. most of the time.

   HANDEDNESS. KspBodyAxes stores (fwd, right, up) with up = right x fwd, which is a
   LEFT-handed ordering. The right-handed triad is (fwd, up, right), and that is the
   order used internally here. Output is converted to the aviation convention:
   roll + = right wing down, pitch + = nose up, yaw + = nose right.

   NOT YET FLIGHT-VERIFIED. Probed against kspBodyAxes directly, the pair that leaves
   the ORIENTATION untouched at pitch 90 is heading +d with roll +d -- the SAME sign,
   not opposite (heading +20 / roll -20 is a genuine 40 deg rotation and reads as one).
   At 89.5 deg the residual is ~0.17 deg per 50 ms step, i.e. ~3 deg/s, which is the
   real motion of a craft half a degree off vertical, against the ~400 deg/s that naive
   Euler differentiation would report on two axes.

   What is NOT yet confirmed is that KSP itself reports the pair that way. If it instead
   pins heading to an arbitrary value near vertical, the basis jumps and the rates spike.
   Bench test: point straight up and roll slowly; roll rate should be smooth, not spiky.
   Until that is run, treat near-vertical rates as unconfirmed.
****************************************************************************************/

struct KcmRateTracker {
  float    prev[3][3];      // previous basis, rows in (fwd, up, right) order
  uint32_t prevMs   = 0;    // timestamp of the sample that produced prev
  bool     primed   = false;
  float    roll     = 0.0f; // deg/s, smoothed, aviation convention
  float    pitch    = 0.0f;
  float    yaw      = 0.0f;
};

// Feed one attitude sample. Returns true when the smoothed rates changed.
//
// Telemetry arrives slower than the draw loop, so repeated identical attitudes are
// "no new packet", not "not rotating", and are ignored rather than integrated -- which
// would otherwise read zero between packets and spike on each arrival. After
// staleMs of genuinely unchanging attitude the rates decay to zero, which is the
// correct reading for a craft that really has stopped.
//
// tauMs is the smoothing time constant; 0 disables smoothing.
bool kcmRateUpdate(KcmRateTracker &t, float headingDeg, float pitchDeg, float rollDeg,
                   uint32_t nowMs, float tauMs = 250.0f, uint32_t staleMs = 600);

void kcmRateReset(KcmRateTracker &t);


/***************************************************************************************
   SHARED RETICLE MARKER LAYER  (MNVR / DOCK / TGT)
   The moving markers that sit on top of the reticleDrawBase chrome. All three screens
   run this same layer -- same clamp, same erase/repair region, same chrome repair --
   and differ only in the per-screen values carried in ReticleGeom.

   MARKER CONVENTION. Angles are boresight-relative (+right, +up) from
   kspBoresightAngles, plotted sx = cx + degRight*scale, sy = cy - degUp*scale. Every
   plotted marker uses this one frame, which is what makes a velocity marker flyable --
   it shows where the craft is going relative to where it is pointing, so a translation
   pulls the marker toward the direction you thrust.

   ROLL REFERENCE is chosen upstream, by whether the caller built its KspBodyAxes with
   the vessel roll or with 0. Every screen in this project uses the vessel roll, so
   screen up is always the craft's roof and one pilot instinct serves every display.
   The chrome is rotationally symmetric, so nothing underneath rotates with the markers.
****************************************************************************************/

// Per-screen reticle configuration. Everything the marker layer needs that differs
// between MNVR (1 marker, roll-free), DOCK (4 markers, roll-referenced) and TGT.
struct ReticleGeom {
  int16_t  cx, cy, r;        // disc centre + radius (px)
  float    scale;            // px per degree (TGT r/60, MNVR/DOCK r/20)
  uint8_t  dotRPrimary;      // target/port marker radius
  uint8_t  dotRVel;          // velocity/prograde marker radius
  uint8_t  eraseHalf;        // erase-rect half-size
  uint8_t  clampMargin;      // px kept clear inside the rim = widest marker half-extent
  const char *const *lbl;    // 4 ring-degree labels, inner -> outer
  const ILI9341_t3_font_t *lblFont;   // font those labels are drawn in
};

// Per-screen erase-before-redraw cache. 9999 = marker not currently shown (skip erase).
// Reset to defaults on screen entry via `cache = ReticleDotCache{};`.
struct ReticleDotCache {
  int16_t primaryX = 9999, primaryY = 9999;   // target / port
  int16_t velX     = 9999, velY     = 9999;   // velocity / prograde
  int16_t antiX    = 9999, antiY    = 9999;   // anti-target (opposite of primary)
  int16_t retroX   = 9999, retroY   = 9999;   // retrograde (opposite of velocity)
  // Pinned = clamped at the scope boundary, so the marker's direction is honest but its
  // distance is not. Cached because the marker is redrawn dimmed while pinned, and that
  // colour change has to force a repaint even when the marker has not moved a pixel.
  bool    primaryPinned = false, velPinned = false;
};

// The derived angles a screen feeds to the dot layer. All four PLOTTED pairs are
// boresight-relative (+right, +up) from kspBoresightAngles, so every marker lives in
// one frame and its distance from centre is its true angular offset from the nose.
// Which frame -- body or horizon -- was decided when the caller built the axes.
//
// appRight/appUp are the one TARGET-referenced pair and are never plotted: they are the
// relative-velocity direction decomposed about the TARGET axis (rolled with the vessel),
// i.e. how far right and above the approach axis the craft is actually travelling. That
// is the exact 3D angle, and it agrees with the on-screen VEL-to-PORT marker gap to
// within 0.31 deg while the target is inside DOCK's +/-20 deg scale, growing to about
// 2.9 deg out at TGT's 60 deg rim (the plotted gap is a difference of two azimuthal-
// equidistant positions, which is only exactly the angle between them when one sits on
// the boresight).
struct ReticleAngles {
  float priRight,   priUp;     // nose -> target/port (primary marker)
  float velRight,   velUp;     // nose -> relative-velocity vector (velocity marker)
  float antiRight,  antiUp;    // anti-target (antipodal of primary)
  float retroRight, retroUp;   // retrograde (antipodal of velocity)
  float appRight,   appUp;     // readout only: velocity vs the target axis (see above)
};

// Boresight angles (+right, +up) -> screen coords for this reticle's scale.
void reticleProject(const ReticleGeom &g, float degRight, float degUp,
                    int16_t &sx, int16_t &sy);

// Clamp a marker to within the scope boundary, keeping g.clampMargin px clear of the
// rim so the widest symbol still draws whole. Returns true if the marker was actually
// clamped -- i.e. it is off-scale and its plotted distance understates the real angle.
// Callers draw a pinned marker in its half-brightness shade so it cannot be mistaken for
// a live reading; the numeric readouts (Nos.Off, Brg/Elv) carry the true value.
bool reticleClampDot(const ReticleGeom &g, int16_t &sx, int16_t &sy);

// Repair scope chrome after a fillRect marker erase: the shared reticleRepair restore
// (rings / cardinals / crosshair / centre dot / good-zone), then the ring degree
// label(s) whose bbox the erase box overlapped, plus the innermost one when the
// good-zone refill painted over it.
void reticleRepairDotChrome(KCM_TFT &tft, const ReticleGeom &g,
                            int16_t bx, int16_t by, uint8_t bh);

// Erase-phase for one marker: if it should be hidden, or has moved > 1 px, erase its
// cached position and repair the chrome, then advance the cache. Callers run every
// erase before any draw, so a moving marker never clips a neighbour, and then redraw
// at the cache position (no sub-pixel smear).
// `restyled` forces the erase/repaint even when the marker has not moved, so a change of
// appearance (pinned <-> live) is not swallowed by the >1 px movement gate.
void reticleEraseDot(KCM_TFT &tft, const ReticleGeom &g,
                     int16_t curX, int16_t curY,
                     int16_t &prevX, int16_t &prevY, bool visible,
                     bool restyled = false);

// Full four-marker layer (DOCK / TGT): erase all, draw bottom-to-top (opposites,
// primary, velocity on top), then restore the inner crosshair the velocity ring can
// clip. Anti-target / retrograde appear only inside the FOV, and each suppresses its
// opposite while shown.
void reticleUpdateDots(KCM_TFT &tft, const ReticleGeom &g, ReticleDotCache &c,
                       const ReticleAngles &a);

// --- Basic formatters ---
String formatInt(uint16_t value);
String formatFloat(float value, uint8_t decimals);
String formatPerc(uint16_t value);
String formatUnits(uint16_t value, String units);
String formatFloatUnits(float value, uint8_t decimals, String units);

// --- Advanced formatters (KSP telemetry) ---
// Note: formatSep() is a dependency of formatAlt() — keep together
// Note: formatSep() drops the decimal part for values >= 1000 (#64)
// Note: formatSep() float core uses int64_t internally so it no longer
//       overflows for values >= 2^31 (#A5). For exact formatting of large
//       integer values, call formatSepI64() directly — float precision
//       caps usable integer range at ~1.6e7.
// Note: formatTime() uses Kerbin day = 6 hours
String formatSep(float value);
String formatSepI64(int64_t value);
String formatTime(float timeVal);
String formatTimeCompact(float timeVal);  // like formatTime() but compresses hours/days to fit tight cells
String formatAlt(float value);
String twString(uint8_t twIndex, bool physTW);

// --- Threshold color selector ---
void thresholdColor(uint16_t value,
                    uint16_t lowVal,  uint16_t lowColor,  uint16_t lowBack,
                    uint16_t midVal,  uint16_t midColor,  uint16_t midBack,
                                      uint16_t highColor, uint16_t highBack,
                    uint16_t &foreColor, uint16_t &backColor);

// Float overload (#42) — eliminates (uint16_t)constrain(x,0,65535) at call sites.
// Clamps value to [0, 65535] before delegating to the uint16_t overload.
void thresholdColor(float value,
                    float lowVal,  uint16_t lowColor,  uint16_t lowBack,
                    float midVal,  uint16_t midColor,  uint16_t midBack,
                                   uint16_t highColor, uint16_t highBack,
                    uint16_t &foreColor, uint16_t &backColor);

/***************************************************************************************
   DISPLAY CACHE
   Hold one DispCache per display block to enable flicker-free updates.
   Pass it to the printDisp overload and the function will skip the redraw entirely
   if the content, colors, and position are unchanged since the last draw.
   Declare in your sketch like:  DispCache altCache;
****************************************************************************************/
struct DispCache {
  String    param      = "";
  String    value      = "";
  uint16_t  paramColor = 0xFFFF;
  uint16_t  valColor   = 0xFFFF;
  uint16_t  valBack    = 0x0000;
  uint16_t  backColor  = 0x0000;
  uint16_t  borderColor= 0x0001;
  uint16_t  x0 = 0, y0 = 0, w = 0, h = 0;
  bool      valid      = false;  // false until first draw
};

/***************************************************************************************
   PrintState — flicker-free rendering state for printValue and printDisp.
   Tracks the previous render's pixel width, background colour, and font height so
   the library can draw text first (no blank frame) then clean up trailing pixels from
   a previously wider value, and do a full clear only on background/font changes.
   Because values are right-aligned, prevWidth also locates the previous value's LEFT
   edge: printValue starts its clear there, so a value wide enough to have been painted
   left of the label gap is cleaned up too rather than ghosting a coloured bar.

   Breaking change from v1.x: printValue and printDisp now require a PrintState &
   parameter. Callers must declare one PrintState per logical display slot.

   Declare alongside your RowCache or DispCache:
     PrintState ps[SCREEN_COUNT][ROW_COUNT];
   or individually:
     PrintState altState;
****************************************************************************************/
struct PrintState {
  uint16_t prevWidth  = 0;       // pixel width of last rendered value
  uint16_t prevBg     = 0x0001;  // sentinel: 0x0001 = no previous render
  uint16_t prevHeight = 0;       // font height of last render (0 = no previous render)
};

// --- Display block functions ---

// Full block draw with flicker-free rendering.
// ps tracks previous render state to avoid blank-frame flicker on rapid updates.
void printDisp(KCM_TFT &tft, const ILI9341_t3_font_t *font,
               uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               const String &param, const String &value,
               uint16_t paramColor, uint16_t valColor, uint16_t valBack,
               uint16_t backColor, uint16_t borderColor,
               PrintState &ps);

// Cached overload — skips redraw entirely if content and colors are unchanged,
// otherwise calls the PrintState overload for flicker-free rendering.
void printDisp(KCM_TFT &tft, const ILI9341_t3_font_t *font,
               uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               const String &param, const String &value,
               uint16_t paramColor, uint16_t valColor, uint16_t valBack,
               uint16_t backColor, uint16_t borderColor,
               DispCache &cache, PrintState &ps);

// Chrome-only redraw — draws the static label + border and fills the block
// background, leaving the value region for printValue() to fill. Call once when
// laying out a screen; use printValue() thereafter for per-update value redraws.
void printDispChrome(KCM_TFT &tft, const ILI9341_t3_font_t *font,
                     uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                     String label,
                     uint16_t labelColor, uint16_t backColor,
                     uint16_t borderColor);

void printValue(KCM_TFT &tft, const ILI9341_t3_font_t *font,
                uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                const String &param, const String &value,
                uint16_t valColor, uint16_t valBack,
                uint16_t backColor,
                PrintState &ps);

void printName(KCM_TFT &tft, const ILI9341_t3_font_t *font,
               uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               const String &value, uint16_t color, uint16_t backColor,
               uint16_t borderColor, byte maxLength = 30);

void printTitle(KCM_TFT &tft, const ILI9341_t3_font_t *font,
                uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                const String &value, uint16_t color, uint16_t backColor,
                uint16_t borderColor);

// --- SD card BMP drawing ---

// Enable verbose library debug output to Serial (I2C scan, touch init steps, etc.).
// Error messages (SD failures, BMP errors) are always printed regardless of this setting.
// Call setKDCDebugMode(debugMode) in setup() after Serial.begin().
void setKDCDebugMode(bool enable);

// Draw a vertical bar graph (bottom-fill). prevVal and newVal in range 0..scale.
// Erases the delta between old and new bar rather than redrawing the full area.
// drawBorder=true draws a white outline around the bar area.
void drawVertBarGraph(KCM_TFT &tft,
                      uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                      int32_t prevVal, int32_t newVal,
                      uint16_t barColor, bool drawBorder,
                      int32_t scale = 1000);

// Draw a semicircular arc indicator with an erasable needle.
// Arc spans ±90° (left to right). prevVal is erased, curVal is drawn.
//
// IMPORTANT (#A8): This function performs a full redraw on every call — the
// arc track (~91 fillCircle calls), both needle phases, and the centre dot.
// It does NOT change-detect on prevVal/curVal. Gate at the call site:
//   if (cur != prev) drawArcDisplay(..., prev, cur, ...);
// A future revision (see C2) will split this into drawArcTrack +
// drawArcNeedle so the track can be drawn once at init and only the needle
// updated per value change.
void drawArcDisplay(KCM_TFT &tft,
                    int16_t cx, int16_t cy,
                    uint16_t radius, uint16_t needleW,
                    float minVal, float maxVal,
                    float prevVal, float curVal,
                    uint16_t color);

// Draw a vertical percentage axis with major/minor ticks and right-justified labels.
// 0% is at barBottom, 100% is at barTop. axisW px are reserved for labels and ticks.
// The axis line is drawn at x0 + axisW - 1.
void drawLabelledAxis(KCM_TFT &tft,
                      uint16_t x0, uint16_t axisW,
                      uint16_t barTop, uint16_t barBottom,
                      const ILI9341_t3_font_t *font,
                      uint16_t axisColor, uint16_t backColor);

// =============================================================================
// BOOT SCREEN RENDERING HELPERS (#15, #16, #17)
// Shared terminal-aesthetic boot sequence primitives used by all KCMk1 panels.
// All helpers stay in graphics mode (setFont/setCursor/print) — never text mode.
// =============================================================================

// Print text at explicit (x, y) with given font and colour — no y advance.
void bsPrint(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t x, uint16_t y,
             const char *text, uint16_t col);

// Print one line at column x, advance y by rowH. Returns new y.
uint16_t bsLine(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t col_x,
                uint16_t y, uint16_t rowH, const char *text, uint16_t col);

// Print with a double-height font; advances y by (font cap_height + 5px). Returns new y.
uint16_t bsBig(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t col_x,
               uint16_t y, const char *text, uint16_t col);

// Advance y by rowH without drawing (blank line). Returns new y.
uint16_t bsBlank(uint16_t y, uint16_t rowH);

// Word-wrap text across multiple lines within maxW pixels. Returns new y.
uint16_t bsWrap(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t col_x,
                uint16_t y, uint16_t rowH,
                const char *text, uint16_t col, uint16_t maxW);

// Fisher-Yates in-place shuffle of a uint8_t index array of length n.
void bsShuffle(uint8_t *arr, uint8_t n);

// Draw a string one character per line within a rectangle — vertical label strip.
// Text is centred horizontally within w and vertically within h.
// The strip is filled with backColor before drawing.
// Use where text rotation is needed but the display controller has no native rotation support.
void drawVerticalText(KCM_TFT &tft,
                      uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                      const ILI9341_t3_font_t *font,
                      const char *text,
                      uint16_t color, uint16_t backColor);

// Draw the shared standby splash BMP from SD card.
// Equivalent to: fillScreen(BLACK) + drawBMP("/StandbySplash_1024x600.bmp", 0, 0).
// setupSD() must have been called first. Shared across all KCMk1 panels.
void drawStandbySplash(KCM_TFT &tft);

// setupSD() must be called once in setup() before any drawBMP() calls.
// Returns true if the SD card was found and initialised successfully.
// drawBMP() will silently skip and return BMP_ERR_SD_INIT if this was not called
// or if it returned false.
bool setupSD();

// Return codes for drawBMP — check against BMP_OK for success.
// BMP_ERR_NOT_24BIT, BMP_ERR_COMPRESSED, BMP_ERR_DIB, and bit-depth codes are
// retained for compatibility but only 24-bit uncompressed BMPs are supported.
enum BMPResult : uint8_t {
  BMP_OK             = 0,  // success
  BMP_ERR_NO_CARD    = 1,  // SD card not detected at SD_DETECT_PIN
  BMP_ERR_SD_INIT    = 2,  // SD not initialised — call setupSD() in setup()
  BMP_ERR_FILE       = 3,  // file not found or could not be opened
  BMP_ERR_SIGNATURE  = 4,  // not a valid BMP file (missing "BM" header)
  BMP_ERR_DIB        = 5,  // DIB header version too old or unsupported
  BMP_ERR_COMPRESSED = 6,  // compressed BMPs are not supported
  BMP_ERR_DIMENSIONS = 7,  // invalid image dimensions (zero or negative width)
  BMP_ERR_READ       = 8,  // unexpected end of file during read
  BMP_ERR_NOT_24BIT  = 9,  // only 24-bit uncompressed BMPs are supported
};

// Draw a 24-bit uncompressed BMP from the SD card at screen position (x, y).
// setupSD() must have been called and returned true before calling this.
// Errors are logged to Serial with the filename and error code.
BMPResult drawBMP(KCM_TFT &tft, const char *filename, uint16_t x, uint16_t y);

// =============================================================================
// --- Vessel enums ---
// Ref: https://www.kerbalspaceprogram.com/ksp/api/_vessel_8cs.html
// Ref: https://www.kerbalspaceprogram.com/ksp/api/class_vessel.html
// =============================================================================

enum VesselType : uint8_t {
  type_Debris   =  0,
  type_Object   =  1,
  type_Unknown  =  2,
  type_Probe    =  3,
  type_Relay    =  4,
  type_Rover    =  5,
  type_Lander   =  6,
  type_Ship     =  7,
  type_Plane    =  8,
  type_Station  =  9,
  type_Base     = 10,
  type_EVA      = 11,
  type_Flag     = 12,
  type_SciCtrlr = 13,
  type_SciPart  = 14,
  type_Part     = 15,
  type_GndPart  = 16
};

// Situation flags — bitmask, multiple can be set simultaneously
enum VesselSituation : uint8_t {
  sit_Landed    =   1,
  sit_Splashed  =   2,
  sit_PreLaunch =   4,
  sit_Flying    =   8,
  sit_SubOrb    =  16,
  sit_Orbit     =  32,
  sit_Escaping  =  64,
  sit_Docked    = 128
};

// =============================================================================
// --- Celestial body parameters ---
// =============================================================================
//
// The BodyParams struct, the _bodyTable[] data, and the getBodyParams() lookups
// have moved to a shared, header-only single source of truth so that both the
// display firmware and the master controller's ascent autopilot use one table:
//
//   Software/Common/body_params.h
//
// Usage is unchanged: BodyParams b = getBodyParams("Kerbin");  (see that header).
#include "C:\Dev\KerbalControllerMk1\Software\Common\body_params.h"

// =============================================================================
// --- Capacitive touch ---
// =============================================================================
// Touch moved to its own library for hardware rev 2: the GSL1680F driver (and
// its 800x480 firmware blob) is replaced by KCM_Touch (FT5316 on software I2C).
// Include <KCM_Touch.h> from the sketch; it provides the same surface:
//   TouchPoint / TouchResult / setupTouch() / isTouched() / readTouch() /
//   clearTouchISR() / touchISRCount().


// =============================================================================
// SYSTEM UTILITIES — Teensy 4.1 (IMXRT1062) specific
// Uses hardware registers only — safe to call from library code.
// Do not call from within an ISR.
// =============================================================================

// Performs a soft reboot via the ARM AIRCR register. Does not return.
void executeReboot();

// Shuts down and resets the USB1 controller. Call immediately before
// executeReboot() to ensure a clean USB re-enumeration on the host after reboot.
// Note: USB reconnection without a full reboot is not supported on this hardware.
void disconnectUSB();

#endif // KERBAL_DISPLAY_COMMON_H

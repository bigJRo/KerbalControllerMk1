/********************************************************************************************************************************
  Celestial Body Parameters — Kerbal Controller Mk1 (shared)

  Single source of truth for per-celestial-body physical / orbital parameters, keyed on the KerbalSimpit SOI name string.
  Shared by both the display firmware (KerbalDisplayCommon library) and the master controller (Controller_Main ascent
  autopilot). Header-only (static table + static-inline lookup) so it can be included from independent Arduino sketch trees
  without any separate compilation unit or linker coordination.

  Values sourced from the KSP wiki (canonical). reentryAlt and highQThreshold are engineering estimates — calibrate
  highQThreshold per body from flight test. synodicPeriod is relative to Kerbin.

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Written for Jeb's Controller Works.
********************************************************************************************************************************/
#ifndef KC_BODY_PARAMS_H
#define KC_BODY_PARAMS_H

#include <Arduino.h>
#include <float.h>   // DBL_MAX
#include <string.h>  // strcmp

// =============================================================================
// --- Celestial body parameters ---
// =============================================================================
//
// Usage:
//   BodyParams currentBody = getBodyParams("Kerbin");
//   // then access: currentBody.radius, currentBody.gravity, etc.
//
//   Call getBodyParams() again whenever Simpit reports a new SOI.
//   If the SOI string is not recognised, all numeric fields are 0 and
//   all string fields are empty — check currentBody.soiName[0] != '\0'
//   to detect a valid result.
//
// All altitude/radius values are in metres unless noted.
// All velocity values are in m/s.
// gravity is in m/s² (not g).
// hasSurface == false for Jool and Kerbol (gas giant / star — no landing).
// hasAtmo == false means flyHigh and lowSpace are 0 and unused in C&W logic.
// hasO2 == true means jet engines work and Kerbals can remove helmets.
// soiAlt is double to preserve precision for large bodies (e.g. Jool ~2.4e12 m).
// soiAlt == DBL_MAX for Kerbol (root body, no SOI boundary).
// synchronousOrbit == 0 means no synchronous orbit achievable within SOI.
// synodicPeriod == 0 for Kerbol (no synodic period relative to itself).
// highQThreshold == 0 means CW_HIGH_Q is suppressed for this body (airless bodies).
//   Calibrate empirically per atmospheric body from flight test.
// reentryAlt == 0 for airless bodies. For atmospheric bodies: Pe below this
//   altitude triggers committed reentry (red tier). Between reentryAlt and
//   max(minSafe,lowSpace) is the aerobrake zone (yellow tier).

struct BodyParams {
  const char* soiName;          // Simpit SOI string — matches what Simpit sends
  const char* dispName;         // Display name, uppercase, max 8 chars + null
  const char* image;            // SD card BMP path, e.g. "/Kerbin-Display_240x168.bmp"
  const char* cond;             // Atmosphere condition string:
                                //   "Vacuum", "Atmosphere", "Breathable", "Plasma"
  // --- Altitude boundaries (metres, from wiki science biome table) ---
  float       minSafe;          // Highest terrain point (m); for Jool/Kerbol: crush/plasma alt
  float       flyHigh;          // Low/High atmosphere science biome boundary (m); 0 if no atmo
  float       lowSpace;         // Atmosphere top / Low space boundary (m); 0 if no atmo
  float       highSpace;        // Low/High space science biome boundary (m)
  float       reentryAlt;       // Pe below this = committed reentry, red tier (m); 0 if no atmo
  double      soiAlt;           // Sphere of influence radius (m); DBL_MAX for Kerbol
  // --- Physical properties ---
  float       radius;           // Mean body radius (m)
  float       gravity;          // Surface gravity (m/s²)
  float       escapeVelocity;   // Escape velocity from surface (m/s)
  // --- Orbital properties ---
  float       synchronousOrbit; // Synchronous orbit altitude (m); 0 if not achievable
  float       synodicPeriod;    // Synodic period relative to Kerbin (s); 0 for Kerbol
  float       orbitInclination; // Orbital inclination relative to Kerbin equator (deg)
  // --- Boolean flags ---
  bool        hasAtmo;          // Body has an atmosphere
  bool        hasO2;            // Atmosphere contains oxygen (jets work, helmets off)
  bool        hasSurface;       // Body has a landable surface
  // --- C&W tuning ---
  float       highQThreshold;   // Dynamic pressure threshold for CW_HIGH_Q (Pa); 0 = suppressed
};

// Field order per row:
//   soiName, dispName, image, cond,
//   minSafe, flyHigh, lowSpace, highSpace, reentryAlt, soiAlt,
//   radius, gravity, escapeVelocity,
//   synchronousOrbit, synodicPeriod, orbitInclination,
//   hasAtmo, hasO2, hasSurface,
//   highQThreshold
static const BodyParams _bodyTable[] = {
  //         soiName    dispName  image                             cond
  //         minSafe   flyHigh  lowSpace  highSpace reentryAlt  soiAlt
  //         radius     gravity   escVel
  //         syncOrb     synodic      incl
  //         atmo   o2     surf   highQ

  { "Kerbol", "KERBOL", "/Kerbol-Display_240x168.bmp", "Plasma",
    1000000,  18000,   600000,   1000000000, 600000,   DBL_MAX,
    261600000, 17.1f,   94672.01f,
    1508045286.0f, 0,      0.0f,
    true,  false, false, 0.0f },

  { "Moho",   "MOHO",   "/Moho-Display_240x168.bmp",   "Vacuum",
    6900,     0,       0,        80000,      0,        9646663.0,
    250000,    2.7f,    1161.41f,
    0,          2918346,    7.0f,
    false, false, true,  0.0f },

  { "Eve",    "EVE",    "/Eve-Display_240x168.bmp",    "Atmosphere",
    7600,     22000,   90000,    400000,     57000,    85109365.0,
    700000,    16.7f,   4831.96f,
    10328472,   14687035,   2.10f,
    true,  false, true,  0.0f },

  { "Gilly",  "GILLY",  "/Gilly-Display_240x168.bmp",  "Vacuum",
    7500,     0,       0,        6000,       0,        126123.0,
    13000,     0.049f,  35.71f,
    42138,      417243,     12.0f,
    false, false, true,  0.0f },

  { "Kerbin", "KERBIN", "/Kerbin-Display_240x168.bmp", "Breathable",
    6800,     18000,   70000,    250000,     45000,    84159286.0,
    600000,    9.81f,   3431.03f,
    2863334,    0,          0.0f,
    true,  true,  true,  0.0f },

  { "Mun",    "MUN",    "/Mun-Display_240x168.bmp",    "Vacuum",
    7100,     0,       0,        60000,      0,        2429559.0,
    200000,    1.63f,   807.08f,
    0,          141115,     0.0f,
    false, false, true,  0.0f },

  { "Minmus", "MINMUS", "/Minmus-Display_240x168.bmp", "Vacuum",
    5800,     0,       0,        30000,      0,        2247428.0,
    60000,     0.491f,  242.61f,
    357940,     1220131,    6.0f,
    false, false, true,  0.0f },

  { "Duna",   "DUNA",   "/Duna-Display_240x168.bmp",   "Atmosphere",
    8300,     12000,   50000,    140000,     20000,    47921949.0,
    320000,    2.94f,   1372.41f,
    2879999,    19645697.0f, 0.06f,
    true,  false, true,  0.0f },

  { "Ike",    "IKE",    "/Ike-Display_240x168.bmp",    "Vacuum",
    12800,    0,       0,        50000,      0,        1049599.0,
    130000,    1.1f,    534.48f,
    0,          65766,      0.2f,
    false, false, true,  0.0f },

  { "Dres",   "DRES",   "/Dres-Display_240x168.bmp",   "Vacuum",
    5700,     0,       0,        25000,      0,        32832840.0,
    138000,    1.13f,   558.00f,
    732244,     11392903,   5.0f,
    false, false, true,  0.0f },

  { "Jool",   "JOOL",   "/Jool-Display_240x168.bmp",   "Atmosphere",
    120000,   120000,  200000,   4000000,    150000,   2455985200.0,
    6000000,   7.85f,   9704.43f,
    15010461,   10090901,   0.05f,
    true,  false, false, 0.0f },

  { "Laythe", "LAYTHE", "/Laythe-Display_240x168.bmp", "Breathable",
    6100,     10000,   50000,    200000,     38000,    3723646.0,
    500000,    7.85f,   2801.43f,
    0,          53007,      0.0f,
    true,  true,  true,  0.0f },

  { "Vall",   "VALL",   "/Vall-Display_240x168.bmp",   "Vacuum",
    8000,     0,       0,        90000,      0,        2406401.0,
    300000,    2.31f,   1176.10f,
    0,          106069,     0.0f,
    false, false, true,  0.0f },

  { "Tylo",   "TYLO",   "/Tylo-Display_240x168.bmp",   "Vacuum",
    13000,    0,       0,        250000,     0,        10856518.0,
    600000,    7.85f,   3068.81f,
    0,          212356,     0.025f,
    false, false, true,  0.0f },

  { "Bop",    "BOP",    "/Bop-Display_240x168.bmp",    "Vacuum",
    21800,    0,       0,        25000,      0,        1221061.0,
    65000,     0.589f,  276.62f,
    0,          547355,     15.0f,
    false, false, true,  0.0f },

  { "Pol",    "POL",    "/Pol-Display_240x168.bmp",    "Vacuum",
    4900,     0,       0,        22000,      0,        1042139.0,
    44000,     0.373f,  181.12f,
    0,          909742,     4.25f,
    false, false, true,  0.0f },

  { "Eeloo",  "EELOO",  "/Eeloo-Display_240x168.bmp",  "Vacuum",
    3800,     0,       0,        60000,      0,        119082940.0,
    210000,    1.69f,   841.83f,
    683690,     9776696,    6.15f,
    false, false, true,  0.0f },
};

static const uint8_t _bodyTableLen = sizeof(_bodyTable) / sizeof(_bodyTable[0]);

static const BodyParams _bodyUnknown = {
  "", "", "", "",
  0, 0, 0, 0, 0, 0.0,
  0, 0.0f, 0.0f,
  0, 0, 0.0f,
  false, false, false,
  0.0f
};

/***************************************************************************************
   GET BODY PARAMETERS
   Looks up the SOI string in the body table and returns a copy of the matching entry.
   Returns a zeroed/empty BodyParams (_bodyUnknown) if the SOI is not recognised.

   Two overloads. The const char* version is the primary — callers parsing Simpit
   packets into char buffers can pass them directly without allocating a String. The
   const String& version delegates via .c_str() and exists for backward compatibility.

   NOTE: The const char* fields (soiName, dispName, image, cond) in the returned struct
   are pointers into string literals in the static _bodyTable array; they remain valid
   for the lifetime of the program. Treat them as read-only.
****************************************************************************************/
static inline BodyParams getBodyParams(const char* SOI) {
  if (SOI == nullptr) return _bodyUnknown;
  for (uint8_t i = 0; i < _bodyTableLen; i++) {
    if (strcmp(SOI, _bodyTable[i].soiName) == 0) {
      return _bodyTable[i];
    }
  }
  return _bodyUnknown;
}

static inline BodyParams getBodyParams(const String& SOI) {
  return getBodyParams(SOI.c_str());
}

#endif  // KC_BODY_PARAMS_H

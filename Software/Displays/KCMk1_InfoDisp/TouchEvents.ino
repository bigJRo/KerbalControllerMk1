/***************************************************************************************
   TouchEvents.ino -- Touch input for Kerbal Controller Mk1 Information Display

   Touch is polled via KCM_Touch each processTouchEvents() call (rev-2 FT5316; no ISR —
   the rev-1 CTP_INT_PIN touchISR()/_touchPending latch is gone, clearTouchISR() is a
   compat no-op).

   Defence layers:
   2. Count filter — accept only count == 1. Multi-finger events on a single-button
      sidebar are never intentional; count != 1 is a strong phantom signal. Applied to
      both the first read and the confirmation re-read.
   3. (removed rev-2) The bottom Y dead zone was a GSL1680 edge-noise workaround.
      The FT5316 does not ghost at the panel boundary, and the bottom sidebar
      button (REEN) extends to the last row, so the band is gone.
   4. Bounds check — reject x >= SCREEN_W or y >= SCREEN_H.
   5. Double-read with coordinate stability — re-read after 8ms; reject if the re-read
      count != 1 OR if coordinates moved more than TOUCH_JITTER_MAX pixels.
      Phantom noise jumps around between reads; real touches are stable.
   6. Debounce 500ms — prevents rapid re-fires within a burst.
   7. Require-release — set on ANY confirmed touch, suppressing the rest of a burst
      until INT goes low.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


static const uint32_t TOUCH_DEBOUNCE_MS  = KCM_TOUCH_DEBOUNCE_MS;     // #3B from SystemConfig
static const uint16_t TOUCH_JITTER_MAX   = KCM_TOUCH_JITTER_MAX_PX;   // #3B px — max coordinate movement across reads

// Repeat press on the SAME sidebar button — a deliberate mode cycle. The full 500 ms
// window is a phantom-rejection budget sized for an arbitrary tap anywhere on the
// panel; it makes cycling punitive, since reaching VEH from SPC costs three presses
// and therefore 1.5 s of enforced waiting. A second press on a button the pilot just
// pressed is the least ambiguous input the panel receives, and layer 7 (require-
// release) already guarantees the finger lifted in between, so it gets a shorter
// window. Every other tap — a first press, a different button, anything in the
// content area — keeps the full budget.
static const uint32_t TOUCH_CYCLE_DEBOUNCE_MS = 150;

static uint32_t lastTouchTime      = 0;
static bool     _waitForRelease    = false;
static uint8_t  _lastSidebarBtn    = 0xFF;   // last sidebar button pressed (0xFF = none)

// Boot phantom guard: require the panel to be seen untouched once before accepting
// any tap, so a settling touch from the FT5316 right after reset cannot fire a
// gesture. Cleared to true the first time the screen is observed untouched.
static bool _bootReleaseSeen = false;


void processTouchEvents() {
  if (!isTouched()) {
    _waitForRelease  = false;
    _bootReleaseSeen = true;   // panel seen untouched — real taps now allowed
    return;
  }
  if (!_bootReleaseSeen) return;   // ignore boot/settling phantom until first release

  // First read
  lastTouch = readTouch();
  if (lastTouch.count == 0) return;

  // Count filter — real single-button presses are always count=1
  if (lastTouch.count != 1) {
    if (debugMode) {
      Serial.print(F("InfoDisp: Touch discarded (count="));
      Serial.print(lastTouch.count);
      Serial.println(F(")"));
    }
    return;
  }

  if (_waitForRelease) return;

  uint16_t x1 = lastTouch.points[0].x;
  uint16_t y1 = lastTouch.points[0].y;

  // Pick the debounce window before spending 8 ms on the confirmation re-read. The
  // first-read coordinates are good enough to classify the tap; the confirmation
  // still has to agree with them, so a phantom cannot buy the shorter window.
  uint32_t now        = millis();
  uint32_t debounceMs = TOUCH_DEBOUNCE_MS;
  if (touchInSidebar(x1) && _lastSidebarBtn != 0xFF &&
      (uint8_t)(y1 / sbBtnH()) == _lastSidebarBtn) {
    debounceMs = TOUCH_CYCLE_DEBOUNCE_MS;
  }
  if (now - lastTouchTime < debounceMs) return;

  // Double-read after 8ms — confirm the touch is real and stable (phantom noise
  // jumps between reads; real touches hold position).
  delay(8);
  TouchResult confirm = readTouch();
  if (confirm.count != 1) {
    if (debugMode) Serial.println(F("InfoDisp: Touch discarded (phantom — count != 1 on reread)"));
    return;
  }
  uint16_t x2 = confirm.points[0].x;
  uint16_t y2 = confirm.points[0].y;
  uint16_t dx = (x2 > x1) ? x2 - x1 : x1 - x2;
  uint16_t dy = (y2 > y1) ? y2 - y1 : y1 - y2;
  if (dx > TOUCH_JITTER_MAX || dy > TOUCH_JITTER_MAX) {
    if (debugMode) {
      Serial.print(F("InfoDisp: Touch discarded (jitter dx="));
      Serial.print(dx);
      Serial.print(F(" dy="));
      Serial.print(dy);
      Serial.println(F(")"));
    }
    return;
  }
  lastTouch = confirm;

  // Bounds check — applied to final coordinates
  if (x2 >= SCREEN_W || y2 >= SCREEN_H) return;

  // Title-bar taps no longer switch modes — mode switching moved to the sidebar
  // buttons (see the sidebar hit test below). A tap on the title bar (y < TITLE_TOP,
  // x < CONTENT_W) simply falls through and is a no-op.

  // Stamp debounce and require-release immediately — suppresses burst tail
  lastTouchTime = now;
  _waitForRelease = true;
  // Cleared here and re-set below only if this tap lands on a sidebar button, so the
  // shortened cycle window is available strictly for a run of presses on one button.
  _lastSidebarBtn = 0xFF;

  if (debugMode) {
    Serial.print(F("InfoDisp: Touch count="));
    Serial.print(lastTouch.count);
    Serial.print(F(" x="));
    Serial.print(x2);
    Serial.print(F(" y="));
    Serial.println(y2);
  }

  // Pre-launch board: tap anywhere in content area to advance to ascent mode
  if (activeScreen == screen_LNCH && _lnchPrelaunchMode &&
      touchInContent(x2) && y2 >= TITLE_TOP) {
    _lnchPrelaunchMode      = false;
    _lnchPrelaunchDismissed = true;   // prevent FLIGHT_STATUS from re-entering
    _lnchOrbitalMode        = false;
    _lnchManualOverride     = false;
    // switchToScreen() forces a full chrome redraw (full rowCache invalidation).
    switchToScreen(screen_LNCH);
    clearTouchISR();
    if (debugMode) Serial.println(F("InfoDisp: Pre-launch board dismissed by tap"));
    return;
  }

  // Attitude screens: the reference chip. One tap pins the other reference (or drops
  // back to auto); the chip's colour says which state it is in. The ball itself is
  // deliberately NOT a target -- it is the primary instrument, and a stray touch
  // changing the reference mid-burn is a bad way to find that out.
  if ((activeScreen == screen_SCFT || activeScreen == screen_ACFT) &&
      touchInContent(x2) && y2 >= TITLE_TOP &&
      refChipHit(touchContentX(x2), y2)) {
    if (activeScreen == screen_SCFT) scftToggleVelRef();
    else                             acftToggleAltRef();
    clearTouchISR();
    if (debugMode) Serial.println(F("InfoDisp: reference chip tapped"));
    return;
  }

  // Ascent Autopilot: content-area taps drive its on-screen keypad / editable
  // fields / ARM button. Sidebar taps (x past the content area) fall through.
  if (activeScreen == screen_LNCHAP && touchInContent(x2) && y2 >= TITLE_TOP) {
    // apScreenTouch() lays its keypad out in content space — hand it the translated x.
    apScreenTouch(touchContentX(x2), y2);
    clearTouchISR();
    return;
  }

  // Sidebar hit test — the SIDEBAR_W column on this unit's outboard edge (left on
  // unit 1, right on unit 2), 6 buttons (SB_BTN_SCREEN).
  // First press of a button (from another screen) goes to that button's context/
  // primary mode; pressing the button that already owns the active screen cycles
  // its modes. Context auto-select still runs on scene/vessel change; a press
  // latches a manual override for the multi-mode buttons.
  if (touchInSidebar(x2)) {
    uint8_t btn = (uint8_t)(y2 / sbBtnH());
    if (btn >= SB_BTN_COUNT) return;
    _lastSidebarBtn = btn;   // arms the shortened debounce for a repeat press

    bool       active   = (screenToButton(activeScreen) == btn);
    ScreenType target   = activeScreen;
    bool       doSwitch = false;

    if (!active) {
      // First press — jump to the button's context/primary screen.
      switch (btn) {
        case SB_PFD_BTN:
          target = pfdSelectedScreen();   // context or last manual PFD sub-screen
          break;
        case SB_TGTDOCK_BTN:
          // Context: docking screen when a target is within docking range, else target.
          target = (state.tgtDistance > 0.0f && state.tgtDistance <= DOCK_DIST_WARN_M)
                     ? screen_DOCK : screen_TGT;
          break;
        case SB_LNDG_BTN:
          target = screen_LNDG;           // primary = powered descent (DESC)
          break;
        default:
          target = SB_BTN_SCREEN[btn];    // LNCH (PRE/ASC/CIRC per flags), ORB, single-mode
          break;
      }
      doSwitch = (target != activeScreen);
    } else {
      // Already on this button — cycle its modes.
      switch (btn) {
        case SB_LNCH_BTN:
          if (_lnchPrelaunchMode) {
            // Dismiss the pre-launch board into the ascent view.
            _lnchPrelaunchMode      = false;
            _lnchPrelaunchDismissed = true;
            _lnchOrbitalMode        = false;
          } else {
            _lnchOrbitalMode = !_lnchOrbitalMode;   // ASC <-> CIRC
          }
          _lnchManualOverride = true;
          target = screen_LNCH; doSwitch = true;
          break;
        case SB_PFD_BTN: {
          // Ring depth is per unit: unit 1 promoted VEH to its own button, so its
          // ring is the three attitude screens (SPC -> ACFT -> ROVR). Unit 2, where
          // the PFD button is the off-role one, keeps all four in the ring.
          const uint8_t PFD_RING_LEN = INFO_DISP_IS_PFD_UNIT ? 3 : 4;
          uint8_t cur = (activeScreen == screen_VEH)  ? 3 :
                        (activeScreen == screen_ROVR) ? 2 :
                        (activeScreen == screen_ACFT) ? 1 : 0;
          _pfdManualSel      = (uint8_t)((cur + 1) % PFD_RING_LEN);
          _pfdManualOverride = true;
          target = pfdScreenForSel(_pfdManualSel); doSwitch = true;
          break;
        }
        case SB_ORB_BTN:
          // ORB -> ORB+ -> MNVR -> ORB
          target = (activeScreen == screen_ORB)    ? screen_ORBADV :
                   (activeScreen == screen_ORBADV) ? screen_MNVR   : screen_ORB;
          doSwitch = true;
          break;
        case SB_TGTDOCK_BTN:
          // TGT -> DOCK -> NAV. The boresight view of the target, the close-in
          // approach view, and the plan view — the same question at three ranges.
          target = (activeScreen == screen_TGT)  ? screen_DOCK
                 : (activeScreen == screen_DOCK) ? screen_NAV : screen_TGT;
          doSwitch = true;
          break;
        case SB_LNDG_BTN:
          target = (activeScreen == screen_LNDGRE) ? screen_LNDG : screen_LNDGRE;
          doSwitch = true;
          break;
        default:
          doSwitch = false;   // single-mode button already active — nothing to cycle
          break;
      }
    }

    // Latch policy, applied whether or not the press changes the screen: pressing
    // through to the screen the ladder currently wants is an explicit return to
    // automatic; anything else is an override, recorded against the ladder's current
    // answer so it releases once that situation passes. See AAA_Globals.ino.
    if (target == contextScreen()) clearManualScreenLatch();
    else                           setManualScreenLatch();

    if (doSwitch) {
      switchToScreen(target);
      clearTouchISR();
    }
  }

}

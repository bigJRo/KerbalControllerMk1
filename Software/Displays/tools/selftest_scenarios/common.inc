// Shared harness for tools/behaviour_selftest.py, prepended to each panel's scenario
// file and appended to the concatenated sketch (so it sees every tab's globals).
//
// The build leaves out KCM_Touch.cpp (its FT5316 driver bit-bangs I2C); the fakes
// below stand in for it, so a scenario can put fingers on the panel.

HardwareSerial Serial, SerialUSB1, Serial1, Serial2, Serial3;
TwoWire Wire, Wire1, Wire2;
volatile uint32_t ARM_DWT_CYCCNT, ARM_DWT_CTRL, ARM_DEMCR;

static int st_fails = 0, st_checks = 0;
#define CHECK(c, m) do { st_checks++; bool ok_ = (c); printf("  %s %s\n", ok_ ? "ok  " : "FAIL", m); \
                         if (!ok_) st_fails++; } while (0)
#define SECTION(s) printf("%s\n", s)
static int stDone() { printf("%d of %d checks failed\n", st_fails, st_checks); return st_fails != 0; }

// ---- Fake touch controller --------------------------------------------------------
static TouchResult st_touch;
bool        isTouched()            { return st_touch.count > 0; }
TouchResult readTouch()            { return st_touch; }
void        setupTouch()           {}
bool        probeTouch()           { return true; }
void        clearTouchISR()        {}
uint32_t    touchISRCount()        { return 0; }
void        setTouchDebug(bool)    {}
static void touchAt(uint16_t x, uint16_t y) { st_touch = TouchResult(); st_touch.count = 1; st_touch.points[0].x = x; st_touch.points[0].y = y; }
static void touch2(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  st_touch = TouchResult(); st_touch.count = 2;
  st_touch.points[0].x = x0; st_touch.points[0].y = y0; st_touch.points[0].id = 0;
  st_touch.points[1].x = x1; st_touch.points[1].y = y1; st_touch.points[1].id = 1;
}
static void untouch() { st_touch = TouchResult(); }

// ---- Driving the panel ------------------------------------------------------------
static void frames(int n) { for (int i = 0; i < n; i++) { loop(); stubMillis() += 50; } }
static void feed(byte t, const void *p, byte n) { onSimpitMessage(t, (byte *)p, n); }
static void scene(uint8_t v) { feed(SCENE_CHANGE_MESSAGE, &v, 1); }   // 0 flight, 1 not
static void vesselChange(uint8_t v) { feed(VESSEL_CHANGE_MESSAGE, &v, 1); }   // 1 switch, 2 dock, 3 undock
static const uint8_t ST_PROCEED = 0x20, ST_DEMO = 0x02;
// A tap: touch, a few frames, release, a few frames (past the debounce).
static void tap(uint16_t x, uint16_t y) { touchAt(x, y); frames(2); untouch(); frames(10); }   // 600 ms: past the 500 ms debounce

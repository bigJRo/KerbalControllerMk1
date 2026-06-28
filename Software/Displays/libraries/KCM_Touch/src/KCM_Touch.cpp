#include "KCM_Touch.h"

/***************************************************************************************
   KCM_Touch — implementation. Bit-banged I2C master + FT5316 reader.
   See KCM_Touch.h for the protocol and bus notes.
****************************************************************************************/

static bool     _touchDebug = false;
static uint32_t _touchEvents = 0;
static bool     _wasTouched  = false;

void setTouchDebug(bool enable) { _touchDebug = enable; }

// --- Timing -----------------------------------------------------------------
// ~2.5us half-period -> ~200kHz, comfortably within FT5316's 400kHz max and slow
// enough for reliable bit-bang on the 10k pull-ups. Tune up if too slow.
#define I2C_HALF_US 3

static inline void sclHigh() { pinMode(KCM_CTP_SCL, INPUT);  }   // release (pull-up)
static inline void sclLow()  { pinMode(KCM_CTP_SCL, OUTPUT); digitalWriteFast(KCM_CTP_SCL, LOW); }
static inline void sdaHigh() { pinMode(KCM_CTP_SDA, INPUT);  }
static inline void sdaLow()  { pinMode(KCM_CTP_SDA, OUTPUT); digitalWriteFast(KCM_CTP_SDA, LOW); }
static inline bool sdaRead() { return digitalReadFast(KCM_CTP_SDA); }
static inline void halfDelay() { delayMicroseconds(I2C_HALF_US); }

// Clock-stretch aware SCL release: wait until the slave lets SCL rise.
static void sclRelease() {
  pinMode(KCM_CTP_SCL, INPUT);
  uint32_t t0 = micros();
  while (!digitalReadFast(KCM_CTP_SCL)) {           // slave holding SCL low
    if ((micros() - t0) > 2000) break;              // 2ms stretch timeout
  }
}

static void i2cStart() {
  sdaHigh(); sclRelease(); halfDelay();
  sdaLow();  halfDelay();
  sclLow();  halfDelay();
}
static void i2cStop() {
  sdaLow();  halfDelay();
  sclRelease(); halfDelay();
  sdaHigh(); halfDelay();
}

// Write one byte, return true if the slave ACKed.
static bool i2cWrite(uint8_t b) {
  for (uint8_t i = 0; i < 8; i++) {
    if (b & 0x80) sdaHigh(); else sdaLow();
    b <<= 1;
    halfDelay();
    sclRelease(); halfDelay();
    sclLow();     halfDelay();
  }
  // ACK clock
  sdaHigh();                       // release SDA for the slave
  halfDelay();
  sclRelease(); halfDelay();
  bool ack = !sdaRead();           // ACK == slave pulls SDA low
  sclLow(); halfDelay();
  return ack;
}

// Read one byte; send ACK (more to read) or NACK (last byte).
static uint8_t i2cRead(bool ack) {
  uint8_t v = 0;
  sdaHigh();                       // release SDA so the slave can drive it
  for (uint8_t i = 0; i < 8; i++) {
    v <<= 1;
    sclRelease(); halfDelay();
    if (sdaRead()) v |= 1;
    sclLow(); halfDelay();
  }
  // ACK/NACK bit
  if (ack) sdaLow(); else sdaHigh();
  halfDelay();
  sclRelease(); halfDelay();
  sclLow(); halfDelay();
  sdaHigh();
  return v;
}

// Read `len` bytes from FT5316 starting at register `reg`. Returns false on NACK.
static bool ftReadRegs(uint8_t reg, uint8_t *buf, uint8_t len) {
  i2cStart();
  if (!i2cWrite((KCM_CTP_I2C_ADDR << 1) | 0)) { i2cStop(); return false; }
  if (!i2cWrite(reg))                          { i2cStop(); return false; }
  i2cStart();   // repeated start
  if (!i2cWrite((KCM_CTP_I2C_ADDR << 1) | 1)) { i2cStop(); return false; }
  for (uint8_t i = 0; i < len; i++)
    buf[i] = i2cRead(i < (len - 1));   // ACK all but the last
  i2cStop();
  return true;
}

void setupTouch() {
  // Idle bus high.
  sdaHigh(); sclHigh();

  // Hardware reset pulse on CTP_/RST (active low).
  pinMode(KCM_CTP_RST, OUTPUT);
  digitalWriteFast(KCM_CTP_RST, HIGH); delay(5);
  digitalWriteFast(KCM_CTP_RST, LOW);  delay(10);
  digitalWriteFast(KCM_CTP_RST, HIGH); delay(300);   // FT5316 boot time

  pinMode(KCM_CTP_INT, INPUT);   // available for an optional fast-path; unused here

  if (_touchDebug) {
    uint8_t id = 0;
    bool ok = ftReadRegs(0xA8, &id, 1);   // FT5x06 vendor/chip id register
    Serial.print(F("KCM_Touch: FT5316 probe "));
    Serial.println(ok ? F("ACK") : F("no-ACK"));
  }
}

bool isTouched() {
  uint8_t status = 0;
  if (!ftReadRegs(0x02, &status, 1)) return false;
  return (status & 0x0F) > 0;
}

static inline uint16_t mapX(uint16_t x, uint16_t y) {
  uint16_t v = KCM_CTP_SWAP_XY ? y : x;
  if (KCM_CTP_INVERT_X) v = (KCM_SCREEN_W - 1) - v;
  return v;
}
static inline uint16_t mapY(uint16_t x, uint16_t y) {
  uint16_t v = KCM_CTP_SWAP_XY ? x : y;
  if (KCM_CTP_INVERT_Y) v = (KCM_SCREEN_H - 1) - v;
  return v;
}

TouchResult readTouch() {
  TouchResult r;
  uint8_t buf[1 + 6 * CTP_MAX_TOUCHES];
  if (!ftReadRegs(0x02, buf, sizeof(buf))) return r;

  uint8_t n = buf[0] & 0x0F;
  if (n > CTP_MAX_TOUCHES) n = CTP_MAX_TOUCHES;
  r.count = n;

  for (uint8_t i = 0; i < n; i++) {
    const uint8_t *p = &buf[1 + i * 6];     // P*_XH, P*_XL, P*_YH, P*_YL
    uint16_t rawX = (uint16_t)((p[0] & 0x0F) << 8) | p[1];
    uint16_t rawY = (uint16_t)((p[2] & 0x0F) << 8) | p[3];
    uint8_t  id   = (p[2] >> 4) & 0x0F;
    r.points[i].x  = mapX(rawX, rawY);
    r.points[i].y  = mapY(rawX, rawY);
    r.points[i].id = id;
    if (_touchDebug) {
      Serial.print(F("KCM_Touch: pt")); Serial.print(i);
      Serial.print(F(" raw(")); Serial.print(rawX); Serial.print(',');
      Serial.print(rawY); Serial.print(F(") -> ("));
      Serial.print(r.points[i].x); Serial.print(',');
      Serial.print(r.points[i].y); Serial.println(')');
    }
  }

  // Rising-edge event counter (touchISRCount API compat).
  bool now = (n > 0);
  if (now && !_wasTouched) _touchEvents++;
  _wasTouched = now;
  return r;
}

void     clearTouchISR()  { /* polling driver — nothing to clear */ }
uint32_t touchISRCount()  { return _touchEvents; }

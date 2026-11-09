// Tester simulation core: runs a per-module tester sketch against the REAL
// module firmware (loaded from a shared library so that RST reloads it with
// fresh globals, like a hardware reset), with a scripted operator.
//
//   sim <module.so> <scenario> [--int-pin N] [--addr 0xNN] [--shots DIR]
//       [--limit SECONDS] [--opt key=value ...]
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_FT6206.h>
#include <tinyNeoPixel_Static.h>
#include <ShiftIn.h>
#include "sim.h"
#include "sim_font.h"
#include <dlfcn.h>
#include <map>
#include <string>
#include <vector>
#include <regex>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>

// ------------------------------------------------------------
//  Global state
// ------------------------------------------------------------
static uint64_t g_us = 0;
static SimCpu   g_cpu = CPU_TESTER;
SimCpu sim_cpu() { return g_cpu; }
uint8_t SREG = 0;
SimSerial Serial;
TwoWire Wire;
SimPort PORTA = {{0}, {0}, {0}, {0}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
SimPort PORTB = {{1}, {1}, {1}, {1}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
SimPort PORTC = {{2}, {2}, {2}, {2}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static std::map<std::string, std::string> g_opts;
static std::string g_shotDir;
static std::string g_serialLog;
static FILE* g_log = stdout;

// Module
static std::string g_soPath;
static void* g_so = nullptr;
static void (*g_modSetup)() = nullptr;
static void (*g_modLoop)() = nullptr;
static void (*g_isr[3])() = {nullptr, nullptr, nullptr};
static bool g_plugged = true;
static uint8_t g_modAddr = 0x20;
static int  g_intPin = PIN_PA1;
static bool g_rstLow = false;
static bool g_rstDead = false;

static uint8_t g_mMode[22], g_mOut[22], g_mIn[22];
static int     g_mAnalog[22], g_mPwm[22];
static uint8_t g_portFlags[3];
static uint32_t g_kbc = 0;          // KBC input levels (index bits)
static uint32_t g_kbcDead = 0;      // inputs that never read pressed
static uint32_t g_kbcStuck = 0;     // inputs that always read pressed
struct Neo { uint8_t rgb[72]; uint16_t n; };
static std::map<uint8_t, Neo> g_neo;

// MAX7219
static uint16_t g_maxSr = 0;
static uint8_t  g_maxReg[16];

// Tester pins
static uint8_t g_tMode[11], g_tOut[11];

// Touch
static int16_t g_touchX = 0, g_touchY = 0;
static uint64_t g_touchUntil = 0;

// Operator
static Scenario* g_sc = nullptr;
struct Act { uint64_t at; sim::ActFn fn; int arg; };
static std::vector<Act> g_acts;

// ------------------------------------------------------------
//  Logging / options
// ------------------------------------------------------------
void sim::log(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(g_log, "[%7.3f] ", g_us / 1e6);
    vfprintf(g_log, fmt, ap);
    fprintf(g_log, "\n");
    va_end(ap);
}
std::string sim::opt(const char* k, const char* d) {
    auto it = g_opts.find(k);
    return it == g_opts.end() ? std::string(d) : it->second;
}
int sim::opt_int(const char* k, int d) {
    auto it = g_opts.find(k);
    return it == g_opts.end() ? d : (int)strtol(it->second.c_str(), nullptr, 0);
}

void SimSerial::out(const char* s) {
    if (g_cpu == CPU_TESTER) { g_serialLog += s; }
}

// ------------------------------------------------------------
//  Time
// ------------------------------------------------------------
uint32_t millis() { return (uint32_t)(g_us / 1000); }
uint32_t micros() { return (uint32_t)g_us; }
uint32_t sim::now_ms() { return millis(); }
static void runModuleLoop();
static void operatorTick();
static void scenarioTick();

static uint64_t g_lastOp = 0, g_lastPhys = 0;
enum { ADV_TIME, ADV_ALL, ADV_SCENARIO };
// ADV_ALL: the tester waits, the module and the operator run.
// ADV_SCENARIO: the module waits inside delay(); the operator's physical
// actions and checks still run (a construction test does most of its work in
// setup()), but not the module loop or the tester.
static bool g_done = false;
static bool g_finished = false, g_finishOk = false;
static std::string g_finishMsg;
static void advance(uint64_t us, int mode) {
    uint64_t end = g_us + us;
    while (g_us < end && !g_done) {
        uint64_t step = std::min<uint64_t>(100, end - g_us);
        g_us += step;
        if (mode == ADV_TIME) continue;
        if (mode == ADV_ALL) runModuleLoop();
        if (g_us - g_lastPhys >= 1000) { g_lastPhys = g_us; scenarioTick(); }
        if (mode == ADV_ALL && g_us - g_lastOp >= 20000) { g_lastOp = g_us; operatorTick(); }
    }
    if (g_done && g_us < end) g_us = end;
}
void delay(uint32_t ms) { advance((uint64_t)ms * 1000, g_cpu == CPU_TESTER ? ADV_ALL : ADV_SCENARIO); }
void delayMicroseconds(uint32_t us) { advance(us, ADV_TIME); }
void cli() {}
void sei() {}

// ------------------------------------------------------------
//  Pins
// ------------------------------------------------------------
static void moduleReset();
static void maxPinChange(uint8_t pin, uint8_t old, uint8_t now);

void pinMode(uint8_t pin, uint8_t mode) {
    if (g_cpu == CPU_TESTER) { if (pin < 11) g_tMode[pin] = mode; return; }
    if (pin < 22) g_mMode[pin] = mode;
}

static void moduleOut(uint8_t pin, uint8_t v) {
    if (pin >= 22) return;
    uint8_t old = g_mOut[pin];
    g_mOut[pin] = v ? 1 : 0;
    if (old != g_mOut[pin]) maxPinChange(pin, old, g_mOut[pin]);
}

void digitalWrite(uint8_t pin, uint8_t v) {
    if (g_cpu == CPU_TESTER) {
        if (pin >= 11) return;
        uint8_t old = g_tOut[pin];
        g_tOut[pin] = v ? 1 : 0;
        if (pin == D2) {                                   // module RST, active low
            if (!v) g_rstLow = true;
            else if (old == 0 && g_rstLow) {
                g_rstLow = false;
                if (!g_rstDead && g_plugged) moduleReset();
            }
        }
        return;
    }
    moduleOut(pin, v);
}

static int moduleLevel(uint8_t pin) {
    if (pin >= 22) return 0;
    if (g_mMode[pin] == OUTPUT) return g_mOut[pin];
    return g_mIn[pin];
}

int digitalRead(uint8_t pin) {
    if (g_cpu == CPU_TESTER) {
        if (pin == D1) {                                   // module INT, pulled up on the tester
            if (!g_plugged || !g_so) return HIGH;
            return (g_mMode[g_intPin] == OUTPUT) ? g_mOut[g_intPin] : HIGH;
        }
        return pin < 11 ? g_tOut[pin] : 0;
    }
    return moduleLevel(pin);
}

int analogRead(uint8_t pin) { return (g_cpu == CPU_MODULE && pin < 22) ? g_mAnalog[pin] : 0; }
void analogWrite(uint8_t pin, int v) {
    if (g_cpu == CPU_MODULE && pin < 22) { g_mPwm[pin] = v; g_mMode[pin] = OUTPUT; }
}

// Ports
uint8_t sim_port_in(int port) {
    uint8_t v = 0;
    for (int b = 0; b < 8; b++) {
        int pin = port * 8 + b;
        if (pin < 22 && moduleLevel(pin)) v |= 1 << b;
    }
    return v;
}
void sim_port_outset(int port, uint8_t m) {
    for (int b = 0; b < 8; b++) if (m & (1 << b)) moduleOut(port * 8 + b, 1);
}
void sim_port_outclr(int port, uint8_t m) {
    for (int b = 0; b < 8; b++) if (m & (1 << b)) moduleOut(port * 8 + b, 0);
}
uint8_t sim_port_flags(int port) { return g_portFlags[port]; }
void sim_port_flags_clear(int port, uint8_t m) { g_portFlags[port] &= ~m; }

static uint8_t* pinCtrl(int pin) {
    SimPort* p = pin < 8 ? &PORTA : (pin < 16 ? &PORTB : &PORTC);
    return &p->PIN0CTRL + (pin % 8);
}

static int g_deadPin = -1;
static void setModuleInput(uint8_t pin, bool level) {
    if (pin >= 22 || (int)pin == g_deadPin) return;
    uint8_t old = g_mIn[pin];
    g_mIn[pin] = level ? 1 : 0;
    if (!old && level && g_so && g_plugged) {
        uint8_t isc = *pinCtrl(pin) & 0x07;
        if (isc == PORT_ISC_RISING_gc || isc == PORT_ISC_BOTHEDGES_gc) {
            int port = pin / 8;
            g_portFlags[port] |= 1 << (pin % 8);
            if (g_isr[port]) { SimCpu c = g_cpu; g_cpu = CPU_MODULE; g_isr[port](); g_cpu = c; }
        }
    }
}

// MAX7219 bit-bang decoder: CLK PA7, DATA PA6, LOAD PA5
static void maxPinChange(uint8_t pin, uint8_t old, uint8_t now) {
    if (pin == PIN_PA7 && !old && now) g_maxSr = (uint16_t)((g_maxSr << 1) | g_mOut[PIN_PA6]);
    if (pin == PIN_PA5 && !old && now) {
        uint8_t reg = (g_maxSr >> 8) & 0x0F;
        g_maxReg[reg] = g_maxSr & 0xFF;
    }
}

std::string sim::seg7() {
    if (g_maxReg[0x0F] & 1) return "8.8.8.8.";
    if (!(g_maxReg[0x0C] & 1)) return "";
    std::string s;
    for (int d = 1; d <= 4; d++) {
        uint8_t v = g_maxReg[d];
        uint8_t code = v & 0x0F;
        char ch = ' ';
        if (g_maxReg[0x09] & (1 << (d - 1))) {
            if (code <= 9) ch = '0' + code;
            else if (code == 0x0A) ch = '-';
            else if (code == 0x0F) ch = ' ';
            else ch = "-EHLP "[code - 0x0A];
        } else ch = v ? '?' : ' ';
        s += ch;
        if (v & 0x80) s += '.';
    }
    return s;
}
int sim::seg7_intensity() { return g_maxReg[0x0A] & 0x0F; }

// NeoPixels
void sim_neo_show(uint8_t pin, const uint8_t* rgb, uint16_t n) {
    Neo& z = g_neo[pin];
    z.n = n;
    memcpy(z.rgb, rgb, std::min<int>(n * 3, 72));
}
bool sim::led(uint8_t pin, uint8_t idx, uint8_t rgb[3]) {
    auto it = g_neo.find(pin);
    if (it == g_neo.end() || idx >= it->second.n) { rgb[0] = rgb[1] = rgb[2] = 0; return false; }
    memcpy(rgb, it->second.rgb + idx * 3, 3);
    return true;
}

// ShiftIn: KBC index -> raw chain bit (inverse of KBC_SR_BUTTON_MAP)
uint32_t sim_shiftin_raw(uint8_t chips) {
    uint32_t lv = (g_kbc & ~g_kbcDead) | g_kbcStuck;
    uint32_t raw = 0;
    for (int k = 0; k < (chips == 3 ? 24 : 16); k++) {
        if (!(lv & (1UL << k))) continue;
        int bit;
        if (chips == 3) bit = k < 8 ? k + 16 : (k < 16 ? k : k - 16);
        else bit = k < 8 ? k + 8 : k - 8;
        raw |= 1UL << bit;
    }
    return raw;
}

// ------------------------------------------------------------
//  Module power / loading
// ------------------------------------------------------------
static void moduleUnload() {
    if (!g_so) return;
    Wire.onReceive(nullptr);
    Wire.onRequest(nullptr);
    dlclose(g_so);
    g_so = nullptr;
    g_modSetup = g_modLoop = nullptr;
    g_isr[0] = g_isr[1] = g_isr[2] = nullptr;
}

static void moduleLoad() {
    memset(g_mMode, 0, sizeof g_mMode);
    memset(g_mOut, 0, sizeof g_mOut);
    memset(g_mPwm, 0, sizeof g_mPwm);
    memset(g_maxReg, 0, sizeof g_maxReg);
    memset(g_portFlags, 0, sizeof g_portFlags);
    for (SimPort* p : {&PORTA, &PORTB, &PORTC}) {
        p->PIN0CTRL = p->PIN1CTRL = p->PIN2CTRL = p->PIN3CTRL = 0;
        p->PIN4CTRL = p->PIN5CTRL = p->PIN6CTRL = p->PIN7CTRL = 0;
    }
    g_neo.clear();
    g_so = dlopen(g_soPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!g_so) { fprintf(stderr, "dlopen: %s\n", dlerror()); exit(3); }
    g_modSetup = (void (*)())dlsym(g_so, "sim_module_setup");
    g_modLoop  = (void (*)())dlsym(g_so, "sim_module_loop");
    g_isr[0] = (void (*)())dlsym(g_so, "sim_isr_PORTA_PORT_vect");
    g_isr[1] = (void (*)())dlsym(g_so, "sim_isr_PORTB_PORT_vect");
    g_isr[2] = (void (*)())dlsym(g_so, "sim_isr_PORTC_PORT_vect");
    if (!g_modSetup || !g_modLoop) { fprintf(stderr, "module_setup/loop missing\n"); exit(3); }
    SimCpu c = g_cpu;
    g_cpu = CPU_MODULE;
    g_modSetup();
    g_cpu = c;
}

static void moduleReset() {
    sim::log("module reset (RST pulse)");
    moduleUnload();
    moduleLoad();
}

void sim::unplug() { sim::log("operator unplugs the module"); moduleUnload(); g_plugged = false; }
void sim::plug()   { sim::log("operator plugs the module in"); g_plugged = true; moduleLoad(); }
bool sim::plugged() { return g_plugged; }

static bool g_inModuleLoop = false;
static void runModuleLoop() {
    if (!g_plugged || !g_so || g_inModuleLoop) return;
    g_inModuleLoop = true;
    SimCpu c = g_cpu;
    g_cpu = CPU_MODULE;
    g_modLoop();
    g_cpu = c;
    g_inModuleLoop = false;
}

// ------------------------------------------------------------
//  Wire: controller side (tester) and target side (module)
// ------------------------------------------------------------
static uint8_t g_txAddr = 0;
static std::vector<uint8_t> g_txBuf, g_rxBuf;   // controller
static size_t g_rxPos = 0;
static std::vector<uint8_t> g_tgtRx, g_tgtTx;   // target
static size_t g_tgtRxPos = 0;
static void (*g_onRecv)(int) = nullptr;
static void (*g_onReq)() = nullptr;
static uint8_t g_inaPtr = 0;

void TwoWire::begin() {}
void TwoWire::begin(uint8_t addr) { if (g_cpu == CPU_MODULE) g_modAddr = addr; }
void TwoWire::onReceive(void (*fn)(int)) { g_onRecv = fn; }
void TwoWire::onRequest(void (*fn)()) { g_onReq = fn; }

void TwoWire::beginTransmission(uint8_t addr) {
    if (g_cpu == CPU_TESTER) { g_txAddr = addr; g_txBuf.clear(); }
}

size_t TwoWire::write(uint8_t b) {
    if (g_cpu == CPU_TESTER) g_txBuf.push_back(b);
    else g_tgtTx.push_back(b);
    return 1;
}

static bool moduleOnBus(uint8_t addr) { return g_plugged && g_so && addr == g_modAddr && g_onRecv; }

// INA228 model: module supply current from what the module is driving.
static double moduleCurrentA() {
    if (!g_plugged || !g_so) return 0.0;
    double mA = 6.0;
    for (auto& kv : g_neo)
        for (int i = 0; i < kv.second.n * 3; i++) mA += kv.second.rgb[i] * 20.0 / 255.0;
    std::string s = sim::seg7();
    if (!s.empty()) mA += (s == "8.8.8.8." ? 90.0 : 30.0);
    if (g_mOut[PIN_PC3] && g_mMode[PIN_PC3] == OUTPUT && g_neo.empty()) mA += 20.0;  // throttle lamps
    if (g_mOut[PIN_PA7] != g_mOut[PIN_PA6] && g_mMode[PIN_PA7] == OUTPUT && g_mMode[PIN_PA6] == OUTPUT)
        mA += 300.0 * g_mPwm[PIN_PC0] / 255.0;                                      // throttle motor
    return mA / 1000.0;
}

static uint32_t inaReg(uint8_t reg) {
    const double lsb = 5.0 / 524288.0;
    double v = g_plugged ? 5.0 : 0.0, i = moduleCurrentA();
    switch (reg) {
        case 0x05: return ((uint32_t)(v / 195.3125e-6)) << 4;
        case 0x07: return ((uint32_t)(i / lsb) & 0xFFFFF) << 4;
        case 0x08: return (uint32_t)(v * i / (3.2 * lsb));
        default:   return 0;
    }
}

uint8_t TwoWire::endTransmission(bool) {
    if (g_cpu != CPU_TESTER) return 0;
    if (g_txAddr == 0x40) { if (!g_txBuf.empty()) g_inaPtr = g_txBuf[0]; return 0; }
    if (g_txAddr == 0x38) return 0;
    if (!moduleOnBus(g_txAddr)) return 2;                  // address NACK
    g_tgtRx = g_txBuf;
    g_tgtRxPos = 0;
    SimCpu c = g_cpu;
    g_cpu = CPU_MODULE;
    g_onRecv((int)g_tgtRx.size());
    g_cpu = c;
    return 0;
}

uint8_t TwoWire::requestFrom(int addr, int n) {
    g_rxBuf.clear();
    g_rxPos = 0;
    if (addr == 0x40) {
        uint32_t v = inaReg(g_inaPtr);
        for (int i = n - 1; i >= 0; i--) g_rxBuf.push_back((v >> (8 * i)) & 0xFF);
        return (uint8_t)n;
    }
    if (!moduleOnBus((uint8_t)addr)) return 0;
    g_tgtTx.clear();
    SimCpu c = g_cpu;
    g_cpu = CPU_MODULE;
    if (g_onReq) g_onReq();
    g_cpu = c;
    for (int i = 0; i < n; i++) g_rxBuf.push_back(i < (int)g_tgtTx.size() ? g_tgtTx[i] : 0xFF);
    return (uint8_t)n;
}

int TwoWire::available() {
    if (g_cpu == CPU_TESTER) return (int)(g_rxBuf.size() - g_rxPos);
    return (int)(g_tgtRx.size() - g_tgtRxPos);
}

int TwoWire::read() {
    if (g_cpu == CPU_TESTER) return g_rxPos < g_rxBuf.size() ? g_rxBuf[g_rxPos++] : -1;
    return g_tgtRxPos < g_tgtRx.size() ? g_tgtRx[g_tgtRxPos++] : -1;
}

// ------------------------------------------------------------
//  Framebuffer + text record
// ------------------------------------------------------------
static uint16_t g_fb[320][240];
static std::vector<SimText> g_texts;

static void dropTexts(int x, int y, int w, int h) {
    g_texts.erase(std::remove_if(g_texts.begin(), g_texts.end(), [&](const SimText& t) {
        return t.x < x + w && t.x + t.w > x && t.y < y + h && t.y + t.h > y;
    }), g_texts.end());
}

void Adafruit_GFX::drawPixel(int16_t x, int16_t y, uint16_t c) {
    if (x >= 0 && x < 240 && y >= 0 && y < 320) g_fb[y][x] = c;
}
void Adafruit_GFX::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) drawPixel(i, j, c);
    dropTexts(x, y, w, h);
}
void Adafruit_GFX::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
    for (int i = x; i < x + w; i++) { drawPixel(i, y, c); drawPixel(i, y + h - 1, c); }
    for (int j = y; j < y + h; j++) { drawPixel(x, j, c); drawPixel(x + w - 1, j, c); }
}
void Adafruit_GFX::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
        int dx = i < r ? r - i : (i >= w - r ? i - (w - r - 1) : 0);
        int dy = j < r ? r - j : (j >= h - r ? j - (h - r - 1) : 0);
        if (dx * dx + dy * dy <= r * r + 1) drawPixel(x + i, y + j, c);
    }
    dropTexts(x, y, w, h);
}
void Adafruit_GFX::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c) {
    for (int i = x + r; i < x + w - r; i++) { drawPixel(i, y, c); drawPixel(i, y + h - 1, c); }
    for (int j = y + r; j < y + h - r; j++) { drawPixel(x, j, c); drawPixel(x + w - 1, j, c); }
    for (int a = 0; a <= r; a++) {
        int b = (int)lround(sqrt((double)r * r - (double)a * a));
        int pts[4][2] = {{x + r - a, y + r - b}, {x + w - 1 - r + a, y + r - b},
                         {x + r - a, y + h - 1 - r + b}, {x + w - 1 - r + a, y + h - 1 - r + b}};
        for (auto& p : pts) drawPixel(p[0], p[1], c);
    }
}
size_t Adafruit_GFX::print(const char* s) {
    int n = (int)strlen(s);
    int x0 = _cx, y0 = _cy;
    for (int k = 0; k < n; k++) {
        unsigned char ch = (unsigned char)s[k];
        if (ch < 32 || ch > 126) ch = '?';
        for (int col = 0; col < 6; col++) {
            uint8_t bits = col < 5 ? SIM_FONT[ch - 32][col] : 0;
            for (int row = 0; row < 8; row++) {
                bool on = (bits >> row) & 1;
                for (int a = 0; a < _ts; a++) for (int b = 0; b < _ts; b++) {
                    int px = _cx + col * _ts + a, py = _cy + row * _ts + b;
                    if (on) drawPixel(px, py, _fg);
                    else if (_bg != _fg) drawPixel(px, py, _bg);
                }
            }
        }
        _cx += 6 * _ts;
    }
    dropTexts(x0, y0, n * 6 * _ts, 8 * _ts);
    g_texts.push_back({x0, y0, n * 6 * _ts, 8 * _ts, std::string(s)});
    return n;
}

std::string sim::text_in(int y0, int y1) {
    std::vector<SimText> v;
    for (auto& t : g_texts) if (t.y >= y0 && t.y < y1) v.push_back(t);
    std::sort(v.begin(), v.end(), [](const SimText& a, const SimText& b) {
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    std::string s;
    int lastY = -100;
    for (auto& t : v) {
        if (!s.empty()) s += (t.y != lastY) ? " " : " ";
        s += t.s;
        lastY = t.y;
    }
    return s;
}

std::vector<SimText> sim::buttons() {
    std::vector<SimText> v;
    for (auto& t : g_texts) if (t.y >= 280) v.push_back(t);
    std::sort(v.begin(), v.end(), [](const SimText& a, const SimText& b) { return a.x < b.x; });
    return v;
}
bool sim::has_button(const char* l) {
    for (auto& t : sim::buttons()) if (t.s == l) return true;
    return false;
}
void sim::tap(int x, int y) {
    g_touchX = (int16_t)x; g_touchY = (int16_t)y;
    g_touchUntil = g_us + 80000;
}
bool sim::tap_button(const char* l) {
    for (auto& t : sim::buttons()) if (t.s == l) { sim::tap(t.x + t.w / 2, t.y + 4); sim::log("tap %s", l); return true; }
    return false;
}
bool sim_touch(int16_t& x, int16_t& y) {
    if (g_us >= g_touchUntil) return false;
    x = g_touchX; y = g_touchY;
    return true;
}

void sim::screenshot(const std::string& name) {
    if (g_shotDir.empty()) return;
    std::string path = g_shotDir + "/" + name + ".ppm";
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return;
    fprintf(f, "P6\n240 320\n255\n");
    for (int y = 0; y < 320; y++) for (int x = 0; x < 240; x++) {
        uint16_t c = g_fb[y][x];
        uint8_t px[3] = {(uint8_t)(((c >> 11) & 0x1F) * 255 / 31), (uint8_t)(((c >> 5) & 0x3F) * 255 / 63),
                         (uint8_t)((c & 0x1F) * 255 / 31)};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

// ------------------------------------------------------------
//  Operator inputs
// ------------------------------------------------------------
void sim::kbc_set(uint8_t idx, bool on) { if (on) g_kbc |= 1UL << idx; else g_kbc &= ~(1UL << idx); }
void sim::pin_set(uint8_t pin, bool level) { setModuleInput(pin, level); }
void sim::analog_set(uint8_t pin, int v) { if (pin < 22) g_mAnalog[pin] = std::max(0, std::min(1023, v)); }
int  sim::analog_get(uint8_t pin) { return pin < 22 ? g_mAnalog[pin] : 0; }
bool sim::pin_out(uint8_t pin) { return pin < 22 && g_mOut[pin]; }
int  sim::pwm_out(uint8_t pin) { return pin < 22 ? g_mPwm[pin] : 0; }
void sim::enc_click(uint8_t a, uint8_t b, int dir) {
    // Quadrature detent: B is sampled on A's rising edge. B low = +1 (CW).
    setModuleInput(b, dir < 0);
    setModuleInput(a, 0);
    setModuleInput(a, 1);
    setModuleInput(a, 0);
    setModuleInput(b, 0);
}
void sim::at(uint32_t ms, sim::ActFn fn, int arg) { g_acts.push_back({g_us + (uint64_t)ms * 1000, fn, arg}); }
bool sim::busy() { return !g_acts.empty(); }
void sim::finish(bool ok, const std::string& msg) {
    if (g_done) return;
    g_done = true;
    g_finished = true;
    g_finishOk = ok;
    g_finishMsg = msg;
    sim::log("construction test %s%s%s", ok ? "PASS" : "FAIL", msg.empty() ? "" : ": ", msg.c_str());
    // Exit now: the module firmware may be inside a wait loop of its own.
    printf("SUMMARY %s\n", ok ? "PASS" : "FAIL");
    if (!msg.empty()) printf("CT %s: %s\n", ok ? "NOTE" : "FAIL", msg.c_str());
    fflush(stdout);
    _exit(ok ? 0 : 1);
}

static void scenarioTick() {
    for (size_t i = 0; i < g_acts.size();) {
        if (g_acts[i].at <= g_us) { Act a = g_acts[i]; g_acts.erase(g_acts.begin() + i); a.fn(a.arg); }
        else i++;
    }
    if (g_sc) g_sc->tick();
}

// ------------------------------------------------------------
//  Generic operator
// ------------------------------------------------------------
static bool g_started = false;
static std::string g_lastKey;
static uint64_t g_lastAct = 0;
static int g_shotN = 0;
static std::string g_lastShotKey;
static std::vector<std::string> g_failedSteps;
static int g_fails = 0;

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

static void unplugReplug(int phase) {
    if (phase == 0) { sim::unplug(); sim::at(2500, unplugReplug, 1); }
    else sim::plug();
}

static bool genericLedCheck(const std::string& instr, int& verdict) {
    static const std::regex re("Is (.+) lit ([A-Z_ ]+), and only that one\\?");
    std::smatch m;
    if (!std::regex_search(instr, m, re)) return false;
    uint8_t pin, idx;
    if (!g_sc->ledOf(m[1].str(), pin, idx)) { verdict = 1; return true; }
    uint8_t rgb[3];
    sim::led(pin, idx, rgb);
    // Compare with the palette colour named in the prompt.
    static const std::map<std::string, std::vector<int>> pal = {
        {"GREEN", {0, 255, 0}}, {"RED", {255, 0, 0}}, {"AMBER", {255, 80, 0}}, {"SKY", {14, 165, 233}},
        {"TEAL", {20, 184, 166}}, {"CORAL", {255, 100, 54}}, {"SEAFOAM", {80, 200, 160}}, {"CYAN", {6, 182, 212}},
        {"PURPLE", {120, 0, 220}}, {"ORANGE", {255, 60, 0}}, {"INDIGO", {60, 0, 200}}, {"YELLOW", {234, 179, 8}},
        {"BLUE", {0, 0, 255}}, {"CHARTREUSE", {163, 230, 53}}, {"PINK", {236, 72, 153}}, {"GOLD", {255, 160, 0}},
        {"LIME", {132, 204, 22}}, {"MAGENTA", {255, 0, 255}}, {"MINT", {100, 255, 160}}, {"ROSE", {255, 80, 120}},
        {"WHITE WARM", {255, 140, 100}}, {"VIOLET", {160, 0, 255}}};
    auto it = pal.find(trim(m[2].str()));
    verdict = (it != pal.end() && rgb[0] == it->second[0] && rgb[1] == it->second[1] && rgb[2] == it->second[2]) ? 1 : 0;
    if (!verdict) sim::log("LED check: %s shows %d,%d,%d, expected %s", m[1].str().c_str(), rgb[0], rgb[1], rgb[2], m[2].str().c_str());
    return true;
}

static void operatorTick() {
    if (g_done || sim::busy() || g_us < g_touchUntil + 60000) return;
    std::string heading = trim(sim::text_in(32, 50));
    std::string instr = trim(sim::text_in(52, 90));
    std::string row0 = trim(sim::text_in(92, 104));
    std::string toast = trim(sim::text_in(262, 278));
    std::string title = trim(sim::text_in(0, 14));
    std::string btns;
    for (auto& b : sim::buttons()) btns += b.s + ",";
    std::string key = heading + "|" + instr + "|" + row0 + "|" + btns;

    if (key != g_lastShotKey && !g_shotDir.empty() && g_shotN < 400) {
        g_lastShotKey = key;
        char nm[64];
        snprintf(nm, sizeof nm, "%03d", g_shotN++);
        sim::screenshot(nm);
    }
    bool same = (key == g_lastKey);
    if (same && g_us - g_lastAct < 2500000) return;
    static int repeat = 0;
    repeat = same ? repeat + 1 : 0;

    static std::string actedKey;
    auto acted = [&]() { g_lastKey = key; g_lastAct = g_us; actedKey = key; };

    // Menu: start the full run once.
    if (!g_started && sim::has_button("RUN ALL")) { sim::tap_button("RUN ALL"); g_started = true; acted(); return; }
    // Summary: finished.
    if (g_started && sim::has_button("MENU") && sim::has_button("RUN ALL")) {
        g_done = true;
        size_t sp = title.find_last_of(' ');
        sim::log("SUMMARY %s", sp == std::string::npos ? title.c_str() : title.c_str() + sp + 1);
        for (int r = 0; r < 12; r++) {
            std::string row = trim(sim::text_in(34 + r * 20, 34 + r * 20 + 18));
            if (!row.empty()) fprintf(g_log, "    %s\n", row.c_str());
        }
        return;
    }
    // Failed step in a full run: note it and continue.
    if (sim::has_button("NEXT") && sim::has_button("RETRY")) {
        sim::log("STEP FAILED: %s -- %s", heading.c_str(), toast.c_str());
        g_failedSteps.push_back(heading);
        g_fails++;
        sim::tap_button("NEXT"); acted(); return;
    }
    if (instr.find("Unplug it") != std::string::npos && sim::has_button("FAIL")) {
        if (repeat == 0) unplugReplug(0);
        acted(); return;
    }
    // A person gives up on an input that does nothing after a few tries.
    if (repeat >= 3 && key == actedKey && sim::has_button("FAIL")) {
        sim::log("operator gives up: %s", row0.empty() ? instr.c_str() : row0.c_str());
        sim::tap_button("FAIL"); acted(); return;
    }
    // Result screens keep the last instruction on screen: only a running step
    // (it always offers SKIP) is acted on.
    if (!sim::has_button("SKIP")) { g_lastKey = key; return; }
    // Physical actions follow the instruction text only (live rows change all the time).
    static std::string lastSpecial;
    static uint64_t lastSpecialAt = 0;
    std::string skey = heading + "|" + instr + "|" + btns;
    if (skey != lastSpecial || g_us - lastSpecialAt > 4000000) {
        if (g_sc->special(heading, instr, row0)) {
            sim::log("operator: %s", instr.c_str());
            lastSpecial = skey; lastSpecialAt = g_us; acted(); return;
        }
    } else if (!sim::has_button("PASS") && !std::regex_search(row0, std::regex("^\\d+/\\d+ "))) {
        g_lastKey = key;
        return;
    }

    // Input prompts: walk row ("3/12  Press AG5.") or instruction.
    static const std::regex walkRe("^\\d+/\\d+ +(Press|Set|Release|Turn|Move|Hold|Return) (.+?)\\.?$");
    static const std::regex instrRe("(Press and release|Press and HOLD) ([^.]+?)(?: for 1 s, then release)?\\.");
    std::smatch m;
    if (std::regex_search(row0, m, walkRe)) {
        if (!g_sc->input(m[1].str(), m[2].str())) sim::log("scenario: no input for '%s %s'", m[1].str().c_str(), m[2].str().c_str());
        acted(); return;
    }
    if (std::regex_search(instr, m, instrRe)) {
        std::string verb = m[1].str() == "Press and HOLD" ? "HOLD" : "Press";
        if (!g_sc->input(verb, trim(m[2].str()))) sim::log("scenario: no input for '%s'", instr.c_str());
        acted(); return;
    }
    // Visual PASS/FAIL checks.
    if (sim::has_button("PASS") && sim::has_button("FAIL")) {
        int v = g_sc->visual(instr);
        if (v < 0 && !genericLedCheck(instr, v)) v = 1;
        sim::log("visual: '%s' -> %s", instr.c_str(), v ? "PASS" : "FAIL");
        sim::tap_button(v ? "PASS" : "FAIL");
        acted(); return;
    }
    g_lastKey = key;   // automatic step: just watch
}

// ------------------------------------------------------------
//  main
// ------------------------------------------------------------
extern void tester_setup();
extern void tester_loop();

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: sim module.so scenario [options]\n"); return 2; }
    g_soPath = argv[1];
    std::string scen = argv[2];
    double limit = 900;
    for (int i = 3; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--int-pin" && i + 1 < argc) g_intPin = atoi(argv[++i]);
        else if (a == "--shots" && i + 1 < argc) { g_shotDir = argv[++i]; mkdir(g_shotDir.c_str(), 0755); }
        else if (a == "--limit" && i + 1 < argc) limit = atof(argv[++i]);
        else if (a == "--opt" && i + 1 < argc) {
            std::string kv = argv[++i];
            size_t eq = kv.find('=');
            g_opts[kv.substr(0, eq)] = eq == std::string::npos ? "1" : kv.substr(eq + 1);
        }
    }
    g_rstDead = sim::opt_int("rst_dead", 0) != 0;
    g_kbcDead = (uint32_t)sim::opt_int("kbc_dead", 0);
    g_kbcStuck = (uint32_t)sim::opt_int("kbc_stuck", 0);
    g_deadPin = sim::opt_int("dead_pin", -1);
    for (int i = 0; i < 22; i++) g_mAnalog[i] = 512;
    g_sc = make_scenario(scen);
    if (!g_sc) { fprintf(stderr, "unknown scenario %s\n", scen.c_str()); return 2; }
    g_sc->begin();          // inputs at power-on (stick centred, lever position ...)
    moduleLoad();
    g_cpu = CPU_TESTER;
    tester_setup();
    while (!g_done && g_us < (uint64_t)(limit * 1e6)) {
        advance(100, ADV_ALL);
        g_cpu = CPU_TESTER;
        tester_loop();
    }
    if (g_finished) {
        printf("SUMMARY %s\n", g_finishOk ? "PASS" : "FAIL");
        if (!g_finishMsg.empty()) printf("CT %s: %s\n", g_finishOk ? "NOTE" : "FAIL", g_finishMsg.c_str());
        return g_finishOk ? 0 : 1;
    }
    if (!g_shotDir.empty()) sim::screenshot("final");
    if (!g_shotDir.empty()) {
        FILE* f = fopen((g_shotDir + "/serial.log").c_str(), "w");
        if (f) { fputs(g_serialLog.c_str(), f); fclose(f); }
    }
    if (!g_done) { sim::log("TIMEOUT: run did not finish"); return 4; }
    printf("%s", g_serialLog.c_str());
    return g_fails ? 1 : 0;
}

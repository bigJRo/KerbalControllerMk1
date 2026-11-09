// Simulated operators, one per module. Each maps the tester's prompts to
// inputs on the simulated module hardware.
#include <Arduino.h>
#include "sim.h"
#include <map>
#include <functional>
#include <algorithm>
#include <math.h>
#include <regex>
#include <string>

static std::string lower(std::string s) { for (auto& ch : s) ch = (char)tolower(ch); return s; }

// ============================================================
//  KerbalButtonCore modules
// ============================================================
struct KbcInput { const char* label; uint8_t idx; const char* on; const char* off; bool momentary; };

static uint8_t g_rel;   // index released by the timed action
static void kbcRelease(int idx) { sim::kbc_set((uint8_t)idx, false); }

struct Rgb { uint8_t r, g, b; };
static bool eq(const uint8_t* p, Rgb c) { return p[0] == c.r && p[1] == c.g && p[2] == c.b; }
static const Rgb C_OFF = {0, 0, 0}, C_WHITE = {255, 255, 255}, C_AMBER = {255, 80, 0}, C_RED = {255, 0, 0},
                 C_CYAN = {6, 182, 212}, C_GREEN = {0, 255, 0};

struct KbcScenario : Scenario {
    std::vector<KbcInput> in;
    std::vector<Rgb> active;       // ACTIVE colour per LED position 0-11
    KbcScenario(std::vector<KbcInput> v, std::vector<Rgb> a) : in(std::move(v)), active(std::move(a)) {}

    // Check all 12 pixels with a per-position predicate.
    template <typename F> int all12(F ok, const char* what) {
        for (uint8_t i = 0; i < 12; i++) {
            uint8_t p[3];
            sim::led(PIN_PA4, i, p);
            if (!ok(i, p)) { sim::log("visual check '%s': LED %u is %u,%u,%u", what, i, p[0], p[1], p[2]); return 0; }
        }
        return 1;
    }
    int visual(const std::string& s) override {
        auto has = [&](const char* k) { return s.find(k) != std::string::npos; };
        auto scaled = [](uint8_t b) { return Rgb{(uint8_t)(255 * b / 255), (uint8_t)(180 * b / 255), (uint8_t)(255 * b / 255)}; };
        if (has("button LEDs off?") || has("LEDs dark?") || has("Brightness 0:"))
            return all12([](uint8_t, const uint8_t* p) { return eq(p, C_OFF); }, "off");
        if (has("dim white (backlight)") || has("Brightness 32"))
            return all12([&](uint8_t, const uint8_t* p) { return eq(p, scaled(32)); }, "enabled 32");
        if (has("Brightness 128"))
            return all12([&](uint8_t, const uint8_t* p) { return eq(p, scaled(128)); }, "enabled 128");
        if (has("Brightness 255"))
            return all12([&](uint8_t, const uint8_t* p) { return eq(p, scaled(255)); }, "enabled 255");
        if (has("ACTIVE colours") || has("the same as ACTIVE"))
            return all12([&](uint8_t i, const uint8_t* p) { return eq(p, active[i]); }, "active");
        if (has("full white"))
            return all12([](uint8_t, const uint8_t* p) { return eq(p, C_WHITE); }, "bulb");
        if (has("Every other button") || has("every-other pattern"))
            return all12([&](uint8_t i, const uint8_t* p) { return eq(p, (i % 2) ? C_OFF : active[i]); }, "pattern");
        if (has("flashing amber"))
            return all12([](uint8_t, const uint8_t* p) { return eq(p, C_AMBER) || eq(p, C_OFF); }, "warning");
        if (has("flashing red"))
            return all12([](uint8_t, const uint8_t* p) { return eq(p, C_RED) || eq(p, C_OFF); }, "alert");
        if (has("steady cyan"))  return all12([](uint8_t, const uint8_t* p) { return eq(p, C_CYAN); }, "armed");
        if (has("steady amber")) return all12([](uint8_t, const uint8_t* p) { return eq(p, C_AMBER); }, "partial");
        if (has("steady red"))   return all12([](uint8_t, const uint8_t* p) { return eq(p, C_RED); }, "cut");
        return -1;
    }

    const KbcInput* find(const std::string& label) {
        for (auto& i : in) if (label == i.label) return &i;
        // switches: "MSTR to RESET" / "MSTR to OPER"
        for (auto& i : in) {
            if (!i.on) continue;
            std::string l = i.label;
            if (label.rfind(l + " to ", 0) == 0 || label.rfind(l + " at ", 0) == 0 ||
                label.rfind(l + " back ", 0) == 0) return &i;
        }
        return nullptr;
    }
    bool input(const std::string& verb, const std::string& label) override {
        const KbcInput* i = find(label);
        if (!i) return false;
        if (verb == "Turn") {                  // Time Control mode selector: 12 = SPC, 13 = RVR
            bool spc = label.find("SPC") != std::string::npos, rvr = label.find("RVR") != std::string::npos;
            sim::kbc_set(12, spc);
            sim::kbc_set(13, rvr);
            return true;
        }
        if (verb == "Press") {
            sim::kbc_set(i->idx, true);
            sim::at(150, kbcRelease, i->idx);
        } else if (verb == "HOLD") {
            sim::kbc_set(i->idx, true);
            sim::at(1300, kbcRelease, i->idx);
        } else if (verb == "Set" || verb == "Hold") {   // switch to its ON position (held until released)
            sim::kbc_set(i->idx, true);
        } else if (verb == "Release" || verb == "Return") {   // back to OFF
            sim::kbc_set(i->idx, false);
        } else return false;
        return true;
    }
    bool ledOf(const std::string& label, uint8_t& pin, uint8_t& idx) override {
        const KbcInput* i = find(label);
        if (!i || i->idx > 11) return false;
        pin = PIN_PA4;
        idx = i->idx;
        return true;
    }
};

static Scenario* actionControl() {
    return new KbcScenario({
        {"AG1", 10}, {"AG2", 8}, {"AG3", 6}, {"AG4", 4}, {"AG5", 2}, {"AG6", 0},
        {"AG7", 11}, {"AG8", 9}, {"AG9", 7}, {"AG10", 5}, {"AG11", 3}, {"AG12", 1}},
        std::vector<Rgb>(12, C_GREEN));
}

// ============================================================
static Scenario* kbc_ui() {
    return new KbcScenario({
        {"MAP ENA", 10}, {"MAP <", 8}, {"MAP >", 6}, {"MAP RESET", 4}, {"UI", 2}, {"SCREENSHOT", 0}, {"IVA", 11}, {"SHIP <", 9}, {"SHIP >", 7}, {"CYCLE NAV", 5}, {"NAV BALL", 3}, {"DEBUG", 1}},
        {{0, 255, 0}, {255, 0, 0}, {0, 255, 0}, {255, 80, 0}, {14, 165, 233}, {0, 255, 0}, {14, 165, 233}, {20, 184, 166}, {14, 165, 233}, {20, 184, 166}, {14, 165, 233}, {255, 100, 54}});
}
static Scenario* kbc_function() {
    return new KbcScenario({
        {"LES", 1}, {"FAIRING", 0}, {"AIR INTAKE", 3}, {"LOCK SURF", 2}, {"AIR BRAKE", 5}, {"FLAPS", 4}, {"ENGINE ALT", 7}, {"SCI COLLECT", 6}, {"ENG GROUP 1", 9}, {"SCI GROUP 1", 8}, {"ENG GROUP 2", 11}, {"SCI GROUP 2", 10}, {"MSTR", 16, "RESET", "OPER", true}, {"DISP", 17, "CLR", "OPER", true}, {"SCE", 18, "AUX", "NORM", false}, {"UPTLM", 19, "XMIT", "INHIB", false}, {"LTG", 20, "TEST", "NORM", true}, {"A/P", 21, "ARM", "INHIB", false}, {"ENGINE", 22, "ARM", "SAFE", false}, {"THRTL", 23, "FINE", "STD", false}},
        {{255, 80, 0}, {255, 0, 0}, {14, 165, 233}, {20, 184, 166}, {80, 200, 160}, {6, 182, 212}, {120, 0, 220}, {255, 60, 0}, {60, 0, 200}, {234, 179, 8}, {0, 0, 255}, {163, 230, 53}});
}
static Scenario* kbc_stability() {
    return new KbcScenario({
        {"TARGET", 0}, {"ANTI TARGET", 1}, {"RADIAL IN", 2}, {"RADIAL OUT", 3}, {"NORMAL", 4}, {"ANTI NORMAL", 5}, {"PROGRADE", 6}, {"RETROGRADE", 7}, {"STAB ASSIST", 8}, {"MANEUVER", 9}, {"HOLD", 10}, {"INVERT", 11}},
        {{0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {255, 0, 255}, {255, 80, 0}});
}
static Scenario* kbc_vehicle() {
    return new KbcScenario({
        {"BRAKES", 0}, {"LIGHTS", 1}, {"ANTENNA", 2}, {"GEAR", 3}, {"FUEL CELL", 4}, {"SOLAR", 5}, {"CARGO DOOR", 6}, {"RADIATOR", 7}, {"HEAT SHIELD", 8}, {"LADDER", 9}, {"MAIN CHUTE", 10}, {"DROGUE", 11}, {"CHUTE", 16, "ARM", "SAFE", false}, {"GEAR", 17, "DOWN", "UP", false}, {"BRAKE", 18, "LOCK", "REL", false}, {"EXT LT", 19, "ILLUM", "OFF", false}, {"SAS", 20, "ENAB", "OFF", false}, {"RCS", 21, "ENAB", "OFF", false}, {"THC/RHC", 22, "PREC", "NORM", false}, {"AUDIO", 23, "LIVE", "MUTE", false}},
        {{255, 0, 0}, {234, 179, 8}, {236, 72, 153}, {0, 255, 0}, {6, 182, 212}, {255, 160, 0}, {20, 184, 166}, {255, 60, 0}, {0, 255, 0}, {132, 204, 22}, {0, 255, 0}, {0, 255, 0}});
}
static Scenario* kbc_time() {
    return new KbcScenario({
        {"ApA", 0}, {"STOP", 1}, {"PeA", 2}, {"PHYS", 3}, {"MNVR", 4}, {"FASTER", 5}, {"SOI", 6}, {"SLOWER", 7}, {"MORN", 8}, {"LOAD", 9}, {"PAUSE", 10}, {"SAVE", 11}, {"the selector", 12, "SPC", "PLN", false}},
        {{234, 179, 8}, {255, 80, 0}, {234, 179, 8}, {163, 230, 53}, {234, 179, 8}, {255, 140, 100}, {234, 179, 8}, {255, 140, 100}, {234, 179, 8}, {100, 255, 160}, {255, 80, 0}, {132, 204, 22}});
}
static Scenario* kbc_aux() {
    return new KbcScenario({
        {"EVA LIGHTS", 0}, {"JET PACK", 1}, {"BOARD", 2}, {"CONST", 3}, {"JUMP", 4}, {"GRAB", 5}, {"EVA CHUTE", 6}, {"HELMET", 7}, {"UNDOCK", 8}, {"FLAG", 9}, {"CP PRI", 10}, {"CP ALT", 11}},
        {{100, 255, 160}, {132, 204, 22}, {0, 255, 0}, {20, 184, 166}, {163, 230, 53}, {80, 200, 160}, {255, 80, 0}, {14, 165, 233}, {255, 160, 0}, {80, 200, 160}, {255, 80, 120}, {236, 72, 153}});
}

// ============================================================
//  KerbalJoystickCore modules
// ============================================================
static void pinRelease(int pin) { sim::pin_set((uint8_t)pin, false); }

struct KjcScenario : Scenario {
    std::string b1, b2;
    Rgb c1, c2;
    KjcScenario(const char* a, Rgb ca, const char* b, Rgb cb) : b1(a), b2(b), c1(ca), c2(cb) {}
    int pinOf(const std::string& l) {
        if (l == "STICK BTN") return PIN_PB5;
        if (l == b1) return PIN_PC0;
        if (l == b2) return PIN_PB3;
        return -1;
    }
    bool input(const std::string& verb, const std::string& label) override {
        int pin = pinOf(label);
        if (pin < 0) return false;
        if (verb == "Press") { sim::pin_set(pin, true); sim::at(150, pinRelease, pin); }
        else if (verb == "HOLD") { sim::pin_set(pin, true); sim::at(1300, pinRelease, pin); }
        else if (verb == "Release") sim::pin_set(pin, false);
        else return false;
        return true;
    }
    bool special(const std::string&, const std::string& instr, const std::string&) override {
        auto has = [&](const char* k) { return instr.find(k) != std::string::npos; };
        if (has("Let go of the stick")) {
            sim::analog_set(PIN_PA5, 512); sim::analog_set(PIN_PA6, 512); sim::analog_set(PIN_PA7, 512);
            return true;
        }
        if (has("ANTI-CLOCKWISE")) { sim::analog_set(PIN_PA7, 0); return true; }
        if (has("CLOCKWISE"))      { sim::analog_set(PIN_PA7, 1023); return true; }
        if (has("fully RIGHT"))    { sim::analog_set(PIN_PA5, 1023); return true; }
        if (has("fully LEFT"))     { sim::analog_set(PIN_PA5, 0); return true; }
        if (has("fully FORWARD"))  { sim::analog_set(PIN_PA6, 1023); return true; }
        if (has("fully BACK"))     { sim::analog_set(PIN_PA6, 0); return true; }
        return false;
    }
    bool ledOf(const std::string& label, uint8_t& pin, uint8_t& idx) override {
        if (label == b1) { pin = PIN_PC1; idx = 0; return true; }
        if (label == b2) { pin = PIN_PC1; idx = 1; return true; }
        return false;
    }
    int both(std::function<bool(int, const uint8_t*)> ok, const char* what) {
        for (int i = 0; i < 2; i++) {
            uint8_t p[3]; sim::led(PIN_PC1, i, p);
            if (!ok(i, p)) { sim::log("visual check '%s': LED %d is %u,%u,%u", what, i, p[0], p[1], p[2]); return 0; }
        }
        return 1;
    }
    int visual(const std::string& s) override {
        auto has = [&](const char* k) { return s.find(k) != std::string::npos; };
        Rgb act[2] = {c1, c2};
        if (has("LEDs off?") || has("LEDs dark?") || has("Brightness 0:"))
            return both([](int, const uint8_t* p) { return eq(p, C_OFF); }, "off");
        if (has("dim white")) return both([](int, const uint8_t* p) { return eq(p, Rgb{32, 32, 32}); }, "enabled");
        if (has("Brightness 255")) return both([](int, const uint8_t* p) { return eq(p, C_WHITE); }, "enabled 255");
        if (has("ACTIVE colours")) return both([&](int i, const uint8_t* p) { return eq(p, act[i]); }, "active");
        if (has("steady amber")) return both([](int, const uint8_t* p) { return eq(p, C_AMBER); }, "warning");
        if (has("steady red")) return both([](int, const uint8_t* p) { return eq(p, C_RED); }, "alert");
        if (has("steady cyan")) return both([](int, const uint8_t* p) { return eq(p, C_CYAN); }, "armed");
        if (has("full white")) return both([](int, const uint8_t* p) { return eq(p, C_WHITE); }, "bulb");
        if (has("First button lit") || has("same pattern"))
            return both([&](int i, const uint8_t* p) { return eq(p, i ? C_OFF : act[0]); }, "pattern");
        return -1;
    }
};

// ============================================================
//  Throttle: lever physics driven by the module's motor outputs
// ============================================================
static void tapNext(int) { sim::tap_button("NEXT"); }

struct ThrottleScenario : Scenario {
    double pos = 300;          // wiper raw 0..1023
    bool touched = false;
    int hand = -1;             // where the hand moves the lever, -1 = none
    void begin() override { sim::analog_set(PIN_PA2, (int)pos); }
    void tick() override {
        bool fwd = sim::pin_out(PIN_PA7), rev = sim::pin_out(PIN_PA6);
        double sp = sim::pwm_out(PIN_PC0) / 255.0 * 2.0;     // counts per ms at full PWM: 2
        if (touched && hand >= 0) {
            if (pos < hand) pos = std::min<double>(hand, pos + 3);
            else if (pos > hand) pos = std::max<double>(hand, pos - 3);
        } else if (fwd && !rev) pos += sp;
        else if (rev && !fwd) pos -= sp;
        pos = std::max(0.0, std::min(1023.0, pos));
        sim::analog_set(PIN_PA2, (int)lround(pos));
        sim::pin_set(PIN_PA3, touched);
        static int n = 0;
        if (sim::opt_int("trace", 0) && ++n % 100 == 0)
            sim::log("lever %.0f touched %d fwd %d rev %d pwm %d", pos, touched, fwd, rev, sim::pwm_out(PIN_PC0));
    }
    static int pinOf(const std::string& l) {
        if (l == "100%") return PIN_PB4;
        if (l == "UP") return PIN_PB3;
        if (l == "DOWN") return PIN_PC1;
        if (l == "0%") return PIN_PC2;
        return -1;
    }
    bool input(const std::string& verb, const std::string& label) override {
        int pin = pinOf(label);
        if (pin < 0) return false;
        if (verb == "Press") { sim::pin_set(pin, true); sim::at(150, pinRelease, pin); return true; }
        if (verb == "Release") { sim::pin_set(pin, false); return true; }
        return false;
    }
    static void letGo(int);
    bool special(const std::string&, const std::string& instr, const std::string&) override {
        auto has = [&](const char* k) { return instr.find(k) != std::string::npos; };
        if (has("lever fully DOWN")) { touched = true; hand = 0; return true; }
        if (has("lever fully UP")) { touched = true; hand = 1023; return true; }
        if (has("Let go of the lever")) { touched = false; hand = -1; return true; }
        if (has("near the middle and keep touching")) { touched = true; hand = 512; return true; }
        if (has("Push the lever UP firmly")) { touched = true; hand = (int)pos + 150; sim::at(2000, letGo, 0); return true; }
        if (has("Press 100% now, then tap NEXT")) {
            sim::pin_set(PIN_PB4, true); sim::at(150, pinRelease, PIN_PB4); sim::at(600, tapNext, 0); return true;
        }
        return false;
    }
    int visual(const std::string& s) override {
        bool lamp = sim::pin_out(PIN_PC3);
        if (s.find("backlights on") != std::string::npos) return lamp ? 1 : 0;
        if (s.find("backlights off") != std::string::npos) return lamp ? 0 : 1;
        return -1;
    }
};
static ThrottleScenario* g_thr = nullptr;
void ThrottleScenario::letGo(int) { g_thr->touched = false; g_thr->hand = -1; }

// ============================================================
//  Dual Encoder
// ============================================================
struct EncClick { uint8_t a, b; int dir; int left; };
static EncClick g_enc;
static void encStep(int) {
    if (g_enc.left <= 0) { sim::tap_button("NEXT"); return; }
    sim::enc_click(g_enc.a, g_enc.b, g_enc.dir);
    g_enc.left--;
    sim::at(60, encStep, 0);
}
static void turnEncoder(uint8_t a, uint8_t b, int dir, int clicks) {
    g_enc = {a, b, dir, clicks};
    sim::at(100, encStep, 0);
}

struct DualEncScenario : Scenario {
    bool input(const std::string& verb, const std::string& label) override {
        int pin = label == "ENC1 PUSH" ? PIN_PC1 : (label == "ENC2 PUSH" ? PIN_PB4 : -1);
        if (pin < 0) return false;
        if (verb == "Press") { sim::pin_set(pin, true); sim::at(150, pinRelease, pin); return true; }
        if (verb == "HOLD") { sim::pin_set(pin, true); sim::at(1300, pinRelease, pin); return true; }
        if (verb == "Release") { sim::pin_set(pin, false); return true; }
        return false;
    }
    bool special(const std::string&, const std::string& instr, const std::string&) override {
        static const std::regex re("Turn (ENC[12]) (clockwise|anticlockwise) exactly (\\d+) clicks");
        std::smatch m;
        if (!std::regex_search(instr, m, re)) return false;
        int n = atoi(m[3].str().c_str()) + sim::opt_int("enc_extra", 0);
        int dir = (m[2].str() == "clockwise" ? 1 : -1) * (sim::opt_int("enc_reversed", 0) ? -1 : 1);
        if (m[1].str() == "ENC1") turnEncoder(PIN_PC3, PIN_PC2, dir, n);
        else turnEncoder(PIN_PA7, PIN_PB5, dir, n);
        return true;
    }
};

// ============================================================
//  Kerbal7SegmentCore modules (GPWS, Pre-Warp Time)
// ============================================================
static int g_k7Clicks = 0, g_k7Dir = 1, g_k7Gap = 100;
static bool g_k7Next = false;
static void k7Click(int) {
    if (g_k7Clicks <= 0) { if (g_k7Next) sim::tap_button("NEXT"); return; }
    sim::enc_click(PIN_PB4, PIN_PB5, g_k7Dir);
    g_k7Clicks--;
    sim::at(g_k7Gap, k7Click, 0);
}
static void k7Turn(int dir, int n, int gap, bool next) {
    if (sim::opt_int("enc_reversed", 0)) dir = -dir;
    g_k7Dir = dir; g_k7Clicks = n; g_k7Gap = gap; g_k7Next = next;
    sim::at(50, k7Click, 0);
}

struct K7Scenario : Scenario {
    std::vector<std::string> keys;     // keycap names for BTN01..BTN03
    explicit K7Scenario(std::vector<std::string> k) : keys(std::move(k)) {}
    int pinOf(const std::string& l) {
        if (l == keys[0]) return PIN_PA1;
        if (l == keys[1]) return PIN_PC2;
        if (l == keys[2]) return PIN_PC1;
        if (l == "the knob") return PIN_PB3;
        return -1;
    }
    void press(int pin) { sim::pin_set(pin, true); sim::at(150, pinRelease, pin); }
    bool input(const std::string& verb, const std::string& label) override {
        int pin = pinOf(label);
        if (pin < 0) return false;
        if (verb == "Press") { press(pin); return true; }
        return false;
    }
    bool special(const std::string&, const std::string& instr, const std::string&) override {
        std::smatch m;
        auto has = [&](const char* k) { return instr.find(k) != std::string::npos; };
        if (has("Nothing may happen") || has("nothing may happen")) {
            for (size_t i = 0; i < keys.size(); i++)
                if (has(("press " + keys[i]).c_str()) || has(("Press " + keys[i]).c_str()) ||
                    has((" and " + keys[i]).c_str())) press(pinOf(keys[i]));
            if (has("turn the knob")) k7Turn(1, 3, 80, false);
            sim::at(800, tapNext, 0);
            return true;
        }
        static const std::regex turnRe("(Turn|Spin) the knob (clockwise|anticlockwise)[^0-9]*(\\d+) clicks");
        if (std::regex_search(instr, m, turnRe)) {
            int n = atoi(m[3].str().c_str());
            bool fast = m[1].str() == "Spin";
            if (has("or more")) n += 5;
            k7Turn(m[2].str() == "clockwise" ? 1 : -1, n, fast ? 20 : 120, true);
            return true;
        }
        static const std::regex pressRe("(Press|Push) (.+?)( again)?( \\(.*\\))?\\.");
        if (std::regex_search(instr, m, pressRe)) {
            int pin = pinOf(m[2].str());
            if (pin >= 0) { press(pin); return true; }
        }
        return false;
    }
    int checkLeds(const std::string& s) {
        static const Rgb DIM = {15, 8, 2}, GOLD = {255, 160, 0};
        auto px = [](int i, uint8_t* p) { sim::led(PIN_PC3, i, p); };
        uint8_t p[3];
        if (s.find("keycaps white") != std::string::npos) {
            for (int i = 0; i < 3; i++) { px(i, p); if (!eq(p, C_WHITE)) return 0; }
        }
        if (s.find("three keycaps dim") != std::string::npos || s.find("keycaps dim") != std::string::npos) {
            for (int i = 0; i < 3; i++) { px(i, p); if (!eq(p, DIM)) return 0; }
        }
        if (s.find("keycaps dark") != std::string::npos) {
            for (int i = 0; i < 3; i++) { px(i, p); if (!eq(p, C_OFF)) return 0; }
        }
        for (size_t k = 0; k < keys.size(); k++) {
            std::regex re(keys[k] + " (lit |still |dim)?(GREEN|AMBER|dim)?");
            std::smatch m;
            std::string t = s;
            while (std::regex_search(t, m, re)) {
                std::string col = m[2].str().empty() ? m[1].str() : m[2].str();
                px((int)k, p);
                if (col == "GREEN" && !eq(p, C_GREEN)) return 0;
                if (col == "AMBER" && !eq(p, C_AMBER)) return 0;
                if ((col == "dim" || col == "dim ") && !eq(p, DIM)) return 0;
                t = m.suffix();
            }
        }
        (void)GOLD;
        return 1;
    }
    int visual(const std::string& s) override {
        std::string d = sim::seg7();
        std::string dt = d;
        dt.erase(0, dt.find_first_not_of(' '));
        std::smatch m;
        int ok = 1;
        if (s.find("8.8.8.8.") != std::string::npos) ok = d == "8.8.8.8.";
        else if (s.find("right digit only") != std::string::npos) ok = d == "   7";
        else if (std::regex_search(s, m, std::regex("(?:shows|display) (\\d+)"))) ok = dt == m[1].str();
        if (s.find("display blank") != std::string::npos || s.find("display and keycaps dark") != std::string::npos)
            ok = ok && d.empty();
        if (s.find("Brightness minimum") != std::string::npos) ok = sim::seg7_intensity() == 0 && !d.empty();
        if (s.find("Brightness maximum") != std::string::npos) ok = sim::seg7_intensity() == 15;
        if (!ok) sim::log("visual check: display '%s' (intensity %d)", d.c_str(), sim::seg7_intensity());
        int l = checkLeds(s);
        if (!l) {
            uint8_t a[3], b[3], c[3];
            sim::led(PIN_PC3, 0, a); sim::led(PIN_PC3, 1, b); sim::led(PIN_PC3, 2, c);
            sim::log("visual check: keycaps %u,%u,%u / %u,%u,%u / %u,%u,%u", a[0], a[1], a[2], b[0], b[1], b[2], c[0], c[1], c[2]);
        }
        return ok && l;
    }
};

Scenario* make_ct_scenario(const std::string& name);

Scenario* make_scenario(const std::string& name) {
    if (Scenario* ct = make_ct_scenario(name)) return ct;
    if (name == "action") return actionControl();
    if (name == "ui") return kbc_ui();
    if (name == "function") return kbc_function();
    if (name == "stability") return kbc_stability();
    if (name == "vehicle") return kbc_vehicle();
    if (name == "time") return kbc_time();
    if (name == "aux") return kbc_aux();
    if (name == "gpws") return new K7Scenario({"ENA", "PROX", "RNDZ"});
    if (name == "prewarp") return new K7Scenario({"5 MIN", "1 HR", "1 DAY"});
    if (name == "dualenc") return new DualEncScenario();
    if (name == "throttle") return g_thr = new ThrottleScenario();
    if (name == "rotation") return new KjcScenario("RESET TRIM", C_GREEN, "TRIM SET", C_AMBER);
    if (name == "translation") return new KjcScenario("CYCLE CAM", Rgb{255, 0, 255}, "CAM RESET", C_GREEN);
    return nullptr;
}

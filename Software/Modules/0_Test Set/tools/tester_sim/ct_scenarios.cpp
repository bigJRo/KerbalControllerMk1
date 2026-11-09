// Simulated technician for the construction-test firmware (Construction Tests/).
// The module runs on its own; these scenarios work its inputs on the timeline
// the firmware documents and check its LEDs, lamps, display and lever. Each
// ends the run with sim::finish(): PASS when everything showed as documented
// (for a fault run: when the fault was indicated as documented).
#include <Arduino.h>
#include "sim.h"
#include <functional>
#include <string>
#include <vector>
#include <algorithm>
#include <math.h>

namespace {

struct Rgb { uint8_t r, g, b; };
bool same(const uint8_t* p, Rgb c) { return p[0] == c.r && p[1] == c.g && p[2] == c.b; }
std::string rgbStr(const uint8_t* p) { char b[24]; snprintf(b, sizeof b, "%u,%u,%u", p[0], p[1], p[2]); return b; }

// ------------------------------------------------------------
//  Timed script: events at absolute times (ms since power-on)
// ------------------------------------------------------------
struct Timed : Scenario {
    struct Ev { uint32_t at; std::function<void()> fn; };
    std::vector<Ev> evs;
    size_t next = 0;
    void at(uint32_t t, std::function<void()> f) { evs.push_back({t, f}); }
    void sortEvents() { std::stable_sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) { return a.at < b.at; }); }
    void tick() override {
        uint32_t now = sim::now_ms();
        while (next < evs.size() && evs[next].at <= now) evs[next++].fn();
        physics();
    }
    virtual void physics() {}
    bool input(const std::string&, const std::string&) override { return false; }
    void fail(const std::string& m) { sim::finish(false, m); }
};

// ============================================================
//  Button boards (KC01_1801 / KC01_1811)
// ============================================================
struct CtButtonBoard : Timed {
    bool wide;
    uint32_t dead, stuck;
    explicit CtButtonBoard(bool w) : wide(w) {}

    void led(uint8_t i, uint8_t p[3]) { sim::led(PIN_PA4, i, p); }
    int termLed(int input) {                 // terminal input -> LED index, or -1
        if (!wide && input >= 12 && input <= 15) return input - 12;
        if (wide && input >= 16 && input <= 23) return input - 16;
        return -1;
    }
    void expectLed(uint8_t i, Rgb c, const char* what) {
        uint8_t p[3]; led(i, p);
        if (!same(p, c)) fail(std::string(what) + ": LED " + std::to_string(i + 1) + " is " + rgbStr(p));
    }

    void begin() override {
        dead = (uint32_t)sim::opt_int("kbc_dead", 0);
        stuck = (uint32_t)sim::opt_int("kbc_stuck", 0);
        // 1. LED test: step (i*3+s) lasts 250 ms; all white 9000-10000 ms.
        static const Rgb STEP[3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
        for (int i = 0; i < 12; i++)
            for (int s = 0; s < 3; s++)
                at((i * 3 + s) * 250 + 125, [=]() {
                    for (int j = 0; j < 12; j++) expectLed(j, j == i ? STEP[s] : Rgb{0, 0, 0}, "LED test");
                });
        at(9500, [=]() { for (int j = 0; j < 12; j++) expectLed(j, {255, 255, 255}, "LED test white"); });

        // Fault runs: check the documented indication, then stop.
        if (stuck) {
            int bit = 0;
            while (!(stuck & (1UL << bit))) bit++;
            bool phantom = wide && bit >= 12 && bit <= 15;
            int l = bit < 12 ? bit : (phantom ? bit - 4 : termLed(bit));
            std::vector<std::string> seenCols;
            for (int k = 0; k < 6; k++)
                at(10400 + k * 60, [=]() {
                    uint8_t p[3]; led(l, p);
                    bool ok = phantom ? (same(p, {255, 0, 255}) || same(p, {0, 0, 0}))
                                      : (same(p, {255, 0, 0}) || same(p, {255, 255, 255}));
                    if (!ok) fail("stuck input " + std::to_string(bit) + ": LED " + std::to_string(l + 1) + " is " + rgbStr(p));
                });
            at(10800, [=]() { sim::finish(true, std::string(phantom ? "unconnected input" : "stuck input") +
                                                " flashing on LED " + std::to_string(l + 1)); });
            sortEvents();
            return;
        }

        // 2. Input test from ~10.1 s: everything dim red.
        at(10400, [=]() { for (int j = 0; j < 12; j++) expectLed(j, {40, 0, 0}, "input test start"); });
        std::vector<int> order;
        for (int i = 0; i < 12; i++) order.push_back(i);
        for (int i = wide ? 16 : 12; i <= (wide ? 23 : 15); i++) order.push_back(i);
        uint32_t t = 10500;
        uint32_t seen = 0;
        uint32_t required = wide ? 0xFF0FFFUL : 0xFFFFUL;
        for (size_t n = 0; n < order.size(); n++, t += 400) {
            int in = order[n];
            bool isDead = dead & (1UL << in);
            if (!isDead) seen |= 1UL << in;
            bool last = (seen & required) == required && !isDead;
            int l = in < 12 ? in : termLed(in);
            at(t, [=]() { sim::kbc_set(in, true); });
            if (last) {
                at(t + 100, [=]() { for (int j = 0; j < 12; j++) expectLed(j, {0, 255, 0}, "completion flash"); });
                at(t + 200, [=]() { sim::kbc_set(in, false); });
                at(t + 1400, [=]() {
                    for (int j = 0; j < 12; j++) expectLed(j, {0, 60, 0}, "all seen");
                    sim::finish(true, "all inputs seen");
                });
                break;
            }
            Rgb held = in < 12 ? Rgb{255, 255, 255} : Rgb{0, 0, 255};
            at(t + 100, [=]() {
                uint8_t p[3]; led(l, p);
                if (isDead && same(p, held)) fail("dead input " + std::to_string(in) + " lit its LED");
                if (!isDead && !same(p, held)) fail("input " + std::to_string(in) + ": LED " + std::to_string(l + 1) + " is " + rgbStr(p));
            });
            at(t + 200, [=]() { sim::kbc_set(in, false); });
            if (in < 12)
                at(t + 300, [=]() { expectLed(l, isDead ? Rgb{40, 0, 0} : Rgb{0, 60, 0}, isDead ? "dead button" : "button seen"); });
        }
        if (dead) at(t + 1500, [=]() {
            // No completion flash: the dead input's LED still dim red.
            int bit = 0;
            while (!(dead & (1UL << bit))) bit++;
            if (bit < 12) expectLed(bit, {40, 0, 0}, "dead button");
            sim::finish(true, "dead input " + std::to_string(bit) + " stays unseen");
        });
        sortEvents();
    }
};

// ============================================================
//  Joystick board (KC01_1831)
// ============================================================
struct CtJoystick : Timed {
    void led(uint8_t i, uint8_t p[3]) { sim::led(PIN_PC1, i, p); }
    void expect2(Rgb a, Rgb b, const char* what) {
        uint8_t p[3], q[3]; led(0, p); led(1, q);
        if (!same(p, a) || !same(q, b)) fail(std::string(what) + ": LEDs " + rgbStr(p) + " / " + rgbStr(q));
    }
    void expectPartial(int l, int ch, const char* what) {   // only channel ch lit, not full
        uint8_t p[3]; led(l, p);
        bool ok = p[ch] > 0 && p[ch] < 255;
        for (int k = 0; k < 3; k++) if (k != ch && p[k]) ok = false;
        if (!ok) fail(std::string(what) + ": LED " + std::to_string(l + 1) + " is " + rgbStr(p));
    }
    void begin() override {
        bool offc = sim::opt_int("joy_offcentre", 0) != 0;
        sim::analog_set(PIN_PA5, offc ? 900 : 512);
        sim::analog_set(PIN_PA6, 512);
        sim::analog_set(PIN_PA7, 512);
        const uint32_t T0 = 96;   // centre sampling 3 x 16 x 2 ms
        Rgb O{0, 0, 0}, R{255, 0, 0}, G{0, 255, 0}, B{0, 0, 255}, W{255, 255, 255};
        at(T0 + 150,  [=]() { expect2(R, O, "LED test"); });
        at(T0 + 450,  [=]() { expect2(G, O, "LED test"); });
        at(T0 + 750,  [=]() { expect2(B, O, "LED test"); });
        at(T0 + 1050, [=]() { expect2(O, R, "LED test"); });
        at(T0 + 1350, [=]() { expect2(O, G, "LED test"); });
        at(T0 + 1650, [=]() { expect2(O, B, "LED test"); });
        at(T0 + 2100, [=]() { expect2(W, W, "LED test"); });
        uint32_t t = T0 + 2800;
        if (offc) {
            at(t, [=]() { expect2({255, 80, 0}, {255, 80, 0}, "off-centre warning"); sim::finish(true, "off-centre axis flagged AMBER"); });
            sortEvents();
            return;
        }
        t += 1100;
        at(t, [=]() { expect2(O, O, "at rest"); });
        auto axis = [&](uint8_t pin, int l, uint32_t& tt) {
            at(tt, [=]() { sim::analog_set(pin, 1023); });  at(tt + 60, [=]() { uint8_t p[3]; led(l, p); if (!same(p, {255, 255, 255})) fail("end + : " + rgbStr(p)); });
            at(tt + 100, [=]() { sim::analog_set(pin, 800); }); at(tt + 160, [=]() { expectPartial(l, 1, "deflection +"); });
            at(tt + 200, [=]() { sim::analog_set(pin, 0); });  at(tt + 260, [=]() { uint8_t p[3]; led(l, p); if (!same(p, {255, 255, 255})) fail("end - : " + rgbStr(p)); });
            at(tt + 300, [=]() { sim::analog_set(pin, 200); }); at(tt + 360, [=]() { expectPartial(l, 0, "deflection -"); });
            at(tt + 400, [=]() { sim::analog_set(pin, 512); }); at(tt + 460, [=]() { expect2(O, O, "released"); });
            tt += 500;
        };
        axis(PIN_PA5, 0, t);
        axis(PIN_PA6, 1, t);
        at(t, [=]() { sim::analog_set(PIN_PA7, 1023); }); at(t + 60, [=]() { expect2(O, {0, 255, 255}, "twist end +"); });
        at(t + 100, [=]() { sim::analog_set(PIN_PA7, 700); }); at(t + 160, [=]() { expectPartial(1, 2, "twist +"); });
        at(t + 200, [=]() { sim::analog_set(PIN_PA7, 0); }); at(t + 260, [=]() { expect2({0, 255, 255}, O, "twist end -"); });
        at(t + 300, [=]() { sim::analog_set(PIN_PA7, 512); });
        t += 400;
        at(t, [=]() { sim::pin_set(PIN_PB5, true); }); at(t + 80, [=]() { expect2({255, 0, 255}, {255, 0, 255}, "stick button"); });
        at(t + 100, [=]() { sim::pin_set(PIN_PB5, false); });
        at(t + 200, [=]() { sim::pin_set(PIN_PC0, true); });  at(t + 280, [=]() { expect2(W, O, "BTN01"); });
        at(t + 300, [=]() { sim::pin_set(PIN_PC0, false); });
        // BTN02 is the last input: the completion flash replaces its white.
        at(t + 400, [=]() { sim::pin_set(PIN_PB3, true); }); at(t + 480, [=]() { expect2(G, G, "completion flash"); });
        at(t + 500, [=]() { sim::pin_set(PIN_PB3, false); });
        at(t + 1800, [=]() { expect2(O, O, "after completion"); sim::finish(true, "all inputs seen"); });
        sortEvents();
    }
};

// ============================================================
//  Throttle board (KC01_1871)
// ============================================================
struct CtThrottle : Timed {
    double pos = 300;
    bool touched = false;
    bool reversed = false, noMotor = false;
    std::vector<uint32_t> lampOn;       // rising edges of the lamp output
    bool lastLamp = false;

    bool lamp() { return sim::pin_out(PIN_PC3); }
    int lever() { return (int)lround(pos); }

    void physics() override {
        bool fwd = sim::pin_out(PIN_PA7), rev = sim::pin_out(PIN_PA6);
        if (reversed) std::swap(fwd, rev);
        double sp = noMotor ? 0 : sim::pwm_out(PIN_PC0) / 255.0 * 2.0;
        if (!touched) {
            if (fwd && !rev) pos += sp;
            else if (rev && !fwd) pos -= sp;
        }
        pos = std::max(0.0, std::min(1023.0, pos));
        sim::analog_set(PIN_PA2, lever());
        sim::pin_set(PIN_PA3, touched);
        bool l = lamp();
        if (l && !lastLamp) lampOn.push_back(sim::now_ms());
        lastLamp = l;
    }

    // Count lamp blinks in [from, to).
    int blinks(uint32_t from, uint32_t to) {
        int n = 0;
        for (uint32_t t : lampOn) if (t >= from && t < to) n++;
        return n;
    }
    void press(uint8_t pin, uint32_t t) {
        at(t, [=]() { sim::pin_set(pin, true); });
        at(t + 100, [=]() { sim::pin_set(pin, false); });
    }
    void begin() override {
        reversed = sim::opt_int("thr_reversed", 0) != 0;
        noMotor = sim::opt_int("thr_nomotor", 0) != 0;
        sim::analog_set(PIN_PA2, lever());
        // 1. Lamp test: off 300 ms, then 3 x (300 on, 300 off).
        at(450, [=]() { if (!lamp()) fail("lamp test: lamps off at 450 ms"); });
        at(750, [=]() { if (lamp()) fail("lamp test: lamps on at 750 ms"); });
        at(2100, [=]() { if (blinks(0, 2100) != 3) fail("lamp test: " + std::to_string(blinks(0, 2100)) + " blinks"); });
        if (reversed || noMotor) {
            int code = reversed ? 4 : 5;
            at(6500, [=]() {
                // The code repeats: N blinks of 500 ms, 1 s pause. Take one burst.
                std::vector<uint32_t> e;
                for (uint32_t t : lampOn) if (t > 2100) e.push_back(t);
                int burst = e.empty() ? 0 : 1;
                for (size_t k = 1; k < e.size() && e[k] - e[k - 1] < 700; k++) burst++;
                if (burst != code) fail("fault code: " + std::to_string(burst) + " blinks, expected " + std::to_string(code));
                sim::finish(true, "fault code " + std::to_string(code) + " shown");
            });
            sortEvents();
            return;
        }
        // 2. Motor test from ~2.6 s: 0 %, 100 %, 50 %, a blink at each.
        at(9000, [=]() {
            int n = blinks(2100, 9000);   // 3 arrival blinks, then the lamps come on
            if (n != 4) fail("motor test: " + std::to_string(n - 1) + " arrival blinks");
            if (std::abs(lever() - 512) > 25) fail("motor test: lever at " + std::to_string(lever()) + ", expected 512");
            if (!lamp()) fail("input test: lamps off");
        });
        // 3. Input test.
        at(9100, [=]() { touched = true; });
        at(9200, [=]() { if (lamp()) fail("touch: lamps still on"); });
        at(9300, [=]() { touched = false; });
        at(9400, [=]() { if (!lamp()) fail("touch released: lamps off"); });
        press(PIN_PB4, 9500);                                                    // 100%
        at(11000, [=]() { if (lever() < 1015) fail("100%: lever at " + std::to_string(lever())); });
        press(PIN_PB3, 11200);                                                   // UP at the top: stays
        press(PIN_PC1, 11600);                                                   // DOWN: -102
        at(12600, [=]() { if (std::abs(lever() - (1023 - 102)) > 30) fail("DOWN: lever at " + std::to_string(lever())); });
        press(PIN_PC2, 12800);                                                   // 0%
        at(14500, [=]() {
            if (lever() > 8) fail("0%: lever at " + std::to_string(lever()));
            int n = blinks(12850, 14500);   // 5 completion blinks, then the lamps come back on
            if (n != 6) fail("completion: " + std::to_string(n - 1) + " blinks, expected 5");
            sim::finish(true, "lamps, motor, touch and buttons ok");
        });
        sortEvents();
    }
};

// ============================================================
//  7-segment board (KC01_1841): follows the prompts on the display
// ============================================================
struct Ct7Seg : Timed {
    int stage = 0;           // 0 wait for 1111, 1..6 prompts, 7 result
    uint32_t stageAt = 0;
    bool acted = false;
    int clicks = 0, clickDir = 0;
    uint32_t nextClick = 0;
    void physics() override {
        uint32_t now = sim::now_ms();
        if (clicks > 0 && now >= nextClick) { sim::enc_click(PIN_PB4, PIN_PB5, clickDir); clicks--; nextClick = now + 60; }
        std::string d = sim::seg7();
        std::string t = d; t.erase(0, t.find_first_not_of(' '));
        uint8_t p[3][3];
        for (int i = 0; i < 3; i++) sim::led(PIN_PC3, i, p[i]);
        int deadPin = sim::opt_int("dead_pin", -1);
        if (stage == 0) { if (t == "1111") { stage = 1; acted = false; } return; }
        if (stage >= 1 && stage <= 6) {
            if (t == std::to_string(stage) && !acted && now > stageAt + 300) {
                acted = true;
                static const uint8_t PIN[4] = {PIN_PA1, PIN_PC2, PIN_PC1, PIN_PB3};
                if (stage <= 4) {
                    uint8_t pin = PIN[stage - 1];
                    if (stage <= 3 && !same(p[stage - 1], {0, 255, 0}) && !same(p[stage - 1], {255, 0, 0})) { fail("prompt " + std::to_string(stage) + ": LED not green"); return; }
                    sim::pin_set(pin, true);          // a dead input times out after 15 s
                    sim::at(150, [](int pn) { sim::pin_set((uint8_t)pn, false); }, pin);
                } else { clicks = 4; clickDir = stage == 5 ? 1 : -1; nextClick = now; }
            }
            // Moved on to the next prompt?
            if (acted && t != std::to_string(stage) && t != "" ) {
                stage++; acted = false; stageAt = now;
            }
            return;
        }
        if (stage == 7) {
            if (deadPin >= 0) {
                if (now > stageAt + 8000) {
                    if (t != "2" || !same(p[1], {255, 0, 0})) fail("fail code: display '" + d + "', LED 2 " + rgbStr(p[1]));
                    sim::finish(true, "dead BTN02 reported as fail code 2");
                }
                return;
            }
            if (now > stageAt + 4000) {
                bool green = same(p[0], {0, 255, 0}) && same(p[1], {0, 255, 0}) && same(p[2], {0, 255, 0});
                if (t != "0" || !green) fail("result: display '" + d + "', LEDs " + rgbStr(p[0]));
                sim::finish(true, "all six inputs passed, result held");
            }
        }
    }
    void begin() override {}
};

}  // namespace

Scenario* make_ct_scenario(const std::string& name) {
    if (name == "ct1801") return new CtButtonBoard(false);
    if (name == "ct1811") return new CtButtonBoard(true);
    if (name == "ct1831") return new CtJoystick();
    if (name == "ct1871") return new CtThrottle();
    if (name == "ct1841") return new Ct7Seg();
    return nullptr;
}

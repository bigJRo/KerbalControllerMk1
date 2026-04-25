// ATT_StaticTest.ino
// Drop this in the sketch folder.
// In AAA_Config.ino set:  bool demoMode = true;
// Wherever the demo sets state.pitch / state.roll (sine wave lines), replace with:
//     ATT_staticTest_update();
//
// Press ENTER (or any key) in Serial Monitor at 115200 to advance to the next case.
// Current case is printed on each advance and on first call.

struct ATT_TestCase {
    float       pitch;
    float       roll;
    const char *label;
};

static const ATT_TestCase ATT_CASES[] = {
    //  pitch    roll    description
    {   0.0f,   0.0f,  "Level           pitch=  0  roll=  0" },

    // Pure pitch — just enough to verify direction
    {  30.0f,   0.0f,  "Pitch up 30     pitch=+30  roll=  0" },
    { -30.0f,   0.0f,  "Pitch down 30   pitch=-30  roll=  0" },

    // Pure roll — just enough to verify direction
    {   0.0f,  45.0f,  "Roll right 45   pitch=  0  roll=+45" },
    {   0.0f, -45.0f,  "Roll left  45   pitch=  0  roll=-45" },
    {   0.0f, 180.0f,  "Inverted        pitch=  0  roll=+180"},

    // Pitch 90 (straight up) at various rolls
    {  90.0f,   0.0f,  "Straight up     pitch=+90  roll=  0" },
    {  90.0f,  90.0f,  "Straight up     pitch=+90  roll=+90" },
    {  90.0f, 180.0f,  "Straight up     pitch=+90  roll=+180"},
    {  90.0f, -90.0f,  "Straight up     pitch=+90  roll=-90" },

    // Combined — gentle
    {  10.0f,  30.0f,  "Climb + bank    pitch=+10  roll=+30" },
    {  10.0f, -30.0f,  "Climb + bank    pitch=+10  roll=-30" },
    { -10.0f,  30.0f,  "Dive  + bank    pitch=-10  roll=+30" },
    { -10.0f, -30.0f,  "Dive  + bank    pitch=-10  roll=-30" },

    // Combined — moderate
    {  20.0f,  45.0f,  "Climb + bank    pitch=+20  roll=+45" },
    {  20.0f, -45.0f,  "Climb + bank    pitch=+20  roll=-45" },
    { -20.0f,  45.0f,  "Dive  + bank    pitch=-20  roll=+45" },
    { -20.0f, -45.0f,  "Dive  + bank    pitch=-20  roll=-45" },

    // Combined — steep
    {  30.0f,  60.0f,  "Steep climb+bnk pitch=+30  roll=+60" },
    {  30.0f, -60.0f,  "Steep climb+bnk pitch=+30  roll=-60" },
    { -30.0f,  60.0f,  "Steep dive+bank pitch=-30  roll=+60" },
    { -30.0f, -60.0f,  "Steep dive+bank pitch=-30  roll=-60" },

    // Knife edge
    {  20.0f,  90.0f,  "Knife edge      pitch=+20  roll=+90" },
    { -20.0f,  90.0f,  "Knife edge      pitch=-20  roll=+90" },
    {  20.0f, -90.0f,  "Knife edge      pitch=+20  roll=-90" },

    // Inverted with pitch
    {  20.0f, 180.0f,  "Inverted climb  pitch=+20  roll=+180"},
    { -20.0f, 180.0f,  "Inverted dive   pitch=-20  roll=+180"},

    // High pitch + roll
    {  45.0f,  45.0f,  "High climb+bank pitch=+45  roll=+45" },
    { -45.0f,  45.0f,  "High dive+bank  pitch=-45  roll=+45" },
    {  45.0f, 135.0f,  "Near inverted   pitch=+45  roll=+135"},
};
static const uint8_t ATT_CASE_COUNT = sizeof(ATT_CASES) / sizeof(ATT_CASES[0]);

static uint8_t _attTestIdx     = 0;
static bool    _attTestStarted = false;

static void _attTestPrint() {
    const ATT_TestCase &tc = ATT_CASES[_attTestIdx];
    Serial.print("\n[ATT TEST ");
    Serial.print(_attTestIdx + 1);
    Serial.print("/");
    Serial.print(ATT_CASE_COUNT);
    Serial.print("]  ");
    Serial.println(tc.label);
    Serial.println("  Press ENTER for next case.");
}

void ATT_staticTest_update() {
    // Print the first case once on startup
    if (!_attTestStarted) {
        _attTestStarted = true;
        _attTestPrint();
    }

    // Advance on any incoming serial byte
    if (Serial.available()) {
        while (Serial.available()) Serial.read();  // drain buffer
        _attTestIdx = (_attTestIdx + 1) % ATT_CASE_COUNT;
        _attTestPrint();
    }

    // Inject current case into state
    const ATT_TestCase &tc = ATT_CASES[_attTestIdx];
    state.pitch = tc.pitch;
    state.roll  = tc.roll;
}

# Controller_Main host tools

Build and behaviour checks for the master sketch that run on any machine with Python 3
and g++, without a Teensy toolchain. They share their method and most of their
stand-ins with the display tools in `Software/Displays/tools/`.

## `host_compile.py`

```
python3 Software/Controller_Main/tools/host_compile.py                     # KerbalSimpit stand-in
python3 Software/Controller_Main/tools/host_compile.py --simpit PATH/src   # the real library
```

Compiles the sketch the way the Arduino IDE does: tabs concatenated main-first, a
prototype hoisted for every function into the main tab after its includes,
`g++ -fsyntax-only -Wall -Wextra`. The real in-repo headers are on the include path
(`Software/Common`, KerbalModuleCommon). Stand-ins cover only what the repo does not
carry:

- the Arduino / Teensy core and Wire (`Software/Displays/tools/host_stubs/`, with the
  Teensy registers the master writes added by `host_stubs/Arduino.h` here);
- Bounce, Adafruit_EMC2101, Adafruit_INA219 and Watchdog_t4 (`host_stubs/` here);
- KerbalSimpit, by default (the displays' stand-in, which carries the library's real
  ids, payload layouts and client methods, constructors included).

**Check against the real KerbalSimpit before a release.** The stand-in is only as good as
the last time it was compared. Clone the library and pass its `src/`:

```
git clone https://github.com/Simpit-team/KerbalSimpitRevamped-Arduino.git
python3 Software/Controller_Main/tools/host_compile.py --simpit KerbalSimpitRevamped-Arduino/src
```

That run is what found the `timewarpToMessage` default-construction error (2.4.0 has no
default constructor).

The sketch includes `Software/Common` headers by absolute Windows path, because the IDE
copies a sketch to a temporary build folder where a relative path would not resolve. The
build copy rewrites those to plain includes; the sketch itself keeps them.

## `master_selftest.py`

```
python3 Software/Controller_Main/tools/master_selftest.py
```

Builds the same source into an executable with a test `main()`, runs `setup()` and
`loop()` against controllable stand-ins (KSP up or down, a log of every I2C write), and
checks what a compile cannot:

- `setup()` returns with KSP not running;
- the four display carriers get PROCEED before KSP is up;
- the Simpit handshake is retried every 2 s, not every loop;
- once KSP answers, the channels are registered, the handler installed, and
  `mySimpit.update()` runs every loop;
- an inbound FLIGHT_STATUS with time warp sets the Annunciator's WARP flag, and leaving
  the flight scene clears it;
- a short SAS_MODE_INFO packet does not fall through into FLIGHT_STATUS.

Exit status 0 when every check passes. Run both tools after any change to the master.

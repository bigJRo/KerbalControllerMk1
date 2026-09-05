# Kerbal Controller Mk1 — Ascent Autopilot I2C Interface

**Byte-level contract between the Information Display (InfoDisp, I2C slave `0x12`) and
Controller_Main (bus master).** This document is the wiring guide for the ascent
autopilot that runs on Controller_Main; the InfoDisp side is implemented in
`Software/Displays/KCMk1_InfoDisp/` (`Screen_LNCH_AscentAP.ino`, `ConsoleShared.ino` for the command
queue and keypad, `I2CSlave.ino`); the master side is `Software/Controller_Main/infodisp_link.ino`.

**The same transport carries the hold-mode autopilot's AIRCRAFT AP and ROVER AP consoles.** Opcodes
`0x12`–`0x37` and the 44-byte (sync `0xA6`) and 28-byte (sync `0xA7`) status pushes are specified in
`Hold_Mode_Autopilot.md` §8; everything below applies to them unchanged.

The Ascent Autopilot screen on the InfoDisp is a **touch console** for the autopilot:
it presents the mission/vehicle parameters and the live guidance readout, and it lets
the pilot edit parameters and ARM/DISARM by touch. The autopilot logic itself lives on
Controller_Main. This interface carries two things:

1. **Commands, InfoDisp → Controller_Main** — the pilot's edits and ARM/DISARM taps,
   embedded in the InfoDisp's normal outbound status packet (`0x12` responds to a master
   read). One command in flight at a time, with a sequence/acknowledge handshake.
2. **Status, Controller_Main → InfoDisp** — the autopilot's `AscentStatus`, pushed as a
   dedicated I2C write so the console can render live guidance and confirm that edited
   parameters were accepted.

All multi-byte numeric fields are **IEEE-754 float32, little-endian** unless stated
otherwise (the InfoDisp is a Teensy 4.1, little-endian, and copies the four payload bytes
straight into a `float`).

---

## 1. Transport summary

| Direction | Trigger | Length | Sync | Purpose |
|-----------|---------|--------|------|---------|
| InfoDisp → Master | master read of `0x12` | 10 bytes | `0xAE` (byte 0) | status + head command |
| Master → InfoDisp | master write, 2 bytes | 2 bytes | — | control byte + command ACK |
| Master → InfoDisp | master write, 40 bytes | 40 bytes | `0xA5` (byte 0) | `AscentStatus` push |

The 2-byte inbound control packet is the pre-existing InfoDisp command channel
(§ *Inbound command* below). Byte 1 of that packet, previously reserved, now carries the
command **ACK** for this interface — no new transaction type is required to acknowledge a
command; any control write the master already sends can carry the ACK.

The 40-byte inbound write is new and is dispatched **by length** in the InfoDisp receive
ISR, so it does not collide with the 2-byte control packet.

---

## 2. Outbound packet (InfoDisp → Master), 10 bytes

Returned whenever the master reads `0x12`. Bytes 0–2 are the standard InfoDisp status
header; bytes 3–9 are the ascent-autopilot **command frame**.

```
Byte 0 : 0xAE          sync / framing byte
Byte 1 : flags         bit0 simpitConnected, bit1 flightScene, bit2 demoMode
Byte 2 : activeScreen  current ScreenType enum value
Byte 3 : cmdSeq        0 = no command pending; else 1..255, unique per queued command
Byte 4 : cmdOp         command opcode (see §4)
Byte 5 : payload[0]  ─┐
Byte 6 : payload[1]   │ float32, little-endian (command argument)
Byte 7 : payload[2]   │
Byte 8 : payload[3]  ─┘
Byte 9 : xsum          XOR of bytes 3..8 (command-frame integrity check)
```

**Reading a command.** On each poll, the master inspects `cmdSeq`:

- `cmdSeq == 0` → no command pending; ignore bytes 4–9.
- `cmdSeq != 0` → a command is waiting. Verify `xsum == XOR(bytes 3..8)`; if it fails,
  discard and re-read on the next poll (InfoDisp keeps the same command exposed until it
  is acknowledged). If it passes, and `cmdSeq` differs from the last one you executed,
  apply the command (§4) **once**, then acknowledge it (§3).

Because InfoDisp holds one command in flight until acknowledged, the master will keep
seeing the same `(cmdSeq, cmdOp, payload)` until it ACKs. Executing strictly on a change
in `cmdSeq` makes repeated reads idempotent.

---

## 3. Inbound command / ACK (Master → InfoDisp), 2 bytes

This is the existing InfoDisp control packet, unchanged except that byte 1 is now defined.

```
Byte 0 : controlByte   (unchanged — requestType nibble + mode bits; see I2CSlave.ino)
Byte 1 : ackSeq        0 = no acknowledgement; else the cmdSeq just executed
```

To acknowledge command `cmdSeq = N`, send any control write (e.g. a NOP control byte)
with `byte1 = N`. InfoDisp pops that command from its queue and exposes the next one (or
`cmdSeq = 0` if the queue is empty) on the following read. Setting `ackSeq = 0` leaves the
queue untouched — safe to send on every ordinary control write when nothing is being
acknowledged.

The ACK only advances the InfoDisp command queue. It does **not** clear the pilot's
pending (cyan) edit on screen — that happens when the accepted value is echoed back in the
status push (§5). This separation means the console shows *delivered but not yet confirmed*
(cyan) versus *confirmed by the autopilot* (green) correctly.

---

## 4. Command opcodes

| Opcode | Name | Payload | Autopilot call |
|--------|------|---------|----------------|
| `0x00` | `NOP` | 0 | — (never emitted with `cmdSeq != 0`) |
| `0x01` | `SET_TARGET_ALT` | metres (float) | `apSetTargetAltitude(v)` |
| `0x02` | `SET_INCLINATION` | degrees 0–180 (float) | `apSetTargetInclination(v)` |
| `0x03` | `SET_LAUNCH_DIR` | `0.0` = north, `1.0` = south | `apSetLaunchSoutherly(v != 0)` |
| `0x04` | `SET_LOFT` | exponent ~0.5–2.0 (float) | `apSetLoft(v)` |
| `0x05` | `SET_ROLL` | degrees −180..180, **or `1e9` = disable** | see below |
| `0x06` | `SET_MAXG` | g cap, `0.0` = off (float) | `apSetMaxG(v)` |
| `0x10` | `ARM` | 0 | `apArm()` |
| `0x11` | `DISARM` | 0 | `apDisarm()` |
| `0x12`–`0x37` | *hold-mode autopilot* | see `Hold_Mode_Autopilot.md` §8.1 | `hp*()` |

**Roll (`0x05`).** The console can either set a roll-hold angle or turn roll hold off. It
encodes *off* as the sentinel `1e9` (well outside the ±180° range):

```c
if (payload >= 1.0e8f) apSetRoll(/*enable=*/false, 0.0f);   // roll hold OFF
else                   apSetRoll(/*enable=*/true,  payload); // hold at `payload` degrees
```

Adapt the call to whatever `apSetRoll` signature Controller_Main uses; the contract is
only that `payload >= 1e8` means "disable roll hold."

**Max-G (`0x06`).** `0.0` means "no G limit." Pass it straight through — the autopilot
already treats `0` as off.

Ranges are clamped on the InfoDisp before a command is queued (target altitude ≥ 0,
inclination 0–180, loft 0.5–2.0, roll ±180, max-G 0–20), so the master may treat incoming
values as pre-validated but should still range-check defensively.

---

## 5. Status push (Master → InfoDisp), 40 bytes

Whenever the autopilot's `AscentStatus` changes (or on a fixed cadence, e.g. 5–10 Hz),
Controller_Main writes 40 bytes to `0x12`:

```
Byte 0 : 0xA5          sync / framing byte
Byte 1 : flags         bit0 armed, bit1 southerly, bit2 rollEnable
Byte 2 : phase         0 IDLE, 1 VERTICAL, 2 GRAVITY TURN, 3 COAST,
                       4 CIRCULARIZE, 5 COMPLETE, 6 ABORT
Byte 3 : reserved      0x00
Bytes 4..39 : nine float32, little-endian, in this order:
   [0] targetAlt      m    (echo of accepted target apoapsis)
   [1] inclination    deg  (echo)
   [2] loft           —    (echo)
   [3] rollDeg        deg  (echo; only meaningful when flags.rollEnable)
   [4] maxG           g    (echo; 0 = off)
   [5] cmdPitch       deg above horizon (live guidance)
   [6] cmdHeading     deg azimuth       (live guidance)
   [7] cmdThrottle    0..1              (live guidance)
   [8] dynPressure    Pa                (live guidance)
```

Apoapsis, periapsis, and g-force shown on the console come from the InfoDisp's own Simpit
telemetry link and are **not** carried here — do not duplicate them in the push.

**Parameter echoes (floats [0]–[4]) are what clear the pilot's pending edits.** When the
pilot edits a parameter, the console shows it in cyan and queues the matching command. The
console clears the cyan state (returns the field to green) only when the corresponding
echo field in this push matches the pending value (within a small tolerance). So the
autopilot should reflect an accepted `apSet*` into the next status push. If a value is
rejected (out of range, wrong mode), simply don't echo it — the console will keep showing
the pilot's entry in cyan, which correctly signals "not yet confirmed."

`phase` drives the banner and the ARM button colour (IDLE grey, VERTICAL/GRAVITY TURN
green, COAST/CIRCULARIZE cyan, COMPLETE sky, ABORT red). `flags.armed` drives the
ARMED/DISARMED banner and, together with an ARM/DISARM command echo, retires the pilot's
pending ARM state.

---

## 6. Master-side wiring (pseudocode)

```c
// ---- once per poll of the InfoDisp (read 10 bytes from 0x12) ----
uint8_t buf[10] = read_infodisp();
if (buf[0] != 0xAE) { /* framing error, skip */ }
else {
    uint8_t seq = buf[3];
    if (seq != 0 && seq != g_lastCmdSeq) {
        uint8_t xs = 0; for (int i = 3; i < 9; i++) xs ^= buf[i];
        if (xs == buf[9]) {
            float payload; memcpy(&payload, &buf[5], 4);
            apply_ap_command(buf[4], payload);   // switch on opcode, call apSet*/apArm/...
            g_lastCmdSeq = seq;
            g_ackPending = seq;                  // send this back as ackSeq next control write
        }
    }
}

// ---- when acknowledging (any control write to 0x12) ----
uint8_t ctrl[2] = { control_byte, g_ackPending };   // g_ackPending = 0 when nothing to ack
write_infodisp(ctrl, 2);
g_ackPending = 0;

// ---- on AscentStatus change / periodic (write 40 bytes to 0x12) ----
uint8_t st[40] = {0};
st[0] = 0xA5;
st[1] = (s.armed?1:0) | (s.southerly?2:0) | (s.rollEnable?4:0);
st[2] = s.phase;
float f[9] = { s.targetApoapsis, s.inclination, s.loft, s.rollDeg, s.maxG,
               s.cmdPitch, s.cmdHeading, s.cmdThrottle, s.dynPressure };
memcpy(&st[4], f, sizeof(f));
write_infodisp(st, 40);
```

`apply_ap_command` maps opcode → autopilot API per §4. `AscentStatus` field names above
(`s.targetApoapsis`, `s.cmdPitch`, …) follow the `apGetStatus()` structure; adjust to the
actual accessor names on Controller_Main.

---

## 7. Handshake example

```
Pilot taps "Tgt Ap", enters 90,000 m, presses ENT.
  InfoDisp: field → cyan (pending); queues {op=0x01, payload=90000.0}, cmdSeq=7.

Master poll → read 0x12:
  buf = AE .. .. 07 01 <90000.0 LE> <xsum>
  seq 7 is new, xsum OK → apSetTargetAltitude(90000); lastCmdSeq=7; ackPending=7.

Master control write → 0x12: ctrl = {NOP, 7}
  InfoDisp: pops command 7; next read shows cmdSeq=0 (queue empty).

Autopilot accepts 90 km; next status push carries targetAlt=90000.
Master status write → 0x12 (40 bytes, targetAlt echo = 90000).
  InfoDisp: pending target matches echo → field → green (confirmed).
```

If the pilot changes the value again before the echo arrives, InfoDisp queues a second
command with a new `cmdSeq` and keeps the field cyan until the *latest* value is echoed.

---

## 8. Notes & edge cases

- **One command in flight.** InfoDisp exposes a single command until it is acknowledged;
  its internal queue (16-slot ring buffer, 15 usable) absorbs bursts of edits. The master must ACK to advance.
  A dropped ACK is self-healing: the command stays exposed and is delivered on a later
  poll (executing on `cmdSeq` change keeps it idempotent).
- **Demo mode.** When `flags.demoMode` (outbound byte 1, bit 2) is set, InfoDisp is
  generating its own `AscentStatus` for layout preview and ignores status pushes. The
  master should not push status in demo mode.
- **Parameters editable at any time.** The console does not gate edits on ARM state; the
  autopilot is expected to accept `apSet*` whenever it can and echo the result. If a
  setter must be rejected in a given mode, decline it and skip the echo (§5).
- **Endianness.** Payload and status floats are little-endian. If Controller_Main is ever
  built big-endian, byte-swap the 4-byte groups.
```

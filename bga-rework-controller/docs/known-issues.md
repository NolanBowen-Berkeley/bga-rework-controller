# Known Issues

Ordered by how much I'd want them fixed before someone else ran this machine.

## 1. Motion cannot be aborted mid-move — safety

`loop()` handles a double-tap by spinning until the move completes:

```c
else { goTo(POS_ALIGN_MM, TRAVEL_FEED_MMS); while (z.isRunning()) z.run(); }
```

Nothing else runs during that loop — no button reads, no display, no thermal check. Once an
ALIGN move starts, there is no input that stops it short of cutting power. On a head that
travels 60 mm toward a populated board, that's the wrong behavior.

**Fix:** drop the blocking `while`, let the existing `if (z.isRunning()) z.run();` at the
bottom of `loop()` service the move, and add an abort on any button press.

## 2. `readEncoderButton()` blocks the motion loop — safety + feel

The function spins inside the press (`while (digitalRead(PIN_ENC_SW) == LOW) delay(5);`)
and then waits up to `DTAP_MS` = 350 ms watching for a second tap. `z.run()` is not called
for that entire window, so the stepper stalls whenever you touch the button, and a jog in
progress stutters.

**Fix:** rewrite as a non-blocking state machine that returns an event when one is available
and returns immediately otherwise.

## 3. Switch bounce can trigger an unintended 60 mm travel move — safety

There is no debounce on the encoder push switch. Contact bounce on a single press can exit
the press-timing loop early, and the residual bounce then falls inside the 350 ms
double-tap window and registers as a second press. `readEncoderButton()` returns 2, which is
the ALIGN command.

A single intended tap can therefore command a full travel move to 60 mm.

**Fix:** require the switch to read stable for ~20 ms before accepting a state change.

## 4. Fast jogging commands large, sudden moves

```c
float step = (le - prev < 40) ? JOG_COARSE_MM : JOG_FINE_MM;
float tgt  = zMM() + step * d;
```

`d` is the number of counts accumulated since the last loop pass. Spin the encoder quickly
and `d` can be 10+ while `step` is simultaneously 0.5 mm — a single loop iteration commands
5 mm or more of travel. The two mechanisms multiply instead of one bounding the other.

**Fix:** clamp per-iteration travel (e.g. `constrain(step * d, -2.0, 2.0)`).

## 5. Jog target computed from current position, not target position

`zMM()` returns `z.currentPosition()`. If you jog again while a previous jog is still
executing, the new target is computed from where the head *is* rather than where it was
*going*, so intermediate counts get swallowed. Ten clicks of encoder produce less than ten
clicks of travel.

**Fix:** accumulate onto `z.targetPosition()` instead.

## 6. MAX6675 read faster than its conversion time

The MAX6675 needs roughly 220 ms per conversion. `heaterC()` is called from `drawOLED()`
every 250 ms (fine), but also from `heaterCold()` on every double-tap and jog, and from
diagnostics page 3 on a 20 ms loop. Reads inside the conversion window return stale or
garbage data — and one of those callers is the thermal interlock.

**Fix:** cache the temperature in a 250 ms-throttled reader and have every caller use the
cached value.

## 7. `delayMicroseconds()` inside an ISR

```c
void IRAM_ATTR encISR() {
  ...
  delayMicroseconds(50);        // blocking delay in interrupt context
  if (digitalRead(PIN_ENC_B)) ...
}
```

Blocking inside an interrupt handler. It works here because the delay is short and nothing
else is interrupt-critical, but it's papering over single-edge decoding that's sensitive to
exactly when B is sampled.

Related: `ENC_DEBOUNCE_US` of 4000 caps the encoder at 250 counts/second, so fast spins drop
counts — which interacts badly with issue 4.

**Fix:** use `ESP32Encoder`, which decodes quadrature in hardware via the PCNT peripheral.
It removes the ISR, the debounce constant, the settle delay, and the direction bug class
entirely. Roughly a 15-line change.

## 8. `attachInterrupt(PIN_ENC_A, ...)` should use `digitalPinToInterrupt()`

Works on ESP32 because GPIO number and interrupt number coincide. Non-portable and will
warn on other cores. One-line fix.

## 9. Re-zero doesn't check that the motor is stopped

Long-hold calls `z.setCurrentPosition(0)` unconditionally. AccelStepper's
`setCurrentPosition()` also zeroes speed, so doing it mid-move leaves the target and
position inconsistent. Guard with `if (z.isRunning()) return;` as `doRelease()` already does.

## 10. "BLOCKED: hot" is printed when the thermocouple is disconnected

`heaterCold()` returns false on NaN, which is the correct fail-safe, but the operator sees a
temperature message when the actual problem is a broken probe. Distinguish the two in the
message.

## 11. Stale documentation

- `docs/wiring.md` gives encoder B as D2 and lists D13 as free. Both wrong; firmware uses
  D13 for encoder B.
- The firmware's own comment `// D35, D13, D16 all free now` contradicts its
  `#define PIN_ENC_B 13` eleven lines above.
- `docs/diagrams/master-diagram-v4-HISTORICAL.svg` documents the v4 architecture — switches,
  four buttons, an I²C OLED on 25/2 — and references `placement_head_v4.ino`. It is not a
  usable build reference for v10.

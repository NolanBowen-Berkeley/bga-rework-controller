# BGA Rework Controller

ESP32 firmware and wiring for a replacement pick-and-place head on an SV560 BGA rework
station.

The station arrived with a dead top-heater Z-axis and no vacuum pickup assembly. The
manufacturer's controller isn't serviceable and the part isn't sold separately, so I
designed a replacement ESP32 motion controller for the placement head and wrote the
firmware for it.

The current build (**v10**) is deliberately minimal: one rotary encoder is the entire
control surface, and placement is done by eye rather than by limit switch. Earlier versions
had a homing switch, a touchdown switch, and two buttons; every one of them was removed
after the manual workflow turned out to be faster and more reliable than the sensed one.


## What it does

- **Z motion** — TMC2209-driven NEMA11 through a 4 mm-pitch leadscrew, 400 steps/mm,
  acceleration-ramped travel so the head doesn't overshoot or ring.
- **Vacuum pick** — pump and 3-way vent valve on MOSFETs, with an XGZP6847A pressure sensor
  confirming the part is actually held.
- **Thermal interlock** — a MAX6675 thermocouple blocks extension below the park plane
  while the head is above 60 °C.
- **Encoder-only UI** — turn to jog, tap for vacuum, double-tap for the align preset, hold
  to release, long-hold to re-zero.
- **Self-test mode** — hold the encoder at power-on for five live diagnostic pages
  (vacuum ADC, temperature, encoder count, pump/valve pulse, motor jog).

## Controls

| Input | Action |
|---|---|
| Turn | Jog Z — 0.05 mm fine, 0.5 mm coarse when turned quickly |
| Tap | Toggle vacuum |
| Double-tap | Move to ALIGN preset (60 mm) |
| Hold, 0.7 s | Release — vent, then pump off |
| Long-hold, 3 s | Re-zero at current position |
| Hold at power-on | Enter self-test |

## Placement workflow

1. Tap to pull vacuum, pick the chip.
2. Double-tap to move to ALIGN, overlay the pads on the split-vision prism.
3. Jog down slowly and watch the CN750 nozzle's own spring compress. Stop by eye.
4. Hold to release.
5. Jog back up.

The CN750's spring provides the compliance that a float mechanism or touchdown switch would
otherwise give you, which is why both were removed.

## Hardware

| | |
|---|---|
| MCU | ESP32 devkit in a screw-terminal breakout |
| Driver | TMC2209, Vref ≈ 0.5 V |
| Motor | NEMA11, 4 mm-pitch leadscrew, 1/8 microstep → 400 steps/mm |
| Nozzle | CN750, Ø10 mm head |
| Vacuum | Pump + 3-way valve on MOSFET modules, XGZP6847A sensor (3.3 V) |
| Temperature | MAX6675 + K-type probe |
| Display | SH1106 128×64 OLED, software SPI |
| Input | One rotary encoder with push switch |
| Power | 12 V brick → buck 5 V → ESP32 VIN; 12 V rail direct to motor, pump, valve |

Full connection list and grounding scheme: [`docs/wiring.md`](docs/wiring.md).
Authoritative pin assignments: [`docs/pinout.md`](docs/pinout.md).

## Building and flashing

Arduino IDE or arduino-cli, ESP32 board package installed.

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/placement_head_v10_encoder_only
arduino-cli upload  --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 \
  firmware/placement_head_v10_encoder_only
```

Libraries: `AccelStepper`, `MAX6675` (Adafruit), `U8g2`.

If the OLED shows garbage, change `U8G2_SH1106_...` to the `SSD1306` equivalent — the
two panels are pin-compatible and visually identical.

If upload fails, disconnect D15 before flashing. It's a boot strapping pin and the MAX6675
loads it during reset. See [`docs/pinout.md`](docs/pinout.md#strapping-pins) for the
permanent fix.

## Design notes

**Why the switches came out.** The original design homed against a limit switch and
detected touchdown with a KW12 roller lever set to click at about 50 g. Both worked on the
bench. Neither survived contact with the actual job: homing burned time on every power
cycle for a machine that gets re-zeroed by eye anyway, and the touchdown switch's trip point
drifted with nozzle choice and board height. Removing them deleted four pins, one pull-up
resistor, and most of the wiring harness.

**Why software debouncing.** The encoder is a mechanical contact on a machine with a stepper
and a diaphragm pump running. It chatters. The ISR filters edges closer than 4 ms apart.
See the review notes in `docs/pinout.md` — this is the part of the firmware I'd most like
to replace with hardware quadrature decoding.

**Why no absolute position.** There's no home switch, so position is meaningful only
relative to wherever you last re-zeroed. Presets are stored as offsets from that zero, and
the operator re-zeros at the park plane at the start of a session.

**Fail-safe on thermocouple loss.** `heaterCold()` compares against a NaN when the probe is
disconnected, and `NaN < 60` evaluates false — so a missing probe blocks motion rather than
allowing it. That was originally an accident, but it's the correct behavior and it stays.

## Status

Working and in use. Known issues and planned fixes are tracked in
[`docs/known-issues.md`](docs/known-issues.md) — the button reader and the ALIGN move are
both blocking, which means motion can't be aborted mid-travel. That's the next thing to fix.

## Repo layout

```
firmware/placement_head_v10_encoder_only/   current firmware
docs/pinout.md                              authoritative pin map
docs/wiring.md                              full connection list, grounding, safety checks
docs/known-issues.md                        open bugs and planned fixes
docs/history.md                             v4 → v10, and why things were removed
docs/diagrams/                              schematics (see note on v4 diagram)
```

## License

MIT

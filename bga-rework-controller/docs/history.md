# Version History

## v10 — encoder-only, manual placement (current)

Everything that could be removed was removed.

| Removed | Freed | Why |
|---|---|---|
| Home switch | D35, one 10 kΩ pull-up | Homing cost time every power cycle on a machine that gets re-zeroed by eye anyway |
| Touchdown switch (KW12 roller lever) | D13 | Trip point drifted with nozzle choice and board height; the CN750's own spring gives better compliance |
| POS button | D16 | Encoder push covers it |
| VAC button | D17 | Was conflicting with TX2 |

Control surface is now one encoder: turn, tap, double-tap, hold, long-hold.
Placement is by eye — jog down, watch the nozzle spring compress, stop.

Encoder B moved from D2 to D13 after D2 proved unreliable (both rotation directions
decremented). D2 is a boot strapping pin with a module pull-down that fights `INPUT_PULLUP`.

## v4 — sensed placement (historical)

Documented in `diagrams/master-diagram-v4-HISTORICAL.svg`. Different architecture:

- Z-home switch on D35 with a 10 kΩ pull-up to 3V3
- Touchdown switch on D13, KW12 roller lever, 0.5–1 mm gap, clicking at ~50 g
- Four buttons on D22/D23/D16/D17
- Encoder on D18/D19/D21
- **I²C** OLED on D25/D2 (`Wire.begin(25, 2)`)
- Firmware `placement_head_v4.ino`

The automated sequence was: pick → ALIGN → SAFE (G−3) → dummy profile → slow descent until
the touchdown switch clicks → 2 s dwell → release → auto-PARK → abort dummy → real reflow.

Interlocks: no extension above 60 °C, click always halts descent, vent before pump-off.
The 60 °C interlock is the one thing that survived unchanged into v10.

**Do not wire from the v4 diagram.** The OLED alone moved from I²C on two pins to software
SPI on five, and every pin in the encoder and button sections was reassigned. It's kept
because the position ladder, the pneumatic layout, and the "key numbers" box are still
accurate and still useful, and because the removals are the interesting part of this
project's story.

<!-- TODO: regenerate the master diagram against the v10 pin map, or crop the still-valid
     panels (position ladder, vacuum board, head stack) out of the v4 SVG into their own
     figures. Until then the HISTORICAL suffix is doing real work. -->

## Key numbers (unchanged across versions)

- 400 steps/mm — 4 mm leadscrew pitch, 1/8 microstepping
- TMC2209 Vref ≈ 0.5–0.6 V for the NEMA11
- Vacuum OK threshold ≈ −20 kPa on a ±40 kPa sensor (raw ADC 1400)
- Travel ≤ 8 mm/s, placement descent ~1.5 mm/s
- Thermal lockout at 60 °C

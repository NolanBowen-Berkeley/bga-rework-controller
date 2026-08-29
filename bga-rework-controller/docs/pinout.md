# Pin Map — authoritative

**This file is the single source of truth.** Where it disagrees with `wiring.md` or any
diagram, this file and the firmware `#define` block win. Verified against
`placement_head_v10_encoder_only.ino`.

## Assignments

| GPIO | Signal | Peripheral | Notes |
|---|---|---|---|
| D26 | STEP | TMC2209 | |
| D27 | DIR | TMC2209 | |
| D14 | EN | TMC2209 | driven LOW in `setup()` — driver always enabled |
| D32 | PWM | Pump MOSFET | 1N4007 across load, band toward LOAD |
| D33 | PWM | Valve MOSFET | 1N4007 across load, band toward LOAD |
| D34 | OUT | XGZP6847A vacuum sensor | input-only pin, ADC1 — correct choice |
| D5 | SCK | MAX6675 | |
| D4 | CS | MAX6675 | |
| D15 | SO | MAX6675 | ⚠ strapping pin — see below |
| D25 | A | Encoder | interrupt on FALLING |
| **D13** | **B** | **Encoder** | ⚠ **not D2 — see conflict below** |
| D12 | SW | Encoder push | ⚠ strapping pin — see below |
| D18 | CLK | OLED (SW SPI) | |
| D19 | MOSI | OLED | |
| D21 | RES | OLED | |
| D22 | DC | OLED | |
| D23 | CS | OLED | |

**Genuinely free:** D35, D16, D17, D2, VP (D36), VN (D39), RX0, TX0

## ⚠ Encoder B pin conflict

Three documents disagree about where encoder B lands:

| Source | Encoder B |
|---|---|
| `firmware/…v10_encoder_only.ino` | **D13** |
| `docs/wiring.md` | D2 |
| `docs/diagrams/master-diagram-v4-HISTORICAL.svg` | D18/D19/D21 (entirely different scheme) |

**The firmware is correct.** The in-code comment records why:
`// D13 - D2 read B unreliably (both dirs went negative)`.

`wiring.md` still says D2 and additionally lists D13 in its "free pins" section, which is
doubly wrong. Wiring the board from that guide gives you an encoder that only counts in one
direction. Both errors are noted inline in that file.

**Why D2 failed:** D2 is an ESP32 boot strapping pin. On most devkits it is tied to the
onboard LED and has a pull-down on the module. `INPUT_PULLUP` fights that pull-down, leaving
the pin at an ambiguous level, so the ISR's read of B returned the same value regardless of
direction — hence both directions decrementing. Not an encoder fault.

## Strapping pins

Three pins in this design are ESP32 boot strapping pins. They work, but they're the reason
flashing is finicky.

| Pin | Boot requirement | Used for | Risk |
|---|---|---|---|
| D15 | must be HIGH at reset | MAX6675 SO | MAX6675 drives this line; can hold it low during reset |
| D12 | must be LOW at reset | Encoder switch | holding the encoder down during reset selects 1.8 V flash → boot failure |
| D2 | must be LOW at reset | *(now free)* | caused the original encoder-B failure |

The current workaround is the note in `wiring.md`: unplug D15 before flashing.

**Permanent fix, recommended:** move MAX6675 SO to **VN (GPIO39)** and the encoder switch to
**D16** or **D17**. GPIO39 is input-only, which is exactly right for a MISO-style signal, and
it is not a strapping pin. That removes both hazards and eliminates the unplug-to-flash
ritual. It's a two-wire change and two `#define` edits.

```c
#define PIN_TC_SO   39   // was 15
#define PIN_ENC_SW  16   // was 12
```

## Grounding

Star ground, joined at one point:

- **Quiet ground (ESP32 GND):** vacuum sensor, thermocouple, OLED, encoder
- **Power ground (12 V −):** motor, pump, valve
- Single junction at ESP32 GND ↔ 12 V −

Keeps stepper and pump switching noise out of the ADC and the thermocouple amplifier.

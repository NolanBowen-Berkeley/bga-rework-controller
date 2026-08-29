# FINAL Wiring Guide — Encoder-Only Manual Build (v10)

The consolidated build after all simplifications: **encoder-only control, no switches, manual placement.** This replaces all earlier wiring guides.

**Firmware:** `placement_head_v10_encoder_only.ino`

> ⚠️ **Two corrections applied — see [`pinout.md`](pinout.md), which is authoritative.**
> This guide originally listed encoder B on **D2** and D13 as free. Both are wrong: the
> firmware uses **D13** for encoder B. D2 was tried first and failed (both rotation
> directions counted down) because it's a boot strapping pin with a module pull-down.
> Corrected inline below.

---

## WHAT CHANGED (from the original design)

- ❌ **Home switch REMOVED** — no homing, no 10kΩ resistor needed (D35 free)
- ❌ **Touchdown switch REMOVED** — manual placement by eye (D13 freed, then ~~free~~ **reused for encoder B**)
- ❌ **POS button REMOVED** — encoder does everything (D16 free)
- ❌ **VAC button REMOVED** — was causing the TX2 conflict (D17 free)
- ✅ **Encoder-only control** — turn/tap/double-tap/hold/long-hold
- ✅ **Manual placement** — CN750 spring cushions contact, no float mechanism
- ✅ **Sensors grounded to ESP32 GND** (quiet ground for clean readings)

---

## THE FULL CONNECTION LIST

Your ESP32 breakout:
- TOP: VIN GND D13 D12 D14 D27 D26 D25 D33 D32 D35 D34 VN VP EN
- BOT: 3V3 GND D15 D2 D4 D16 D17 D5 D18 D19 D21 RX0 TX0 D22 D23

### MOTOR DRIVER (TMC2209)
| Signal | Pin |
|---|---|
| STEP | D26 |
| DIR | D27 |
| EN | D14 |
| VDD/VIO | 3V3 |
| GND | GND |
| VM | +12V (distribution, NOT ESP32) |
| 1A 1B 2A 2B | motor's 4 coil wires |

Set current pot to ~0.5V (motor disconnected, 12V on).

### PUMP MOSFET
| Terminal | Connects to |
|---|---|
| PWM | D32 |
| GND | ESP32 GND |
| + | +12V (distribution) |
| − | ground (distribution) |
| LOAD | pump wire 1 |
| − (2nd) | pump wire 2 |

Diode: 1N4007 across pump (LOAD to −), **band toward LOAD**.

### VALVE MOSFET
| Terminal | Connects to |
|---|---|
| PWM | D33 |
| GND | ESP32 GND |
| + | +12V (distribution) |
| − | ground (distribution) |
| LOAD | valve wire 1 |
| − (2nd) | valve wire 2 |

Diode: 1N4007 across valve (LOAD to −), **band toward LOAD**.

### VACUUM SENSOR (XGZP6847A)
| Sensor pin | Connects to |
|---|---|
| Pin 4 (Vdd) | 3V3 ⚠️ NOT 5V |
| Pin 3 (Vss) | **ESP32 GND** (quiet ground) |
| Pin 5 (OUT) | D34 |
| Pins 1,2,6 | nothing (float) |

### THERMOCOUPLE (MAX6675)
| Signal | Pin |
|---|---|
| VCC | 3V3 |
| GND | **ESP32 GND** (quiet ground) |
| SCK | D5 |
| CS | D4 |
| SO | D15 |

Probe's 2 wires → MAX6675's screw terminal (+/−). If temp reads backwards, swap them.

### SPI OLED (7-pin)
| OLED pin | Pin |
|---|---|
| GND | GND |
| VCC | 3V3 |
| D0 (CLK) | D18 |
| D1 (MOSI) | D19 |
| RES | D21 |
| DC | D22 |
| CS | D23 |

### ENCODER (your A/B/C/D/E pinout)
| Encoder pin | What | Connects to |
|---|---|---|
| A | rotation A | D25 |
| B | rotation B | **D13** ~~D2~~ ⚠️ |
| C | rotation common | GND |
| D | switch pin 1 | D12 |
| E | switch pin 2 | GND |

**This is the ONLY control input.** No other buttons.

⚠️ **Encoder B is D13, not D2.** D2 is an ESP32 strapping pin tied to the onboard LED with a
module pull-down that fights `INPUT_PULLUP`, so the ISR read the same level regardless of
rotation direction. D2 is genuinely free; D13 is not.

### POWER
| Signal | Connects to |
|---|---|
| buck 5V out | VIN |
| 3V3 pin | (powers sensors/OLED) |
| GND | common ground |

12V distribution: barrel red → + rail, barrel black → − rail. + feeds motor VM + both MOSFET +. − feeds motor GND + both MOSFET − + buck − + ESP32 GND (common ground).

---

## GROUNDING (important for clean readings)

Two "grounds" that meet at one point (star ground):
- **Quiet ground (ESP32 GND):** vacuum sensor, thermocouple, OLED, encoder — sensitive stuff
- **Power ground (12V −):** motor, pump, valve — noisy, high-current stuff
- They join at ONE common point (ESP32 GND ↔ 12V ground)

This keeps motor/pump noise out of your sensor readings.

---

## CONTROLS (v10 encoder-only)

| Action | Does |
|---|---|
| Turn encoder | jog Z (slow=fine 0.05mm, fast=coarse 0.5mm) |
| Tap | toggle vacuum on/off |
| Double-tap | go to ALIGN preset height |
| Hold (0.7s) | release (vent + pump off) |
| Long-hold (3s) | re-zero at current position |
| Hold at startup | enter SELF-TEST |

---

## MANUAL PLACEMENT FLOW

1. Tap → vacuum on, pick the chip
2. Double-tap → go to ALIGN, align on split vision
3. Turn → jog down slowly, watch the CN750 spring compress, stop
4. Hold → release
5. Jog up

The CN750's own spring cushions contact — no float mechanism, no touchdown switch. You stop by eye.

---

## SELF-TEST (hold encoder at startup)

5 pages, tap to advance, hold to exit:
1. Overview
2. Vacuum sensor (live ADC, suck to test)
3. Thermocouple (live temp, pinch probe)
4. Encoder (turn, watch count)
5. Pump/Motor (double-tap to pulse pump+valve, jog motor)

---

## FREE PINS (unused now)

D35, **D2**, D16, D17, VN (GPIO39), VP (GPIO36), RX0, TX0 — available if you add features later.

~~D13~~ is **not** free — it carries encoder B. ~~EN~~ is the ESP32 reset pin, not a GPIO.

VN and VP are input-only, which makes them ideal for moving MAX6675 SO off the strapping
pin D15. See [`pinout.md`](pinout.md#strapping-pins).

---

## SAFETY CHECKS before 12V (meter, continuity, brick unplugged)

- + to − : NO beep (no short)
- +12V to 3V3 : NO beep (12V not on ESP32)
- ESP32 GND to 12V − : BEEP (common ground)
- across pump/valve : NO beep (diodes OK)

---

## MOUNTING

- Modules mounted solidly (foam tape or standoffs) so nothing shifts
- Board/modules in the project box on M2 nylon standoffs (no shorts to floor)
- Trim all leads flush
- Keep USB port + D15 reachable (for flashing)
- Bundle wires by destination: motor+sensors → head, encoder+OLED → panel, MOSFET power → 12V strip
- Cut wires to length (removes most of the tangle)

---

## FLASHING

Upload `placement_head_v10_encoder_only.ino`. Needs libraries: AccelStepper, MAX6675 (Adafruit), U8g2.
If OLED garbled: change `SH1106` → `SSD1306` on the oled(...) line.
If flashing fails: unplug D15 (boot-sensitive), and don't hold the encoder button down
during reset — D12 is also a strapping pin. `pinout.md` describes a two-wire change that
removes both problems permanently.

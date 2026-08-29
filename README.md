# Firmware

`placement_head_v10_encoder_only/` — current build.

## Dependencies

| Library | Source |
|---|---|
| AccelStepper | Library Manager, by Mike McCauley |
| MAX6675 | Library Manager, Adafruit |
| U8g2 | Library Manager, by olikraus |

Board: ESP32 Dev Module, ESP32 Arduino core 2.x.

## Build

```bash
arduino-cli lib install AccelStepper "MAX6675 library" U8g2
arduino-cli compile --fqbn esp32:esp32:esp32 placement_head_v10_encoder_only
arduino-cli upload  --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 placement_head_v10_encoder_only
```

Serial monitor at 115200.

## Before flashing

Disconnect D15 (MAX6675 SO). It's a boot strapping pin and the amplifier can hold it low
through reset, which puts the ESP32 into the wrong boot mode. Don't hold the encoder button
down during reset either — D12 is also a strapping pin.

See `../docs/pinout.md#strapping-pins` for the two-wire change that removes both problems.

## Tuning constants

All at the top of the `.ino`:

| Constant | Value | Meaning |
|---|---|---|
| `STEPS_PER_MM` | 400 | 4 mm pitch, 1/8 microstep |
| `POS_ALIGN_MM` | 60 | Split-vision prism height |
| `Z_TRAVEL_LIMIT` | 100 | Soft limit, ± from zero |
| `JOG_FINE_MM` / `JOG_COARSE_MM` | 0.05 / 0.5 | Per-detent travel |
| `TEMP_LOCKOUT_C` | 60 | No extension above this |
| `VAC_OK_RAW` | 1400 | ≈ −20 kPa on the ±40 kPa sensor |

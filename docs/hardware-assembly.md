# Hardware Assembly Reference

Daisy Seed modulation pedal — wiring guide and physical pin assignments.

---

## Daisy Seed Overview

| Property | Value |
|----------|-------|
| MCU | STM32H750 (ARM Cortex-M7 @ 480 MHz) |
| SDRAM | 64 MB external |
| Flash | 128 KB internal (+ QSPI for presets) |
| Audio codec | AK4556 (24-bit, stereo I/O) |
| Sample rate | 48 kHz |
| Block size | 48 samples |

---

## Physical Pin Orientation

```
USB connector is at the BOTTOM of the board.
Pin 1 (D0) is the bottom-most pin on the RIGHT side row.
Pins count upward: 1 → 20 on the right, then 21 → 40 on the left.

        LEFT side          RIGHT side
  [21] 3V3A        AGND  [20]
  [22] D15          D14  [15]
  [23] D16          D13  [14]
  [24] D17          D12  [13]
  [25] D18          D11  [12]
  [26] D19          D10  [11]
  [27] D20           D9  [10]
  [28] D21           D8  [ 9]
  [29] D22           D7  [ 8]
  [30] D23           D6  [ 7]
  [31] D24           D5  [ 6]
  [32] D25           D4  [ 5]
  [33] D26           D3  [ 4]
  [34] D27           D2  [ 3]
  [35] D28           D1  [ 2]
  [36] D29 (USB D-)  D0  [ 1]  ← bottom / USB end
  [37] D30 (USB D+)
  [38] 3V3D
  [39] VIN
  [40] DGND
```

> Audio I/O pins (right side pins 16–19) and AGND (pin 20) are at the top of the right row.
> DGND (pin 40) is at the bottom of the left row.
> Use DGND (pin 40) as the common ground for all encoders and switches.

---

## GPIO Pin Map

### Mode Encoder (with push button)

Uses the libDaisy `daisy::Encoder` class — quadrature + integrated switch.

| Signal | D-number | Physical pin | EC11 leg |
|--------|----------|--------------|----------|
| A | **D0** | **1** (right side, bottom) | side |
| B | **D1** | **2** | side |
| SW | **D2** | **3** | switch pin |
| Common | — | **40** DGND | middle (C) |

> **Function:** Scrolls through the 12 modulation modes. Hold down to enter shift layer for preset/parameter access.

### Parameter Encoders (no push button)

Four bare quadrature encoders. Pull-ups applied internally — no external resistors needed.

| Encoder | A pin | Phys | B pin | Phys | Primary layer | Shift layer |
|---------|-------|------|-------|------|---------------|-------------|
| Enc 1 | **D7** | **8** | **D8** | **9** | Speed | P1 |
| Enc 2 | **D9** | **10** | **D10** | **11** | Depth | P2 |
| Enc 3 | **D27** | **34** | **D28** | **35** | Mix | Level |
| Enc 4 | **D29** | **36** | **D30** | **37** | Tone | *(unused)* |

> ⚠️ **D29 (pin 36) and D30 (pin 37) are the USB D−/D+ lines.** Enc 4 (Tone) uses these pins,
> which conflicts with USB MIDI. These pins can be used as GPIO only when USB is not active
> (e.g. UART-only MIDI build). If you need USB MIDI, reroute Enc 4 to unused GPIO pins and
> update `pin_map.h` accordingly.

For all param encoders: connect the encoder's **common/C pin** to **DGND (pin 40)**.

### Footswitches

Normally-open momentary switches. Internal pull-up via libDaisy `daisy::Switch`.

| Label | D-number | Physical pin | Behaviour |
|-------|----------|--------------|-----------|
| BYPASS | **D3** | **4** | Toggle true-bypass / effect |
| TAP | **D4** | **5** | Tap tempo; preset load (chord with mode enc SW); preset save (hold 700 ms while chorded) |

### Outputs

| Label | D-number | Physical pin | Direction | Notes |
|-------|----------|--------------|-----------|-------|
| RELAY | **D5** | **6** | OUTPUT | HIGH = effect in signal path |
| LED_BYPASS | **D6** | **7** | OUTPUT | HIGH = active; add series resistor (e.g. 33 Ω for 2 V / 20 mA LED) |

---

## Audio I/O

On-board AK4556 codec via SAI peripheral. Physical pins 16–19 on the right-side header.

| Signal | Format |
|--------|--------|
| Audio In | Mono — firmware reads left channel only |
| Audio Out L + R | Stereo |

---

## OLED Display (I2C)

SSD1306 or SSD1309 compatible 128×64. Firmware initialises I2C\_1 (PB8/PB9 on STM32H7).

| Signal | Notes |
|--------|-------|
| SCL | I2C1 — add 4.7 kΩ pull-up to 3.3 V |
| SDA | I2C1 — add 4.7 kΩ pull-up to 3.3 V |
| VCC | 3.3 V |
| GND | DGND (pin 40) |

- **I2C address:** `0x3C`
- **Update rate:** ~30 fps (33 ms interval, main-loop driven)

---

## MIDI (UART)

| Interface | D-number | Physical pin | Direction | Notes |
|-----------|----------|--------------|-----------|-------|
| UART RX | **D13** | **14** | INPUT | 5 V → 3.3 V via opto-isolator (6N138 recommended) |
| UART TX | **D14** | **15** | OUTPUT | 3.3 V → 5 V via 220 Ω + diode for DIN-5 output |

- **Baud rate:** 31.25 kbaud (standard MIDI)
- **USB MIDI:** class-compliant via the USB connector — no extra wiring; conflicts with Enc 4 (see warning above)

### MIDI CC Map (defaults)

| CC | Parameter |
|----|-----------|
| 14 | Speed |
| 15 | Depth |
| 16 | Mix |
| 17 | Tone |
| 18 | P1 |
| 19 | P2 |
| 20 | Level |

Program Change 0–11 selects the modulation mode.

---

## Complete Pin Reference

| Physical pin | D-number | Function | Dir |
|-------------|----------|----------|-----|
| 1 | D0 | Mode encoder A | IN |
| 2 | D1 | Mode encoder B | IN |
| 3 | D2 | Mode encoder switch | IN |
| 4 | D3 | Bypass footswitch | IN |
| 5 | D4 | Tap/Preset footswitch | IN |
| 6 | D5 | Relay control | OUT |
| 7 | D6 | Bypass LED | OUT |
| 8 | D7 | Enc 1 A — Speed / P1 | IN |
| 9 | D8 | Enc 1 B — Speed / P1 | IN |
| 10 | D9 | Enc 2 A — Depth / P2 | IN |
| 11 | D10 | Enc 2 B — Depth / P2 | IN |
| 14 | D13 | UART TX — MIDI Out | OUT |
| 15 | D14 | UART RX — MIDI In | IN |
| 34 | D27 | Enc 3 A — Mix / Level | IN |
| 35 | D28 | Enc 3 B — Mix / Level | IN |
| 36 | D29 | Enc 4 A — Tone ⚠️ USB D− | IN |
| 37 | D30 | Enc 4 B — Tone ⚠️ USB D+ | IN |
| 38 | — | 3V3 Digital | PWR |
| 39 | — | VIN (9 V in) | PWR |
| 40 | — | DGND | GND |

---

## Power

- **Supply:** 9 V DC centre-negative (standard pedal PSU) into pin 39 (VIN)
- **Regulation:** on-board 3.3 V LDO
- **Current draw (typical):** ~250 mA @ 9 V (Daisy Seed + OLED + relay coil)
- **3.3 V for peripherals:** available at pin 38 (3V3D) or pin 21 (3V3A for analog circuits)

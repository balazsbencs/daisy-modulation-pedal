# Daisy Delay Pedal

A multi-mode digital delay guitar pedal firmware for the [Electrosmith Daisy Seed](https://electro-smith.com/products/daisy-seed) (ARM Cortex-M7 @ 480 MHz, 64 MB SDRAM, AKM 24-bit codec).

## Features

- **10 delay modes** selectable via rotary encoder
- **4 encoder pots + shift layer** for controlling all 7 delay parameters
- **SSD1306 OLED display** showing mode and parameter state at 30 fps
- **Tap tempo** footswitch with BPM averaging
- **8 onboard presets** stored in QSPI flash (mode + all 7 params)
- **MIDI control** via USB MIDI and TRS MIDI Type A
  - CC mapping for all 7 parameters
  - Program Change for mode selection
  - MIDI Clock sync for delay time
  - MIDI Learn (hold encoder + twist)
- **True bypass relay** with LED indicator
- **Mono in / Stereo out**, 48 kHz / 24-bit

---

## Delay Modes

| # | Name | Character |
|---|------|-----------|
| 0 | **Duck** | Wet signal ducks when input is loud, swells in silence |
| 1 | **Swell** | AD envelope on repeats creates pad-like volume swells |
| 2 | **Trem** | LFO amplitude modulates the wet signal (tremolo on repeats) |
| 3 | **Digital** | Clean, pristine repeats with tone control |
| 4 | **DBucket** | BBD emulation: HF rolloff + clock noise per repeat |
| 5 | **Tape** | Wow/flutter modulation, HF rolloff, soft saturation |
| 6 | **Dual** | Two independent delay lines panned L/R |
| 7 | **Pattern** | Multi-tap rhythmic patterns (straight / dotted / triplet) |
| 8 | **Filter** | LFO-modulated resonant SVF filter sweeps on repeats |
| 9 | **Lo-Fi** | Progressive bit-crush + sample rate reduction per repeat |

---

## Hardware

### Daisy Seed Pin Assignments

| Function | Pin | Notes |
|----------|-----|-------|
| Mode encoder A | D0 | Quadrature input |
| Mode encoder B | D1 | Quadrature input |
| Mode encoder button | D2 | Shift / preset chord |
| Bypass footswitch | D3 | Toggle effect in/out |
| Tap/preset footswitch | D4 | Tap tempo or preset action (chorded) |
| Relay control | D5 | True bypass relay |
| Bypass LED | D6 | Active indicator |
| Param encoder 1 A/B | D7 / D8 | Primary: Time, Shift: Grit |
| Param encoder 2 A/B | D9 / D10 | Primary: Repeats, Shift: Mod Speed |
| Param encoder 3 A/B | D27 / D28 | Primary: Mix, Shift: Mod Depth |
| Param encoder 4 A/B | D29 / D30 | Primary: Filter, Shift: reserved |
| OLED SCL | D11 | I2C1 clock |
| OLED SDA | D12 | I2C1 data |
| TRS MIDI TX | D13 | UART TX |
| TRS MIDI RX | D14 | UART RX |

### Encoder Layer Map

Primary layer (mode encoder button not held):
- Param encoder 1 → Time
- Param encoder 2 → Repeats
- Param encoder 3 → Mix
- Param encoder 4 → Filter

Shift layer (hold mode encoder button):
- Param encoder 1 → Grit
- Param encoder 2 → Mod Speed
- Param encoder 3 → Mod Depth
- Param encoder 4 → Reserved

---

## MIDI

### Default CC Map

| CC | Parameter |
|----|-----------|
| 14 | Time |
| 15 | Repeats |
| 16 | Mix |
| 17 | Filter |
| 18 | Grit |
| 19 | Mod Speed |
| 20 | Mod Depth |

CC values 0–127 map to the full parameter range for that knob.

### Program Change

Program Change 0–9 selects the corresponding delay mode.

### MIDI Clock

When MIDI Clock is received, the delay time is overridden and locked to the beat tempo. The OLED displays `MIDI` as the tempo source. Clock lock expires 2 seconds after the last clock tick.

### MIDI Learn

1. Hold the encoder button and twist the encoder — the pedal enters MIDI Learn mode for the currently highlighted parameter.
2. Send any CC message from your controller.
3. That CC is now mapped to that parameter.

Mappings reset on power cycle (non-volatile storage not yet implemented).

---

## Presets

- **8 local slots** in onboard QSPI flash
- Each preset stores:
  - Delay mode
  - All 7 normalized parameter values

### Preset Workflow

1. Hold the **mode encoder button** (preset chord modifier).
2. While held, turn the mode encoder to select slot `P1..P8` (shown on OLED).
3. While still held:
   - Tap footswitch short press: **load** selected slot.
   - Tap footswitch hold (`>= 700 ms`): **save** current mode + params to slot.

Tap tempo behavior is unchanged when the mode encoder button is not held.

---

## Building

### Requirements

- `arm-none-eabi-gcc` toolchain (≥ 13.x recommended)
  - macOS: `brew install --cask gcc-arm-embedded`
  - Linux: `sudo apt install gcc-arm-none-eabi`
- GNU Make

### First-time Setup

```bash
# Clone the repo (if not already done)
git clone <repo-url> delay
cd delay

# Initialize all submodules (libDaisy + its nested deps + DaisySP)
git submodule update --init --recursive

# Build libDaisy
make -C third_party/libDaisy -j4

# Build DaisySP
make -C third_party/DaisySP -j4
```

### Building the Firmware

```bash
make -j4
```

Output files are in `build/`:
- `delay.elf` — ELF with debug symbols
- `delay.bin` — raw binary for flashing
- `delay.hex` — Intel HEX format

### Flashing

**Via DFU (USB):**

Put the Daisy Seed into DFU mode (hold BOOT, tap RESET, release BOOT), then:

```bash
make program-dfu
```

**Via ST-Link:**

```bash
make program
```

---

## Project Structure

```
delay/
├── Makefile
├── src/
│   ├── main.cpp                  # Entry point, main loop
│   ├── config/
│   │   ├── pin_map.h             # GPIO/ADC pin constants
│   │   ├── constants.h           # Sample rate, buffer sizes, MIDI CCs
│   │   └── delay_mode_id.h       # enum class DelayModeId
│   ├── hardware/
│   │   ├── controls.h/.cpp       # Encoder + switch polling
│   ├── audio/
│   │   ├── stereo_frame.h        # POD StereoFrame type
│   │   ├── audio_engine.h/.cpp   # Audio callback, double-buffered params
│   │   └── bypass.h/.cpp         # Bypass state machine
│   ├── params/
│   │   ├── param_id.h            # enum class ParamId
│   │   ├── param_range.h         # ParamRange + curve mapping
│   │   ├── param_set.h/.cpp      # Immutable parameter snapshot
│   │   └── param_map.h/.cpp      # Per-mode parameter ranges
│   ├── dsp/
│   │   ├── delay_line_sdram.h/.cpp   # SDRAM-backed delay line
│   │   ├── dc_blocker.h              # One-pole DC blocker
│   │   ├── envelope_follower.h/.cpp  # Attack/release envelope
│   │   ├── fast_math.h               # fast_sin / fast_cos polynomial (no libm)
│   │   ├── lfo.h/.cpp                # Sine/triangle LFO
│   │   ├── tone_filter.h/.cpp        # One-knob LP/HP filter
│   │   └── saturation.h              # Soft-clip saturation
│   ├── modes/
│   │   ├── delay_mode.h          # Abstract base class
│   │   ├── mode_registry.h/.cpp  # Owns all 10 mode instances
│   │   ├── digital_delay.h/.cpp
│   │   ├── duck_delay.h/.cpp
│   │   ├── swell_delay.h/.cpp
│   │   ├── trem_delay.h/.cpp
│   │   ├── dbucket_delay.h/.cpp
│   │   ├── tape_delay.h/.cpp
│   │   ├── dual_delay.h/.cpp
│   │   ├── pattern_delay.h/.cpp
│   │   ├── filter_delay.h/.cpp
│   │   └── lofi_delay.h/.cpp
│   ├── midi/
│   │   └── midi_handler.h/.cpp   # USB + TRS MIDI, CC routing, learn
│   ├── display/
│   │   ├── display_layout.h      # Screen layout constants
│   │   └── display_manager.h/.cpp # OLED rendering at 30 fps
│   ├── presets/
│   │   └── preset_manager.h/.cpp # QSPI-backed preset storage
│   └── tempo/
│       ├── tap_tempo.h/.cpp      # Tap detection + BPM averaging
│       └── tempo_sync.h/.cpp     # MIDI Clock > Tap > control priority
└── third_party/
    ├── libDaisy/                 # git submodule
    └── DaisySP/                  # git submodule
```

---

## Architecture Notes

### Parameter Flow

```
Mode/Param Encoders ──┐
MIDI CC ──────────────┼──► ParamEditState ──► ParamSet (immutable snapshot)
Preset Recall ────────┘                               │
                                             ▼
Tap/MIDI Clock ──► TempoSync ──► time override
                                             │
                                             ▼
                                    AudioEngine::SetParams()
                                             │
                                    (double-buffer swap)
                                             │
                                             ▼
                                    AudioCallback (ISR)
                                             │
                                    DelayMode::Process()
                                             │
                                    ┌────────┴────────┐
                                    ▼                 ▼
                                 Codec L out       Codec R out
```

### Key Design Decisions

1. **Immutable `ParamSet`** — A fresh snapshot is taken each main loop iteration. The audio ISR never touches live control values directly, preventing data races.

2. **Static SDRAM allocation** — All delay buffers (`~6.3 MB total`) are declared with `DSY_SDRAM_BSS` at file scope. No heap, no `malloc`. The linker places them in SDRAM automatically.

3. **All 10 modes pre-constructed** — Mode switching is a pointer swap + `Reset()`, not allocation. Zero audio glitches on mode change beyond the reset clearing the buffer.

4. **Double-buffered parameters** — Main loop writes to the idle buffer and sets a dirty flag; the ISR swaps the read index at block entry. No mutex needed on Cortex-M7 for aligned word stores.

5. **Tempo priority** — `TempoSync` implements a priority chain: MIDI Clock (highest) → Tap Tempo → Encoder parameter time. MIDI Clock lock expires after 2 s of silence.
6. **Persistent presets** — `PresetManager` stores mode + normalized params in QSPI using `PersistentStorage`, with checksum validation on boot.

### SDRAM Memory Budget

| Buffer | Size | Mode |
|--------|------|------|
| Primary delay (3 s) | 576 KB | Most modes |
| Dual L (3 s) | 576 KB | Dual mode |
| Dual R (3 s) | 576 KB | Dual mode |
| Pattern (3 s) | 576 KB | Pattern mode |
| Filter (3 s) | 576 KB | Filter mode |
| Lo-Fi (3 s) | 576 KB | Lo-Fi mode |
| + others | ~3.3 MB | Remaining modes |
| **Total** | **~6.3 MB** | < 10% of 64 MB SDRAM |

---

## Flash Usage

```
FLASH:   88.65%  (~114 KB / 128 KB)
SRAM:    19.95%  (~102 KB / 512 KB)
SDRAM:    9.44%  (6.3 MB / 64 MB)
```

### Flash optimisations applied

| Change | Saving |
|--------|--------|
| `-Os` instead of `-O2` (suppresses loop unrolling / aggressive inlining) | ~5 KB |
| `fast_sin` / `fast_cos` polynomial replacing libm `sinf` / `cosf` | ~2.5 KB |
| **Total recovered** | **~7.5 KB** |

`fast_sin` is a 5th-order polynomial accurate to ±0.5 % — sufficient for LFO
and filter-coefficient use.  It lives in `src/dsp/fast_math.h`.

### If you need more room

- **LTO** (`CPPFLAGS += -flto` + `LDFLAGS += -flto`): 10–20 % saving but
  requires verifying `DSY_SDRAM_BSS` buffers still land in the correct section
  (check the `.map` file after enabling).
- **QSPI boot**: the on-board 8 MB QSPI flash is effectively unlimited for
  this codebase; relocating the firmware there removes the 128 KB constraint
  entirely.

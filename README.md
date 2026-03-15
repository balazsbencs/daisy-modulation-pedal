# Daisy Modulation Pedal

A 12-mode modulation effects pedal built on the [Electrosmith Daisy Seed](https://electro-smith.com/products/daisy-seed) (ARM Cortex-M7 @ 480 MHz, 64 MB SDRAM, 24-bit codec). Includes a companion JUCE VST3/Standalone plugin sharing the same DSP code.

## Modes

| # | Mode | Sub-modes | Core DSP |
|---|------|-----------|----------|
| 0 | Chorus | dBucket / Multi / Vibrato / Detune / Digital | BBD emulator, modulated delay |
| 1 | Flanger | Silver / Grey / Black± / Zero± | Short delay, through-zero |
| 2 | Rotary | — | Dual LFO, crossover, Doppler delay |
| 3 | Vibe | — | 4-stage allpass + LDR AM |
| 4 | Phaser | 2/4/6/8/12/16-stage / Barber Pole | Allpass chain, quadrature LFO |
| 5 | Filter | LP / Wah / HP × 8 waveshapes | SVF, envelope follower |
| 6 | Formant | AA / EE / EYE / OH / OOH | Dual SVF bandpass |
| 7 | Vintage Trem | Tube / Harmonic / Photoresistor | LFO amplitude mod |
| 8 | Pattern Trem | 16 patterns × 3 subdivisions | Pattern sequencer |
| 9 | AutoSwell | — | Envelope follower, optional chorus |
| 10 | Destroyer | — | Bitcrush, decimation, SVF, vinyl noise |
| 11 | Quadrature | AM / FM / FreqShift± | Hilbert transform, audio-rate carrier |

## Build

### Prerequisites

```bash
git submodule update --init --recursive
make -C third_party/libDaisy -j4
make -C third_party/DaisySP -j4
```

Requires `arm-none-eabi-g++` (ARM GNU Toolchain ≥ 13.x).

### Firmware

```bash
make -j4
# Outputs: build/modulation.bin, build/modulation.elf
```

Flash via USB DFU (hold BOOT, tap RESET, release BOOT):

```bash
make program-dfu
```

Flash via ST-Link:

```bash
make program
```

### VST Plugin

```bash
mkdir -p desktop/vst/build && cd desktop/vst/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
make install-user        # VST3 + Standalone → ~/Library/Audio/Plug-Ins/VST3 + ~/Applications
```

## Architecture

All 7 parameters (`speed`, `depth`, `mix`, `tone`, `p1`, `p2`, `level`) flow as normalized `[0, 1]` values and are converted to physical units at the last moment via `map_param()`.

```
Encoders / MIDI CC / Preset recall
        ↓
  ParamEditState  (normalized floats, main loop only)
        ↓
  BuildParams() → map_param() → ParamSet  (immutable snapshot, physical units)
        ↓
  TempoSync override  — clamps ps.speed to beat period in Hz
                        skipped for AutoSwell (speed = attack time in seconds)
                        skipped for Destroyer (speed = decimation rate)
        ↓
  AudioEngine::SetParams()  (writes to idle double-buffer)
        ↓
  AudioCallback ISR → ModMode::Process()
```

`ModMode` interface: `Init()` · `Reset()` · `Prepare(ParamSet)` · `Process(float) → StereoFrame`

- **`Prepare`** — block-rate coefficient updates (LFO tick, filter coefficients). Called once per audio block before `Process`.
- **`Process`** — per-sample DSP. Returns wet signal only; `AudioEngine` applies constant-power dry/wet crossfade.
- **`Reset`** — clears all DSP state on mode switch. No allocation.

Thread safety between the audio ISR and main loop is handled by a double-buffered `ParamSet[2]` — the main loop writes to the idle buffer; the ISR swaps the read index at block entry. No mutexes needed on Cortex-M7 for aligned word stores.

All SDRAM delay buffers are file-scope `static float DSY_SDRAM_BSS` arrays. No heap allocation in the audio path.

## Source Layout

```
src/
  main.cpp               — hardware init, main loop, BuildParams(), ActivateMode()
  config/                — pin map, constants, mode IDs
  audio/                 — AudioEngine (ISR, dry/wet mix, double-buffer), bypass
  modes/                 — ModMode base + 12 modulation mode implementations
  dsp/                   — DSP building blocks (LFO, SVF, AllpassFilter, DelayLine, …)
  params/                — ParamSet, ParamRange, map_param(), per-mode ranges
  tempo/                 — TempoSync (MIDI clock > tap tempo > encoder arbitration)
  midi/                  — MIDI CC/clock/PC/learn handler
  presets/               — FNV-1a CRC-guarded preset storage (QSPI flash)
  hardware/              — Controls (encoders, footswitches)
  display/               — OLED display manager
desktop/vst/             — JUCE plugin (reuses src/dsp, src/modes, src/params directly)
docs/                    — Design documentation
```

## Documentation

- [`docs/modes.md`](docs/modes.md) — All 12 modes: DSP algorithms, sub-modes, parameter mappings
- [`docs/parameters.md`](docs/parameters.md) — Parameter scheme, encoder layout, per-mode ranges, MIDI CC map
- [`docs/architecture.md`](docs/architecture.md) — System architecture and design decisions
- [`docs/dsp-blocks.md`](docs/dsp-blocks.md) — DSP building block specifications
- [`USER_MANUAL.md`](USER_MANUAL.md) — End-user guide (controls, modes, presets, MIDI)

## Flash Usage

```
FLASH:   94.21%  (~121 KB / 128 KB)
SRAM:    20.03%  (~103 KB / 512 KB)
SDRAM:    0.03%  (~25 KB / 64 MB)
```

FLASH is tight. Avoid adding string literals, large lookup tables, or additional template instantiations. If headroom is needed, LTO (`-flto`) typically recovers 10–20%.

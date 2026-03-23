# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Firmware (embedded, ARM Cortex-M7)

First-time only — build the libraries:
```bash
git submodule update --init --recursive
make -C third_party/libDaisy -j4
make -C third_party/DaisySP -j4
```

Incremental build:
```bash
make -j4
# Output: build/modulation.bin, build/modulation.elf
```

Flash to hardware:
```bash
make program-dfu   # USB DFU (hold BOOT, tap RESET, release BOOT first)
make program       # ST-Link
```

### VST Plugin (desktop, JUCE)

The VST build lives in `desktop/vst/`. It is a separate CMake project and shares `src/` DSP/modes/params code directly (no firmware HAL).

```bash
cd desktop/vst/build
make -j4                    # build VST3 + Standalone
make install-user           # install to ~/Library/Audio/Plug-Ins/VST3 + ~/Applications
make install-user-vst3      # VST3 only
make install-user-standalone
```

If the build directory does not exist yet:
```bash
mkdir desktop/vst/build && cd desktop/vst/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

There are no automated tests in this project.

## Architecture

### Parameter Pipeline

All 7 parameters (`speed`, `depth`, `mix`, `tone`, `p1`, `p2`, `level`) flow as normalized `[0, 1]` values through the system and are converted to physical units at the last moment via `map_param()`:

```
Encoders / MIDI CC / Preset recall
        ↓
  ParamEditState  (normalized floats, main loop only)
        ↓
  BuildParams() → map_param() → ParamSet  (physical units, immutable snapshot)
        ↓
  TempoSync override (clamps ps.speed when tap/MIDI clock active)
        ↓
  AudioEngine::SetParams()  (writes to idle double-buffer)
        ↓
  AudioCallback ISR → ModMode::Process()
```

`ParamSet` is the immutable snapshot struct — never mutate it, always produce a new one. `ParamRange` in `src/params/param_range.h` defines `{min, max, curve}` per parameter; `map_param()` / `unmap_param()` convert between normalized and physical.

**Per-mode speed ranges** (`src/params/param_map.cpp`):
- Most modes: `0.05 – 10.0 Hz`
- Quadrature: `0.5 – 2000 Hz` (audio-rate carrier)
- AutoSwell: `10 – 500 ms` (attack time, not LFO rate)
- Destroyer: `1× – 48×` (decimation rate)
- See `docs/parameters.md` for full per-mode range table

### Mode System

`ModMode` (abstract base in `src/modes/mod_mode.h`) has four virtual methods:
- `Init()` — called once at startup
- `Reset()` — called on mode switch to clear state / buffers
- `Prepare(const ParamSet&)` — optional per-block setup (e.g. set filter coefficients)
- `Process(StereoFrame, const ParamSet&) → StereoFrame` — per-sample DSP, returns **wet signal only**

`ModeRegistry` owns all 12 statically-allocated mode instances. Mode switching is a pointer swap + `Reset()` — no allocation, no audio dropout beyond the state clear.

The audio engine applies dry/wet mixing (constant-power equal-power crossfade on `mix`), not the modes themselves. Modes should output wet-only.

### 12 Modulation Modes

| ID | Mode | Sub-modes | Key DSP Blocks |
|----|------|-----------|----------------|
| 0 | Chorus | dBucket/Multi/Vibrato/Detune/Digital | BBDEmulator, DelayLine, LFO |
| 1 | Flanger | Silver/Grey/Black±/Zero± | DelayLine, LFO |
| 2 | Rotary | — | LFO (×2), DelayLine, Saturation |
| 3 | Vibe | — | AllpassFilter (×4), LFO |
| 4 | Phaser | 2/4/6/8/12/16-stage, Barber Pole | AllpassFilter (×16), LFO |
| 5 | Filter | LP/Wah/HP × 8 waveshapes | SVF, LFO, EnvelopeFollower |
| 6 | Formant | AA/EE/EYE/OH/OOH | SVF (×2), LFO |
| 7 | Vintage Trem | Tube/Harmonic/Photoresistor | LFO |
| 8 | Pattern Trem | 16 patterns × 3 subdivisions | PatternSequencer |
| 9 | AutoSwell | — | EnvelopeFollower, optional DelayLine |
| 10 | Destroyer | — | SVF |
| 11 | Quadrature | AM/FM/FreqShift± | HilbertTransform, LFO |

Full mode documentation: `docs/modes.md`

### SDRAM Allocation

Modulation effects use short delay lines (~25 KB total vs 6.3 MB for the delay pedal). All buffers use `DSY_SDRAM_BSS` (placed in external SDRAM by the linker). They must be **file-scope static** — never stack or heap.

### Thread Safety (firmware)

The audio ISR and main loop communicate via a double-buffered `ParamSet[2]` in `AudioEngine`. Main loop writes to the idle buffer and sets `param_dirty_`; the ISR swaps the read index at block entry. No mutexes — relies on aligned-word-store atomicity on Cortex-M7. Never read `param_buf_` from the main loop after calling `SetParams()`.

### Flash Constraint

FLASH is at ~85% capacity (128 KB total). Be conservative when adding new features — avoid string literals, large lookup tables, or additional template instantiations. The modulation pedal replaces 10 delay modes with 12 modulation modes; monitor flash usage after each mode addition.

### VST Plugin vs Firmware

The VST (`desktop/vst/`) reuses `src/dsp/`, `src/params/`, and `src/modes/` directly. It stubs out the hardware layer via `desktop/vst/include/daisy_seed.h`. Key differences:
- `DSY_SDRAM_BSS` is a no-op in the stub (buffers land in normal BSS)
- The VST normalizes all parameters as `[0, 1]` APVTS parameters and maps them in `PluginProcessor::buildParamsFromState()`
- Mode changes in the VST do not automatically refresh parameter text displays until the knob is interacted with

### Tempo Priority Chain

`TempoSync` (`src/tempo/`) arbitrates the `speed` parameter (LFO rate):
1. **MIDI Clock** (highest) — locks LFO rate to beat period; expires 2 s after last tick
2. **Tap Tempo** — averaging up to 4 taps; timeout 2 s; period → Hz conversion
3. **Encoder / MIDI CC** — normal control (override = 0, ignored by `BuildParams`)

### Documentation

Detailed design documentation lives in `docs/`:
- `docs/architecture.md` — system architecture overview
- `docs/modes.md` — all 12 modes with DSP algorithms and parameter mappings
- `docs/parameters.md` — parameter scheme, encoder mapping, per-mode ranges
- `docs/dsp-blocks.md` — DSP building block specifications
- `docs/implementation-plan.md` — phased implementation roadmap
- `docs/file-changes.md` — file-level change list

# currentDate
Today's date is 2026-03-15.

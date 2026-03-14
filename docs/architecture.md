# System Architecture

## Overview

The modulation pedal runs on an Electrosmith Daisy Seed (ARM Cortex-M7 @ 480 MHz, 64 MB SDRAM, AKM 24-bit codec at 48 kHz). It provides 12 modulation effects, each with 7 parameters, 8 presets, MIDI control, tap tempo, and an OLED display.

## Parameter Pipeline

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

### Parameters

| ID | Name  | Physical Unit         | Default Range       | Curve |
|----|-------|-----------------------|---------------------|-------|
| 0  | Speed | Hz (LFO rate)         | 0.05 – 10.0 Hz     | log   |
| 1  | Depth | 0 – 1 scalar          | 0.0 – 1.0           | linear |
| 2  | Mix   | 0 – 1 (wet/dry)       | 0.0 – 1.0           | linear |
| 3  | Tone  | 0 – 1 (LP ↔ HP)       | 0.0 – 1.0           | linear |
| 4  | P1    | mode-dependent         | mode-dependent       | varies |
| 5  | P2    | mode-dependent         | mode-dependent       | varies |
| 6  | Level | 0 – 1 (output gain)   | 0.0 – 1.0           | linear |

Per-mode parameter overrides are documented in `docs/parameters.md`.

## Mode System

`ModMode` (abstract base in `src/modes/mod_mode.h`) has four virtual methods:

- `Init()` — called once at startup
- `Reset()` — called on mode switch to clear state / buffers
- `Prepare(const ParamSet&)` — optional per-block setup (e.g. recompute filter coefficients)
- `Process(float, const ParamSet&) → StereoFrame` — per-sample DSP, returns **wet signal only**

`ModeRegistry` owns all 12 statically-allocated mode instances. Mode switching is a pointer swap + `Reset()` — no allocation, no audio dropout beyond the state clear.

### Mode List

| ID | Mode          | Sub-modes                                      |
|----|---------------|-------------------------------------------------|
| 0  | Chorus        | dBucket, Multi, Vibrato, Detune, Digital        |
| 1  | Flanger       | Silver, Grey, Black+, Black−, Zero+, Zero−      |
| 2  | Rotary        | —                                               |
| 3  | Vibe          | —                                               |
| 4  | Phaser        | 2/4/6/8/12/16-stage, Barber Pole                |
| 5  | Filter        | LP, Wah, HP (× 8 LFO waveshapes incl Env±)     |
| 6  | Formant       | AA, EE, EYE, OH, OOH                            |
| 7  | Vintage Trem  | Tube, Harmonic, Photoresistor                   |
| 8  | Pattern Trem  | 8 beats × 16 subdivisions                       |
| 9  | AutoSwell     | —                                               |
| 10 | Destroyer     | —                                               |
| 11 | Quadrature    | AM, FM, FreqShift+, FreqShift−                  |

## DSP Building Blocks

### Reused from Delay Pedal (modified)

| Block             | Change for Modulation                                |
|-------------------|------------------------------------------------------|
| LFO               | Extended from 2 to 7 waveforms (Sine, Triangle, Square, RampUp, RampDown, SampleAndHold, Exponential) |
| ToneFilter        | Unchanged — one-knob LP/HP                           |
| EnvelopeFollower  | Unchanged — attack/release detector                  |
| Saturation        | Unchanged — inline soft-clip                         |
| DCBlocker         | Unchanged — one-pole DC removal                      |
| DelayLineSDRAM    | Short lines only (~25 KB total vs 6.3 MB for delay)  |

### New DSP Blocks

| Block             | Purpose                                              |
|-------------------|------------------------------------------------------|
| AllpassFilter     | Phase-shifting stages for Phaser / Vibe              |
| SVF               | State-variable filter (LP/BP/HP/Notch) for Filter / Formant |
| BBDEmulator       | Bucket-brigade analog chorus emulation               |
| HilbertTransform  | 90° phase shift network for Quadrature freq shifting |
| PatternSequencer  | 8×16 beat-synced amplitude pattern for Pattern Trem  |

Full block specifications in `docs/dsp-blocks.md`.

## SDRAM Budget

Modulation effects use very short delay lines compared to the delay pedal:

| Buffer                    | Size    | Used By                    |
|---------------------------|---------|----------------------------|
| Chorus delay line (×5)    | ~5 KB   | Chorus (Multi/Detune)      |
| Flanger delay line (×2)   | ~2 KB   | Flanger                    |
| Rotary horn/drum delays   | ~2 KB   | Rotary                     |
| Vibe delay                | ~1 KB   | Vibe                       |
| BBD emulator buffer       | ~8 KB   | Chorus (dBucket sub-mode)  |
| Hilbert FIR buffers       | ~4 KB   | Quadrature                 |
| Miscellaneous scratch     | ~3 KB   | Various modes              |
| **Total**                 | **~25 KB** | **< 0.04% of 64 MB SDRAM** |

All delay buffers use `DSY_SDRAM_BSS` (placed in external SDRAM by the linker). They must be **file-scope static** — never stack or heap.

## AudioEngine

The `AudioEngine` is unchanged from the delay pedal architecture:

- **Constant-power dry/wet crossfade** — `sin/cos` mapping on the `mix` parameter avoids the −3 dB dip at 50/50. Modes output wet-only.
- **Double-buffered ParamSet** — main loop writes to idle buffer and sets `param_dirty_`; ISR swaps the read index at block entry. No mutexes — relies on aligned-word-store atomicity on Cortex-M7.
- **Bypass** — relay-based true bypass with LED indicator; bypass state is also double-buffered.

## Thread Safety

The audio ISR and main loop communicate via a double-buffered `ParamSet[2]` in `AudioEngine`. Main loop writes to the idle buffer and sets `param_dirty_`; the ISR swaps the read index at block entry. No mutexes — relies on aligned-word-store atomicity on Cortex-M7. Never read `param_buf_` from the main loop after calling `SetParams()`.

## Flash Constraint

FLASH is at ~93% capacity (128 KB total) in the delay pedal. The modulation pedal replaces 10 delay modes with 12 modulation modes of comparable complexity. Monitor flash usage carefully:

- Prefer inline functions and compile-time constants over lookup tables
- Avoid string literals beyond mode/sub-mode names
- Use `-Os` optimization
- Consider LTO or QSPI boot if needed

## VST Plugin

The VST (`desktop/vst/`) reuses `src/dsp/`, `src/params/`, and `src/modes/` directly. It stubs out the hardware layer via `desktop/vst/include/daisy_seed.h`:

- `DSY_SDRAM_BSS` is a no-op (buffers land in normal BSS)
- All parameters normalized as `[0, 1]` APVTS parameters
- Mode changes in the VST do not automatically refresh parameter text displays until the knob is interacted with

## Tempo Priority Chain

`TempoSync` (`src/tempo/`) arbitrates the `speed` parameter for LFO rate:

1. **MIDI Clock** (highest) — locks LFO rate to beat subdivision; expires 2 s after last tick
2. **Tap Tempo** — averaging up to 4 taps; timeout 2 s; period → Hz conversion
3. **Encoder / MIDI CC** — normal control (override = 0, ignored by `BuildParams`)

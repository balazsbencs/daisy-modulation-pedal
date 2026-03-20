# Plan: Stereo Input Support

## Context

The modulation pedal currently reads only the left input channel (`IN_L`), ignoring `IN_R`. The Daisy Seed hardware already provides stereo I/O — `IN_R` exists but is unused. The goal is to support stereo in → stereo out (e.g., from another stereo pedal), while gracefully handling mono input (single guitar cable, left only).

## Approach

### Signal flow change

```
Before:  IN_L → mono float → mode->Process(float) → StereoFrame wet → mix with mono dry
After:   IN_L+IN_R → StereoFrame → mode->Process(StereoFrame) → StereoFrame wet → mix with stereo dry
```

### Mono detection (engine-level)

When only the left jack is connected, `IN_R` reads near-zero (codec grounds unconnected inputs). The engine detects this via block-level energy check on `IN_R` with hysteresis, and copies `IN_L` to `IN_R` so modes always receive a valid stereo pair. This keeps all mode code simple — no per-mode mono/stereo branching.

### Mode processing strategy (3 tiers)

**Tier 1 — Gain-modulation modes** (VintTrem, PatternTrem, AutoSwell): These compute a gain/envelope and multiply. Apply the same gain to both channels independently. Near-zero flash cost.

**Tier 2 — Mono-process modes** (Flanger, Vibe, Filter, Formant, Destroyer, Phaser): These have stateful DSP (filters, delay lines, allpass chains) that can't be trivially duplicated without doubling state. Use `input.mono()` to feed the existing DSP chain, output `{wet, wet}`. Stereo image is preserved in the dry path by the engine's per-channel mixing.

**Tier 3 — Already-stereo modes** (Chorus, Rotary, Quadrature): Already create stereo character from their DSP (different delay taps, quadrature LFOs, Hilbert transform). Use `input.mono()` as source — the stereo output comes from the effect, not the input.

## Steps

### 1. Add `mono()` helper to StereoFrame
**File:** `src/audio/stereo_frame.h`
- Add `float mono() const { return (left + right) * 0.5f; }` to the struct

### 2. Change ModMode::Process signature
**File:** `src/modes/mod_mode.h`
- `Process(float input, ...) → Process(StereoFrame input, ...)`

### 3. Update AudioEngine for stereo input + mono detection
**File:** `src/audio/audio_engine.h`
- Add private members: `float in_r_energy_`, `int mono_count_`, `bool is_mono_`

**File:** `src/audio/audio_engine.cpp`
- At block entry: compute energy of `IN_R`, update `is_mono_` with hysteresis (3 blocks to switch)
- Sample loop: read both `IN_L[i]` and `IN_R[i]`; if `is_mono_`, set `in_r = in_l`
- Pass `StereoFrame{in_l, in_r}` to `mode->Process()`
- Per-channel dry/wet mixing:
  ```
  OUT_L[i] = (in_l * mix_dry_ + wet.left  * mix_wet_) * level
  OUT_R[i] = (in_r * mix_dry_ + wet.right * mix_wet_) * level
  ```
- Bypass: `OUT_L[i] = in_l; OUT_R[i] = in_r`

### 4. Update Tier 1 modes (gain-modulation → true stereo)
Each computes a gain, applies to both channels:

- **VintTrem** (`src/modes/vintage_trem_mode.cpp`): `return {input.left * gain, input.right * gain}`
- **PatternTrem** (`src/modes/pattern_trem_mode.cpp`): `return {input.left * trem_gain, input.right * trem_gain}`
- **AutoSwell** (`src/modes/auto_swell_mode.cpp`): Track envelope on `input.mono()`, apply `swell_gain_` to both channels. Delay line (shimmer) stays mono using `input.mono()`.

### 5. Update Tier 2 modes (mono process, `input.mono()`)
Replace `float input` with `StereoFrame input`, use `input.mono()` as the DSP source:

- **FlangerMode** (`src/modes/flanger_mode.cpp`)
- **VibeMode** (`src/modes/vibe_mode.cpp`)
- **FilterMode** (`src/modes/filter_mode.cpp`)
- **FormantMode** (`src/modes/formant_mode.cpp`)
- **DestroyerMode** (`src/modes/destroyer_mode.cpp`)
- **PhaserMode** (`src/modes/phaser_mode.cpp`)

### 6. Update Tier 3 modes (already-stereo, `input.mono()`)
Same mechanical change — use `input.mono()` as the mono source for stereo-creating DSP:

- **ChorusMode** (`src/modes/chorus_mode.cpp`)
- **RotaryMode** (`src/modes/rotary_mode.cpp`)
- **QuadratureMode** (`src/modes/quadrature_mode.cpp`)

### 7. Update VST plugin
**File:** `desktop/vst/src/PluginProcessor.cpp`
- Currently mono-sums: `const float dry = 0.5f * (left[i] + right[i])`
- Change to pass `StereoFrame{left[i], right[i]}` to `Process()`
- Per-channel dry/wet mix: `left[i] = left[i] * dry_g + wet.left * wet_g` (same for right)

## Verification

1. `make -j4` — firmware builds, flash usage stays under 90%
2. `cd desktop/vst/build && make -j4` — VST builds
3. Flash to hardware:
   - Mono input (guitar in left jack only) → stereo output with effects, same behavior as before
   - Stereo input (both jacks) → both channels processed, stereo image preserved in dry path
   - Mode switching works for all 12 modes
   - Bypass passes L→L, R→R (no longer mono-duplicates to both)
4. VST in DAW: stereo track, verify both channels processed

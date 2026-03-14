# Phased Implementation Plan

## Phase 1: Infrastructure + Core Modes

**Goal:** Rename parameter/mode infrastructure from delay to modulation; implement 3 representative modes covering key DSP patterns.

### 1a. Infrastructure Rename

- Rename `DelayMode` → `ModMode` (base class + all references)
- Rename `DelayModeId` → `ModModeId` (enum + all references)
- Rename `delay_mode_id.h` → `mod_mode_id.h`
- Update `ParamId` enum: Time→Speed, Repeats→Depth, Filter→Tone, Grit→P1, ModSpd→P2, ModDep→Level (keep Mix)
- Update `ParamSet` struct fields to match new parameter names
- Update `param_map.h/.cpp` with new default ranges for modulation
- Update `constants.h`: NUM_MODES = 12, MIDI CC comments
- Update `mode_registry.h/.cpp` for 12 mode slots
- Update `main.cpp`, `display_manager.cpp`, `midi_handler.cpp` for new names
- Verify firmware builds and boots (no functional change yet)

### 1b. LFO Expansion

- Add 5 new waveforms to `LFO`: Square, RampUp, RampDown, SampleAndHold, Exponential
- Add `Reset()` and `GetPhase()` methods
- Unit test all 7 waveshapes (in VST build, no hardware needed)

### 1c. New DSP Blocks — AllpassFilter

- Implement `AllpassFilter` in `src/dsp/allpass_filter.h`
- Single first-order allpass with modulatable coefficient

### 1d. Chorus Mode

- Implement `ChorusMode` with 5 sub-modes
- Requires: LFO (extended), DelayLineSDRAM (short), BBDEmulator (new)
- Implement `BBDEmulator` in `src/dsp/bbd_emulator.h/.cpp`
- Test on hardware with all 5 sub-modes

### 1e. Phaser Mode

- Implement `PhaserMode` with 7 sub-modes (2/4/6/8/12/16-stage + Barber Pole)
- Requires: AllpassFilter (new), LFO, DCBlocker
- Test on hardware with various stage counts

### 1f. Vintage Trem Mode

- Implement `VintageTremMode` with 3 sub-modes
- Requires: LFO (extended) only — simplest mode, good validation of infrastructure
- Test on hardware

### 1g. VST — Infrastructure + 3 Modes

- Update `CMakeLists.txt`: remove old delay mode sources, add new mode + DSP block sources
- Update `daisy_seed.h` stub if any new SDRAM macros are needed
- Update `PluginProcessor`:
  - APVTS parameters: Time→Speed, Repeats→Depth, Filter→Tone, Grit→P1, ModSpd→P2, ModDep→Level
  - `buildParamsFromState()` maps new param names to `ParamSet`
  - `textFromValueFunction` / `valueFromTextFunction` for Speed (Hz), per-mode P1/P2 labels
  - Mode dropdown: 12 entries (9 placeholders for now)
- Update `PluginEditor`:
  - Knob labels: Speed / Depth / Mix / Tone (primary), P1 / P2 / Level (shift or always-visible)
  - Mode dropdown with 12 names
- Verify VST3 + Standalone build and that Chorus, Phaser, Vintage Trem produce audio

### Phase 1 Deliverable
- Firmware boots with 3 working modes (Chorus, Phaser, Vintage Trem)
- VST builds and runs with same 3 modes (remaining 9 are placeholders)
- All new DSP blocks (AllpassFilter, BBDEmulator) validated

---

## Phase 2: Modulation Classics

**Goal:** Implement the 4 classic modulation effects that build on Phase 1 DSP blocks.

### 2a. SVF Block

- Implement `SVF` in `src/dsp/svf.h` (TPT variant for modulation stability)
- LP/BP/HP/Notch outputs, per-sample-safe coefficient update

### 2b. Flanger Mode

- Implement `FlangerMode` with 6 sub-modes
- Requires: DelayLineSDRAM (short), LFO
- Through-zero sub-modes need a second delay line for the dry path

### 2c. Vibe Mode

- Implement `VibeMode`
- Requires: AllpassFilter (4 stages), LFO with nonlinear LDR curve

### 2d. Rotary Mode

- Implement `RotaryMode`
- Requires: LFO (2 independent), DelayLineSDRAM (Doppler), Saturation (drive)
- Crossover filter for horn/drum split

### 2e. Filter Mode

- Implement `FilterMode` with 3 filter types × 8 waveshapes
- Requires: SVF (new), LFO (extended), EnvelopeFollower

### 2f. VST — Add Phase 2 Modes

- Add Flanger, Vibe, Rotary, Filter mode sources to `CMakeLists.txt`
- Add `svf.h` to the build
- Update mode dropdown — 7 real entries, 5 placeholders
- Add per-mode P1/P2 display labels for new modes (e.g. Filter: "Resonance" / "Waveshape")
- Test all 7 modes in Standalone app — verify parameter ranges and audio output

### Phase 2 Deliverable
- 7 working modes on firmware and VST
- SVF block validated across Filter mode sub-modes

---

## Phase 3: Extended Effects

**Goal:** Implement the 4 more specialized modulation effects.

### 3a. PatternSequencer Block

- Implement `PatternSequencer` in `src/dsp/pattern_sequencer.h/.cpp`
- 16 built-in patterns, tap tempo sync, subdivision support

### 3b. Formant Mode

- Implement `FormantMode` with 5 vowel sub-modes
- Requires: SVF (2 instances in bandpass mode), LFO
- Formant frequency tables (compile-time)

### 3c. Pattern Trem Mode

- Implement `PatternTremMode`
- Requires: PatternSequencer (new)
- Tap tempo integration for beat-synced patterns

### 3d. AutoSwell Mode

- Implement `AutoSwellMode`
- Requires: EnvelopeFollower, optional DelayLineSDRAM (chorus)
- Volume envelope with separate attack/release

### 3e. Destroyer Mode

- Implement `DestroyerMode`
- Requires: SVF (post-destruction filter)
- Inline bitcrush + decimation (no new DSP blocks)

### 3f. VST — Add Phase 3 Modes

- Add Formant, Pattern Trem, AutoSwell, Destroyer mode sources to `CMakeLists.txt`
- Add `pattern_sequencer.h/.cpp` to the build
- Update mode dropdown — 11 real entries, 1 placeholder (Quadrature)
- Add per-mode P1/P2 display labels for new modes
- Pattern Trem: verify tap-tempo-equivalent behavior (host tempo sync or manual rate)
- Test all 11 modes in Standalone app

### Phase 3 Deliverable
- 11 working modes on firmware and VST
- All DSP blocks except HilbertTransform validated

---

## Phase 4: Quadrature (Hilbert Transform)

**Goal:** Implement the most DSP-intensive mode using Hilbert transform for true frequency shifting.

### 4a. HilbertTransform Block

- Implement `HilbertTransform` in `src/dsp/hilbert_transform.h`
- Two parallel 4th-order allpass chains
- Validate 90° phase accuracy across 20 Hz – 20 kHz

### 4b. Quadrature Mode

- Implement `QuadratureMode` with 4 sub-modes (AM/FM/FreqShift+/FreqShift−)
- AM uses simple multiplication (no Hilbert)
- FM uses audio-rate delay modulation
- FreqShift± uses HilbertTransform for single-sideband shifting

### 4c. VST — Final Mode + Polish

- Add Quadrature mode + `hilbert_transform.h` to `CMakeLists.txt`
- All 12 modes in dropdown — no more placeholders
- Add per-mode P1/P2 display labels for Quadrature (Carrier Waveform / Sub-mode)
- Add sub-mode selector UI (dropdown or secondary parameter) for all modes that have sub-modes
- Verify parameter automation works for all 7 params across all 12 modes
- Test FreqShift± in DAW to confirm Hilbert transform sounds correct without hardware

### Phase 4 Deliverable
- All 12 modes working on firmware and VST
- Full mode cycling and preset save/load verified on firmware
- VST3 + Standalone fully functional with all 12 modes

---

## Dependencies Between Phases

```
Phase 1 (Infrastructure + Chorus/Phaser/VTrem + VST foundation)
    │
    └── Phase 2 (Flanger/Vibe/Rotary/Filter + VST update)
            │
            └── Phase 3 (Formant/PatternTrem/AutoSwell/Destroyer + VST update)
                    │
                    └── Phase 4 (Quadrature + VST final polish)
```

Each phase includes VST updates so the plugin stays in sync with firmware DSP code. The VST Standalone app serves as a rapid iteration tool — test DSP changes without flashing hardware.

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Flash overflow (128 KB) | Monitor after each mode; enable LTO; consider QSPI boot |
| CPU overload (48 kHz × 48 block) | Profile each mode; Phaser 16-stage and Quadrature are heaviest |
| SDRAM contention | All buffers are short (~25 KB total) — very low risk |
| Through-zero flanger clicks | Careful zero-crossing interpolation; 2 ms crossfade at zero |
| Hilbert phase accuracy | Pre-computed coefficients validated in MATLAB/Python offline |
| Preset compatibility | New preset format; add version byte to header |
| VST/firmware divergence | VST updated in every phase; Standalone app used for rapid DSP testing |

# File-Level Change List

## Files to Delete

Remove all 10 delay mode implementations:

| File | Reason |
|------|--------|
| `src/modes/digital_delay.h` | Replaced by modulation modes |
| `src/modes/digital_delay.cpp` | |
| `src/modes/duck_delay.h` | |
| `src/modes/duck_delay.cpp` | |
| `src/modes/swell_delay.h` | |
| `src/modes/swell_delay.cpp` | |
| `src/modes/trem_delay.h` | |
| `src/modes/trem_delay.cpp` | |
| `src/modes/dbucket_delay.h` | |
| `src/modes/dbucket_delay.cpp` | |
| `src/modes/tape_delay.h` | |
| `src/modes/tape_delay.cpp` | |
| `src/modes/dual_delay.h` | |
| `src/modes/dual_delay.cpp` | |
| `src/modes/pattern_delay.h` | |
| `src/modes/pattern_delay.cpp` | |
| `src/modes/filter_delay.h` | |
| `src/modes/filter_delay.cpp` | |
| `src/modes/lofi_delay.h` | |
| `src/modes/lofi_delay.cpp` | |

## Files to Create

### New Mode Implementations (12 modes)

| File | Mode |
|------|------|
| `src/modes/mod_mode.h` | Abstract base class (replaces `delay_mode.h`) |
| `src/modes/chorus.h` | Chorus mode header |
| `src/modes/chorus.cpp` | Chorus mode implementation |
| `src/modes/flanger.h` | Flanger mode |
| `src/modes/flanger.cpp` | |
| `src/modes/rotary.h` | Rotary mode |
| `src/modes/rotary.cpp` | |
| `src/modes/vibe.h` | Vibe mode |
| `src/modes/vibe.cpp` | |
| `src/modes/phaser.h` | Phaser mode |
| `src/modes/phaser.cpp` | |
| `src/modes/filter_mod.h` | Filter mode (renamed to avoid conflict with `filter_delay`) |
| `src/modes/filter_mod.cpp` | |
| `src/modes/formant.h` | Formant mode |
| `src/modes/formant.cpp` | |
| `src/modes/vintage_trem.h` | Vintage Trem mode |
| `src/modes/vintage_trem.cpp` | |
| `src/modes/pattern_trem.h` | Pattern Trem mode |
| `src/modes/pattern_trem.cpp` | |
| `src/modes/auto_swell.h` | AutoSwell mode |
| `src/modes/auto_swell.cpp` | |
| `src/modes/destroyer.h` | Destroyer mode |
| `src/modes/destroyer.cpp` | |
| `src/modes/quadrature.h` | Quadrature mode |
| `src/modes/quadrature.cpp` | |

### New DSP Blocks (5 blocks)

| File | Block |
|------|-------|
| `src/dsp/allpass_filter.h` | First-order allpass for Phaser/Vibe |
| `src/dsp/svf.h` | State-variable filter (LP/BP/HP/Notch) |
| `src/dsp/bbd_emulator.h` | BBD analog chorus emulation |
| `src/dsp/bbd_emulator.cpp` | (needs SDRAM buffer) |
| `src/dsp/hilbert_transform.h` | 90° phase shift network |
| `src/dsp/pattern_sequencer.h` | Rhythmic amplitude pattern |
| `src/dsp/pattern_sequencer.cpp` | (pattern tables + tap sync) |

## Files to Modify

### Config

| File | Changes |
|------|---------|
| `src/config/constants.h` | `NUM_MODES = 12`, update MIDI CC comments, remove `MAX_DELAY_SAMPLES` (replace with mode-specific short buffer sizes) |
| `src/config/delay_mode_id.h` → `src/config/mod_mode_id.h` | Rename file; enum `ModModeId` with 12 entries: Chorus, Flanger, Rotary, Vibe, Phaser, Filter, Formant, VintageTrem, PatternTrem, AutoSwell, Destroyer, Quadrature |

### Parameters

| File | Changes |
|------|---------|
| `src/params/param_id.h` | Rename enum values: Time→Speed, Repeats→Depth, Filter→Tone, Grit→P1, ModSpd→P2, ModDep→Level (keep Mix at position 2) |
| `src/params/param_set.h` | Rename struct fields: `time`→`speed`, `repeats`→`depth`, `filter`→`tone`, `grit`→`p1`, `mod_spd`→`p2`, `mod_dep`→`level` |
| `src/params/param_set.cpp` | Update `get()` switch and `make_default()` |
| `src/params/param_map.h` | Update default ranges: SPEED (0.05–10 Hz), DEPTH (0–1), TONE (0–1), P1/P2 (mode-dependent), LEVEL (0–1); reference `ModModeId` |
| `src/params/param_map.cpp` | Per-mode range overrides for all 12 modes (especially Speed ranges per mode) |
| `src/params/param_range.h` | No changes (generic range mapping unchanged) |

### Modes

| File | Changes |
|------|---------|
| `src/modes/delay_mode.h` | Delete (replaced by `mod_mode.h`) |
| `src/modes/mode_registry.h` | Update to reference `ModMode`, `ModModeId`, 12 slots |
| `src/modes/mode_registry.cpp` | Register all 12 new mode instances, include new headers |

### Audio

| File | Changes |
|------|---------|
| `src/audio/audio_engine.h` | Change `DelayMode*` → `ModMode*`; update includes |
| `src/audio/audio_engine.cpp` | Update type references (no logic changes) |

### DSP

| File | Changes |
|------|---------|
| `src/dsp/lfo.h` | Add 5 new `LfoWave` variants, `Reset()`, `GetPhase()` |
| `src/dsp/lfo.cpp` | Implement new waveshapes, reset, phase getter |
| `src/dsp/delay_line_sdram.h` | No changes (interface unchanged) |
| `src/dsp/delay_line_sdram.cpp` | Remove large 3-second buffers; add short modulation buffers |

### Hardware / UI

| File | Changes |
|------|---------|
| `src/main.cpp` | Update mode/param references; 12-mode cycling |
| `src/display/display_manager.h` | Update param label rendering for new names |
| `src/display/display_manager.cpp` | Mode names, sub-mode display, Speed/Depth labels |
| `src/midi/midi_handler.h` | Update CC comments |
| `src/midi/midi_handler.cpp` | Program Change range 0–11 (was 0–9) |
| `src/presets/preset_manager.h` | Add version byte to preset format (breaking change) |
| `src/presets/preset_manager.cpp` | Handle new preset format + migration from old |

### Build System

| File | Changes |
|------|---------|
| `Makefile` | Remove old delay mode .cpp sources; add new mode + DSP block .cpp sources |
| `desktop/vst/CMakeLists.txt` | Same source file updates; add new DSP blocks (updated each phase to stay in sync) |

### VST Plugin (updated each phase alongside firmware)

| File | Changes |
|------|---------|
| `desktop/vst/CMakeLists.txt` | Remove delay mode sources, add modulation mode + DSP block sources incrementally per phase |
| `desktop/vst/include/daisy_seed.h` | Verify stub covers any new SDRAM macros (likely no changes) |
| `desktop/vst/src/PluginProcessor.h` | Rename APVTS parameters (Speed/Depth/Mix/Tone/P1/P2/Level); update `ModModeId` references; 12 modes |
| `desktop/vst/src/PluginProcessor.cpp` | `buildParamsFromState()` maps new param names; per-mode `textFromValueFunction` / `valueFromTextFunction` for Speed ranges and P1/P2 labels; mode dropdown 12 entries |
| `desktop/vst/src/PluginEditor.h` | Update knob labels; add sub-mode selector UI component |
| `desktop/vst/src/PluginEditor.cpp` | Primary knobs: Speed/Depth/Mix/Tone; secondary: P1/P2/Level; mode dropdown with 12 entries; sub-mode dropdown per mode; per-mode P1/P2 label text |

### Documentation

| File | Changes |
|------|---------|
| `CLAUDE.md` | Full rewrite for modulation pedal (see `docs/claude-md.md`) |
| `README.md` | Update feature list, mode table, parameter names, memory budget |
| `USER_MANUAL.md` | Full rewrite: 12 modulation modes, new parameter descriptions |

## Summary

| Action | Count |
|--------|-------|
| Delete | 20 files (10 delay mode .h/.cpp pairs) + 1 file (`delay_mode.h`) |
| Create | 31 files (12 mode .h/.cpp pairs + 5 DSP block files + `mod_mode.h` + `mod_mode_id.h`) |
| Modify | 26 files (params, config, audio, dsp, display, midi, presets, build, VST plugin, docs) |
| Rename | 1 file (`delay_mode_id.h` → `mod_mode_id.h`) |

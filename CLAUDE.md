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
# Output: build/delay.bin, build/delay.elf
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

All 7 parameters (`time`, `repeats`, `mix`, `filter`, `grit`, `mod_spd`, `mod_dep`) flow as normalized `[0, 1]` values through the system and are converted to physical units at the last moment via `map_param()`:

```
Encoders / MIDI CC / Preset recall
        ↓
  ParamEditState  (normalized floats, main loop only)
        ↓
  BuildParams() → map_param() → ParamSet  (physical units, immutable snapshot)
        ↓
  TempoSync override (clamps ps.time when tap/MIDI clock active)
        ↓
  AudioEngine::SetParams()  (writes to idle double-buffer)
        ↓
  AudioCallback ISR → DelayMode::Process()
```

`ParamSet` is the immutable snapshot struct — never mutate it, always produce a new one. `ParamRange` in `src/params/param_range.h` defines `{min, max, curve}` per parameter; `map_param()` / `unmap_param()` convert between normalized and physical.

**Per-mode time ranges** (`src/params/param_map.cpp`):
- All modes except Lofi: `60 ms – 2500 ms`
- Lofi: `2 ms – 2500 ms`
- These are the only mode-specific overrides; all other parameters use `default_ranges`.

### Mode System

`DelayMode` (abstract base in `src/modes/delay_mode.h`) has four virtual methods:
- `Init()` — called once at startup
- `Reset()` — called on mode switch to clear buffers
- `Prepare(const ParamSet&)` — optional per-block setup (e.g. set filter coefficients)
- `Process(float, const ParamSet&) → StereoFrame` — per-sample DSP, returns **wet signal only**

`ModeRegistry` owns all 10 statically-allocated mode instances. Mode switching is a pointer swap + `Reset()` — no allocation, no audio dropout beyond the buffer clear.

The audio engine applies dry/wet mixing (constant-power equal-power crossfade on `mix`), not the modes themselves. Modes should output wet-only.

### SDRAM Allocation

All delay line buffers use `DSY_SDRAM_BSS` (placed in external SDRAM by the linker). They must be **file-scope static** — never stack or heap. The `daisy_seed.h` include (pulled in via `delay_line_sdram.h`) is required for this attribute to be defined. Total usage: ~6.3 MB of 64 MB SDRAM.

### Thread Safety (firmware)

The audio ISR and main loop communicate via a double-buffered `ParamSet[2]` in `AudioEngine`. Main loop writes to the idle buffer and sets `param_dirty_`; the ISR swaps the read index at block entry. No mutexes — relies on aligned-word-store atomicity on Cortex-M7. Never read `param_buf_` from the main loop after calling `SetParams()`.

### Flash Constraint

FLASH is at ~93% capacity (128 KB total). Be conservative when adding new features — avoid string literals, large lookup tables, or additional template instantiations.

### VST Plugin vs Firmware

The VST (`desktop/vst/`) reuses `src/dsp/`, `src/params/`, and `src/modes/` directly. It stubs out the hardware layer via `desktop/vst/include/daisy_seed.h`. Key differences:
- `DSY_SDRAM_BSS` is a no-op in the stub (buffers land in normal BSS)
- The VST normalizes all parameters as `[0, 1]` APVTS parameters and maps them in `PluginProcessor::buildParamsFromState()`
- The Time knob in the editor displays mapped ms values via `textFromValueFunction` / `valueFromTextFunction` using the current mode's `ParamRange`
- Mode changes in the VST do not automatically refresh the Time knob text display until the knob is interacted with

### Tempo Priority Chain

`TempoSync` (`src/tempo/`) arbitrates the `time` parameter:
1. **MIDI Clock** (highest) — locks `ps.time` to beat period; expires 2 s after last tick
2. **Tap Tempo** — averaging up to 4 taps; timeout 2 s
3. **Encoder / MIDI CC** — normal control (override = 0, ignored by `BuildParams`)

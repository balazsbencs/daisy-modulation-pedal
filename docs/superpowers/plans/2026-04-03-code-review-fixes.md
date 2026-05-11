# Code Review Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix all CRITICAL and HIGH issues, plus selected MEDIUM issues, from the comprehensive code review.

**Architecture:** Changes are organized into independent tasks grouped by subsystem. Each task targets a single file or tightly coupled pair. No new files are created; all changes modify existing code. The project has no automated tests, so verification is done via firmware build (`make -j4`) and VST build (`cd desktop/vst/build && make -j4`).

**Tech Stack:** C++17, ARM Cortex-M7 (libDaisy), JUCE 8 (VST3), CMake

---

## File Map

| Task | Files Modified | Responsibility |
|------|---------------|----------------|
| 1 | `desktop/vst/src/PluginProcessor.h` | VST thread safety: atomic mode/P1/P2 |
| 2 | `desktop/vst/src/PluginProcessor.cpp` | VST sample rate support + state restore order |
| 3 | `src/modes/formant_mode.h`, `src/modes/formant_mode.cpp` | Per-sample LFO in FormantMode |
| 4 | `src/dsp/lfo.h`, `src/dsp/lfo.cpp` | LFO safety: guard SetPhaseOffset, S&H on negative wrap |
| 5 | `src/dsp/pattern_sequencer.h` | Integer timing counter |
| 6 | `src/modes/phaser_mode.cpp` | Sub-mode index clamping |
| 7 | `src/modes/chorus_mode.cpp` | Reset cached fields |
| 8 | `src/audio/audio_engine.cpp` | Clamp mix before trig |
| 9 | `src/params/param_range.h` | Clamp in map_param, fix doc comment |
| 10 | `src/tempo/tempo_sync.cpp` | Reset clock_count_ on timeout |
| 11 | `src/main.cpp` | Exclude Destroyer from tempo override |
| 12 | `src/modes/destroyer_mode.h`, `src/modes/destroyer_mode.cpp` | Add DC blocker |
| 13 | `src/modes/quadrature_mode.h`, `src/modes/quadrature_mode.cpp` | Add DC blocker to AM path |
| 14 | `src/modes/auto_swell_mode.h` | Remove dead `swelling_` member |
| 15 | `src/presets/preset_manager.cpp` | Fix CRC padding + save-on-load wear |
| 16 | `src/display/display_manager.cpp` | PutUInt buffer overflow |
| 17 | `src/config/pin_map.h` | Rename stale pot names |

---

### Task 1: VST Thread Safety — Atomic Mode and Per-Mode P1/P2

**Files:**
- Modify: `desktop/vst/src/PluginProcessor.h:52-63`

This task makes `current_mode_` atomic and protects `p1_per_mode_`/`p2_per_mode_` with `std::atomic<float>`. The `active_mode_` pointer is only written in `ensureModeFromParam()` which is called from `processBlock()` (audio thread) and `setStateInformation()` (message thread). Making `current_mode_` atomic plus a SpinLock for the mode switch protects both.

- [ ] **Step 1: Add atomic includes and SpinLock to PluginProcessor.h**

Replace lines 52-63:

```cpp
// old:
    pedal::ModeRegistry mode_registry_;
    pedal::ModModeId    current_mode_ = pedal::ModModeId::Chorus;
    pedal::ModMode*     active_mode_  = nullptr;

    // Per-mode memory for P1 and P2 normalized values.
    float p1_per_mode_[12];
    float p2_per_mode_[12];

public:
    float getP1ForMode(int mode) const { return p1_per_mode_[juce::jlimit(0, 11, mode)]; }
    float getP2ForMode(int mode) const { return p2_per_mode_[juce::jlimit(0, 11, mode)]; }
    void  saveP1ForMode(int mode, float v) { p1_per_mode_[juce::jlimit(0, 11, mode)] = v; }
    void  saveP2ForMode(int mode, float v) { p2_per_mode_[juce::jlimit(0, 11, mode)] = v; }
```

with:

```cpp
    pedal::ModeRegistry mode_registry_;
    std::atomic<pedal::ModModeId> current_mode_ {pedal::ModModeId::Chorus};
    pedal::ModMode*     active_mode_  = nullptr;
    juce::SpinLock      modeLock_;

    // Per-mode memory for P1 and P2 normalized values (atomic for cross-thread access).
    std::atomic<float> p1_per_mode_[pedal::NUM_MODES];
    std::atomic<float> p2_per_mode_[pedal::NUM_MODES];

public:
    pedal::ModModeId getCurrentMode() const { return current_mode_.load(std::memory_order_acquire); }
    float getP1ForMode(int mode) const { return p1_per_mode_[juce::jlimit(0, pedal::NUM_MODES - 1, mode)].load(std::memory_order_relaxed); }
    float getP2ForMode(int mode) const { return p2_per_mode_[juce::jlimit(0, pedal::NUM_MODES - 1, mode)].load(std::memory_order_relaxed); }
    void  saveP1ForMode(int mode, float v) { p1_per_mode_[juce::jlimit(0, pedal::NUM_MODES - 1, mode)].store(v, std::memory_order_relaxed); }
    void  saveP2ForMode(int mode, float v) { p2_per_mode_[juce::jlimit(0, pedal::NUM_MODES - 1, mode)].store(v, std::memory_order_relaxed); }
```

Also add `#include <atomic>` at the top of the file and remove the old `getCurrentMode()` at line 42.

- [ ] **Step 2: Update PluginProcessor.cpp to use SpinLock in ensureModeFromParam**

In `PluginProcessor.cpp:104-113`, wrap the mode switch in a lock:

```cpp
void ModulationPluginProcessor::ensureModeFromParam() {
    const int next_mode = juce::jlimit(0, pedal::NUM_MODES - 1,
        (int)std::lround(getParam(apvts_, "mode")));
    const auto next_id = static_cast<pedal::ModModeId>(next_mode);
    if (next_id == current_mode_.load(std::memory_order_acquire)) return;

    const juce::SpinLock::ScopedLockType lock(modeLock_);
    current_mode_.store(next_id, std::memory_order_release);
    active_mode_  = mode_registry_.get(next_id);
    if (active_mode_ != nullptr) active_mode_->Reset();
}
```

Also update `buildParamsFromState` to use `current_mode_.load()` and the constructor `p1_per_mode_`/`p2_per_mode_` init to use `.store()`. Update `getStateInformation`/`setStateInformation` to use `.load()`/`.store()` on the per-mode arrays.

- [ ] **Step 3: Build and verify**

Run: `cd /Users/bbalazs/daisy/modulation/desktop/vst/build && make -j4`
Expected: Clean build with no warnings about atomic operations.

- [ ] **Step 4: Commit**

```bash
git add desktop/vst/src/PluginProcessor.h desktop/vst/src/PluginProcessor.cpp
git commit -m "fix: add thread safety to VST mode switching and per-mode P1/P2 arrays

current_mode_ is now std::atomic, p1/p2_per_mode_ are std::atomic<float>,
and ensureModeFromParam() is guarded by a SpinLock to prevent torn pointer
reads of active_mode_ across audio and message threads."
```

---

### Task 2: VST Sample Rate Support + State Restore Order

**Files:**
- Modify: `desktop/vst/src/PluginProcessor.h:44-46`
- Modify: `desktop/vst/src/PluginProcessor.cpp:59,74,201-218`

The VST ignores the host sample rate, causing all DSP timing to be wrong at non-48kHz. Also, state restore applies per-mode P1/P2 before `replaceState()` which can clobber them.

- [ ] **Step 1: Add sample_rate_ member to PluginProcessor.h**

After the `modeLock_` member, add:

```cpp
    float sample_rate_ = pedal::SAMPLE_RATE;
```

- [ ] **Step 2: Store sample rate in prepareToPlay and use it in buildParamsFromState**

In `PluginProcessor.cpp:59`, change:

```cpp
void ModulationPluginProcessor::prepareToPlay(double, int) {
```

to:

```cpp
void ModulationPluginProcessor::prepareToPlay(double sampleRate, int) {
    sample_rate_ = static_cast<float>(sampleRate);
```

In `buildParamsFromState`, after building `ps`, add a sample-rate correction for speed:

```cpp
    // Rescale speed from 48kHz-based mapping to the actual host sample rate.
    // LFO phase increment is proportional to rate/sample_rate, so if the host
    // runs at 96kHz the perceived rate halves unless we compensate.
    if (sample_rate_ > 0.0f && sample_rate_ != pedal::SAMPLE_RATE) {
        ps.speed *= sample_rate_ / pedal::SAMPLE_RATE;
    }
```

Note: A full fix would require passing sample_rate to all DSP blocks (SVF, EnvelopeFollower, etc.). This is a targeted fix for the most audible issue (LFO speed). Add a TODO comment noting filter coefficients also depend on SAMPLE_RATE.

- [ ] **Step 3: Fix state restore order in setStateInformation**

In `PluginProcessor.cpp:201-218`, swap the order so `replaceState` happens first:

```cpp
void ModulationPluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr) return;
    if (!xmlState->hasTagName(apvts_.state.getType())) return;

    // Replace APVTS state first (triggers parameter listeners).
    apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));

    // Then restore per-mode P1/P2 — these won't be clobbered by listeners now.
    if (auto* perMode = xmlState->getChildByName("PerModeParams")) {
        for (auto* el : perMode->getChildWithTagNameIterator("Mode")) {
            const int m = el->getIntAttribute("index", -1);
            if (m >= 0 && m < pedal::NUM_MODES) {
                saveP1ForMode(m, (float)el->getDoubleAttribute("p1", getP1ForMode(m)));
                saveP2ForMode(m, (float)el->getDoubleAttribute("p2", getP2ForMode(m)));
            }
        }
    }

    ensureModeFromParam();
}
```

- [ ] **Step 4: Build and verify**

Run: `cd /Users/bbalazs/daisy/modulation/desktop/vst/build && make -j4`
Expected: Clean build.

- [ ] **Step 5: Commit**

```bash
git add desktop/vst/src/PluginProcessor.h desktop/vst/src/PluginProcessor.cpp
git commit -m "fix: use host sample rate in VST and fix state restore ordering

Store sampleRate from prepareToPlay and rescale LFO speed accordingly.
Swap replaceState/PerModeParams order in setStateInformation to prevent
listener callbacks from clobbering the just-restored per-mode values."
```

---

### Task 3: FormantMode Per-Sample LFO

**Files:**
- Modify: `src/modes/formant_mode.h:21-26`
- Modify: `src/modes/formant_mode.cpp:30-64`

FormantMode calls `lfo_.PrepareBlock()` in `Prepare()`, violating the per-sample LFO convention. The fix moves the LFO into `Process()` but keeps `SetFreq()` (which calls `tanf()`) in `Prepare()` to avoid per-sample `tanf()`. The formant slew is driven by a per-sample LFO value cached from the previous sample.

- [ ] **Step 1: Add cached LFO and target fields to formant_mode.h**

Replace lines 21-26:

```cpp
    Lfo      lfo_;
    Svf      f1_, f2_;   // formant 1 and 2 bandpass filters
    DcBlocker dc_;

    float f1_hz_ = 500.0f;
    float f2_hz_ = 1500.0f;
```

with:

```cpp
    Lfo      lfo_;
    Svf      f1_, f2_;
    DcBlocker dc_;

    float f1_hz_       = 500.0f;
    float f2_hz_       = 1500.0f;
    float target_f1_   = 500.0f;
    float target_f2_   = 1500.0f;
    float morph_depth_ = 0.5f;
    int   update_count_ = 0;
```

- [ ] **Step 2: Rewrite Prepare to set targets only, and Process to advance LFO per-sample**

Replace `formant_mode.cpp:30-64` with:

```cpp
void FormantMode::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.speed);

    // Target vowel from p2 (0..1 -> 5 vowels)
    const int vowel = static_cast<int>(params.p2 * 4.999f);
    target_f1_ = kVowels[vowel].f1;
    target_f2_ = kVowels[vowel].f2;
    morph_depth_ = params.depth;

    // P1 controls formant bandwidth (Q: higher = narrower)
    const float q1 = 2.0f + params.p1 * 8.0f;
    const float q2 = 4.0f + params.p1 * 10.0f;
    f1_.SetQ(q1);
    f2_.SetQ(q2);

    // Apply current formant frequencies (updated per-sample in Process via slew).
    // SetFreq calls tanf() — safe here at block rate, not per-sample.
    f1_.SetFreq(f1_hz_);
    f2_.SetFreq(f2_hz_);
}

StereoFrame FormantMode::Process(StereoFrame input, const ParamSet& /*params*/) {
    // Per-sample LFO for smooth formant morph
    const float lfo_val = lfo_.Process();
    const float t = 0.5f + 0.5f * lfo_val;    // 0..1
    const float morph = morph_depth_ * t;

    // Slew formant frequencies toward target
    f1_hz_ += (target_f1_ - f1_hz_) * morph * 0.1f;
    f2_hz_ += (target_f2_ - f2_hz_) * morph * 0.1f;

    // Update SVF coefficients every 16 samples to amortize tanf() cost
    // while still tracking the per-sample LFO smoothly.
    if (++update_count_ >= 16) {
        update_count_ = 0;
        f1_.SetFreq(f1_hz_);
        f2_.SetFreq(f2_hz_);
    }

    const float mono = input.mono();
    f1_.Process(mono);
    f2_.Process(mono);
    float wet = (f1_.bp() + f2_.bp()) * 0.5f;
    wet = dc_.Process(wet);
    return {wet, wet};
}
```

- [ ] **Step 3: Update Reset to clear new fields**

In `formant_mode.cpp:20-28`, add the new fields:

```cpp
void FormantMode::Reset() {
    lfo_.Init(0.5f, LfoWave::Sine);
    f1_.Reset(); f2_.Reset();
    f1_.SetQ(4.0f);
    f2_.SetQ(6.0f);
    dc_.Init();
    f1_hz_       = kVowels[0].f1;
    f2_hz_       = kVowels[0].f2;
    target_f1_   = kVowels[0].f1;
    target_f2_   = kVowels[0].f2;
    morph_depth_ = 0.5f;
    update_count_ = 0;
}
```

- [ ] **Step 4: Build firmware**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4`
Expected: Clean build.

- [ ] **Step 5: Build VST**

Run: `cd /Users/bbalazs/daisy/modulation/desktop/vst/build && make -j4`
Expected: Clean build.

- [ ] **Step 6: Commit**

```bash
git add src/modes/formant_mode.h src/modes/formant_mode.cpp
git commit -m "fix: use per-sample LFO in FormantMode instead of PrepareBlock

Move lfo_.Process() into Process() for smooth formant morph at sample rate.
SVF SetFreq (tanf) is called every 16 samples to amortize cost while
tracking the per-sample LFO smoothly. Fixes staircase artifacts at high
LFO speeds."
```

---

### Task 4: LFO Safety — Guard SetPhaseOffset and S&H on Negative Wrap

**Files:**
- Modify: `src/dsp/lfo.h:23-28`
- Modify: `src/dsp/lfo.cpp:59-61`

`SetPhaseOffset` has unbounded while-loops that freeze the device on NaN/Inf. S&H is not triggered on negative phase wrapping.

- [ ] **Step 1: Guard SetPhaseOffset against NaN/Inf/huge values**

Replace `lfo.h:23-28`:

```cpp
    void SetPhaseOffset(float offset_radians) {
        // Normalize to [0, 2pi) to satisfy fast_sin() contract
        static constexpr float TWO_PI = 6.28318530717958647692f;
        while (offset_radians >= TWO_PI) offset_radians -= TWO_PI;
        while (offset_radians < 0.0f) offset_radians += TWO_PI;
        phase_offset_ = offset_radians;
    }
```

with:

```cpp
    void SetPhaseOffset(float offset_radians) {
        static constexpr float TWO_PI = 6.28318530717958647692f;
        // Guard against NaN, Inf, or huge values that would spin the while-loop
        if (offset_radians != offset_radians || offset_radians > 1e6f || offset_radians < -1e6f) {
            phase_offset_ = 0.0f;
            return;
        }
        while (offset_radians >= TWO_PI) offset_radians -= TWO_PI;
        while (offset_radians < 0.0f) offset_radians += TWO_PI;
        phase_offset_ = offset_radians;
    }
```

- [ ] **Step 2: Add S&H update on negative phase wrap**

Replace `lfo.cpp:59-61`:

```cpp
    while (phase_ < 0.0f) {
        phase_ += TWO_PI;
    }
```

with:

```cpp
    while (phase_ < 0.0f) {
        phase_ += TWO_PI;
        rand_     = rand_ * 1664525u + 1013904223u;
        sh_value_ = static_cast<float>(static_cast<int32_t>(rand_)) * (1.0f / 2147483648.0f);
    }
```

- [ ] **Step 3: Build firmware**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4`
Expected: Clean build.

- [ ] **Step 4: Commit**

```bash
git add src/dsp/lfo.h src/dsp/lfo.cpp
git commit -m "fix: guard LFO SetPhaseOffset against NaN/Inf, trigger S&H on negative wrap

SetPhaseOffset now returns early with offset=0 if input is NaN, Inf, or
absurdly large (>1e6). S&H value is now updated on negative phase wrapping
so reverse-LFO S&H mode works correctly."
```

---

### Task 5: PatternSequencer Integer Timing Counter

**Files:**
- Modify: `src/dsp/pattern_sequencer.h:12-14,31-35,68-69`

Float accumulation of `sample_counter_` drifts over minutes. Switch to integer.

- [ ] **Step 1: Replace float counter with integer**

Replace the full `PatternSequencer` class body in `pattern_sequencer.h` with:

```cpp
class PatternSequencer {
public:
    void Reset() {
        sample_counter_ = 0;
        current_step_   = 0;
        step_active_    = true;
    }

    void SetPeriodSamples(float samples_per_beat) {
        period_ = (samples_per_beat > 1.0f)
            ? static_cast<int>(samples_per_beat + 0.5f) : 1;
    }

    void SetPattern(int pattern_idx, int steps_per_beat) {
        pattern_idx_    = pattern_idx  & 0xF;
        steps_per_beat_ = (steps_per_beat < 1) ? 1
                        : (steps_per_beat > 16) ? 16 : steps_per_beat;
    }

    float Process() {
        const int step_samples = period_ / steps_per_beat_;
        const int threshold = (step_samples > 0) ? step_samples : 1;
        ++sample_counter_;
        if (sample_counter_ >= threshold) {
            sample_counter_ -= threshold;
            current_step_ = (current_step_ + 1) % 16;
            step_active_  = GetStep(pattern_idx_, current_step_);
        }
        return step_active_ ? 1.0f : 0.0f;
    }

    int CurrentStep() const { return current_step_; }

private:
    // GetStep stays the same (keep existing static method)
```

Keep the `GetStep` static method and `kPatterns` array unchanged. Replace the member declarations at the bottom:

```cpp
    int      period_         = static_cast<int>(SAMPLE_RATE);
    int      sample_counter_ = 0;
    int      current_step_   = 0;
    int      pattern_idx_    = 0;
    int      steps_per_beat_ = 4;
    bool     step_active_    = true;
};
```

- [ ] **Step 2: Build firmware**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4`
Expected: Clean build.

- [ ] **Step 3: Commit**

```bash
git add src/dsp/pattern_sequencer.h
git commit -m "fix: use integer timing counter in PatternSequencer to prevent drift

Float sample_counter_ accumulated rounding error over minutes of playback.
Switching to int eliminates timing drift entirely and is cheaper on CPU."
```

---

### Task 6: Phaser Sub-Mode Index Clamping

**Files:**
- Modify: `src/modes/phaser_mode.cpp:35`

Unclamped array index into `kStageCounts[7]` if `p2` exceeds 1.0.

- [ ] **Step 1: Add clamp**

Replace line 35:

```cpp
    const int sub = static_cast<int>(params.p2 * 6.999f);
```

with:

```cpp
    int sub = static_cast<int>(params.p2 * 6.999f);
    if (sub < 0) sub = 0;
    if (sub > 6) sub = 6;
```

- [ ] **Step 2: Build firmware**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4`
Expected: Clean build.

- [ ] **Step 3: Commit**

```bash
git add src/modes/phaser_mode.cpp
git commit -m "fix: clamp phaser sub-mode index to prevent out-of-bounds read

kStageCounts has 7 elements (0-6). Without clamping, float drift in p2
could produce index 7, reading past the array."
```

---

### Task 7: Chorus Reset Cached Fields

**Files:**
- Modify: `src/modes/chorus_mode.cpp:26-33`

`sub_mode_`, `base_samps_`, `mod_depth_`, `delays_[]` not cleared on Reset.

- [ ] **Step 1: Reset all cached fields**

Replace lines 26-33:

```cpp
void ChorusMode::Reset() {
    s_chorus_line.Reset();
    for (auto& l : lfo_) l.Reset();
    dc_.Init();
    dc_r_.Init();
    bbd_.Reset();
    rand_ = 12345;
}
```

with:

```cpp
void ChorusMode::Reset() {
    s_chorus_line.Reset();
    for (auto& l : lfo_) l.Reset();
    dc_.Init();
    dc_r_.Init();
    bbd_.Reset();
    rand_ = 12345;
    sub_mode_   = 4;
    base_samps_ = 48.0f;
    mod_depth_  = 0.0f;
    delays_[0]  = 0.0f;
    delays_[1]  = 0.0f;
    delays_[2]  = 0.0f;
}
```

- [ ] **Step 2: Build firmware**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4`
Expected: Clean build.

- [ ] **Step 3: Commit**

```bash
git add src/modes/chorus_mode.cpp
git commit -m "fix: reset cached sub-mode fields in ChorusMode::Reset

Prevents stale sub_mode_, base_samps_, mod_depth_, and delays_ from
persisting across mode switches for up to 48 samples before Prepare runs."
```

---

### Task 8: Clamp Mix Before Trig in AudioEngine

**Files:**
- Modify: `src/audio/audio_engine.cpp:102-107`

If `params.mix` drifts outside [0,1], `fast_cos`/`fast_sin` get out-of-domain input.

- [ ] **Step 1: Add clamp before trig computation**

Replace lines 102-107:

```cpp
    if (params.mix != last_mix_) {
        last_mix_ = params.mix;
        const float angle = params.mix * 1.57079632679f; // pi/2
        mix_dry_          = fast_cos(angle);
        mix_wet_          = fast_sin(angle);
    }
```

with:

```cpp
    if (params.mix != last_mix_) {
        last_mix_ = params.mix;
        const float mix = (params.mix < 0.0f) ? 0.0f
                        : (params.mix > 1.0f) ? 1.0f : params.mix;
        const float angle = mix * 1.57079632679f; // pi/2
        mix_dry_          = fast_cos(angle);
        mix_wet_          = fast_sin(angle);
    }
```

- [ ] **Step 2: Build firmware**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4`
Expected: Clean build.

- [ ] **Step 3: Commit**

```bash
git add src/audio/audio_engine.cpp
git commit -m "fix: clamp mix to [0,1] before fast_cos/fast_sin in audio engine

Prevents out-of-domain input to fast trig functions if params.mix
drifts outside [0,1] due to floating-point arithmetic."
```

---

### Task 9: Clamp in map_param and Fix Doc Comment

**Files:**
- Modify: `src/params/param_range.h:9-10,28-31`

`map_param` doesn't clamp input; negative values with fractional exponent produce NaN. Doc comment claims `t^(2^curve)` but code computes `t^(curve+1)`.

- [ ] **Step 1: Fix doc comment and add clamp**

Replace line 9-10:

```cpp
    // Curve: 0=linear, positive=log (boost lows), negative=exp (boost highs)
    float curve; // exponent applied: value = t^(2^curve)
```

with:

```cpp
    // Curve: 0=linear, positive=log (boost lows), negative=exp (boost highs)
    // Exponent: curve>0 -> t^(curve+1), curve<0 -> t^(1/(1-curve))
    float curve;
```

Replace lines 28-31:

```cpp
inline float map_param(float normalized, const ParamRange& range) {
    float curved = apply_curve(normalized, range.curve);
    return range.min + curved * (range.max - range.min);
}
```

with:

```cpp
inline float map_param(float normalized, const ParamRange& range) {
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    float curved = apply_curve(normalized, range.curve);
    return range.min + curved * (range.max - range.min);
}
```

- [ ] **Step 2: Build firmware**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4`
Expected: Clean build.

- [ ] **Step 3: Commit**

```bash
git add src/params/param_range.h
git commit -m "fix: clamp normalized input in map_param and correct curve doc comment

Negative normalized values with fractional exponent produce NaN via powf.
Doc comment now correctly describes the actual curve formula."
```

---

### Task 10: Reset clock_count_ on MIDI Clock Timeout

**Files:**
- Modify: `src/tempo/tempo_sync.cpp:53-56`

When MIDI clock times out, `clock_count_` is not reset, causing the first beat after reconnection to arrive at the wrong time.

- [ ] **Step 1: Add clock_count_ and last_beat_ms_ reset to timeout branch**

Replace lines 53-56:

```cpp
        if (silence > MIDI_CLOCK_TIMEOUT_MS) {
            midi_active_   = false;
            midi_period_s_ = -1.0f;
        }
```

with:

```cpp
        if (silence > MIDI_CLOCK_TIMEOUT_MS) {
            midi_active_   = false;
            midi_period_s_ = -1.0f;
            clock_count_   = 0;
            last_beat_ms_  = 0;
        }
```

- [ ] **Step 2: Build firmware**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4`
Expected: Clean build.

- [ ] **Step 3: Commit**

```bash
git add src/tempo/tempo_sync.cpp
git commit -m "fix: reset MIDI clock_count_ and last_beat_ms_ on timeout

Without resetting, the first beat after MIDI clock reconnection was
misaligned because clock_count_ continued from the stale value."
```

---

### Task 11: Exclude Destroyer from Tempo Sync Override

**Files:**
- Modify: `src/main.cpp:109`

Tempo override applies Hz-based rate to Destroyer's decimation-rate parameter, which is semantically wrong.

- [ ] **Step 1: Add Destroyer to the exclusion condition**

Replace line 109:

```cpp
    if (speed_override > 0.0f && mode != pedal::ModModeId::AutoSwell) {
```

with:

```cpp
    if (speed_override > 0.0f
        && mode != pedal::ModModeId::AutoSwell
        && mode != pedal::ModModeId::Destroyer) {
```

- [ ] **Step 2: Apply the same fix in the VST**

In `PluginProcessor.cpp:89`, replace:

```cpp
    if (host_period_s > 0.0f && current_mode_ != ModModeId::AutoSwell) {
```

with:

```cpp
    if (host_period_s > 0.0f
        && current_mode_.load(std::memory_order_acquire) != ModModeId::AutoSwell
        && current_mode_.load(std::memory_order_acquire) != ModModeId::Destroyer) {
```

(Note: if Task 1 is done first, use `.load()` as shown. If not yet done, use the non-atomic form.)

- [ ] **Step 3: Build both targets**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4 && cd desktop/vst/build && make -j4`
Expected: Both build cleanly.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp desktop/vst/src/PluginProcessor.cpp
git commit -m "fix: exclude Destroyer from tempo sync speed override

Destroyer's speed parameter is a decimation rate (1x-48x), not Hz.
Applying an Hz-based tempo override produces nonsensical values."
```

---

### Task 12: Add DC Blocker to Destroyer Output

**Files:**
- Modify: `src/modes/destroyer_mode.h:3,22`
- Modify: `src/modes/destroyer_mode.cpp:12,55`

Bit crushing with asymmetric input produces DC offset that passes through the LP filter.

- [ ] **Step 1: Add DcBlocker member to header**

In `destroyer_mode.h`, add include after line 3:

```cpp
#include "../dsp/dc_blocker.h"
```

Add member after `rand_` at line 27:

```cpp
    DcBlocker dc_;
```

- [ ] **Step 2: Init/Reset the DC blocker and apply in Process**

In `destroyer_mode.cpp:11-18`, add `dc_.Init();` inside `Reset()`:

```cpp
void DestroyerMode::Reset() {
    svf_.Reset();
    dc_.Init();
    held_sample_  = 0.0f;
    decimate_acc_ = 0.0f;
    decimate_rate_= 1.0f;
    bits_         = 16;
    rand_         = 12345;
}
```

In `destroyer_mode.cpp`, after `float wet = svf_.lp();` (line 55), add DC blocking before the noise section:

```cpp
    wet = dc_.Process(wet);
```

- [ ] **Step 3: Build firmware**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4`
Expected: Clean build.

- [ ] **Step 4: Commit**

```bash
git add src/modes/destroyer_mode.h src/modes/destroyer_mode.cpp
git commit -m "fix: add DC blocker to Destroyer output path

Bit crushing with asymmetric input produces DC offset that the SVF LP
filter passes through. DcBlocker removes it before the wet output."
```

---

### Task 13: Add DC Blocker to Quadrature AM Path

**Files:**
- Modify: `src/modes/quadrature_mode.h:4,30`
- Modify: `src/modes/quadrature_mode.cpp:32,86-91`

Ring modulation at low carrier rates produces DC offset.

- [ ] **Step 1: Add DcBlocker member to header**

In `quadrature_mode.h`, add include after line 4:

```cpp
#include "../dsp/dc_blocker.h"
```

Add member after `sub_mode_` at line 32:

```cpp
    DcBlocker dc_;
```

- [ ] **Step 2: Init/Reset and apply in AM path**

In `quadrature_mode.cpp:32-38`, add `dc_.Init();` inside `Reset()`:

```cpp
void QuadratureMode::Reset() {
    hilbert_.Reset();
    lfo_.Init(1.0f, LfoWave::Sine);
    dc_.Init();
    carrier_phase_ = 0.0f;
    phase_inc_     = 0.0f;
    sub_mode_      = 0;
}
```

In the AM case (`quadrature_mode.cpp:82-91`), apply DC blocking:

```cpp
        case 0: {
            const float ring = dc_.Process(mono * cos_c);
            const float p1   = params.p1;
            return {
                ring,
                ring * (1.0f - p1) + dc_.Process(mono * (-sin_c)) * p1,
            };
        }
```

Wait — using `dc_` twice in the same sample is wrong (one filter, two paths). Need two DC blockers, but that adds a member. Simpler: apply one DC blocker to the final blended output. Revise:

```cpp
        case 0: {
            const float ring = mono * cos_c;
            const float p1   = params.p1;
            const float out_l = ring;
            const float out_r = ring * (1.0f - p1) + (mono * (-sin_c)) * p1;
            return {dc_.Process(out_l), dc_.Process(out_r)};
        }
```

Actually, one DC blocker processing two channels sequentially in one sample is still wrong — it shares state. For AM, DC on both channels is correlated (same carrier), so a single blocker on the mono ring signal before stereo split is cleaner:

```cpp
        case 0: {
            const float ring = dc_.Process(mono * cos_c);
            const float p1   = params.p1;
            return {
                ring,
                ring * (1.0f - p1) + (mono * (-sin_c)) * p1,
            };
        }
```

The right channel's `mono * (-sin_c)` term is 90 degrees offset and has similar DC characteristics. The DC blocker on the ring signal handles the dominant DC component. This is a reasonable trade-off.

- [ ] **Step 3: Build firmware**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4`
Expected: Clean build.

- [ ] **Step 4: Commit**

```bash
git add src/modes/quadrature_mode.h src/modes/quadrature_mode.cpp
git commit -m "fix: add DC blocker to Quadrature AM ring modulation output

Low carrier frequencies produce DC offset in ring modulation.
DcBlocker on the primary ring signal removes it."
```

---

### Task 14: Remove Dead `swelling_` Member from AutoSwell

**Files:**
- Modify: `src/modes/auto_swell_mode.h:28`
- Modify: `src/modes/auto_swell_mode.cpp:27`

`swelling_` is declared and initialized in `Reset()` but never read.

- [ ] **Step 1: Remove from header**

Delete line 28 from `auto_swell_mode.h`:

```cpp
    bool  swelling_    = false;
```

- [ ] **Step 2: Remove from Reset**

In `auto_swell_mode.cpp:27`, delete:

```cpp
    swelling_    = false;
```

- [ ] **Step 3: Build firmware**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4`
Expected: Clean build.

- [ ] **Step 4: Commit**

```bash
git add src/modes/auto_swell_mode.h src/modes/auto_swell_mode.cpp
git commit -m "refactor: remove unused swelling_ member from AutoSwellMode"
```

---

### Task 15: Fix CRC Padding and Save-on-Load Flash Wear

**Files:**
- Modify: `src/presets/preset_manager.cpp:47,127-131`

CRC hashes uninitialized padding bytes. `LoadSlot` saves on every call even when active slot is unchanged.

- [ ] **Step 1: Zero-initialize CRC copy**

Replace line 47:

```cpp
    PresetStorageState copy = state;
```

with:

```cpp
    PresetStorageState copy{};
    std::memcpy(&copy, &state, sizeof(PresetStorageState));
```

Wait — `{}` zero-initializes, then `memcpy` copies the full struct including padding. The padding in `copy` gets overwritten by `memcpy` with `state`'s padding (also potentially uninitialized). The real fix is to zero-init `copy` and then copy fields individually. But that's fragile if fields change. Better approach: zero the whole thing, then copy:

```cpp
    PresetStorageState copy;
    std::memset(&copy, 0, sizeof(copy));
    copy = state;
    copy.crc = 0;
```

This ensures the padding in `copy` is zero (since `memset` zeros everything, then the field-by-field copy from `state` only writes the actual fields, and padding remains zero). Actually no — `copy = state` copies the full object representation including padding on most compilers. The only truly safe approach is to hash individual fields. But for a pragmatic fix that works on ARM GCC (which does use simple memcpy for struct assignment):

```cpp
    PresetStorageState copy;
    std::memset(&copy, 0, sizeof(copy));
    // Copy fields individually to avoid copying uninitialized padding
    copy.magic       = state.magic;
    copy.version     = state.version;
    copy.active_slot = state.active_slot;
    copy.crc         = 0;
    for (int i = 0; i < PRESET_SLOT_COUNT; ++i) {
        copy.slots[i] = state.slots[i];
    }
```

- [ ] **Step 2: Only save on LoadSlot when active slot actually changes**

Replace lines 127-131:

```cpp
    active_slot_    = s;
    st.active_slot  = static_cast<uint16_t>(s);
    st.crc          = ComputeCrc(st);
    storage().Save();
    return true;
```

with:

```cpp
    const bool slot_changed = (active_slot_ != s);
    active_slot_ = s;
    if (slot_changed) {
        st.active_slot  = static_cast<uint16_t>(s);
        st.crc          = ComputeCrc(st);
        storage().Save();
    }
    return true;
```

- [ ] **Step 3: Build firmware**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4`
Expected: Clean build.

- [ ] **Step 4: Commit**

```bash
git add src/presets/preset_manager.cpp
git commit -m "fix: zero-init CRC copy to avoid padding-dependent checksums

Also only save to flash on LoadSlot when active_slot actually changes,
reducing QSPI flash wear from rapid preset cycling."
```

---

### Task 16: PutUInt Buffer Overflow Fix

**Files:**
- Modify: `src/display/display_manager.cpp:19`

`char tmp[6]` is too small for full `uint32_t` range (up to 10 digits).

- [ ] **Step 1: Increase buffer size**

Replace line 19:

```cpp
    char tmp[6];
```

with:

```cpp
    char tmp[10];
```

- [ ] **Step 2: Build firmware**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4`
Expected: Clean build.

- [ ] **Step 3: Commit**

```bash
git add src/display/display_manager.cpp
git commit -m "fix: increase PutUInt buffer from 6 to 10 chars for full uint32_t range"
```

---

### Task 17: Rename Stale Delay-Pedal Pot Names

**Files:**
- Modify: `src/config/pin_map.h:7-13`

Pot names still reference the delay pedal (`POT_TIME`, `POT_REPEATS`, etc.).

- [ ] **Step 1: Rename pot constants**

Replace lines 7-13:

```cpp
constexpr int POT_TIME    = 0;
constexpr int POT_REPEATS = 1;
constexpr int POT_MIX     = 2;
constexpr int POT_FILTER  = 3;
constexpr int POT_GRIT    = 4;
constexpr int POT_MOD_SPD = 5;
constexpr int POT_MOD_DEP = 6;
```

with:

```cpp
constexpr int POT_SPEED = 0;
constexpr int POT_DEPTH = 1;
constexpr int POT_MIX   = 2;
constexpr int POT_TONE  = 3;
constexpr int POT_P1    = 4;
constexpr int POT_P2    = 5;
constexpr int POT_LEVEL = 6;
```

- [ ] **Step 2: Update all references to old names**

Search for `POT_TIME`, `POT_REPEATS`, `POT_FILTER`, `POT_GRIT`, `POT_MOD_SPD`, `POT_MOD_DEP` in `src/` and update them to the new names. Likely locations: `src/hardware/controls.cpp`.

- [ ] **Step 3: Build firmware**

Run: `cd /Users/bbalazs/daisy/modulation && make -j4`
Expected: Clean build. Any missed references will cause compile errors — fix them.

- [ ] **Step 4: Commit**

```bash
git add src/config/pin_map.h src/hardware/controls.cpp
git commit -m "refactor: rename stale delay-pedal pot names to modulation parameter names

POT_TIME→POT_SPEED, POT_REPEATS→POT_DEPTH, POT_FILTER→POT_TONE,
POT_GRIT→POT_P1, POT_MOD_SPD→POT_P2, POT_MOD_DEP→POT_LEVEL."
```

---

## Deferred Issues (Not in This Plan)

These were identified in the review but are not critical enough to fix now, or require larger architectural changes:

| Issue | Reason Deferred |
|-------|----------------|
| `tanf()` in SVF (flash cost) | Requires writing a fast_tan approximation — separate task |
| DelayLine off-by-one | Needs careful verification with all callers; functionally masked by minimum delays |
| FZ bit (denormal protection) | Needs firmware startup code audit; VST already handles via `ScopedNoDenormals` |
| Soft bypass/mode-switch crossfade | UX improvement, not a correctness bug |
| `__builtin_sqrtf` portability | Only matters if MSVC build is ever attempted |
| FilterMode per-sample `tanf()` in LFO path | Performance concern; needs fast_tan or reduced-rate update strategy |
| VST `UndoManager` | Nice-to-have feature, not a bug |
| Hardcoded `12` vs `NUM_MODES` in VST | Low risk; partially addressed by Task 1 |

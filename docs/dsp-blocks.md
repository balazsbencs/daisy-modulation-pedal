# DSP Building Blocks

## Existing Blocks (Modified)

### LFO — `src/dsp/lfo.h`

Extended from 2 waveforms (Sine, Triangle) to 7:

```cpp
enum class LfoWave : uint8_t {
    Sine,           // Smooth sinusoidal
    Triangle,       // Linear ramp up/down
    Square,         // Hard on/off toggle
    RampUp,         // Sawtooth rising (0→1 then reset)
    RampDown,       // Sawtooth falling (1→0 then reset)
    SampleAndHold,  // Random value held per cycle
    Exponential,    // Exponential curve (fast attack, slow decay)
};
```

**Interface:**
```cpp
class Lfo {
public:
    void Init(float rate_hz = 1.0f, LfoWave wave = LfoWave::Sine);
    void SetRate(float rate_hz);
    void SetWave(LfoWave wave);
    float Process();        // Per-sample, returns −1..+1
    float PrepareBlock();   // Per-block, returns −1..+1
    void Reset();           // Reset phase to 0
    float GetPhase() const; // Current phase 0..1 (for sync)
};
```

**Used by:** All modes except Destroyer and AutoSwell.

**Notes:**
- `SampleAndHold` uses a simple LFSR for pseudo-random values
- `Exponential` waveshape: `exp(-phase * k) * 2 - 1` where k controls steepness
- `GetPhase()` added for multi-LFO sync (Rotary horn/drum, Multi chorus taps)

### ToneFilter — `src/dsp/tone_filter.h`

Unchanged. One-knob LP/HP filter: 0.0 = full LP, 0.5 = flat, 1.0 = full HP.

**Used by:** All modes (post-effect tone shaping).

### EnvelopeFollower — `src/dsp/envelope_follower.h`

Unchanged. Attack/release envelope detector.

**Interface:**
```cpp
class EnvelopeFollower {
public:
    void Init(float attack_ms, float release_ms);
    void SetAttack(float attack_ms);
    void SetRelease(float release_ms);
    float Process(float input);  // Returns envelope 0..1
};
```

**Used by:** Filter (Env± waveshapes), AutoSwell (trigger detection).

### Saturation — `src/dsp/saturation.h`

Unchanged. Inline soft-clip: `tanh`-style or polynomial approximation.

**Used by:** Rotary (drive stage), Destroyer (optional).

### DCBlocker — `src/dsp/dc_blocker.h`

Unchanged. One-pole DC blocking filter.

**Used by:** Phaser (after feedback loop), Quadrature (after frequency shifting).

### DelayLineSDRAM — `src/dsp/delay_line_sdram.h`

Unchanged interface, but buffers are much shorter for modulation:

**Interface:**
```cpp
template <size_t MaxSamples>
class DelayLineSDRAM {
public:
    void Init();
    void Write(float sample);
    float Read(float delay_samples) const;  // Linear interpolation
    float ReadNearest(size_t delay_samples) const;
};
```

**Used by:** Chorus (1–20 ms lines), Flanger (0.1–10 ms lines), Rotary (Doppler), Vibe, AutoSwell (optional chorus), Quadrature (FM sub-mode).

---

## New Blocks

### AllpassFilter — `src/dsp/allpass_filter.h`

First-order allpass filter for phase-shifting applications. The building block for Phaser and Vibe modes.

**Interface:**
```cpp
class AllpassFilter {
public:
    void Init();
    void Reset();
    void SetCoefficient(float coeff);  // −1..+1, sets center frequency
    float Process(float input);        // Returns phase-shifted output
};
```

**Algorithm:**
```
y[n] = coeff * x[n] + x[n-1] - coeff * y[n-1]
```

Where `coeff = (tan(π * fc / fs) - 1) / (tan(π * fc / fs) + 1)` for center frequency `fc`.

**Used by:**
- **Phaser** — 2 to 16 cascaded stages
- **Vibe** — 4 stages with nonlinear LDR-emulated coefficient modulation
- **HilbertTransform** — internal implementation uses allpass pairs

**Notes:**
- Stateless beyond one sample of delay (`x[n-1]`, `y[n-1]`)
- Coefficient can be modulated per-sample for smooth sweeps
- Maximum 16 instances needed (Phaser 16-stage sub-mode)

### SVF (State-Variable Filter) — `src/dsp/svf.h`

Multi-mode resonant filter providing simultaneous LP, BP, HP, and Notch outputs.

**Interface:**
```cpp
enum class SvfMode : uint8_t { LowPass, BandPass, HighPass, Notch };

class Svf {
public:
    void Init();
    void Reset();
    void SetFreq(float freq_hz);   // Cutoff frequency
    void SetQ(float q);            // Resonance (0.5–20.0; 0.707 = Butterworth)
    void SetMode(SvfMode mode);
    float Process(float input);    // Returns selected mode output

    // Access all outputs simultaneously (for Formant mode)
    float GetLP() const;
    float GetBP() const;
    float GetHP() const;
    float GetNotch() const;
};
```

**Algorithm:** Chamberlin SVF (topology-preserving transform for stability at high frequencies):
```
hp = (input - 2*q*bp - lp) / (1 + 2*q*f + f*f)
bp = bp + f * hp
lp = lp + f * bp
notch = hp + lp
```

Where `f = 2 * sin(π * fc / fs)` and `q = 1 / (2 * Q)`.

**Used by:**
- **Filter** — main resonant filter (LP/BP/HP selectable)
- **Formant** — two parallel SVFs in bandpass mode for F1/F2
- **Destroyer** — post-destruction filter shaping

**Notes:**
- Use the TPT (topology-preserving transform) variant for stability when modulating cutoff
- Maximum 4 SVF instances needed simultaneously (2× Formant + headroom)
- Per-sample coefficient update is safe (no zipper noise with TPT)

### BBDEmulator — `src/dsp/bbd_emulator.h`

Bucket-brigade device emulation for authentic analog chorus tones. Models clock noise, HF rolloff, and companding artifacts.

**Interface:**
```cpp
class BBDEmulator {
public:
    void Init(size_t num_stages = 512);  // 256–4096 BBD stages
    void Reset();
    void SetClockRate(float rate_hz);    // BBD clock frequency
    void SetToneRolloff(float amount);   // HF loss per stage 0..1
    float Process(float input);          // Returns BBD-processed output
};
```

**Algorithm:**
- Input → compressor → delay chain → expander → output
- Delay chain: circular buffer sampled at `clock_rate` (not audio rate)
- Clock noise: low-level noise injection proportional to `1/clock_rate`
- HF rolloff: one-pole LP per effective stage group (approximated, not per-stage)
- Companding: `sqrt()` compress on input, square expand on output (approximating µ-law)

**Used by:**
- **Chorus** (dBucket sub-mode) — primary chorus sound

**Notes:**
- SDRAM buffer for the internal delay (~8 KB for 4096 stages × 2 bytes)
- Clock rate modulated by LFO to create chorus effect
- `num_stages` controls delay time range (more stages = longer delay at same clock rate)
- HF rolloff accumulates naturally — no need for explicit filter at output

### HilbertTransform — `src/dsp/hilbert_transform.h`

90° phase-shift network for frequency shifting and single-sideband modulation. Produces in-phase (I) and quadrature (Q) outputs.

**Interface:**
```cpp
class HilbertTransform {
public:
    void Init();
    void Reset();
    // Process one sample, returns I and Q (90° apart)
    void Process(float input, float& out_i, float& out_q);
};
```

**Algorithm:** Approximated using two parallel allpass chains with carefully chosen coefficients:

```
Chain A (I path): 4 cascaded first-order allpass filters
Chain B (Q path): 4 cascaded first-order allpass filters

Coefficients chosen so that:
  phase(A) - phase(B) ≈ 90° across 20 Hz – 20 kHz
```

Allpass coefficients (Hilbert pair, optimized for 48 kHz):
- Chain A: [0.4021921162, 0.8561710882, 0.9722909545, 0.9952884791]
- Chain B: [0.2024577582, 0.6890814947, 0.9360654323, 0.9882295227]

**Used by:**
- **Quadrature** (FreqShift+ and FreqShift− sub-modes)

**Frequency shifting usage:**
```
// Upper sideband (FreqShift+):
output = I * cos(2π * shift_hz * t) - Q * sin(2π * shift_hz * t)

// Lower sideband (FreqShift−):
output = I * cos(2π * shift_hz * t) + Q * sin(2π * shift_hz * t)
```

**Notes:**
- 8 allpass stages total (4 per chain) — lightweight on CPU
- Phase accuracy degrades below ~20 Hz (acceptable for guitar)
- One sample of group delay between I and Q paths — compensate with a single-sample delay on I
- Uses `AllpassFilter` internally (8 instances)
- SDRAM usage: ~4 KB for allpass state buffers

### PatternSequencer — `src/dsp/pattern_sequencer.h`

8-step amplitude pattern generator for rhythmic tremolo effects.

**Interface:**
```cpp
class PatternSequencer {
public:
    void Init();
    void Reset();
    void SetPattern(uint32_t pattern);  // 8 steps × 4 bits = 32 bits
    void SetRate(float rate_hz);        // Step rate
    void SetSubdivision(int sub);       // 0=straight, 1=triplet, 2=dotted
    float Process();                    // Returns amplitude 0..1 per sample
    void SyncToTap(float beat_period);  // Sync to tap tempo
};
```

**Pattern encoding:** Each `uint32_t` encodes 8 steps of 4-bit amplitude (0–15):
```
Bits [3:0]   = Step 0 amplitude (0–15 → 0.0–1.0)
Bits [7:4]   = Step 1 amplitude
...
Bits [31:28] = Step 7 amplitude
```

**Built-in patterns (16 presets):**

| # | Name | Pattern | Character |
|---|------|---------|-----------|
| 0 | Full On | `FFFFFFFF` | No effect (bypass) |
| 1 | Straight 8ths | `F0F0F0F0` | Basic on/off |
| 2 | Dotted 8ths | `FFF000F0` | Dotted rhythm |
| 3 | Gallop | `F0F0F000` | Triplet gallop |
| 4 | Tresillo | `F00F00F0` | 3-3-2 pattern |
| 5 | Habanera | `F0F00F0F` | Cuban rhythm |
| 6 | Samba | `F00FF0F0` | Brazilian pulse |
| 7 | Ramp Up | `13579BDF` | Gradual swell |
| 8 | Ramp Down | `FDB97531` | Gradual fade |
| 9 | Stutter | `F000F000` | Quarter note stutter |
| 10 | Syncopation | `0F0FF0F0` | Off-beat emphasis |
| 11 | Waltz | `F00000F0` | 3/4 feel |
| 12 | Pendulum | `F8420248` | Swing back and forth |
| 13 | Heartbeat | `F0F00000` | Double pulse |
| 14 | Morse | `F0FFF0F0` | Short-long pattern |
| 15 | Random | (LFSR) | Pseudo-random per cycle |

**Used by:**
- **Pattern Trem** — primary DSP block

**Notes:**
- Step transitions use 2 ms cosine crossfade to avoid clicks
- Subdivision scales step duration: straight = 1×, triplet = 2/3×, dotted = 3/2×
- `SyncToTap()` overrides `SetRate()` when tap tempo is active
- Pattern #15 (Random) generates a new random pattern each cycle via LFSR

---

## Block Usage Matrix

| Block | Chorus | Flanger | Rotary | Vibe | Phaser | Filter | Formant | V.Trem | P.Trem | AutoSwell | Destroyer | Quadrature |
|-------|--------|---------|--------|------|--------|--------|---------|--------|--------|-----------|-----------|------------|
| LFO | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | | | | ✓ |
| ToneFilter | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| EnvFollower | | | | | | ✓ | | | | ✓ | | |
| Saturation | | | ✓ | | | | | | | | ✓ | |
| DCBlocker | | | | | ✓ | | | | | | | ✓ |
| DelayLineSDRAM | ✓ | ✓ | ✓ | | | | | | | ✓ | | ✓ |
| AllpassFilter | | | | ✓ | ✓ | | | | | | | |
| SVF | | | | | | ✓ | ✓ | | | | ✓ | |
| BBDEmulator | ✓ | | | | | | | | | | | |
| HilbertTransform | | | | | | | | | | | | ✓ |
| PatternSequencer | | | | | | | | | ✓ | | | |

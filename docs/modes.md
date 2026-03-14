# Modulation Modes

All 12 modes inherit from `ModMode` and implement `Init()`, `Reset()`, `Prepare()`, and `Process()`. Every mode outputs **wet signal only** — the AudioEngine applies dry/wet mixing via constant-power crossfade.

Sub-modes are selected via the shift-layer encoder (P2 or dedicated sub-mode slot). Each mode section below documents: description, DSP algorithm, sub-modes, parameter mapping, and implementation notes.

---

## 0. Chorus

**Description:** Pitch-modulated delay creates thickening, doubling, and detuning effects. Five sub-modes cover the full range from vintage analog to modern digital chorus.

**DSP Algorithm:**
- Short modulated delay line (1–20 ms range)
- LFO modulates delay time → pitch wobble
- Multi-voice variants use multiple delay taps with phase-offset LFOs

**Sub-modes:**

| Sub-mode | Algorithm | Character |
|----------|-----------|-----------|
| dBucket  | BBDEmulator — clock-noise + HF rolloff + companding artifacts | Vintage CE-1/CE-2 analog chorus |
| Multi    | 3 delay taps, 120° LFO phase offset | Rich, wide tri-chorus |
| Vibrato  | Single delay, 100% wet, no dry blend | Pure pitch vibrato |
| Detune   | 2 fixed-offset pitch shifts (±cents), no LFO | Static detune / doubler |
| Digital  | Clean interpolated delay, no coloration | Pristine modern chorus |

**Parameter Mapping:**

| Param | Role | Range |
|-------|------|-------|
| Speed | LFO rate | 0.05 – 5.0 Hz |
| Depth | Delay time modulation depth | 0.0 – 1.0 |
| Mix   | Wet/dry | 0.0 – 1.0 |
| Tone  | Post-chorus LP/HP | 0.0 – 1.0 |
| P1    | Delay base time (ms) | 1.0 – 20.0 ms |
| P2    | Sub-mode select | 0–4 (dBucket/Multi/Vibrato/Detune/Digital) |
| Level | Output gain | 0.0 – 1.0 |

**Implementation Notes:**
- dBucket sub-mode uses `BBDEmulator` DSP block; others use `DelayLineSDRAM` with interpolated reads
- Multi sub-mode requires 3 delay taps — use a single shared delay line with 3 read heads
- Vibrato forces mix to 1.0 internally (ignores user mix setting)
- Detune sub-mode ignores Speed (no LFO); P1 controls detune amount in cents

---

## 1. Flanger

**Description:** Very short modulated delay (0.1–10 ms) mixed with dry signal creates comb-filter sweeps. Feedback creates resonant peaks. Through-zero flanging crosses zero delay for jet-engine sounds.

**DSP Algorithm:**
- Short delay line with LFO-modulated read position
- Feedback path from output back to delay input
- Through-zero: delay sweeps through zero → inverted signal cancellation
- Positive/negative feedback yields different harmonic emphasis

**Sub-modes:**

| Sub-mode | Feedback | Through-Zero | Character |
|----------|----------|--------------|-----------|
| Silver   | Positive, moderate | No | Classic subtle flanger |
| Grey     | Negative, moderate | No | Hollow, nasal tone |
| Black+   | Positive, high | No | Intense metallic resonance |
| Black−   | Negative, high | No | Deep hollow resonance |
| Zero+    | Positive | Yes | Jet-engine sweep |
| Zero−    | Negative | Yes | Inverted jet sweep |

**Parameter Mapping:**

| Param | Role | Range |
|-------|------|-------|
| Speed | LFO rate | 0.01 – 5.0 Hz |
| Depth | Sweep range | 0.0 – 1.0 |
| Mix   | Wet/dry | 0.0 – 1.0 |
| Tone  | Post-flanger LP/HP | 0.0 – 1.0 |
| P1    | Feedback amount | −0.95 – +0.95 (sign set by sub-mode) |
| P2    | Sub-mode select | 0–5 |
| Level | Output gain | 0.0 – 1.0 |

**Implementation Notes:**
- Through-zero sub-modes require a second (dry) delay line to allow the wet path to cross zero offset
- Feedback must be clamped to prevent self-oscillation (max ±0.95)
- Very short delay lines: ~480 samples (10 ms) maximum — fits easily in SDRAM
- Sub-mode sets default feedback sign; P1 adjusts magnitude

---

## 2. Rotary

**Description:** Leslie rotating speaker simulation. Two-band split: horn (HF) rotates fast, drum (LF) rotates slow. Includes overdrive characteristic of a cranked Leslie amp.

**DSP Algorithm:**
- Crossover filter splits signal into LF (drum) and HF (horn)
- Each band: LFO → amplitude modulation + Doppler pitch shift (short modulated delay)
- Horn and drum have independent speed ramps (slow ↔ fast with acceleration curves)
- Optional drive stage before the rotor simulation

**Parameter Mapping:**

| Param | Role | Range |
|-------|------|-------|
| Speed | Rotor speed (slow/fast ramp target) | 0.4 – 7.0 Hz (horn); drum = speed × 0.56 |
| Depth | Doppler depth / amplitude modulation depth | 0.0 – 1.0 |
| Mix   | Wet/dry | 0.0 – 1.0 |
| Tone  | Crossover frequency bias | 0.0 – 1.0 |
| P1    | Drive amount | 0.0 – 1.0 |
| P2    | Horn/drum balance | 0.0 – 1.0 (0 = all drum, 1 = all horn) |
| Level | Output gain | 0.0 – 1.0 |

**Implementation Notes:**
- Horn and drum use independent LFOs with different rates (horn ≈ 1.8× drum)
- Speed ramp: when Speed changes, LFO rate smoothly accelerates/decelerates (emulates motor inertia)
- Use `Saturation` block for drive stage
- True stereo output: L/R from 180° phase offset on each rotor

---

## 3. Vibe

**Description:** 1960s UniVibe emulation — a phase-shift effect with pronounced amplitude modulation from mismatched photoresistor/LDR stages. Warmer and more "throaty" than a standard phaser.

**DSP Algorithm:**
- 4 cascaded allpass stages (like a phaser) with non-uniform frequency spacing
- LDR emulation: LFO drives a nonlinear response curve (asymmetric, not pure sine)
- Amplitude modulation component: LFO also modulates signal level (unlike pure phaser)
- Wet signal mixed back with dry

**Parameter Mapping:**

| Param | Role | Range |
|-------|------|-------|
| Speed | LFO rate | 0.5 – 10.0 Hz |
| Depth | Sweep depth + AM depth | 0.0 – 1.0 |
| Mix   | Wet/dry | 0.0 – 1.0 |
| Tone  | Post-vibe LP/HP | 0.0 – 1.0 |
| P1    | Intensity (regen / feedback) | 0.0 – 0.8 |
| P2    | Chorus/Vibrato mode blend | 0.0 – 1.0 (0 = chorus, 1 = vibrato) |
| Level | Output gain | 0.0 – 1.0 |

**Implementation Notes:**
- Uses `AllpassFilter` block (4 stages)
- LDR nonlinearity: apply asymmetric curve to LFO output before driving allpass frequencies
- Amplitude modulation is what distinguishes Vibe from Phaser — keep both phase + AM components
- P2 at 1.0 (vibrato) sets mix to 100% wet internally

---

## 4. Phaser

**Description:** Cascaded allpass filters create frequency-dependent phase shifts. When mixed with dry signal, the moving notches create the characteristic phaser sweep. Variable stage count controls density.

**DSP Algorithm:**
- N cascaded first-order allpass filters (N = 2, 4, 6, 8, 12, or 16)
- LFO sweeps the allpass center frequencies in unison
- Feedback (regeneration) from output back to input for sharper notches
- Barber Pole: continuous upward or downward sweep using quadrature LFO trick

**Sub-modes:**

| Sub-mode | Stages | Character |
|----------|--------|-----------|
| 2-stage  | 2      | Subtle, 1 notch |
| 4-stage  | 4      | Classic, 2 notches |
| 6-stage  | 6      | Rich, 3 notches |
| 8-stage  | 8      | Dense, 4 notches |
| 12-stage | 12     | Lush, 6 notches |
| 16-stage | 16     | Ultra-dense, 8 notches |
| Barber Pole | 4+4 | Infinitely rising/falling illusion |

**Parameter Mapping:**

| Param | Role | Range |
|-------|------|-------|
| Speed | LFO rate | 0.02 – 8.0 Hz |
| Depth | Sweep range (allpass freq min/max) | 0.0 – 1.0 |
| Mix   | Wet/dry | 0.0 – 1.0 |
| Tone  | Post-phaser LP/HP | 0.0 – 1.0 |
| P1    | Regeneration (feedback) | 0.0 – 0.95 |
| P2    | Sub-mode select (stage count) | 0–6 |
| Level | Output gain | 0.0 – 1.0 |

**Implementation Notes:**
- Uses `AllpassFilter` block — maximum 16 stages needed
- Barber Pole uses two 4-stage chains with quadrature (90° offset) LFOs, crossfaded
- Allpass frequency sweep range: ~100 Hz to ~4 kHz (mapped from Depth)
- Feedback clamped at 0.95 to prevent self-oscillation
- Stage count change triggers `Reset()` to avoid artifacts

---

## 5. Filter

**Description:** Resonant filter sweep driven by LFO or envelope follower. Classic auto-wah, synth-like filter sweeps, and DJ-style filtering.

**DSP Algorithm:**
- State-variable filter (SVF) provides simultaneous LP/BP/HP/Notch outputs
- LFO or envelope follower modulates cutoff frequency
- Resonance (Q) creates peaked response at cutoff
- 8 modulation waveshapes including envelope follower (auto-wah)

**Sub-modes (filter type):**

| Sub-mode | Filter Output | Character |
|----------|--------------|-----------|
| LP       | Low-pass     | Warm, muffled sweep |
| Wah      | Bandpass     | Classic wah-wah |
| HP       | High-pass    | Thin, bright sweep |

**LFO Waveshapes (selected via P2):**

| Shape | Description |
|-------|-------------|
| Sine | Smooth sweep |
| Triangle | Linear sweep |
| Square | Abrupt toggle |
| RampUp | Sawtooth rising |
| RampDown | Sawtooth falling |
| S&H | Random stepped |
| Env+ | Envelope follower (pick attack opens filter) |
| Env− | Inverse envelope (pick attack closes filter) |

**Parameter Mapping:**

| Param | Role | Range |
|-------|------|-------|
| Speed | LFO rate (ignored in Env± modes) | 0.02 – 10.0 Hz |
| Depth | Sweep range | 0.0 – 1.0 |
| Mix   | Wet/dry | 0.0 – 1.0 |
| Tone  | Base cutoff frequency | 0.0 – 1.0 (maps to 80 Hz – 12 kHz) |
| P1    | Resonance (Q) | 0.5 – 20.0 |
| P2    | LFO waveshape / envelope select | 0–7 |
| Level | Output gain | 0.0 – 1.0 |

**Implementation Notes:**
- Uses `SVF` DSP block for the main filter
- Uses `EnvelopeFollower` block for Env± waveshapes
- When using Env±, Speed param is repurposed as envelope sensitivity
- Sub-mode (LP/Wah/HP) is selected via shift-layer encoder (separate from P2 waveshape)
- Resonance at high Q with high Depth can produce loud peaks — apply soft limiting after filter

---

## 6. Formant

**Description:** Vowel-shaping filter that morphs between vocal formants. Two bandpass SVFs track the first two formant frequencies (F1, F2) of each vowel, creating talking/singing guitar tones.

**DSP Algorithm:**
- Two parallel SVF bandpass filters tuned to F1 and F2 frequencies
- LFO morphs between adjacent vowel formant pairs
- Smooth interpolation of F1/F2 frequencies and bandwidths between vowels
- Output is sum of both bandpass channels

**Sub-modes (vowel targets):**

| Sub-mode | Vowel | F1 (Hz) | F2 (Hz) |
|----------|-------|---------|---------|
| AA       | "ah"  | 730     | 1090    |
| EE       | "ee"  | 270     | 2290    |
| EYE      | "eye" | 400     | 2050    |
| OH       | "oh"  | 570     | 840     |
| OOH      | "ooh" | 300     | 870     |

**Parameter Mapping:**

| Param | Role | Range |
|-------|------|-------|
| Speed | Morph rate (LFO between vowel pairs) | 0.05 – 5.0 Hz |
| Depth | Morph range (how far between vowels) | 0.0 – 1.0 |
| Mix   | Wet/dry | 0.0 – 1.0 |
| Tone  | Formant shift (±semitones offset to F1/F2) | 0.0 – 1.0 |
| P1    | Bandwidth / resonance | 0.0 – 1.0 (narrow → wide) |
| P2    | Vowel pair select | 0–4 (AA/EE/EYE/OH/OOH) |
| Level | Output gain | 0.0 – 1.0 |

**Implementation Notes:**
- Uses two `SVF` blocks in bandpass mode
- Formant frequencies stored as compile-time tables (5 vowels × 2 formants)
- LFO morphs between the selected vowel (P2) and the next in sequence (wraps around)
- Tone shifts both F1 and F2 up/down together (±1 octave range)
- Bandwidth (P1) controls SVF Q: low P1 = high Q (narrow, more vocal); high P1 = low Q (wide, subtle)

---

## 7. Vintage Trem

**Description:** Classic tremolo — amplitude modulation of the signal. Three sub-modes emulate different hardware tremolo circuits.

**DSP Algorithm:**
- LFO modulates signal amplitude
- Sub-mode determines the LFO waveshape and modulation character:
  - Tube: soft, asymmetric bias-vary tremolo
  - Harmonic: separate LFOs for even/odd harmonics (Fender brown-panel style)
  - Photoresistor: opto-coupler with nonlinear, slightly lagging response

**Sub-modes:**

| Sub-mode | Waveshape | Character |
|----------|-----------|-----------|
| Tube | Asymmetric sine (bias shift) | Warm, amp-like throb |
| Harmonic | Dual sine (fundamental + harmonic) | Complex pulsing |
| Photoresistor | Filtered triangle (LDR lag) | Smooth, organic swell |

**Parameter Mapping:**

| Param | Role | Range |
|-------|------|-------|
| Speed | Tremolo rate | 1.0 – 15.0 Hz |
| Depth | Modulation depth (volume dip amount) | 0.0 – 1.0 |
| Mix   | Wet/dry | 0.0 – 1.0 |
| Tone  | Post-trem LP/HP | 0.0 – 1.0 |
| P1    | Waveform shape / symmetry | 0.0 – 1.0 |
| P2    | Sub-mode select | 0–2 (Tube/Harmonic/Photoresistor) |
| Level | Output gain | 0.0 – 1.0 |

**Implementation Notes:**
- Uses extended `LFO` block with appropriate waveshape per sub-mode
- Tube: apply soft-clip curve to LFO output before modulating amplitude
- Harmonic: sum of two LFOs at f and 2f with adjustable ratio (P1 controls harmonic balance)
- Photoresistor: one-pole LP filter on LFO output to simulate LDR lag (P1 controls lag amount)
- Output is `input * (1.0 - depth * lfo_output)` — ensure lfo_output is in [0, 1] range

---

## 8. Pattern Trem

**Description:** Programmable rhythmic tremolo. An 8-beat pattern of 16 amplitude subdivisions creates stutter, gate, and rhythmic effects synced to tap tempo.

**DSP Algorithm:**
- `PatternSequencer` steps through an 8-step amplitude pattern
- Each step has a 4-bit amplitude level (0–15 → 0.0–1.0)
- Sequencer clock derived from tap tempo or Speed knob
- 16 subdivisions per beat (straight, triplet, or dotted)
- Smooth transitions between steps via short crossfade

**Parameter Mapping:**

| Param | Role | Range |
|-------|------|-------|
| Speed | Pattern rate / BPM (overridden by tap tempo) | 1.0 – 15.0 Hz |
| Depth | Pattern depth (0 = no effect, 1 = full gating) | 0.0 – 1.0 |
| Mix   | Wet/dry | 0.0 – 1.0 |
| Tone  | Post-trem LP/HP | 0.0 – 1.0 |
| P1    | Pattern select (preset patterns) | 0–15 (16 built-in patterns) |
| P2    | Subdivision (straight/triplet/dotted) | 0–2 |
| Level | Output gain | 0.0 – 1.0 |

**Implementation Notes:**
- Uses `PatternSequencer` DSP block
- 16 built-in patterns stored as `uint32_t` each (8 steps × 4 bits)
- Tap tempo integration: period from `TempoSync` sets the pattern beat length
- Crossfade between adjacent steps (~2 ms) to avoid clicks
- Patterns include: straight 8ths, gallop, tresillo, habanera, samba, random, etc.

---

## 9. AutoSwell

**Description:** Envelope-triggered volume swell that removes pick attack and creates pad-like, bowed sounds. Optional subtle chorus adds width.

**DSP Algorithm:**
- Envelope follower detects input level
- When input exceeds threshold, trigger a slow volume ramp-up
- Attack time controls how quickly volume reaches full
- Release controls how quickly volume fades after input stops
- Optional chorus (single delay tap with LFO) on the output

**Parameter Mapping:**

| Param | Role | Range |
|-------|------|-------|
| Speed | Attack time | 10 – 500 ms (log) |
| Depth | Swell intensity (threshold sensitivity) | 0.0 – 1.0 |
| Mix   | Wet/dry | 0.0 – 1.0 |
| Tone  | Post-swell LP/HP | 0.0 – 1.0 |
| P1    | Release time | 50 – 2000 ms (log) |
| P2    | Chorus amount (0 = off, 1 = full) | 0.0 – 1.0 |
| Level | Output gain | 0.0 – 1.0 |

**Implementation Notes:**
- Uses `EnvelopeFollower` block for input detection
- Volume envelope: one-pole smoothing with separate attack/release coefficients
- Chorus uses a single short delay line (~5 ms) with subtle LFO modulation
- When P2 = 0, chorus is bypassed entirely (saves CPU)
- Swell output is `input * envelope_gain` where envelope_gain ramps 0→1 on trigger

---

## 10. Destroyer

**Description:** Lo-fi destruction — bitcrushing, sample rate reduction, filter mangling, and vinyl noise. Creative distortion/degradation effect.

**DSP Algorithm:**
- Bit depth reduction: quantize sample to N bits (1–16)
- Sample rate decimation: hold-and-sample at reduced rate
- Resonant filter (SVF): LP/BP/HP shapes the crushed signal
- Optional vinyl noise: filtered noise floor added to output

**Parameter Mapping:**

| Param | Role | Range |
|-------|------|-------|
| Speed | Decimation rate (sample rate divisor) | 1× – 48× (maps to 48 kHz → 1 kHz) |
| Depth | Bit depth (1–16 bits) | 0.0 – 1.0 (maps to 16 → 1 bits) |
| Mix   | Wet/dry | 0.0 – 1.0 |
| Tone  | Destruction filter cutoff | 0.0 – 1.0 |
| P1    | Filter resonance | 0.0 – 1.0 |
| P2    | Vinyl noise amount | 0.0 – 1.0 |
| Level | Output gain | 0.0 – 1.0 |

**Implementation Notes:**
- Bit reduction: `floor(sample * levels) / levels` where `levels = 2^bits`
- Decimation: hold current sample for N audio samples before reading next
- Uses `SVF` block for post-destruction filter
- Vinyl noise: low-pass filtered white noise from simple LFSR random generator
- No new DSP blocks needed beyond `SVF` — bitcrush and decimation are inline operations

---

## 11. Quadrature

**Description:** Ring modulation, frequency shifting, and AM synthesis using Hilbert transform for true single-sideband frequency shifting. From subtle detuning to metallic clangorous tones.

**DSP Algorithm:**
- Hilbert transform produces two outputs 90° apart (I and Q components)
- Carrier oscillator (also I/Q quadrature pair)
- AM: `input * (1 + depth * carrier)` — classic ring mod when depth = 1
- FM: carrier modulates a short delay time (vibrato-like but at audio rates)
- FreqShift: `I*cos(ωt) − Q*sin(ωt)` (upper sideband) or `I*cos(ωt) + Q*sin(ωt)` (lower)

**Sub-modes:**

| Sub-mode | Algorithm | Character |
|----------|-----------|-----------|
| AM | Amplitude modulation / ring mod | Metallic, clangorous |
| FM | Frequency modulation at audio rates | Bell-like, inharmonic |
| FreqShift+ | Upper sideband only (Hilbert) | Ascending barberpole |
| FreqShift− | Lower sideband only (Hilbert) | Descending barberpole |

**Parameter Mapping:**

| Param | Role | Range |
|-------|------|-------|
| Speed | Carrier frequency | 0.5 – 2000 Hz (log) |
| Depth | Modulation depth / mix | 0.0 – 1.0 |
| Mix   | Wet/dry | 0.0 – 1.0 |
| Tone  | Post-mod LP/HP | 0.0 – 1.0 |
| P1    | Carrier waveform (sine → square blend) | 0.0 – 1.0 |
| P2    | Sub-mode select | 0–3 (AM/FM/FreqShift+/FreqShift−) |
| Level | Output gain | 0.0 – 1.0 |

**Implementation Notes:**
- Uses `HilbertTransform` DSP block for FreqShift± sub-modes
- Hilbert implemented as pair of allpass chains (Hilbert approximation via 4th-order allpass network)
- AM sub-mode does not need Hilbert — simple multiplication
- FM sub-mode: carrier modulates delay time of a very short delay line (~1–2 ms)
- Speed range is much wider than other modes (up to 2 kHz for audio-rate effects)
- At low carrier frequencies (<20 Hz), AM becomes tremolo and FreqShift becomes subtle detuning

# Daisy Modulation Pedal — User Manual

## Overview

Daisy Modulation is a stereo modulation effects pedal with 12 modes covering chorus, flanger, rotary, vibe, phaser, filter, formant, tremolo, AutoSwell, bit destruction, and quadrature synthesis. Controls include 4 parameter encoders with a shift layer, a mode encoder, two footswitches, an OLED display, 8 preset slots, tap tempo, and full MIDI control via USB and TRS.

---

## Hardware Controls

### Encoders

There are five rotary encoders, each with a push button.

**Mode Encoder** (leftmost)
- Twist to cycle through the 12 modes
- Press and hold to enter the shift layer (access P1, P2, Level)

**Parameter Encoders 1–4**

| Layer | Encoder 1 | Encoder 2 | Encoder 3 | Encoder 4 |
|-------|-----------|-----------|-----------|-----------|
| Primary (no hold) | Speed | Depth | Mix | Tone |
| Shift (hold mode) | P1 | P2 | Level | — |

**Fast Turning**: Twisting quickly (within 40 ms of the previous detent) increases step size from 0.5% to 1.5% per detent.

---

### Footswitches

**Bypass Footswitch**
Toggles true bypass. The relay clicks audibly. The bypass LED is lit when the effect is active.

**Tap / Preset Footswitch**

| Mode encoder held? | Short press | Long press (≥700 ms) |
|--------------------|-------------|----------------------|
| No | Tap tempo | — |
| Yes | Load preset | Save preset |

---

### Display

The OLED (128×64) shows:
- Current mode name
- Parameter level bars
- Bypass status
- Tempo source: **Pot**, **Tap**, or **MIDI**
- Current BPM when tap or MIDI clock is active
- Preset slot (P1–P8) during preset selection
- Confirmation after saving or loading a preset

---

## Parameters

All modes share the same 7 parameters. Each parameter flows as a normalized value internally and is mapped to physical units for DSP.

| # | Name | Primary Encoder | Physical Range | Notes |
|---|------|-----------------|----------------|-------|
| 0 | **Speed** | Enc 1 | Mode-dependent | LFO rate, attack time, decimation rate, or carrier frequency |
| 1 | **Depth** | Enc 2 | 0–1 | Modulation intensity |
| 2 | **Mix** | Enc 3 | 0–1 | Constant-power wet/dry crossfade |
| 3 | **Tone** | Enc 4 | 0–1 | Post-effect filter: 0 = LP (dark), 0.5 = flat, 1 = HP (bright) |
| 4 | **P1** | Enc 1 (shift) | Mode-dependent | Mode-specific parameter 1 |
| 5 | **P2** | Enc 2 (shift) | Mode-dependent | Sub-mode select or mode-specific parameter 2 |
| 6 | **Level** | Enc 3 (shift) | 0–2× (0 to +6 dB) | Output gain; unity at 50% |

### Speed by Mode

Speed is repurposed in three modes:

| Mode | Speed controls | Range |
|------|---------------|-------|
| AutoSwell | Attack time | 10–500 ms |
| Destroyer | Decimation rate | 1× – 48× |
| Quadrature | Carrier frequency | 0.5–2000 Hz |
| All others | LFO rate | 0.05–10 Hz (mode-specific max) |

---

## Modes

Modes are selected by twisting the mode encoder. All modes output wet signal only — the Mix parameter controls wet/dry blend. Stereo output is produced by all modes.

---

### 0 — Chorus

Pitch-modulated delay that thickens and doubles the signal. Five sub-modes from vintage analog to modern digital.

| Sub-mode (P2) | Character |
|---------------|-----------|
| dBucket (0) | Vintage CE-1/CE-2 — BBD clock noise + HF rolloff |
| Multi (1) | Wide tri-chorus — 3 taps at 120° LFO offsets |
| Vibrato (2) | Pure pitch vibrato (100% wet internally) |
| Detune (3) | Static pitch detuning / doubler, no LFO |
| Digital (4) | Clean, transparent, no coloration |

| Parameter | Function | Range |
|-----------|----------|-------|
| Speed | LFO rate | 0.05–5.0 Hz |
| Depth | Delay modulation depth | 0–1 |
| Mix | Wet/dry | 0–1 |
| Tone | Post-chorus filter | LP ↔ HP |
| P1 | Base delay time | 1–20 ms |
| P2 | Sub-mode | 0–4 |
| Level | Output gain | 0–+6 dB |

---

### 1 — Flanger

Very short modulated delay (0.1–10 ms) mixed with dry signal creates sweeping comb-filter effects. Through-zero modes add jet-engine sweeps.

| Sub-mode (P2) | Feedback | Character |
|---------------|----------|-----------|
| Silver (0) | +, moderate | Classic subtle flanger |
| Grey (1) | −, moderate | Hollow, nasal |
| Black+ (2) | +, high | Intense metallic resonance |
| Black− (3) | −, high | Deep hollow resonance |
| Zero+ (4) | +, through-zero | Jet-engine sweep |
| Zero− (5) | −, through-zero | Inverted jet sweep |

| Parameter | Function | Range |
|-----------|----------|-------|
| Speed | LFO rate | 0.01–5.0 Hz |
| Depth | Sweep range | 0–1 |
| Mix | Wet/dry | 0–1 |
| Tone | Post-flanger filter | LP ↔ HP |
| P1 | Feedback amount | 0–0.95 (sign from sub-mode) |
| P2 | Sub-mode | 0–5 |
| Level | Output gain | 0–+6 dB |

---

### 2 — Rotary

Leslie rotating speaker simulation. Horn (HF) rotates faster than drum (LF). Doppler pitch shift and amplitude modulation combine for a rich, organic sound.

| Parameter | Function | Range |
|-----------|----------|-------|
| Speed | Rotor speed (ramp target) | 0.4–7.0 Hz (horn); drum ≈ speed × 0.56 |
| Depth | Doppler + AM depth | 0–1 |
| Mix | Wet/dry | 0–1 |
| Tone | Crossover frequency bias | 0–1 |
| P1 | Drive (pre-rotor saturation) | 0–1 |
| P2 | Horn/drum balance | 0 = all drum, 1 = all horn |
| Level | Output gain | 0–+6 dB |

**Tip**: Slow Speed with high Depth and some P1 drive is a classic Hammond organ texture.

---

### 3 — Vibe

1960s UniVibe emulation. Four cascaded allpass stages with nonlinear LFO response and simultaneous amplitude modulation create a warmer, throatier character than a standard phaser.

| Parameter | Function | Range |
|-----------|----------|-------|
| Speed | LFO rate | 0.5–10.0 Hz |
| Depth | Sweep + AM depth | 0–1 |
| Mix | Wet/dry | 0–1 |
| Tone | Post-vibe filter | LP ↔ HP |
| P1 | Intensity (regen/feedback) | 0–0.8 |
| P2 | Chorus/Vibrato blend | 0 = chorus, 1 = vibrato |
| Level | Output gain | 0–+6 dB |

P2 at 1.0 forces Mix to 100% wet (pure vibrato). P2 below 1.0 blends dry back in for the classic chorus-vibe sound.

---

### 4 — Phaser

Cascaded allpass filters create phase-shift notches that sweep across the frequency spectrum. Stage count controls density; Barber Pole creates an infinite ascending or descending sweep illusion.

| Sub-mode (P2) | Stages | Character |
|---------------|--------|-----------|
| 2-stage (0) | 2 | Subtle, 1 notch |
| 4-stage (1) | 4 | Classic, 2 notches |
| 6-stage (2) | 6 | Rich, 3 notches |
| 8-stage (3) | 8 | Dense, 4 notches |
| 12-stage (4) | 12 | Lush, 6 notches |
| 16-stage (5) | 16 | Ultra-dense, 8 notches |
| Barber Pole (6) | 4+4 | Infinite rising/falling sweep illusion |

| Parameter | Function | Range |
|-----------|----------|-------|
| Speed | LFO rate | 0.02–8.0 Hz |
| Depth | Sweep range | 0–1 |
| Mix | Wet/dry | 0–1 |
| Tone | Notch center frequency range | LP ↔ HP |
| P1 | Regeneration (feedback) | 0–0.95 |
| P2 | Sub-mode | 0–6 |
| Level | Output gain | 0–+6 dB |

**Barber Pole**: Two independent 4-stage allpass chains run in quadrature (90° LFO offset) and are crossfaded continuously. The result is a phaser sweep that appears to rise (or fall) without ever resetting.

---

### 5 — Filter

Resonant state-variable filter driven by LFO or envelope follower. Three filter types and eight waveshapes cover auto-wah through synth-style sweeps.

**Filter type** is set by the sub-mode encoder (separate layer from P2).

**LFO Waveshape (P2)**:

| P2 | Waveshape | Character |
|----|-----------|-----------|
| 0 | Sine | Smooth sweep |
| 1 | Triangle | Linear sweep |
| 2 | Square | Abrupt toggle |
| 3 | Ramp Up | Rising sawtooth |
| 4 | Ramp Down | Falling sawtooth |
| 5 | S&H | Random step |
| 6 | Env+ | Auto-wah (pick opens filter) |
| 7 | Env− | Inverted auto-wah (pick closes filter) |

| Parameter | Function | Range |
|-----------|----------|-------|
| Speed | LFO rate (not used in Env± modes) | 0.02–10.0 Hz |
| Depth | Sweep range | 0–1 |
| Mix | Wet/dry | 0–1 |
| Tone | Base cutoff frequency | 80 Hz–12 kHz |
| P1 | Resonance (Q) | 0.5–20.0 |
| P2 | LFO waveshape / envelope | 0–7 |
| Level | Output gain | 0–+6 dB |

---

### 6 — Formant

Dual bandpass SVF filters track vocal formant frequencies (F1, F2), creating talking and singing tones. An LFO morphs between adjacent vowel pairs.

| Sub-mode (P2) | Vowel | Character |
|---------------|-------|-----------|
| AA (0) | "ah" | Open, warm |
| EE (1) | "ee" | Bright, nasal |
| EYE (2) | "eye" | Diphthong sweep |
| OH (3) | "oh" | Round, hollow |
| OOH (4) | "ooh" | Dark, vocal |

| Parameter | Function | Range |
|-----------|----------|-------|
| Speed | Morph rate between vowel pair | 0.05–5.0 Hz |
| Depth | Morph range | 0–1 |
| Mix | Wet/dry | 0–1 |
| Tone | Formant frequency shift | ±1 octave |
| P1 | Bandwidth / resonance | 0 = narrow, 1 = wide |
| P2 | Vowel pair | 0–4 |
| Level | Output gain | 0–+6 dB |

The LFO sweeps between the selected vowel (P2) and the next in sequence (wraps from OOH back to AA).

---

### 7 — Vintage Trem

Classic amplitude tremolo. Three sub-modes emulate different analog tremolo circuits.

| Sub-mode (P2) | Circuit | Character |
|---------------|---------|-----------|
| Tube (0) | Asymmetric bias-shift | Warm, amp-like throb |
| Harmonic (1) | Dual LFO at f + 2f | Complex pulsing |
| Photoresistor (2) | Opto-coupler with LDR lag | Smooth, organic swell |

| Parameter | Function | Range |
|-----------|----------|-------|
| Speed | Tremolo rate | 1.0–15.0 Hz |
| Depth | Volume dip amount | 0–1 |
| Mix | Wet/dry | 0–1 |
| Tone | Post-trem filter | LP ↔ HP |
| P1 | Waveform shape / symmetry | 0–1 |
| P2 | Sub-mode | 0–2 |
| Level | Output gain | 0–+6 dB |

---

### 8 — Pattern Trem

Rhythmic tremolo with 16 built-in amplitude patterns. Syncs to tap tempo or MIDI clock for groove-locked gating, stutters, and polyrhythms.

| Parameter | Function | Range |
|-----------|----------|-------|
| Speed | Pattern rate (overridden by tap/MIDI clock) | 1.0–15.0 Hz |
| Depth | Pattern depth (0 = no effect, 1 = full gating) | 0–1 |
| Mix | Wet/dry | 0–1 |
| Tone | Post-trem filter | LP ↔ HP |
| P1 | Pattern select | 0–15 (16 built-in patterns) |
| P2 | Subdivision | 0 = straight, 1 = triplet, 2 = dotted |
| Level | Output gain | 0–+6 dB |

Patterns include straight 8ths, gallop, tresillo, habanera, samba, and others.

**Tip**: Set Speed with tap tempo and let P1 + P2 dial the groove.

---

### 9 — AutoSwell

Envelope-triggered volume swell that removes the pick attack, producing bowed or pad-like textures. Optional chorus adds shimmer.

The signal logic is inverted from a standard compressor: when input gets loud, gain drops; when input gets quiet, gain rises slowly.

| Parameter | Function | Range |
|-----------|----------|-------|
| Speed | Attack time (short = fast swell) | 10–500 ms |
| Depth | Swell intensity / sensitivity | 0–1 |
| Mix | Wet/dry | 0–1 |
| Tone | Post-swell filter | LP ↔ HP |
| P1 | Release time | 50–2000 ms |
| P2 | Chorus shimmer amount | 0 = off, 1 = full |
| Level | Output gain | 0–+6 dB |

**Note**: Tap tempo and MIDI clock do not affect AutoSwell — Speed controls a time constant, not an LFO rate.

---

### 10 — Destroyer

Lo-fi destruction through bitcrushing, sample-rate decimation, and a resonant filter. Speed and Depth are independent controls.

| Parameter | Function | Range |
|-----------|----------|-------|
| Speed | Decimation rate (sample-rate divisor) | 1× – 48× (48 kHz → 1 kHz) |
| Depth | Bit depth reduction | 0 = 16-bit, 1 = 1-bit |
| Mix | Wet/dry | 0–1 |
| Tone | LP filter cutoff on crushed signal | 80 Hz–20 kHz |
| P1 | Filter resonance | 0.5–8.5 Q |
| P2 | Vinyl noise amount | 0 = none, 1 = max |
| Level | Output gain | 0–+6 dB |

Speed and Depth are completely independent — you can have extreme bitcrushing with no decimation, or decimation only at full bit depth. The post-LP filter (Tone + P1) shapes the final crushed sound.

**Note**: Tap tempo and MIDI clock do not affect Destroyer — Speed is a decimation multiplier, not an LFO rate.

---

### 11 — Quadrature

Ring modulation, frequency shifting, and AM/FM synthesis using a Hilbert transform for true single-sideband effects. The audio-rate carrier creates sounds from subtle detuning to metallic clangour.

| Sub-mode (P2) | Algorithm | Character |
|---------------|-----------|-----------|
| AM (0) | Amplitude mod / ring mod | Metallic, clangorous |
| FM (1) | Frequency mod at audio rates | Bell-like, inharmonic |
| FreqShift+ (2) | Upper sideband (Hilbert) | Ascending shift |
| FreqShift− (3) | Lower sideband (Hilbert) | Descending shift |

| Parameter | Function | Range |
|-----------|----------|-------|
| Speed | Carrier frequency | 0.5–2000 Hz |
| Depth | Modulation depth | 0–1 |
| Mix | Wet/dry | 0–1 |
| Tone | Post-mod filter | LP ↔ HP |
| P1 | Carrier waveform (sine → square blend) | 0–1 |
| P2 | Sub-mode | 0–3 |
| Level | Output gain | 0–+6 dB |

At low carrier frequencies (< 20 Hz), AM becomes tremolo and FreqShift becomes subtle detuning. At 20–200 Hz the effect becomes robotic and pitch-shifting. Above 200 Hz metallic and clangorous sounds emerge.

---

## Tap Tempo

Press the tap footswitch (without holding the mode encoder) to tap in a tempo. After 2 or more taps the average interval is used to set Speed. Up to 4 taps are averaged.

**Timeout**: 2 seconds of inactivity resets the tap state.

Tap tempo applies to: Chorus, Flanger, Rotary, Vibe, Phaser, Filter, Formant, Vintage Trem, Pattern Trem.

Tap tempo does **not** apply to: AutoSwell (Speed = attack time), Destroyer (Speed = decimation rate), Quadrature (Speed = carrier frequency).

**Priority chain**: MIDI Clock > Tap Tempo > Encoder

---

## Presets

Eight preset slots (P1–P8) store mode + all 7 normalized parameter values in onboard QSPI flash. Presets survive power cycles.

### Selecting a Slot
Hold the mode encoder button and twist the mode encoder. The display shows the current slot.

### Loading a Preset
With a slot selected (mode encoder held), quickly press the tap footswitch.

### Saving a Preset
With a slot selected (mode encoder held), hold the tap footswitch for ≥ 700 ms. The display confirms the save.

### Power-On Defaults
If no valid preset is found in flash, the pedal boots with these defaults:

| Parameter | Default |
|-----------|---------|
| Speed | 30% |
| Depth | 50% |
| Mix | 50% |
| Tone | 50% (flat) |
| P1 | 0% |
| P2 | 0% |
| Level | 50% (unity) |

---

## MIDI

The pedal accepts MIDI over **USB** (class-compliant, no driver needed) and **TRS Type A**. Both ports are active simultaneously.

### CC Map

| CC | Parameter |
|----|-----------|
| 14 | Speed |
| 15 | Depth |
| 16 | Mix |
| 17 | Tone |
| 18 | P1 |
| 19 | P2 |
| 20 | Level |

CC values 0–127 map linearly to normalized 0–1 for each parameter.

### Program Change — Mode Select

| PC | Mode |
|----|------|
| 0 | Chorus |
| 1 | Flanger |
| 2 | Rotary |
| 3 | Vibe |
| 4 | Phaser |
| 5 | Filter |
| 6 | Formant |
| 7 | Vintage Trem |
| 8 | Pattern Trem |
| 9 | AutoSwell |
| 10 | Destroyer |
| 11 | Quadrature |

### MIDI Clock Sync

When MIDI Clock (0xF8) messages are received, Speed is locked to the beat period (in Hz). The display shows **MIDI** as the tempo source. Lock expires 2 seconds after the last tick. MIDI Stop (0xFC) cancels immediately.

Applies to: Chorus, Flanger, Rotary, Vibe, Phaser, Filter, Formant, Vintage Trem, Pattern Trem.
Does **not** apply to: AutoSwell, Destroyer, Quadrature.

### MIDI Learn

1. Hold the mode encoder button and twist any parameter encoder — the pedal enters MIDI Learn for that parameter.
2. Send any CC from your controller.
3. That CC is now mapped to the parameter.

Mappings reset on power cycle.

---

## VST Plugin

A VST3 and Standalone version is available in `desktop/vst/`. It uses the same DSP code as the firmware and adds:

- All 12 modes and 7 parameters as APVTS knobs
- Tempo sync to host BPM with subdivision selector (1/1, 1/2, 1/4., 1/4, 1/8., 1/8, 1/8T, 1/16)
- Stereo output on all modes

See the [README](README.md) for build instructions.

---

## Audio Specifications

| Spec | Value |
|------|-------|
| Sample rate | 48 kHz |
| Bit depth | 24-bit |
| Block size | 48 samples (1 ms) |
| Input | Mono |
| Output | Stereo (all modes) |
| Bypass | True (relay) |

---

## Quick Reference

### Footswitch Summary

| Action | Result |
|--------|--------|
| Tap bypass | Toggle effect on/off |
| Tap tap footswitch | Tap tempo |
| Hold mode encoder + twist mode encoder | Select preset slot P1–P8 |
| Hold mode encoder + tap tap footswitch | Load selected preset |
| Hold mode encoder + hold tap footswitch (700 ms) | Save to selected preset |
| Twist mode encoder | Change mode |

### Parameter Cheat Sheet

| Mode | Speed | Depth | Tone | P1 | P2 |
|------|-------|-------|------|----|----|
| Chorus | LFO rate | delay mod | filter | base delay | sub-mode |
| Flanger | LFO rate | sweep | filter | feedback | sub-mode |
| Rotary | rotor speed | doppler+AM | crossover | drive | horn/drum |
| Vibe | LFO rate | sweep+AM | filter | regen | chorus blend |
| Phaser | LFO rate | sweep | notch freq | regen | sub-mode |
| Filter | LFO rate | sweep | cutoff | resonance | waveshape |
| Formant | morph rate | morph range | freq shift | bandwidth | vowel |
| Vintage Trem | rate | depth | filter | shape | sub-mode |
| Pattern Trem | rate | depth | filter | pattern | subdivision |
| AutoSwell | attack time | sensitivity | filter | release | chorus amt |
| Destroyer | decimation | bit depth | cutoff | resonance | vinyl noise |
| Quadrature | carrier Hz | mod depth | filter | waveform | sub-mode |

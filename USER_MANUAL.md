# Daisy Delay — User Manual

## Overview

Daisy Delay is a stereo delay pedal built on the Electrosmith Daisy Seed platform. It features 10 delay modes, 7 parameters with a shift layer, 8 preset slots, tap tempo, MIDI control (USB + TRS), and a true bypass relay.

---

## Hardware Controls

### Encoders

There are five rotary encoders, each with a push button.

**Mode Encoder** (leftmost)
- Twist to cycle through the 10 delay modes (wraps around)
- Press and hold to enter the shift layer (see below)

**Parameter Encoders** (4 encoders)
- Without holding mode encoder: control the primary 4 parameters
- While holding mode encoder: control the shift-layer 3 parameters

| Layer | Encoder 1 | Encoder 2 | Encoder 3 | Encoder 4 |
|-------|-----------|-----------|-----------|-----------|
| Primary | Time | Repeats | Mix | Filter |
| Shift (hold mode) | Grit | Mod Speed | Mod Depth | — |

**Fast Turning**: Twisting an encoder quickly (within 40 ms of the previous detent) increases the step size from 0.5% to 1.5% per detent, allowing faster sweeps without overshooting.

---

### Footswitches

**Bypass Footswitch**
Toggles true bypass. The relay clicks audibly. The bypass LED is lit when the effect is active, off when bypassed. The pedal boots in bypass mode until fully initialized.

**Tap / Preset Footswitch**
Behavior depends on whether the mode encoder button is held:

| Mode encoder button | Short press | Long press (≥700 ms) |
|---------------------|-------------|----------------------|
| Not held | Tap tempo | — |
| Held | Load preset | Save preset |

---

### Display

The OLED (128×64) updates at ~30 fps and shows:
- Current mode name
- Parameter level bars for all visible parameters
- Bypass status
- Tempo source: **Pot**, **Tap**, or **MIDI**
- Current BPM when tap or MIDI clock is active
- Preset slot (P1–P8) when selecting presets
- Confirmation message after saving or loading a preset

---

## Parameters

All parameters are normalized internally as `[0, 1]` and mapped to physical units for DSP. Encoders and MIDI CC both address the same normalized values.

### Time
**Range**: 60 ms – 2500 ms (2 ms – 2500 ms in Lo-Fi mode)
**Curve**: Logarithmic — more resolution at short times
**MIDI CC**: 14

Sets the delay line length. When tap tempo or MIDI clock is active, this parameter is overridden by the computed beat period. The pot/encoder still holds its position and resumes control when tempo lock expires.

### Repeats
**Range**: 0 – 0.98
**Curve**: Linear
**MIDI CC**: 15

Controls feedback amount. At 0.98 the delay self-oscillates (infinite repeats). Fully clockwise approaches but never quite reaches infinite feedback to prevent runaway.

### Mix
**Range**: 0 (dry only) – 1.0 (wet only)
**Curve**: Linear, constant-power crossfade
**MIDI CC**: 16

Blends the dry signal with the wet (delay) signal. The constant-power curve preserves perceived loudness at the 50% midpoint.

### Filter
**Range**: 0 – 1.0
**Curve**: Linear
**MIDI CC**: 17

Tone control on the delay output. At 0.5 the filter is neutral. Below 0.5 it progressively low-passes (darkens) the repeats; above 0.5 it high-passes (thins) them.

### Grit
**Range**: 0 – 1.0
**Curve**: Linear
**MIDI CC**: 18

A mode-specific character control. Its exact effect varies by mode — see the mode descriptions below.

### Mod Speed
**Range**: 0.05 – 10.0 Hz
**Curve**: Logarithmic — more resolution at slow rates
**MIDI CC**: 19

LFO rate for modulation effects. In Swell mode this controls envelope attack time instead.

### Mod Depth
**Range**: 0 – 1.0
**Curve**: Linear
**MIDI CC**: 20

Controls how much the LFO (or envelope) affects the signal. The exact target varies by mode.

---

## Delay Modes

Modes are selected by twisting the mode encoder. There are 10 modes total. All modes output wet signal only; the audio engine applies the Mix crossfade.

### 0 — Duck
The wet signal ducks (attenuates) when the input is loud, then swells back up in silences. Useful for keeping repeats audible between notes without cluttering busy playing.

| Parameter | Function |
|-----------|----------|
| Time | Delay time |
| Repeats | Feedback |
| Filter | Tone |
| **Grit** | Duck depth (0 = no ducking, 1 = maximum) |
| Mod Speed | LFO rate |
| Mod Depth | Delay time modulation depth |

---

### 1 — Swell
An attack-decay envelope is triggered by input peaks, creating soft pad-like volume swells on the repeats.

| Parameter | Function |
|-----------|----------|
| Time | Delay time |
| Repeats | Feedback |
| **Mod Speed** | Attack time (fast knob = short attack) |
| **Mod Depth** | Decay time |
| Filter | Not used |
| Grit | Not used |

---

### 2 — Trem
A sine LFO applies tremolo to the wet output, creating rhythmic amplitude pulsing on the repeats. The feedback path is taken before the tremolo so repeat levels stay consistent.

| Parameter | Function |
|-----------|----------|
| Time | Delay time |
| Repeats | Feedback |
| Filter | Tone |
| Mod Speed | Tremolo rate |
| Mod Depth | Tremolo depth |
| Grit | Not used |

---

### 3 — Digital
Clean, transparent delay with gentle time modulation. The most neutral mode — best for straightforward echo without added color.

| Parameter | Function |
|-----------|----------|
| Time | Delay time |
| Repeats | Feedback |
| Filter | Tone |
| Mod Speed | LFO rate |
| Mod Depth | Delay time modulation depth |
| Grit | Not used |

---

### 4 — DBucket
Emulates a BBD (bucket-brigade device) analog delay chip. Each repeat progressively loses high-frequency content and picks up clock noise, just like a vintage analog delay.

| Parameter | Function |
|-----------|----------|
| Time | Delay time |
| Repeats | Feedback |
| **Grit** | HF rolloff steepness AND noise amount (both together) |
| Mod Speed | Wow/flutter rate |
| Mod Depth | Wow/flutter depth |
| Filter | Not used (controlled by Grit) |

With Grit at zero the repeats are relatively bright and quiet. At maximum, repeats darken noticeably and clock noise becomes audible.

---

### 5 — Tape
Simulates a tape echo machine with wow/flutter time modulation and progressive soft saturation on the feedback path. Repeats warm and compress as they accumulate.

| Parameter | Function |
|-----------|----------|
| Time | Delay time |
| Repeats | Feedback |
| **Grit** | Saturation drive (0 = clean, 1 = heavy compression/clipping) |
| Mod Speed | Wow/flutter rate |
| Mod Depth | Wow/flutter depth |
| Filter | Tone |

---

### 6 — Dual
Two independent delay lines panned left and right, slightly detuned from each other and animated by the LFO. Creates a wide stereo image and chorus-like shimmer. This is the only mode with true stereo output.

| Parameter | Function |
|-----------|----------|
| Time | Left channel delay time |
| Repeats | Feedback (both channels) |
| Filter | Tone (both channels) |
| Mod Speed | LFO rate (animates detune) |
| Mod Depth | Detune amount (0 = mono-locked, 1 = maximum stereo spread) |
| Grit | Not used |

---

### 7 — Pattern
Three delay taps at rhythmically meaningful intervals, creating syncopated repeating patterns. The base Time sets the beat subdivision; Grit selects the rhythmic pattern.

| Parameter | Function |
|-----------|----------|
| Time | Base delay (tap 1 timing) |
| Repeats | Feedback from first tap |
| **Grit** | Pattern: 0–33% = straight, 33–67% = dotted, 67–100% = triplet |
| Mod Speed | LFO rate |
| Mod Depth | Delay time modulation depth |
| Filter | Tone |

**Patterns**:
- **Straight**: taps at 1×, 2×, 3× base time
- **Dotted**: taps at 1×, 1.5×, 3× (swing feel)
- **Triplet**: taps at 0.67×, 1.33×, 2× (triplet feel)

Works especially well with tap tempo — set the beat and let Grit choose the groove.

---

### 8 — Filter
A resonant state-variable filter sweeps over the repeats, driven by the LFO. At high Mod Depth, the filter approaches self-oscillation, generating pitched tones.

| Parameter | Function |
|-----------|----------|
| Time | Delay time |
| Repeats | Feedback |
| Mod Speed | Filter sweep rate |
| **Mod Depth** | Sweep depth AND resonance (high = more resonant, can self-oscillate) |
| Filter | Not used |
| Grit | Not used |

**Tip**: Set Repeats high and Mod Depth near maximum for synth-like tonal effects.

---

### 9 — Lo-Fi
Bit-crushing and sample-rate reduction are applied to each repeat, progressively degrading audio into crunchy lo-fi territory. Has the shortest minimum time of any mode (2 ms).

| Parameter | Function |
|-----------|----------|
| Time | Delay time (2 ms – 2500 ms) |
| Repeats | Feedback |
| **Grit** | Bit depth (16-bit at 0 → 4-bit at max) AND decimation factor (1× → 16×) |
| Mod Speed | LFO rate (triangle wave) |
| Mod Depth | Delay time modulation depth |
| Filter | Not used |

Grit is the most dramatic knob in this mode — the first few percent already introduces noticeable artifacts.

---

## Presets

Eight preset slots (P1–P8) are stored in onboard flash and survive power cycles. Each slot saves the current mode and all 7 parameter values.

### Selecting a Slot
Hold the mode encoder button and twist the mode encoder. The display shows the current slot (P1–P8).

### Loading a Preset
While holding the mode encoder button and with a slot selected, quickly press the tap footswitch. The pedal switches to the saved mode and recalls all parameter values immediately.

### Saving a Preset
While holding the mode encoder button and with a slot selected, hold the tap footswitch for 700 ms or longer. The display shows a confirmation message for 1 second.

### Power-On Defaults
If no valid preset is found in flash (e.g., first power-on), the pedal boots with these defaults:

| Parameter | Default |
|-----------|---------|
| Time | 50% |
| Repeats | 40% |
| Mix | 50% |
| Filter | 50% (neutral) |
| Grit | 0% |
| Mod Speed | 50% |
| Mod Depth | 0% |

---

## MIDI

The pedal accepts MIDI over both **USB** (class-compliant, no driver needed) and **TRS Type A**. Both ports are active simultaneously.

### CC Control

| CC # | Parameter |
|------|-----------|
| 14 | Time |
| 15 | Repeats |
| 16 | Mix |
| 17 | Filter |
| 18 | Grit |
| 19 | Mod Speed |
| 20 | Mod Depth |

CC values 0–127 map linearly to the normalized parameter range 0–1.

### Program Change — Mode Select

| PC # | Mode |
|------|------|
| 0 | Duck |
| 1 | Swell |
| 2 | Trem |
| 3 | Digital |
| 4 | DBucket |
| 5 | Tape |
| 6 | Dual |
| 7 | Pattern |
| 8 | Filter |
| 9 | Lo-Fi |

### MIDI Clock Sync

When MIDI Clock (0xF8) messages are received, the Time parameter is locked to the beat period. The display shows **MIDI** as the tempo source. After 2 seconds without a clock pulse, the lock expires and pot/encoder control resumes. MIDI Stop (0xFC) cancels the lock immediately.

**Priority chain**: MIDI Clock > Tap Tempo > Pot/Encoder

### MIDI Learn

1. Hold the mode encoder button
2. Twist the mode encoder — the pedal enters MIDI Learn for the highlighted parameter
3. Send any CC from your controller

The first CC received is mapped to that parameter. Mappings are session-only and reset on power cycle.

---

## Tap Tempo

Press the tap footswitch (without holding the mode encoder button) to tap in a tempo. After 2 or more taps the average interval is computed and the Time parameter is locked to that beat period. Up to 4 taps are averaged for accuracy.

**BPM range**: 40–240 BPM
**Timeout**: 2 seconds of inactivity resets the tap state and returns control to the pot/encoder.

The current BPM and source (**Tap**) are shown on the display while active.

Tap tempo takes priority over pot/encoder control but yields to MIDI Clock if clock messages arrive.

---

## Audio Specifications

| Spec | Value |
|------|-------|
| Sample rate | 48 kHz |
| Bit depth | 24-bit |
| Block size | 48 samples (1 ms) |
| Input | Mono |
| Output | Stereo (true stereo in Dual mode; L=R in all other modes) |
| Max delay time | 2500 ms |
| Bypass | True (relay) |

---

## Quick Reference

### Parameter Cheat Sheet

| Parameter | Duck | Swell | Trem | Digital | DBucket | Tape | Dual | Pattern | Filter | Lo-Fi |
|-----------|------|-------|------|---------|---------|------|------|---------|--------|-------|
| Time | delay | delay | delay | delay | delay | delay | L delay | base | delay | delay (2ms min) |
| Repeats | fbk | fbk | fbk | fbk | fbk | fbk | fbk | fbk | fbk | fbk |
| Mix | wet/dry | wet/dry | wet/dry | wet/dry | wet/dry | wet/dry | wet/dry | wet/dry | wet/dry | wet/dry |
| Filter | tone | — | tone | tone | — | tone | tone | tone | — | — |
| **Grit** | **duck depth** | — | — | — | **HF + noise** | **saturation** | — | **pattern** | — | **bits + decim** |
| Mod Speed | LFO rate | attack | trem rate | LFO rate | flutter | flutter | LFO rate | LFO rate | sweep rate | LFO rate |
| Mod Depth | time mod | decay | trem depth | time mod | flutter | flutter | stereo width | time mod | resonance | time mod |

### Footswitch Quick Reference

| Action | Result |
|--------|--------|
| Tap bypass footswitch | Toggle effect on/off |
| Tap tap footswitch | Tap tempo |
| Hold mode encoder + twist mode encoder | Select preset slot P1–P8 |
| Hold mode encoder + tap tap footswitch | Load selected preset |
| Hold mode encoder + hold tap footswitch (700ms) | Save to selected preset |
| Twist mode encoder (no hold) | Change delay mode |

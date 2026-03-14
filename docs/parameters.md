# Parameter Scheme

## 7 Universal Parameters

All modulation modes share the same 7-parameter structure. Parameters flow as normalized `[0, 1]` values and are converted to physical units at the last moment via `map_param()`.

| ID | Name  | Encoder Layer | Physical Unit | Description |
|----|-------|---------------|---------------|-------------|
| 0  | Speed | Primary (Enc 1) | Hz or ms | LFO rate, carrier freq, or time constant |
| 1  | Depth | Primary (Enc 2) | 0–1 scalar | Modulation depth / intensity |
| 2  | Mix   | Primary (Enc 3) | 0–1 scalar | Constant-power wet/dry crossfade |
| 3  | Tone  | Primary (Enc 4) | 0–1 scalar | Post-effect LP (0) ↔ HP (1), 0.5 = flat |
| 4  | P1    | Shift (Enc 1)   | mode-dependent | Mode-specific parameter 1 |
| 5  | P2    | Shift (Enc 2)   | mode-dependent | Mode-specific parameter 2 / sub-mode select |
| 6  | Level | Shift (Enc 3)   | 0–1 scalar | Output gain (unity at 0.5, boost to +6 dB at 1.0) |

Shift layer is accessed by holding the mode encoder button.

## Encoder Mapping

### Primary Layer (no shift)

| Encoder | Parameter | Display |
|---------|-----------|---------|
| 1       | Speed     | Rate in Hz or BPM |
| 2       | Depth     | Percentage 0–100% |
| 3       | Mix       | Percentage 0–100% |
| 4       | Tone      | LP ← · → HP |

### Shift Layer (hold mode button)

| Encoder | Parameter | Display |
|---------|-----------|---------|
| 1       | P1        | Mode-dependent label + value |
| 2       | P2        | Sub-mode name or value |
| 3       | Level     | dB value (−∞ to +6) |
| 4       | Sub-mode  | Mode variant name |

## Per-Mode P1/P2 Interpretation

| Mode | P1 | P1 Range | P2 | P2 Range |
|------|-----|----------|-----|----------|
| Chorus | Base delay time | 1–20 ms | Sub-mode | 0–4 (dBucket/Multi/Vibrato/Detune/Digital) |
| Flanger | Feedback amount | −0.95 – +0.95 | Sub-mode | 0–5 (Silver/Grey/Black+/Black−/Zero+/Zero−) |
| Rotary | Drive | 0.0–1.0 | Horn/drum balance | 0.0–1.0 |
| Vibe | Intensity (regen) | 0.0–0.8 | Chorus/Vibrato blend | 0.0–1.0 |
| Phaser | Regeneration | 0.0–0.95 | Sub-mode | 0–6 (2/4/6/8/12/16-stage/Barber Pole) |
| Filter | Resonance (Q) | 0.5–20.0 | LFO waveshape | 0–7 (Sine/Tri/Sq/Up/Dn/S&H/Env+/Env−) |
| Formant | Bandwidth | 0.0–1.0 | Vowel pair | 0–4 (AA/EE/EYE/OH/OOH) |
| Vintage Trem | Waveform shape | 0.0–1.0 | Sub-mode | 0–2 (Tube/Harmonic/Photoresistor) |
| Pattern Trem | Pattern select | 0–15 | Subdivision | 0–2 (Straight/Triplet/Dotted) |
| AutoSwell | Release time | 50–2000 ms | Chorus amount | 0.0–1.0 |
| Destroyer | Filter resonance | 0.0–1.0 | Vinyl noise | 0.0–1.0 |
| Quadrature | Carrier waveform | 0.0–1.0 | Sub-mode | 0–3 (AM/FM/FreqShift+/FreqShift−) |

## Per-Mode Speed Ranges

Most modes use the default Speed range (0.05–10.0 Hz). Exceptions:

| Mode | Speed Range | Curve | Notes |
|------|-------------|-------|-------|
| Chorus | 0.05–5.0 Hz | log | Slower max for subtle chorus |
| Flanger | 0.01–5.0 Hz | log | Very slow sweeps common |
| Phaser | 0.02–8.0 Hz | log | Wide range for slow sweeps |
| Vintage Trem | 1.0–15.0 Hz | log | Faster range for tremolo |
| Pattern Trem | 1.0–15.0 Hz | log | Rhythmic rates |
| AutoSwell | 10–500 ms | log | Repurposed as attack time |
| Destroyer | 1×–48× | log | Repurposed as decimation rate |
| Quadrature | 0.5–2000 Hz | log | Audio-rate carrier frequency |

## Tap Tempo

Tap tempo syncs the Speed parameter (LFO rate) for applicable modes:

- **Period → Hz conversion:** `rate_hz = 1.0 / tap_period_seconds`
- **Subdivision:** When synced, the pattern trem uses the tap period as one beat
- **Override:** While tap tempo is active, the Speed encoder is locked (visual indicator on OLED)
- **Timeout:** 2 seconds without a tap releases the lock

Modes where tap tempo applies: Chorus, Flanger, Rotary, Vibe, Phaser, Filter, Formant, Vintage Trem, Pattern Trem.

Modes where tap tempo does NOT apply: AutoSwell (Speed = attack time), Destroyer (Speed = decimation), Quadrature (Speed = carrier freq).

## MIDI CC Mapping

| CC  | Parameter | Range |
|-----|-----------|-------|
| 14  | Speed     | 0–127 → normalized 0.0–1.0 |
| 15  | Depth     | 0–127 → normalized 0.0–1.0 |
| 16  | Mix       | 0–127 → normalized 0.0–1.0 |
| 17  | Tone      | 0–127 → normalized 0.0–1.0 |
| 18  | P1        | 0–127 → normalized 0.0–1.0 |
| 19  | P2        | 0–127 → normalized 0.0–1.0 |
| 20  | Level     | 0–127 → normalized 0.0–1.0 |

Program Change 0–11 selects modes (Chorus...Quadrature).

MIDI Clock locks LFO rate to beat period (2 s timeout after last tick).

# DSP New Capabilities — Phase C Design

**Date:** 2026-05-16
**Status:** Approved
**Depends on:** Phase A (Hermite interpolation, LFO humanization, WaveShaper), Phase B (mode character wiring)
**Target project:** `modulation/` only

---

## Goal

Add three capabilities to the modulation pedal building on the Phase A/B DSP foundation: expression pedal hardware input, a combined wah/envelope-filter mode, and a stereo widener mode. No changes to existing modes or multi-fx.

---

## Scope

### Deliverable 1 — Expression Pedal Infrastructure

First-time ADC initialization for the modulation firmware. The project currently uses encoders exclusively; the ADC peripheral is never initialized despite 7 channels being defined in `pin_map.h`.

**Hardware:** TRS expression pedal jack wired to a free ADC-capable pin on the Daisy Seed. Select from `daisy::seed::A0–A11` — confirm against PCB layout that the chosen pin is physically accessible and not conflicting with existing signals.

**Convention:** toe-down = 1.0 (filter open), heel-down = 0.0 (filter closed). Standard TRS expression pedal: tip = wiper, ring = supply (3.3 V from Daisy).

**Firmware changes:**

| File | Change |
|------|--------|
| `src/config/pin_map.h` | Add `constexpr auto EXP_PEDAL_PIN = daisy::seed::AX;` (X = chosen pin) |
| `src/hardware/controls.h` | Add `AdcHandle adc_;` member; declare `float GetExprPedal() const` and `float expr_pedal_` |
| `src/hardware/controls.cpp` | Init `adc_` in `Controls::Init()`; poll and smooth in `Controls::Poll()`; one-pole IIR (`coeff ≈ 0.033`) |
| `src/main.cpp` | Read `controls.GetExprPedal()` in main loop; pass into `BuildParams()` call |
| `src/params/param_set.h` | Add `float expr_pedal = 0.5f;` to `ParamSet` (neutral; all 12 existing modes silently ignore it) |
| `src/params/param_map.cpp` | Populate `expr_pedal` field in `BuildParams()` |

---

### Deliverable 2 — WahMode (mode ID 12)

Combines expression-pedal wah, auto-wah (envelope), and rhythmic wah (LFO) in a single mode. All three share the same SVF bandpass core; P2 selects the control source.

**Mode ID:** 12 — requires `NUM_MODES = 13` in `src/config/constants.h`

**Control source selection (P2):**

| P2 range | Source | Behavior |
|----------|--------|----------|
| 0.0 – 0.33 | Expression pedal | `params.expr_pedal` drives cutoff directly |
| 0.33 – 0.66 | Envelope follower | Louder input → higher cutoff (auto-wah) |
| 0.66 – 1.0 | LFO (Sine) | Periodic cutoff sweep at `params.speed` Hz |

**Cutoff computation:**
```
base_hz   = map_param(params.tone, 200.0f, 3000.0f, log)
depth_hz  = params.depth * 2500.0f
cutoff_hz = base_hz + depth_hz * control_value   // control_value ∈ [0, 1]
```

The `SetFreq()` call (contains `tanf()`) must go in `Prepare()`, not per-sample in `Process()`. Cache the computed `cutoff_hz_` across the block and apply it in `Prepare()`. For envelope mode, the per-sample envelope value is accumulated in `Process()` and applied next block — same deferred pattern as `FilterMode`.

**Parameters:**

| Param | Physical range | Notes |
|-------|----------------|-------|
| `speed` | 0.05–10 Hz | LFO rate (LFO mode); also used to scale envelope attack 2–100 ms |
| `depth` | 0.0–1.0 | Sweep range (0 = no movement, 1 = full 2500 Hz above base) |
| `mix` | 0.0–1.0 | Wet/dry — engine handles |
| `tone` | 200–3000 Hz (log) | Base cutoff / wah center frequency |
| `p1` | Q: 0.5–15 | Resonance peak sharpness |
| `p2` | 0.0–1.0 | Source selector: three equal thirds |
| `level` | 0.0–2.0 | Output gain — engine handles |

**DSP members:** `Svf svf_`, `EnvelopeFollower env_`, `Lfo lfo_`, `DcBlocker dc_`, `float cutoff_hz_`, `float env_val_`

**Files:**

| File | Change |
|------|--------|
| `src/modes/wah_mode.h` | NEW — `WahMode` class inheriting `ModMode` |
| `src/modes/wah_mode.cpp` | NEW — implementation |
| `src/config/constants.h` | `NUM_MODES` 12 → 13 |
| `src/config/mod_mode_id.h` | Add `Wah = 12` |
| `src/modes/mode_registry.cpp` | Add `static WahMode s_wah;`, register in `Init()` |
| `Makefile` | Add `src/modes/wah_mode.cpp` to `CPP_SOURCES` |

---

### Deliverable 3 — StereoWidener (mode ID 13)

Builds a stereo image from mono guitar input using Haas psychoacoustics with quadrature LFO modulation.

**Mode ID:** 13 — requires `NUM_MODES = 14` in `src/config/constants.h`

**Algorithm:**
- Take mono input: `float in = input.mono()`
- **L path:** `in` → `delay_l_` (fixed ~1 ms, LFO at 0°) → `dc_l_`
- **R path:** `in` → `delay_r_` (Haas offset 2–25 ms, LFO at 90°) → `dc_r_`
- Return `StereoFrame{left_out, right_out}`

The 90° LFO phase offset between L and R makes the image breathe naturally. With `depth = 0` the image is static spatial spread. With `depth = 1` the delay times modulate fully, adding chorus-like width movement.

**Memory:** Both delay buffers are plain arrays in DTCMRAM (static member arrays, no `DSY_SDRAM_BSS`):
- `delay_l_`: 48 samples (1 ms at 48 kHz) × 4 B = 192 B
- `delay_r_`: 1200 samples (25 ms at 48 kHz) × 4 B = 4.8 KB

**LFO modulation of delay time:**
```
float lfo_val   = lfo_.Process()         // [-1, +1]
float mod_samps = lfo_val * params.depth * (tone_samps * 0.3f)
delay_l_.SetDelay(1.0f + mod_samps)
delay_r_.SetDelay(tone_samps + mod_samps_quadrature)
```
LFO on R is offset 90°; `lfo_r_phase_offset_ = kHalfPi` set in `Init()`.

**Parameters:**

| Param | Physical range | Notes |
|-------|----------------|-------|
| `speed` | 0.05–5 Hz | LFO rate |
| `depth` | 0.0–1.0 | LFO swing amount (0 = static spread) |
| `mix` | 0.0–1.0 | Wet/dry — engine handles |
| `tone` | 2–25 ms (log) | Base Haas offset on R (controls stereo width) |
| `p1` | 0.0–1.0 | LFO waveform: 0–0.33 = Sine, 0.33–0.66 = Triangle, 0.66–1.0 = SmoothRandom |
| `p2` | — | Unused |
| `level` | 0.0–2.0 | Output gain — engine handles |

**DSP members:** `DelayLine<48> delay_l_`, `DelayLine<1200> delay_r_`, `Lfo lfo_`, `DcBlocker dc_l_`, `DcBlocker dc_r_`

**Files:**

| File | Change |
|------|--------|
| `src/modes/stereo_widener.h` | NEW — `StereoWidener` class inheriting `ModMode` |
| `src/modes/stereo_widener.cpp` | NEW — implementation |
| `src/config/constants.h` | `NUM_MODES` 13 → 14 |
| `src/config/mod_mode_id.h` | Add `StereoWidener = 13` |
| `src/modes/mode_registry.cpp` | Add `static StereoWidener s_stereo_widener;`, register in `Init()` |
| `Makefile` | Add `src/modes/stereo_widener.cpp` to `CPP_SOURCES` |

---

## Deliberately Unchanged

- All existing 12 modes — no DSP, parameter, or behavior changes
- `multi-fx/` project — Phase C is modulation-only
- `ParamSet` grows by one `float expr_pedal` field — existing modes receive it but never read it

---

## Flash Budget

| | Value |
|--|-------|
| Current usage | ~87% of 128 KB internal flash (~111 KB) |
| Free headroom | ~17 KB |
| Estimated Phase C addition | ~5–7 KB total |
| Projected post-Phase C | ~91–92% |

Breakdown: expression pedal infra ~1 KB, WahMode ~2–3 KB, StereoWidener ~2–3 KB.

---

## Verification

| Test | Mode | Settings | Pass criterion |
|------|------|----------|----------------|
| Expr pedal routing | WahMode | P2 < 0.33 | Filter sweeps smoothly as pedal moves; no zipper noise |
| Auto-wah | WahMode | P2 0.33–0.66 | Harder playing opens filter; light playing closes it |
| Rhythmic wah | WahMode | P2 > 0.66, speed 1 Hz | Audible 1 Hz sweep |
| Wah resonance | WahMode | p1 high (Q > 10) | Pronounced peak / quack character |
| Stereo spread | StereoWidener | tone 50%, depth 0 | Clear L/R separation on headphones |
| Stereo LFO | StereoWidener | depth 100%, speed 0.3 Hz | Smooth image movement |
| Mono compatibility | StereoWidener | mix 100%, tone any | Summed to mono: acceptable (minimal comb filtering) |
| Flash budget | — | `make -j4` | Binary stays ≤ 128 KB |
| Regression | all modes | cycle through all 14 modes | No click, dropout, or crash on mode switch |

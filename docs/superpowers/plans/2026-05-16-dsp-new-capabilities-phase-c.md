# DSP New Capabilities — Phase C Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add expression pedal ADC input, a combined Wah/EnvelopeFilter mode, and a StereoWidener mode to the modulation firmware.

**Architecture:** Three sequential tasks — expression pedal infrastructure (ADC init + ParamSet field) must land before WahMode (which reads `params.expr_pedal`); StereoWidener is independent. All new modes follow the `FilterMode` / `RotaryMode` patterns already in the codebase: file-scope static DSP objects, `Init()` → `Reset()` → `Prepare()` → `Process()`.

**Tech Stack:** C++17, libDaisy (Daisy Seed H750, 48 kHz, ARM Cortex-M7), no heap, audio ISR. Build: `make -j4` from `modulation/`. No automated test suite — build success + flash budget check serve as the verification gate.

---

## File Map

| File | Action | Description |
|------|--------|-------------|
| `src/config/pin_map.h` | Modify | Add `EXP_PEDAL_PIN` |
| `src/hardware/controls.h` | Modify | Add `AdcHandle`, `GetExprPedal()`, `expr_pedal_` |
| `src/hardware/controls.cpp` | Modify | ADC init + poll + smooth in `Init()` / `Poll()` |
| `src/params/param_set.h` | Modify | Add `float expr_pedal = 0.5f` field |
| `src/main.cpp:311` | Modify | Remove `const`, add `params.expr_pedal = controls.GetExprPedal()` |
| `src/config/constants.h` | Modify (×2) | `NUM_MODES` 12→13 (Task 2), 13→14 (Task 3) |
| `src/config/mod_mode_id.h` | Modify (×2) | Add `Wah=12` (Task 2), `StereoWidener=13` (Task 3) |
| `src/modes/wah_mode.h` | Create | `WahMode` class declaration |
| `src/modes/wah_mode.cpp` | Create | `WahMode` implementation |
| `src/modes/stereo_widener.h` | Create | `StereoWidener` class declaration |
| `src/modes/stereo_widener.cpp` | Create | `StereoWidener` implementation |
| `src/modes/mode_registry.cpp` | Modify (×2) | Register each new mode in its own task |
| `Makefile` | Modify (×2) | Add each new `.cpp` to `CPP_SOURCES` |

---

## Task 1: Expression Pedal Infrastructure

**Files:**
- Modify: `src/config/pin_map.h`
- Modify: `src/hardware/controls.h`
- Modify: `src/hardware/controls.cpp`
- Modify: `src/params/param_set.h`
- Modify: `src/main.cpp`

> **Hardware note:** `daisy::seed::A0` is the first ADC-capable analog input on the Daisy Seed. The modulation project currently never initialises the ADC peripheral, so no ADC pins are allocated. Verify `A0` is physically accessible on your PCB for the TRS expression pedal jack (tip = wiper, ring = 3.3V supply from Daisy).

- [ ] **Step 1: Add expression pedal pin to `src/config/pin_map.h`**

Add one line inside `namespace pedal { namespace pins {`:

```cpp
// Expression pedal (TRS input, wiper on A0)
constexpr daisy::Pin EXP_PEDAL_PIN = daisy::seed::A0;
```

Full file after edit:

```cpp
#pragma once
#include "daisy_seed.h"

namespace pedal {
namespace pins {
// ADC pots (index = ADC channel)
constexpr int POT_TIME    = 0;
constexpr int POT_REPEATS = 1;
constexpr int POT_MIX     = 2;
constexpr int POT_FILTER  = 3;
constexpr int POT_GRIT    = 4;
constexpr int POT_MOD_SPD = 5;
constexpr int POT_MOD_DEP = 6;
// Expression pedal (TRS input, wiper on A0)
constexpr daisy::Pin EXP_PEDAL_PIN = daisy::seed::A0;
// GPIO
constexpr daisy::Pin ENC_A         = daisy::seed::D0;
constexpr daisy::Pin ENC_B         = daisy::seed::D1;
constexpr daisy::Pin ENC_SW        = daisy::seed::D2;
constexpr daisy::Pin PARAM_ENC_0_A = daisy::seed::D7;
constexpr daisy::Pin PARAM_ENC_0_B = daisy::seed::D8;
constexpr daisy::Pin PARAM_ENC_1_A = daisy::seed::D9;
constexpr daisy::Pin PARAM_ENC_1_B = daisy::seed::D10;
constexpr daisy::Pin PARAM_ENC_2_A = daisy::seed::D27;
constexpr daisy::Pin PARAM_ENC_2_B = daisy::seed::D28;
constexpr daisy::Pin PARAM_ENC_3_A = daisy::seed::D29;
constexpr daisy::Pin PARAM_ENC_3_B = daisy::seed::D30;
constexpr daisy::Pin SW_BYPASS     = daisy::seed::D3;
constexpr daisy::Pin SW_TAP        = daisy::seed::D4;
constexpr daisy::Pin RELAY         = daisy::seed::D5;
constexpr daisy::Pin LED_BYPASS    = daisy::seed::D6;
} // namespace pins
} // namespace pedal
```

- [ ] **Step 2: Add ADC member and accessor to `src/hardware/controls.h`**

Add `#include "daisy_seed.h"` is already present. Add three things to the `Controls` class:

```cpp
// In the public section, after Poll():
float GetExprPedal() const { return expr_pedal_; }

// In the private section, after enc_timer_:
daisy::AdcHandle adc_;
float            expr_pedal_ = 0.5f;
```

Full `controls.h` after edit:

```cpp
#pragma once
#include "daisy_seed.h"
#include "../params/param_set.h"
#include "../config/mod_mode_id.h"
#include "../config/constants.h"

namespace pedal {

struct ControlState {
    int  mode_encoder_increment;
    int  param_encoder_increment[4]{};
    bool mode_encoder_pressed;
    bool mode_encoder_held;
    bool bypass_pressed;
    bool tap_pressed;
    bool tap_released;
    bool tap_held;
    uint32_t tap_held_ms;
    bool bypass_held;
};

class Controls {
public:
    void Init(daisy::DaisySeed& hw);
    void Poll();
    const ControlState& state() const { return state_; }
    float GetExprPedal() const { return expr_pedal_; }

private:
    class QuadEncoder {
      public:
        void Init(daisy::Pin a, daisy::Pin b);
        void IsrPoll(volatile int8_t& out);
      private:
        uint8_t ReadState();
        daisy::GPIO a_;
        daisy::GPIO b_;
        uint8_t raw_prev_ = 0;
        uint8_t stable_   = 0;
        int8_t  accum_    = 0;
    };

    static void EncoderIsrCallback(void* data);

    daisy::Encoder     encoder_;
    QuadEncoder        param_enc_[4];
    daisy::Switch      sw_bypass_;
    daisy::Switch      sw_tap_;
    ControlState       state_{};
    daisy::TimerHandle enc_timer_;
    daisy::AdcHandle   adc_;
    float              expr_pedal_ = 0.5f;
    volatile int8_t    isr_delta_[4]{};
    volatile int8_t    isr_mode_delta_ = 0;
};

} // namespace pedal
```

- [ ] **Step 3: Add ADC init and polling to `src/hardware/controls.cpp`**

In `Controls::Init()`, add ADC initialisation **after** `enc_timer_.Start()`:

```cpp
    // Expression pedal ADC (single channel on A0)
    daisy::AdcChannelConfig adc_cfg;
    adc_cfg.InitSingle(pins::EXP_PEDAL_PIN);
    adc_.Init(&adc_cfg, 1);
    adc_.Start();
```

In `Controls::Poll()`, add one-pole smoothing **at the end of the function**, after `state_.bypass_held = ...`:

```cpp
    // Expression pedal: one-pole IIR smooth (same coefficient as pot smoothing)
    const float raw_expr = adc_.GetFloat(0);
    expr_pedal_ += POT_SMOOTH * (raw_expr - expr_pedal_);
```

Full `controls.cpp` after both edits:

```cpp
#include "controls.h"
#include "../config/pin_map.h"

namespace pedal {

static const int8_t kTransitionTable[16] = {
    0, -1,  1,  0,
    1,  0,  0, -1,
   -1,  0,  0,  1,
    0,  1, -1,  0,
};

void Controls::QuadEncoder::Init(daisy::Pin a, daisy::Pin b) {
    a_.Init(a, daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
    b_.Init(b, daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
    const uint8_t initial = ReadState();
    raw_prev_ = initial;
    stable_   = initial;
    accum_    = 0;
}

void Controls::QuadEncoder::IsrPoll(volatile int8_t& out) {
    const uint8_t raw = ReadState();
    if (raw != raw_prev_) { raw_prev_ = raw; return; }
    if (raw == stable_) return;
    const uint8_t index = static_cast<uint8_t>((stable_ << 2) | raw);
    stable_ = raw;
    accum_ += kTransitionTable[index];
    if (accum_ >= 2)       { accum_ = 0; out += 1; }
    else if (accum_ <= -2) { accum_ = 0; out -= 1; }
}

uint8_t Controls::QuadEncoder::ReadState() {
    return static_cast<uint8_t>((a_.Read() ? 1 : 0) | (b_.Read() ? 2 : 0));
}

void Controls::EncoderIsrCallback(void* data) {
    Controls* self = static_cast<Controls*>(data);
    for (int i = 0; i < 4; ++i)
        self->param_enc_[i].IsrPoll(self->isr_delta_[i]);
    self->encoder_.Debounce();
    int32_t inc = self->encoder_.Increment();
    if (inc != 0) self->isr_mode_delta_ += static_cast<int8_t>(inc);
}

void Controls::Init(daisy::DaisySeed& hw) {
    (void)hw;

    encoder_.Init(pins::ENC_A, pins::ENC_B, pins::ENC_SW);
    param_enc_[0].Init(pins::PARAM_ENC_0_A, pins::PARAM_ENC_0_B);
    param_enc_[1].Init(pins::PARAM_ENC_1_A, pins::PARAM_ENC_1_B);
    param_enc_[2].Init(pins::PARAM_ENC_2_A, pins::PARAM_ENC_2_B);
    param_enc_[3].Init(pins::PARAM_ENC_3_A, pins::PARAM_ENC_3_B);
    sw_bypass_.Init(pins::SW_BYPASS);
    sw_tap_.Init(pins::SW_TAP);

    daisy::TimerHandle::Config tim_cfg;
    tim_cfg.periph     = daisy::TimerHandle::Config::Peripheral::TIM_3;
    tim_cfg.dir        = daisy::TimerHandle::Config::CounterDir::UP;
    tim_cfg.period     = 1999;
    tim_cfg.enable_irq = true;
    enc_timer_.Init(tim_cfg);
    enc_timer_.SetPrescaler(119);
    enc_timer_.SetCallback(EncoderIsrCallback, this);
    enc_timer_.Start();

    // Expression pedal ADC (single channel on A0)
    daisy::AdcChannelConfig adc_cfg;
    adc_cfg.InitSingle(pins::EXP_PEDAL_PIN);
    adc_.Init(&adc_cfg, 1);
    adc_.Start();
}

void Controls::Poll() {
    int param_delta[4];
    int mode_delta;
    __disable_irq();
    for (int i = 0; i < 4; ++i) {
        param_delta[i] = isr_delta_[i];
        isr_delta_[i]  = 0;
    }
    mode_delta = isr_mode_delta_;
    isr_mode_delta_ = 0;
    __enable_irq();

    sw_bypass_.Debounce();
    sw_tap_.Debounce();

    for (int i = 0; i < 4; ++i)
        state_.param_encoder_increment[i] = param_delta[i];

    state_.mode_encoder_increment = mode_delta;
    state_.mode_encoder_pressed   = encoder_.FallingEdge();
    state_.mode_encoder_held      = encoder_.Pressed();
    state_.bypass_pressed  = sw_bypass_.RisingEdge();
    state_.tap_pressed     = sw_tap_.RisingEdge();
    state_.tap_released    = sw_tap_.FallingEdge();
    state_.tap_held        = sw_tap_.Pressed();
    state_.tap_held_ms     = static_cast<uint32_t>(sw_tap_.TimeHeldMs());
    state_.bypass_held     = sw_bypass_.Pressed();

    // Expression pedal: one-pole IIR smooth (same coefficient as pot smoothing)
    const float raw_expr = adc_.GetFloat(0);
    expr_pedal_ += POT_SMOOTH * (raw_expr - expr_pedal_);
}

} // namespace pedal
```

- [ ] **Step 4: Add `expr_pedal` field to `src/params/param_set.h`**

Add one line after `float level`:

```cpp
struct ParamSet {
    float speed;
    float depth;
    float mix;
    float tone;
    float p1;
    float p2;
    float level;
    float expr_pedal = 0.5f;  // expression pedal position [0=heel, 1=toe]; 0.5 when unplugged

    float get(ParamId id) const;
    static ParamSet make_default();
};
```

- [ ] **Step 5: Populate `expr_pedal` in `src/main.cpp`**

Line 311 currently reads:
```cpp
        const pedal::ParamSet params = BuildParams(param_edit, current_mode, speed_override);
        audio_engine.SetParams(params);
```

Change `const` → non-const and add the assignment on the line between them:

```cpp
        pedal::ParamSet params = BuildParams(param_edit, current_mode, speed_override);
        params.expr_pedal = controls.GetExprPedal();
        audio_engine.SetParams(params);
```

- [ ] **Step 6: Build to verify**

```bash
cd /Users/bbalazs/daisy/modulation && make -j4 2>&1 | tail -8
```

Expected: clean build, binary size ≤ 128 KB. Any error here is most likely a missing `#include "daisy_seed.h"` or an `AdcHandle` API mismatch — check the libDaisy version in `third_party/libDaisy/` if the `InitSingle` call fails.

- [ ] **Step 7: Commit**

```bash
cd /Users/bbalazs/daisy/modulation
git add src/config/pin_map.h src/hardware/controls.h src/hardware/controls.cpp \
        src/params/param_set.h src/main.cpp
git commit -m "feat: expression pedal ADC input — controls, ParamSet, main loop"
```

---

## Task 2: WahMode

**Files:**
- Create: `src/modes/wah_mode.h`
- Create: `src/modes/wah_mode.cpp`
- Modify: `src/config/constants.h`
- Modify: `src/config/mod_mode_id.h`
- Modify: `src/modes/mode_registry.cpp`
- Modify: `Makefile`

WahMode combines expression-pedal wah, envelope auto-wah, and LFO rhythmic wah in a single mode. P2 selects the control source. All three share the same SVF bandpass core. Follows the `FilterMode` pattern exactly — expensive `SetFreq()` (contains `tanf()`) goes in `Prepare()`, per-sample work stays in `Process()`.

- [ ] **Step 1: Create `src/modes/wah_mode.h`**

```cpp
#pragma once
#include "mod_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/svf.h"
#include "../dsp/envelope_follower.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

/// Resonant bandpass filter with three control sources selected by P2:
///   P2 < 0.33  → expression pedal (toe-down = filter open)
///   P2 < 0.66  → envelope follower (louder = filter opens)
///   P2 >= 0.66 → LFO sweep (rhythmic wah)
///
/// tone  : base cutoff center 200–3000 Hz (log)
/// depth : sweep range above base (0–2500 Hz)
/// p1    : resonance Q (0.5–15)
/// speed : LFO rate Hz (LFO mode) / envelope attack scale 2–100 ms (env mode)
class WahMode : public ModMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const ParamSet& params) override;
    StereoFrame Process(StereoFrame input, const ParamSet& params) override;
    const char* Name() const override { return "Wah"; }

private:
    Lfo              lfo_;
    Svf              svf_;
    EnvelopeFollower env_;
    DcBlocker        dc_;

    float base_hz_     = 800.0f;   // cached in Prepare(); used in Process() for LFO/env
    float depth_hz_    = 0.0f;     // cached in Prepare()
    float env_cutoff_  = 800.0f;   // envelope mode: updated each Process(), applied next Prepare()
    int   source_      = 0;        // 0=expr, 1=envelope, 2=LFO
};

} // namespace pedal
```

- [ ] **Step 2: Create `src/modes/wah_mode.cpp`**

```cpp
#include "wah_mode.h"
#include "../config/constants.h"

namespace pedal {

static constexpr float kWahBaseMin  = 200.0f;
static constexpr float kWahBaseMax  = 3000.0f;
static constexpr float kWahDepthMax = 2500.0f;
static constexpr float kWahFreqMax  = 8000.0f;

void WahMode::Init() {
    Reset();
}

void WahMode::Reset() {
    lfo_.Init(1.0f, LfoWave::Sine);
    svf_.Reset();
    env_.Init(5.0f, 80.0f);
    dc_.Init();
    base_hz_    = 800.0f;
    depth_hz_   = 0.0f;
    env_cutoff_ = 800.0f;
    source_     = 0;
}

void WahMode::Prepare(const ParamSet& params) {
    // Source selection: three equal thirds of P2
    if      (params.p2 < 0.33f) source_ = 0;
    else if (params.p2 < 0.66f) source_ = 1;
    else                         source_ = 2;

    // Cache base + depth for per-sample use in Process()
    base_hz_  = kWahBaseMin + params.tone * (kWahBaseMax - kWahBaseMin);
    depth_hz_ = params.depth * kWahDepthMax;

    // Resonance Q: P1 [0,1] → [0.5, 15]
    svf_.SetQ(0.5f + params.p1 * 14.5f);

    if (source_ == 0) {
        // Expression pedal: block-rate value, apply immediately
        float cutoff = base_hz_ + depth_hz_ * params.expr_pedal;
        if (cutoff > kWahFreqMax) cutoff = kWahFreqMax;
        if (cutoff < 10.0f)       cutoff = 10.0f;
        svf_.SetFreq(cutoff);
    } else if (source_ == 1) {
        // Envelope: apply cutoff computed during previous block's Process()
        // Scale attack from speed: 0→2ms, 1→100ms
        env_.SetAttack(2.0f + params.speed * 98.0f);
        svf_.SetFreq(env_cutoff_);
    } else {
        // LFO: SetFreq called per-sample in Process() for smooth sweep
        lfo_.SetRate(params.speed);
    }
}

StereoFrame WahMode::Process(StereoFrame input, const ParamSet& /*params*/) {
    const float in = input.mono();

    if (source_ == 1) {
        // Envelope: compute per-sample, store for next Prepare()
        const float env_val = env_.Process(in);  // [0, 1]
        float cutoff = base_hz_ + depth_hz_ * env_val;
        if (cutoff > kWahFreqMax) cutoff = kWahFreqMax;
        if (cutoff < 10.0f)       cutoff = 10.0f;
        env_cutoff_ = cutoff;
    } else if (source_ == 2) {
        // LFO: sweep per-sample for smooth motion
        const float ctrl = 0.5f + 0.5f * lfo_.Process();  // [0, 1]
        float cutoff = base_hz_ + depth_hz_ * ctrl;
        if (cutoff > kWahFreqMax) cutoff = kWahFreqMax;
        if (cutoff < 10.0f)       cutoff = 10.0f;
        svf_.SetFreq(cutoff);
    }
    // source_ == 0 (expr pedal): SetFreq already applied in Prepare()

    svf_.Process(in);
    float wet = svf_.bp();

    // Soft-clip high-Q resonance peaks
    if (wet >  1.0f) wet =  1.0f;
    if (wet < -1.0f) wet = -1.0f;

    wet = dc_.Process(wet);
    return {wet, wet};
}

} // namespace pedal
```

- [ ] **Step 3: Bump `NUM_MODES` in `src/config/constants.h`**

Change:
```cpp
constexpr int NUM_MODES = 12;
```
To:
```cpp
constexpr int NUM_MODES = 13;
```

- [ ] **Step 4: Add `Wah` to `src/config/mod_mode_id.h`**

```cpp
enum class ModModeId : uint8_t {
    Chorus     = 0,
    Flanger    = 1,
    Rotary     = 2,
    Vibe       = 3,
    Phaser     = 4,
    Filter     = 5,
    Formant    = 6,
    VintTrem   = 7,
    PattTrem   = 8,
    AutoSwell  = 9,
    Destroyer  = 10,
    Quadrature = 11,
    Wah        = 12,
    COUNT      = 13,
};
```

- [ ] **Step 5: Register `WahMode` in `src/modes/mode_registry.cpp`**

Add `#include "wah_mode.h"` after the last existing include. Add the static instance and registration:

```cpp
#include "mode_registry.h"
#include "chorus_mode.h"
#include "flanger_mode.h"
#include "rotary_mode.h"
#include "vibe_mode.h"
#include "phaser_mode.h"
#include "filter_mode.h"
#include "vintage_trem_mode.h"
#include "formant_mode.h"
#include "pattern_trem_mode.h"
#include "auto_swell_mode.h"
#include "destroyer_mode.h"
#include "quadrature_mode.h"
#include "wah_mode.h"

namespace pedal {

static ChorusMode      s_chorus;
static PhaserMode      s_phaser;
static VintageTremMode s_vint_trem;
static FlangerMode     s_flanger;
static RotaryMode      s_rotary;
static VibeMode        s_vibe;
static FilterMode      s_filter;
static FormantMode     s_formant;
static PatternTremMode s_patt_trem;
static AutoSwellMode   s_autoswell;
static DestroyerMode   s_destroyer;
static QuadratureMode  s_quadrature;
static WahMode         s_wah;

void ModeRegistry::Init() {
    modes_[static_cast<uint8_t>(ModModeId::Chorus)]     = &s_chorus;
    modes_[static_cast<uint8_t>(ModModeId::Flanger)]    = &s_flanger;
    modes_[static_cast<uint8_t>(ModModeId::Rotary)]     = &s_rotary;
    modes_[static_cast<uint8_t>(ModModeId::Vibe)]       = &s_vibe;
    modes_[static_cast<uint8_t>(ModModeId::Phaser)]     = &s_phaser;
    modes_[static_cast<uint8_t>(ModModeId::Filter)]     = &s_filter;
    modes_[static_cast<uint8_t>(ModModeId::Formant)]    = &s_formant;
    modes_[static_cast<uint8_t>(ModModeId::VintTrem)]   = &s_vint_trem;
    modes_[static_cast<uint8_t>(ModModeId::PattTrem)]   = &s_patt_trem;
    modes_[static_cast<uint8_t>(ModModeId::AutoSwell)]  = &s_autoswell;
    modes_[static_cast<uint8_t>(ModModeId::Destroyer)]  = &s_destroyer;
    modes_[static_cast<uint8_t>(ModModeId::Quadrature)] = &s_quadrature;
    modes_[static_cast<uint8_t>(ModModeId::Wah)]        = &s_wah;

    for (int i = 0; i < NUM_MODES; ++i)
        modes_[i]->Init();
}

ModMode* ModeRegistry::get(ModModeId id) {
    return modes_[static_cast<uint8_t>(id)];
}

void ModeRegistry::Reset(ModModeId id) {
    modes_[static_cast<uint8_t>(id)]->Reset();
}

} // namespace pedal
```

- [ ] **Step 6: Add `wah_mode.cpp` to `Makefile`**

In `CPP_SOURCES`, add after `quadrature_mode.cpp`:

```makefile
    src/modes/wah_mode.cpp \
```

The `CPP_SOURCES` block should end with:
```makefile
    src/modes/quadrature_mode.cpp \
    src/modes/wah_mode.cpp \
    src/modes/mode_registry.cpp \
```

- [ ] **Step 7: Build and check flash budget**

```bash
cd /Users/bbalazs/daisy/modulation && make -j4 2>&1 | tail -10
```

Expected: clean build. The flash percentage line should read ≤ 95% (budget warning is anything approaching 100%).

- [ ] **Step 8: Commit**

```bash
cd /Users/bbalazs/daisy/modulation
git add src/modes/wah_mode.h src/modes/wah_mode.cpp \
        src/config/constants.h src/config/mod_mode_id.h \
        src/modes/mode_registry.cpp Makefile
git commit -m "feat: WahMode — expression pedal, envelope, and LFO bandpass filter"
```

---

## Task 3: StereoWidener

**Files:**
- Create: `src/modes/stereo_widener.h`
- Create: `src/modes/stereo_widener.cpp`
- Modify: `src/config/constants.h`
- Modify: `src/config/mod_mode_id.h`
- Modify: `src/modes/mode_registry.cpp`
- Modify: `Makefile`

StereoWidener builds a stereo image from mono guitar input using Haas psychoacoustics. L gets a minimal 1 ms delay; R gets a tone-controlled 2–25 ms Haas offset. Both paths are swept by the same LFO at 90° quadrature — the same technique RotaryMode uses for its horn/drum quadrature pairs. Delay buffers are plain static arrays (no `DSY_SDRAM_BSS`) so they land in DTCMRAM.

- [ ] **Step 1: Create `src/modes/stereo_widener.h`**

```cpp
#pragma once
#include "mod_mode.h"
#include "../dsp/delay_line_sdram.h"
#include "../dsp/lfo.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

/// Stereo widener using Haas psychoacoustics + quadrature LFO modulation.
/// Builds a stereo image from a mono input:
///   L: 1 ms fixed delay, LFO at 0°
///   R: tone-controlled Haas offset (2–25 ms), same LFO at 90°
///
/// tone  : Haas offset 2–25 ms (stereo width)
/// speed : LFO rate 0.05–5 Hz
/// depth : LFO modulation amount (0 = static spread, 1 = full sweep)
/// p1    : LFO waveform (0–0.33=Sine, 0.33–0.66=Triangle, 0.66–1.0=SmoothRandom)
class StereoWidener : public ModMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const ParamSet& params) override;
    StereoFrame Process(StereoFrame input, const ParamSet& params) override;
    const char* Name() const override { return "Width"; }

private:
    Lfo       lfo_l_;   // L channel LFO at 0°
    Lfo       lfo_r_;   // R channel LFO at 90°
    DcBlocker dc_l_;
    DcBlocker dc_r_;

    float haas_samps_  = 480.0f;  // cached in Prepare() from tone param
    float depth_samps_ = 0.0f;    // LFO swing in samples, cached in Prepare()
};

} // namespace pedal
```

- [ ] **Step 2: Create `src/modes/stereo_widener.cpp`**

```cpp
#include "stereo_widener.h"
#include "../config/constants.h"

namespace pedal {

// Buffer sizes in DTCMRAM (no DSY_SDRAM_BSS — short buffers, ~5 KB total)
static constexpr size_t kBufL = 48;    // 1 ms at 48 kHz
static constexpr size_t kBufR = 1200;  // 25 ms at 48 kHz

static float          s_buf_l[kBufL];
static float          s_buf_r[kBufR];
static DelayLineSdram s_delay_l;
static DelayLineSdram s_delay_r;

static constexpr float kHalfPi      = 1.57079632679489661923f;
static constexpr float kHaasMinMs   = 2.0f;
static constexpr float kHaasMaxMs   = 25.0f;
static constexpr float kHaasMinSamp = kHaasMinMs  * 0.001f * SAMPLE_RATE;  // ~96 samples
static constexpr float kHaasMaxSamp = kHaasMaxMs  * 0.001f * SAMPLE_RATE;  // ~1200 samples
static constexpr float kLfoMinHz    = 0.05f;
static constexpr float kLfoMaxHz    = 5.0f;

void StereoWidener::Init() {
    s_delay_l.Init(s_buf_l, kBufL);   // file-scope kBufL = 48
    s_delay_r.Init(s_buf_r, kBufR);   // file-scope kBufR = 1200
    lfo_l_.Init(0.5f, LfoWave::Sine);
    lfo_r_.Init(0.5f, LfoWave::Sine);
    lfo_r_.SetPhaseOffset(kHalfPi);
    lfo_r_.Reset();
    dc_l_.Init();
    dc_r_.Init();
    haas_samps_  = (kHaasMinSamp + kHaasMaxSamp) * 0.5f;
    depth_samps_ = 0.0f;
}

void StereoWidener::Reset() {
    s_delay_l.Reset();
    s_delay_r.Reset();
    lfo_l_.Init(0.5f, LfoWave::Sine);
    lfo_r_.Init(0.5f, LfoWave::Sine);
    lfo_r_.SetPhaseOffset(kHalfPi);
    lfo_r_.Reset();
    dc_l_.Init();
    dc_r_.Init();
    haas_samps_  = (kHaasMinSamp + kHaasMaxSamp) * 0.5f;
    depth_samps_ = 0.0f;
}

void StereoWidener::Prepare(const ParamSet& params) {
    // LFO rate
    const float rate = kLfoMinHz + params.speed * (kLfoMaxHz - kLfoMinHz);
    lfo_l_.SetRate(rate);
    lfo_r_.SetRate(rate);

    // LFO waveform from P1: three equal thirds
    LfoWave wave;
    if      (params.p1 < 0.33f) wave = LfoWave::Sine;
    else if (params.p1 < 0.66f) wave = LfoWave::Triangle;
    else                         wave = LfoWave::SmoothRandom;
    lfo_l_.SetWave(wave);
    lfo_r_.SetWave(wave);

    // Haas offset: tone [0,1] → kHaasMinSamp..kHaasMaxSamp (log-like via linear for simplicity)
    haas_samps_ = kHaasMinSamp + params.tone * (kHaasMaxSamp - kHaasMinSamp);

    // LFO swing: depth * 30% of Haas offset (keeps delay positive throughout modulation)
    depth_samps_ = params.depth * haas_samps_ * 0.3f;
}

StereoFrame StereoWidener::Process(StereoFrame input, const ParamSet& /*params*/) {
    const float in    = input.mono();
    const float mod_l = lfo_l_.Process();   // [-1, +1]
    const float mod_r = lfo_r_.Process();   // [-1, +1] at 90°

    // L: minimal 1 ms base delay ± LFO swing; clamp to [2, 46] for Hermite 4-tap safety
    float dl = 24.0f + mod_l * (depth_samps_ * 0.5f);   // centre at 24 samps (0.5 ms)
    if (dl <  2.0f) dl =  2.0f;
    if (dl > 46.0f) dl = 46.0f;

    // R: Haas offset ± LFO swing; clamp to [2, 1198]
    float dr = haas_samps_ + mod_r * depth_samps_;
    if (dr <    2.0f) dr =    2.0f;
    if (dr > 1198.0f) dr = 1198.0f;

    s_delay_l.Write(in);
    s_delay_r.Write(in);

    const float out_l = dc_l_.Process(s_delay_l.ReadAt(dl));
    const float out_r = dc_r_.Process(s_delay_r.ReadAt(dr));

    return {out_l, out_r};
}

} // namespace pedal
```

- [ ] **Step 3: Bump `NUM_MODES` in `src/config/constants.h`**

Change:
```cpp
constexpr int NUM_MODES = 13;
```
To:
```cpp
constexpr int NUM_MODES = 14;
```

- [ ] **Step 4: Add `StereoWidener` to `src/config/mod_mode_id.h`**

```cpp
enum class ModModeId : uint8_t {
    Chorus        = 0,
    Flanger       = 1,
    Rotary        = 2,
    Vibe          = 3,
    Phaser        = 4,
    Filter        = 5,
    Formant       = 6,
    VintTrem      = 7,
    PattTrem      = 8,
    AutoSwell     = 9,
    Destroyer     = 10,
    Quadrature    = 11,
    Wah           = 12,
    StereoWidener = 13,
    COUNT         = 14,
};
```

- [ ] **Step 5: Register `StereoWidener` in `src/modes/mode_registry.cpp`**

```cpp
#include "mode_registry.h"
#include "chorus_mode.h"
#include "flanger_mode.h"
#include "rotary_mode.h"
#include "vibe_mode.h"
#include "phaser_mode.h"
#include "filter_mode.h"
#include "vintage_trem_mode.h"
#include "formant_mode.h"
#include "pattern_trem_mode.h"
#include "auto_swell_mode.h"
#include "destroyer_mode.h"
#include "quadrature_mode.h"
#include "wah_mode.h"
#include "stereo_widener.h"

namespace pedal {

static ChorusMode      s_chorus;
static PhaserMode      s_phaser;
static VintageTremMode s_vint_trem;
static FlangerMode     s_flanger;
static RotaryMode      s_rotary;
static VibeMode        s_vibe;
static FilterMode      s_filter;
static FormantMode     s_formant;
static PatternTremMode s_patt_trem;
static AutoSwellMode   s_autoswell;
static DestroyerMode   s_destroyer;
static QuadratureMode  s_quadrature;
static WahMode         s_wah;
static StereoWidener   s_stereo_widener;

void ModeRegistry::Init() {
    modes_[static_cast<uint8_t>(ModModeId::Chorus)]        = &s_chorus;
    modes_[static_cast<uint8_t>(ModModeId::Flanger)]       = &s_flanger;
    modes_[static_cast<uint8_t>(ModModeId::Rotary)]        = &s_rotary;
    modes_[static_cast<uint8_t>(ModModeId::Vibe)]          = &s_vibe;
    modes_[static_cast<uint8_t>(ModModeId::Phaser)]        = &s_phaser;
    modes_[static_cast<uint8_t>(ModModeId::Filter)]        = &s_filter;
    modes_[static_cast<uint8_t>(ModModeId::Formant)]       = &s_formant;
    modes_[static_cast<uint8_t>(ModModeId::VintTrem)]      = &s_vint_trem;
    modes_[static_cast<uint8_t>(ModModeId::PattTrem)]      = &s_patt_trem;
    modes_[static_cast<uint8_t>(ModModeId::AutoSwell)]     = &s_autoswell;
    modes_[static_cast<uint8_t>(ModModeId::Destroyer)]     = &s_destroyer;
    modes_[static_cast<uint8_t>(ModModeId::Quadrature)]    = &s_quadrature;
    modes_[static_cast<uint8_t>(ModModeId::Wah)]           = &s_wah;
    modes_[static_cast<uint8_t>(ModModeId::StereoWidener)] = &s_stereo_widener;

    for (int i = 0; i < NUM_MODES; ++i)
        modes_[i]->Init();
}

ModMode* ModeRegistry::get(ModModeId id) {
    return modes_[static_cast<uint8_t>(id)];
}

void ModeRegistry::Reset(ModModeId id) {
    modes_[static_cast<uint8_t>(id)]->Reset();
}

} // namespace pedal
```

- [ ] **Step 6: Add `stereo_widener.cpp` to `Makefile`**

In `CPP_SOURCES`, add after `wah_mode.cpp`:

```makefile
    src/modes/wah_mode.cpp \
    src/modes/stereo_widener.cpp \
    src/modes/mode_registry.cpp \
```

- [ ] **Step 7: Build and check flash budget**

```bash
cd /Users/bbalazs/daisy/modulation && make -j4 2>&1 | tail -10
```

Expected: clean build. The flash line should show ≤ ~93%. If it exceeds 128 KB, check that both `s_buf_l` and `s_buf_r` do **not** have `DSY_SDRAM_BSS` (they should not — they're plain static arrays targeting DTCMRAM).

- [ ] **Step 8: Commit**

```bash
cd /Users/bbalazs/daisy/modulation
git add src/modes/stereo_widener.h src/modes/stereo_widener.cpp \
        src/config/constants.h src/config/mod_mode_id.h \
        src/modes/mode_registry.cpp Makefile
git commit -m "feat: StereoWidener mode — Haas spread + quadrature LFO"
```

---

## Verification

From the spec (`docs/superpowers/specs/2026-05-16-dsp-new-capabilities-phase-c-design.md`):

| Test | Mode | Settings | Pass criterion |
|------|------|----------|----------------|
| Expr pedal routing | WahMode | P2 < 0.33 | Filter sweeps smoothly; no zipper noise |
| Auto-wah | WahMode | P2 0.33–0.66 | Harder playing opens filter; light playing closes it |
| Rhythmic wah | WahMode | P2 > 0.66, speed 1 Hz | Audible 1 Hz sweep |
| Wah resonance | WahMode | p1 ≥ 0.8 | Pronounced quack/peak at high Q |
| Stereo spread | StereoWidener | tone 50%, depth 0 | Clear L/R separation on headphones |
| Stereo LFO | StereoWidener | depth 100%, speed 0.3 Hz | Smooth image movement, not choppy |
| Mono compat | StereoWidener | mix 100%, tone any | Summed to mono: usable (some comb is expected) |
| Flash budget | — | `make -j4` | Binary ≤ 128 KB |
| Regression | all 14 modes | cycle through all | No click, dropout, or crash on mode switch |

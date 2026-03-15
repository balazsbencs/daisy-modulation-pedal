#include "chorus_mode.h"
#include "../config/constants.h"
#include <cmath>

namespace pedal {

// Chorus needs a longer delay than MAX_MOD_DELAY_SAMPLES (25ms).
// Use 50ms = 2400 samples; SDRAM buffer cost is minimal.
static constexpr size_t kChorusBufSize = 2400;
static float DSY_SDRAM_BSS s_chorus_buf[kChorusBufSize];
static DelayLineSdram s_chorus_line;

static constexpr float PI_2_3 = 2.094395f; // 2π/3 = 120°

void ChorusMode::Init() {
    s_chorus_line.Init(s_chorus_buf, kChorusBufSize);
    for (int i = 0; i < 3; ++i) {
        lfo_[i].Init(0.5f, LfoWave::Sine);
        lfo_[i].SetPhaseOffset(static_cast<float>(i) * PI_2_3);
    }
    dc_.Init();
}

void ChorusMode::Reset() {
    s_chorus_line.Reset();
    for (auto& l : lfo_) l.Reset();
    dc_.Init();
    bbd_.Reset();
    rand_ = 12345;
}

void ChorusMode::Prepare(const ParamSet& params) {
    // Sub-mode: 0=dBucket, 1=Multi, 2=Vibrato, 3=Detune, 4=Digital
    sub_mode_ = static_cast<int>(params.p2 * 4.999f);

    for (auto& l : lfo_) l.SetRate(params.speed);

    // Base delay: p1 maps 1ms..20ms → 48..960 samples
    const float base_samps = 48.0f + params.p1 * 912.0f;

    // LFO depth: params.depth * 480 samples = ±10ms variation
    const float mod_depth = params.depth * 480.0f;

    if (sub_mode_ == 3) {
        // Detune: two static offsets with no LFO — creates a fixed pitch shift
        delays_[0] = base_samps - mod_depth * 0.3f;
        delays_[1] = base_samps + mod_depth * 0.3f;
        delays_[2] = base_samps;
    } else if (sub_mode_ == 1) {
        // Multi: 3 taps with 120° LFO phase offset
        for (int i = 0; i < 3; ++i) {
            const float lfo_val = lfo_[i].PrepareBlock();
            delays_[i] = base_samps + mod_depth * lfo_val;
            if (delays_[i] < 1.0f) delays_[i] = 1.0f;
            if (delays_[i] >= static_cast<float>(kChorusBufSize - 2))
                delays_[i] = static_cast<float>(kChorusBufSize - 2);
        }
    } else {
        // Single-voice: use lfo_[0]
        const float lfo_val = lfo_[0].PrepareBlock();
        delays_[0] = base_samps + mod_depth * lfo_val;
        if (delays_[0] < 1.0f) delays_[0] = 1.0f;
        if (delays_[0] >= static_cast<float>(kChorusBufSize - 2))
            delays_[0] = static_cast<float>(kChorusBufSize - 2);
    }
}

StereoFrame ChorusMode::Process(float input, const ParamSet& params) {
    float write_in = input;

    // dBucket pre-coloration
    if (sub_mode_ == 0) {
        write_in = bbd_.Process(input, 0.15f, rand_);
    }

    s_chorus_line.Write(write_in);

    float wet_l, wet_r;

    if (sub_mode_ == 1) {
        // Multi: average 3 taps, pan L/R with tap 2 and 3
        const float t0 = s_chorus_line.ReadAt(delays_[0]);
        const float t1 = s_chorus_line.ReadAt(delays_[1]);
        const float t2 = s_chorus_line.ReadAt(delays_[2]);
        wet_l = (t0 + t1) * 0.5f;
        wet_r = (t0 + t2) * 0.5f;
    } else if (sub_mode_ == 3) {
        // Detune: L=tap0, R=tap1 (stereo detuning)
        wet_l = s_chorus_line.ReadAt(delays_[0]);
        wet_r = s_chorus_line.ReadAt(delays_[1]);
    } else {
        // Single-voice (dBucket, Vibrato, Digital)
        float wet = s_chorus_line.ReadAt(delays_[0]);
        if (sub_mode_ == 0) {
            wet = bbd_.Deemphasis(wet);  // dBucket post-coloration
        }
        wet_l = wet_r = wet;
    }

    wet_l = dc_.Process(wet_l);
    wet_r = dc_.Process(wet_r);  // Note: uses same DC blocker — acceptable for mono input

    return {wet_l, wet_r};
}

} // namespace pedal

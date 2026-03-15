#include "phaser_mode.h"
#include "../config/constants.h"

namespace pedal {

static const int kStageCounts[] = {2, 4, 6, 8, 12, 16, 16};  // last = Barber Pole (same count)

void PhaserMode::Init() {
    Reset();
}

void PhaserMode::Reset() {
    lfo_.Init(0.5f, LfoWave::Sine);
    for (auto& s : stages_) s.Reset();
    dc_.Init();
    lfo_coeff_ = 0.0f;
    feedback_  = 0.0f;
    num_stages_ = 4;
}

void PhaserMode::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.speed);

    // Sub-mode from p2: 0..6 → stage counts
    const int sub = static_cast<int>(params.p2 * 6.999f);
    num_stages_ = kStageCounts[sub];

    // LFO output sweeps allpass coefficient
    // tone biases the center: 0=low center, 1=high center
    // depth controls sweep range
    const float lfo_val = lfo_.PrepareBlock(); // -1..+1
    const float center  = 0.1f + params.tone * 0.7f;   // 0.1..0.8
    lfo_coeff_ = center + params.depth * 0.4f * lfo_val;
    if (lfo_coeff_ >  0.99f) lfo_coeff_ =  0.99f;
    if (lfo_coeff_ < -0.99f) lfo_coeff_ = -0.99f;
}

StereoFrame PhaserMode::Process(float input, const ParamSet& params) {
    // Apply feedback (regen), clamped to avoid instability
    const float regen = params.p1 * 0.95f;
    float x = input + feedback_ * regen;

    // Set coefficient for all active stages and process
    for (int i = 0; i < num_stages_; ++i) {
        stages_[i].SetCoeff(lfo_coeff_);
        x = stages_[i].Process(x);
    }

    x = dc_.Process(x);
    feedback_ = x;
    return {x, x};
}

} // namespace pedal

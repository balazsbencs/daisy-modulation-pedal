#include "vibe_mode.h"
#include "../config/constants.h"
#include <cmath>

namespace pedal {

void VibeMode::Init() {
    Reset();
}

void VibeMode::Reset() {
    lfo_.Init(1.0f, LfoWave::Sine);
    for (auto& s : stages_) s.Reset();
    dc_.Init();
    lfo_coeff_ = 0.0f;
    am_gain_   = 1.0f;
    feedback_  = 0.0f;
}

void VibeMode::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.speed);
    const float raw_lfo = lfo_.PrepareBlock(); // -1..+1

    // LDR nonlinearity: asymmetric curve (UniVibe photocell response).
    // Compress positive half, expand negative → brighter attack, slower decay.
    float ldr;
    if (raw_lfo >= 0.0f) {
        ldr = raw_lfo * raw_lfo;  // 0..+1 → 0..+1 (compressed)
    } else {
        ldr = -(raw_lfo * raw_lfo); // 0..-1 → 0..-1 (same but sign)
    }

    // Allpass coefficient — must be negative to place notches in the audio band.
    // For H(z)=(a+z⁻¹)/(1+a·z⁻¹), the N-stage comb notch for dry+wet is in the
    // audio band only for a < 0. Positive a pushes notches above 10 kHz.
    // Center −0.70 → notch ≈ 1.5 kHz; depth sweeps ±0.25 → 800 Hz – 4 kHz.
    const float sweep = params.depth * 0.25f;
    lfo_coeff_ = -0.70f + sweep * ldr;  // −0.95 .. −0.45 at depth=1

    // Amplitude modulation: slight gain reduction on positive LFO half
    // (characteristic UniVibe "throb")
    am_gain_ = 1.0f - params.depth * 0.3f * (0.5f + 0.5f * ldr);
    if (am_gain_ < 0.1f) am_gain_ = 0.1f;
}

StereoFrame VibeMode::Process(float input, const ParamSet& params) {
    // Regen feedback
    const float regen = params.p1 * 0.7f;
    float x = input + feedback_ * regen;

    // 4 allpass stages with slight per-stage coefficient offsets (non-uniform)
    const float offsets[4] = {0.0f, 0.1f, -0.1f, 0.05f};
    for (int i = 0; i < kStages; ++i) {
        float c = lfo_coeff_ + offsets[i] * params.depth;
        if (c > -0.01f) c = -0.01f;  // keep negative so notches stay in audio band
        if (c < -0.99f) c = -0.99f;
        stages_[i].SetCoeff(c);
        x = stages_[i].Process(x);
    }

    // Amplitude modulation
    x *= am_gain_;

    // P2=1 → pure vibrato (remove dry from mix externally, but here just preserve wet)
    x = dc_.Process(x);
    feedback_ = x;
    return {x, x};
}

} // namespace pedal

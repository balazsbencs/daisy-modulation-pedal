#include "filter_mode.h"
#include "../config/constants.h"
#include <cmath>

namespace pedal {

void FilterMode::Init() {
    Reset();
}

void FilterMode::Reset() {
    lfo_.Init(1.0f, LfoWave::Sine);
    svf_.Reset();
    env_.Init(5.0f, 80.0f);
    dc_.Init();
    mod_val_            = 0.5f;
    envelope_cutoff_hz_ = 1000.0f;
    use_env_            = false;
    env_inv_            = false;
}

void FilterMode::Prepare(const ParamSet& params) {
    // Waveshape from P2: 0=Sine, 1=Tri, 2=Sq, 3=RampUp, 4=RampDown, 5=S&H, 6=Env+, 7=Env-
    const int shape = static_cast<int>(params.p2 * 7.999f);

    use_env_ = (shape >= 6);
    env_inv_ = (shape == 7);

    if (!use_env_) {
        static const LfoWave kWaves[6] = {
            LfoWave::Sine, LfoWave::Triangle, LfoWave::Square,
            LfoWave::RampUp, LfoWave::RampDown, LfoWave::SampleAndHold
        };
        lfo_.SetWave(kWaves[shape]);
        lfo_.SetRate(params.speed);
        const float lfo_val = lfo_.PrepareBlock(); // -1..+1
        mod_val_ = 0.5f + 0.5f * lfo_val;          // 0..1
    }
    // Env modes: mod_val_ computed per-sample in Process()

    // Filter type from tone: <0.4=LP, 0.4-0.6=Wah(BP), >0.6=HP
    if      (params.tone < 0.4f) ftype_ = 0;
    else if (params.tone > 0.6f) ftype_ = 2;
    else                          ftype_ = 1;

    // Resonance Q from P1 (0..1 → 0.5..20)
    const float q = 0.5f + params.p1 * 19.5f;
    svf_.SetQ(q);

    if (!use_env_) {
        // Base cutoff from tone center (80Hz..12kHz log-ish) + depth sweep
        const float base_hz = 80.0f + params.tone * 11920.0f;
        const float sweep   = base_hz * params.depth * mod_val_;
        const float cutoff  = 80.0f + sweep;
        svf_.SetFreq(cutoff > 12000.0f ? 12000.0f : cutoff);
    } else {
        // Apply envelope cutoff computed during the previous block's Process() calls.
        // This moves tanf() out of the per-sample ISR hot path.
        svf_.SetFreq(envelope_cutoff_hz_);
    }
}

StereoFrame FilterMode::Process(float input, const ParamSet& params) {
    if (use_env_) {
        // Envelope follower modulates cutoff.
        // Store the desired cutoff for Prepare() to apply via SetFreq() (block-rate),
        // keeping tanf() out of the per-sample ISR hot path.
        float env_val = env_.Process(input);  // 0..1
        if (env_inv_) env_val = 1.0f - env_val;
        const float base_hz = 80.0f + params.tone * 2000.0f; // lower base for auto-wah
        const float cutoff  = base_hz + env_val * params.depth * 3000.0f;
        envelope_cutoff_hz_ = cutoff > 8000.0f ? 8000.0f : cutoff;
    }

    svf_.Process(input);

    float wet;
    switch (ftype_) {
        case 0:  wet = svf_.lp();    break;
        case 1:  wet = svf_.bp();    break;
        case 2:  wet = svf_.hp();    break;
        default: wet = svf_.lp();    break;
    }

    // Soft-clip to tame high-Q resonance peaks
    if (wet >  1.0f) wet =  1.0f;
    if (wet < -1.0f) wet = -1.0f;

    wet = dc_.Process(wet);
    return {wet, wet};
}

} // namespace pedal

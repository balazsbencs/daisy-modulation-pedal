#include "auto_swell_mode.h"
#include "../config/constants.h"
#include <cmath>

namespace pedal {

// Small delay buffer for optional chorus shimmer (P2)
static constexpr size_t kSwellBufSize    = 1200;
// Fixed chorus delay: 580 samples = ~12.08 ms at 48 kHz
static constexpr float  kSwellChorusDelay = 580.0f;
static float DSY_SDRAM_BSS s_swell_buf[kSwellBufSize];
static DelayLineSdram s_swell_line;

void AutoSwellMode::Init() {
    s_swell_line.Init(s_swell_buf, kSwellBufSize);
    env_.Init(5.0f, 200.0f);
    dc_.Init();
}

void AutoSwellMode::Reset() {
    s_swell_line.Reset();
    env_.Init(5.0f, 200.0f);
    dc_.Init();
    swell_gain_  = 0.0f;
    swelling_    = false;
}

void AutoSwellMode::Prepare(const ParamSet& params) {
    // Speed (0.05..10 Hz): repurpose as attack time (fast speed = short attack)
    // Map: speed 0.05 Hz → ~10 sec attack (coef very small), speed 10 Hz → ~50ms
    const float attack_ms  = 1000.0f / (params.speed * 5.0f);  // 20ms..4000ms
    const float release_ms = 50.0f + params.p1 * 1950.0f;      // 50ms..2000ms

    // One-pole IIR coefficients: α = 1 - exp(-1 / (τ * fs))
    attack_coef_  = 1.0f - expf(-1.0f / (attack_ms  * 0.001f * SAMPLE_RATE));
    release_coef_ = 1.0f - expf(-1.0f / (release_ms * 0.001f * SAMPLE_RATE));
}

StereoFrame AutoSwellMode::Process(float input, const ParamSet& params) {
    const float env_val = env_.Process(input);  // 0..1 signal level

    // When signal is present, release gain (let it drop to near-zero)
    // When signal decays, ramp gain back up (the swell)
    const float threshold = 0.05f;
    if (env_val > threshold) {
        // Input is loud: release gain quickly
        swell_gain_ += release_coef_ * (0.0f - swell_gain_);
    } else {
        // Input is quiet: slowly ramp gain up (the swell attack)
        swell_gain_ += attack_coef_ * (1.0f - swell_gain_);
    }

    float wet = input * swell_gain_ * (1.0f + params.depth);

    // Optional chorus shimmer from P2: blend in a short modulated delay
    if (params.p2 > 0.05f) {
        const float chorus_delay = kSwellChorusDelay;
        s_swell_line.Write(wet);
        s_swell_line.SetDelay(chorus_delay);
        const float chorus_wet = s_swell_line.Read();
        wet = wet * (1.0f - params.p2 * 0.3f) + chorus_wet * params.p2 * 0.3f;
    }

    wet = dc_.Process(wet);
    return {wet, wet};
}

} // namespace pedal

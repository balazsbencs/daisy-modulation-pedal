#include "auto_swell_mode.h"
#include "../config/constants.h"
#include <cmath>

namespace pedal {

// Small delay buffer for optional shimmer/doubling effect (P2)
static constexpr size_t kSwellBufSize   = 1200;
// Fixed shimmer delay: 580 samples ≈ 12 ms — creates comb/doubling, not modulated chorus
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
    // Speed (0.010..0.500 s): attack time in seconds (short = fast swell)
    const float attack_ms  = params.speed * 1000.0f;           // 10ms..500ms
    const float release_ms = 50.0f + params.p1 * 1950.0f;      // 50ms..2000ms

    // One-pole IIR coefficients: α = 1 - exp(-1 / (τ * fs))
    swell_coef_ = 1.0f - expf(-1.0f / (attack_ms  * 0.001f * SAMPLE_RATE));
    duck_coef_  = 1.0f - expf(-1.0f / (release_ms * 0.001f * SAMPLE_RATE));
}

StereoFrame AutoSwellMode::Process(float input, const ParamSet& params) {
    const float env_val = env_.Process(input);  // 0..1 signal level

    // When signal is present, release gain (let it drop to near-zero)
    // When signal decays, ramp gain back up (the swell)
    const float threshold = 0.05f;
    if (env_val > threshold) {
        // Input is loud: kill gain quickly (duck)
        swell_gain_ += duck_coef_ * (0.0f - swell_gain_);
    } else {
        // Input is quiet: slowly ramp gain up (the swell opens)
        swell_gain_ += swell_coef_ * (1.0f - swell_gain_);
    }

    // depth boosts the swell peak (+6 dB max); clamp to [-1, +1] to prevent clipping.
    float wet = input * swell_gain_ * (1.0f + params.depth);
    if (wet >  1.0f) wet =  1.0f;
    if (wet < -1.0f) wet = -1.0f;

    // Always write to delay line so buffer is primed when P2 is turned up
    s_swell_line.Write(wet);

    // Optional shimmer from P2: blend in a fixed-delay tap (12 ms doubling, not modulated)
    if (params.p2 > 0.05f) {
        s_swell_line.SetDelay(kSwellChorusDelay);
        const float chorus_wet = s_swell_line.Read();
        wet = wet * (1.0f - params.p2 * 0.3f) + chorus_wet * params.p2 * 0.3f;
    }

    wet = dc_.Process(wet);
    return {wet, wet};
}

} // namespace pedal

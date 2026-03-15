#include "quadrature_mode.h"
#include "../config/constants.h"
#include "../dsp/fast_math.h"

namespace pedal {

static constexpr float kTwoPi  = 6.28318530f;
static constexpr float kHalfPi = 1.57079633f;

// Wrap x into [0, 2π). Phase increments are small so the loop is at most once.
static float wrap2pi(float x) noexcept {
    if (x >= kTwoPi) x -= kTwoPi;
    if (x < 0.0f)    x += kTwoPi;
    return x;
}

// Cosine via fast_sin: cos(x) = sin(x + π/2).
static float fcos(float x) noexcept {
    return fast_sin(wrap2pi(x + kHalfPi));
}

// ---------------------------------------------------------------------------

void QuadratureMode::Init() {
    hilbert_.Init();
    lfo_.Init(1.0f, LfoWave::Sine);
    carrier_phase_ = 0.0f;
    phase_inc_     = 0.0f;
    sub_mode_      = 0;
}

void QuadratureMode::Reset() {
    hilbert_.Reset();
    lfo_.Init(1.0f, LfoWave::Sine);
    carrier_phase_ = 0.0f;
}

void QuadratureMode::Prepare(const ParamSet& params) {
    sub_mode_ = static_cast<int>(params.p2 * 3.999f);
    if (sub_mode_ < 0) sub_mode_ = 0;
    if (sub_mode_ > 3) sub_mode_ = 3;

    const float sr = static_cast<float>(SAMPLE_RATE);
    phase_inc_ = kTwoPi * params.speed / sr;

    if (sub_mode_ == 1) {
        lfo_.SetRate(params.speed);
    }
}

StereoFrame QuadratureMode::Process(float input, const ParamSet& params) {
    auto [re, im] = hilbert_.Process(input);

    // Advance carrier phase — FM mode varies instantaneous rate with LFO.
    if (sub_mode_ == 1) {
        const float lfo_val = lfo_.Process();  // -1..+1
        const float sr      = static_cast<float>(SAMPLE_RATE);
        const float inst_inc = kTwoPi * (params.speed + params.depth * 80.0f * lfo_val) / sr;
        carrier_phase_ = wrap2pi(carrier_phase_ + inst_inc);
    } else {
        carrier_phase_ = wrap2pi(carrier_phase_ + phase_inc_);
    }

    const float cos_c = fcos(carrier_phase_);
    const float sin_c = fast_sin(carrier_phase_);

    switch (sub_mode_) {
        case 0: {
            // AM — ring modulation with stereo rotation.
            // Left: input*cos, Right: input*-sin.
            // P1 blends between mono ring-mod (0) and full quadrature rotation (1).
            const float ring = input * cos_c;
            const float p1   = params.p1;
            return {
                ring * (1.0f - p1) + (input * cos_c) * p1,
                ring * (1.0f - p1) + (input * (-sin_c)) * p1,
            };
        }

        case 1: {
            // FM — Hilbert-based pitch vibrato.
            // The LFO sweeps the instantaneous carrier frequency, creating
            // a pitch wobble via single-sideband rotation of the analytic signal.
            const float wet = re * cos_c - im * sin_c;
            return {wet, wet};
        }

        case 2: {
            // FreqShift+ — single-sideband upward shift.
            // P1 blends shifted signal with unshifted real part.
            const float shifted = re * cos_c - im * sin_c;
            const float blend   = params.p1;
            const float out     = shifted * (1.0f - blend) + re * blend;
            return {out, out};
        }

        case 3: {
            // FreqShift- — single-sideband downward shift.
            const float shifted = re * cos_c + im * sin_c;
            const float blend   = params.p1;
            const float out     = shifted * (1.0f - blend) + re * blend;
            return {out, out};
        }

        default:
            return {input, input};
    }
}

} // namespace pedal

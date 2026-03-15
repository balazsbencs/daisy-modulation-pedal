#pragma once
#include "../config/constants.h"

namespace pedal {

/// Simple bucket-brigade device emulator.
/// Models: pre/de-emphasis LP filters, clock noise injection, subtle saturation.
/// Used by ChorusMode (dBucket sub-mode) to give a vintage CE-1/CE-2 character.
class BbdEmulator {
public:
    void Reset() {
        lp1_ = 0.0f;
        lp2_ = 0.0f;
        hp_  = 0.0f;
    }

    /// Process one sample through the BBD coloration chain.
    /// noise_amount: 0..1, clock noise injection level.
    /// Returns the colored sample (use as input to the delay line).
    float Process(float input, float noise_amount, uint32_t& rand_state) {
        // 2-pole LP emulating pre-emphasis (cutoff ~6 kHz)
        constexpr float k = 0.45f;
        lp1_ += k * (input - lp1_);
        lp2_ += k * (lp1_  - lp2_);

        // Subtle clock noise injection
        rand_state = rand_state * 1664525u + 1013904223u;
        const float noise = static_cast<float>(static_cast<int32_t>(rand_state))
                          * (1.0f / 2147483648.0f) * noise_amount * 0.002f;

        // Soft saturation (tanh approximation)
        const float x = lp2_ + noise;
        const float sat = x * (27.0f + x * x) / (27.0f + 9.0f * x * x);

        return sat;
    }

    /// Apply de-emphasis (HF boost) to the output of the delay line.
    float Deemphasis(float delayed) {
        constexpr float k = 0.45f;
        hp_ += k * (delayed - hp_);
        return delayed + (delayed - hp_) * 0.3f; // shelf boost
    }

private:
    float lp1_ = 0.0f;
    float lp2_ = 0.0f;
    float hp_  = 0.0f;
};

} // namespace pedal

#include "lfo.h"

namespace pedal {

static constexpr float TWO_PI = 6.28318530717958647692f;

void Lfo::Init(float rate_hz, LfoWave wave) {
    phase_ = 0.0f;
    wave_  = wave;
    SetRate(rate_hz);
}

void Lfo::SetRate(float rate_hz) {
    phase_inc_ = TWO_PI * rate_hz * INV_SAMPLE_RATE;
}

static float lfo_compute(float phase, LfoWave wave) {
    switch (wave) {
        case LfoWave::Sine:
            return fast_sin(phase);
        case LfoWave::Triangle:
            return (phase < 3.14159265f)
                ? (-1.0f + phase * (2.0f / 3.14159265f))
                : ( 3.0f - phase * (2.0f / 3.14159265f));
        default:
            return 0.0f; // unreachable; silences -Wswitch-default
    }
}

float Lfo::Process() {
    const float out = lfo_compute(phase_, wave_);
    phase_ += phase_inc_;
    if (phase_ >= TWO_PI) phase_ -= TWO_PI;
    return out;
}

float Lfo::PrepareBlock() {
    const float out = lfo_compute(phase_, wave_);
    phase_ += phase_inc_ * static_cast<float>(BLOCK_SIZE);
    // Use while-loop rather than a single subtract: phase_inc_ * BLOCK_SIZE
    // can exceed TWO_PI if SetRate() is called with a very high frequency.
    while (phase_ >= TWO_PI) phase_ -= TWO_PI;
    return out;
}

} // namespace pedal

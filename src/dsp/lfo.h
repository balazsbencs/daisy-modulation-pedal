#pragma once
#include "../config/constants.h"
#include "fast_math.h"

namespace pedal {

enum class LfoWave { Sine, Triangle };

class Lfo {
public:
    void Init(float rate_hz = 1.0f, LfoWave wave = LfoWave::Sine);
    void SetRate(float rate_hz);
    void SetWave(LfoWave wave) { wave_ = wave; }
    // Per-sample: advance phase by one sample, return -1..+1.
    float Process();
    // Per-block: advance phase by BLOCK_SIZE samples, return -1..+1.
    // Use from Prepare() to avoid calling sinf() 48 times per block.
    float PrepareBlock();

private:
    float    phase_     = 0.0f;
    float    phase_inc_ = 0.0f;
    LfoWave  wave_      = LfoWave::Sine;
};

} // namespace pedal

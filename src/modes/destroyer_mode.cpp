#include "destroyer_mode.h"
#include "../config/constants.h"
#include <cmath>

namespace pedal {

void DestroyerMode::Init() {
    Reset();
}

void DestroyerMode::Reset() {
    svf_.Reset();
    lfo_.Init(0.5f, LfoWave::Sine);
    held_sample_  = 0.0f;
    decimate_acc_ = 0.0f;
    decimate_rate_= 1.0f;
    bits_         = 16;
    rand_         = 12345;
}

void DestroyerMode::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.speed);

    // Bitcrush: depth 0..1 → bits 16..4
    bits_ = 16 - static_cast<int>(params.depth * 12.0f);
    if (bits_ < 1) bits_ = 1;

    // Decimation rate: depth → 1× to 48× (reduces effective sample rate)
    decimate_rate_ = 1.0f + params.depth * 47.0f;

    // Post-filter
    const float cutoff = 80.0f + params.tone * (SAMPLE_RATE * 0.45f - 80.0f);
    svf_.SetFreq(cutoff > 20000.0f ? 20000.0f : cutoff);
    svf_.SetQ(0.5f + params.p1 * 8.0f);
}

StereoFrame DestroyerMode::Process(float input, const ParamSet& params) {
    // Sample-rate decimation: hold sample for decimate_rate_ samples
    decimate_acc_ += 1.0f;
    if (decimate_acc_ >= decimate_rate_) {
        decimate_acc_ -= decimate_rate_;
        held_sample_ = input;
    }
    float x = held_sample_;

    // Bit crushing: quantize to `bits_` bits
    if (bits_ < 16) {
        const float levels = static_cast<float>(1 << bits_);
        x = floorf(x * levels + 0.5f) / levels;
    }

    // Vinyl noise from P2 upper range
    if (params.p2 > 0.5f) {
        rand_ = rand_ * 1664525u + 1013904223u;
        const float noise = static_cast<float>(static_cast<int32_t>(rand_))
                          * (1.0f / 2147483648.0f)
                          * (params.p2 - 0.5f) * 0.04f;
        x += noise;
    }

    // Post-filter
    svf_.Process(x);
    float wet;
    const float ftype = params.p2 * 2.0f;  // 0=LP, 1=BP, 2=HP
    if      (ftype < 0.5f) wet = svf_.lp();
    else if (ftype < 1.5f) wet = svf_.bp();
    else                   wet = svf_.hp();

    return {wet, wet};
}

} // namespace pedal

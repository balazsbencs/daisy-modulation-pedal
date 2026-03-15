#pragma once
#include "mod_mode.h"
#include "../dsp/svf.h"
#include "../dsp/lfo.h"

namespace pedal {

/// Bitcrusher + decimator + resonant filter.
/// Speed: LFO rate for filter sweep or decimation rate modulation.
/// Depth: crush/decimate amount.
/// Tone: post-destruction filter cutoff.
/// P1: filter resonance.
/// P2: filter type (0=LP, 0.5=BP, 1=HP) + vinyl noise blend.
class DestroyerMode : public ModMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const ParamSet& params) override;
    StereoFrame Process(float input, const ParamSet& params) override;
    const char* Name() const override { return "Destroyer"; }

private:
    Svf      svf_;
    Lfo      lfo_;
    float    held_sample_  = 0.0f;
    float    decimate_acc_ = 0.0f;
    float    decimate_rate_= 1.0f;  // samples between hold updates
    int      bits_         = 16;
    uint32_t rand_         = 12345;
};

} // namespace pedal

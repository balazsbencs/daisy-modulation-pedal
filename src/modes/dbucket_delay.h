#pragma once
#include "delay_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/tone_filter.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

class DbucketDelay : public DelayMode {
public:
    void Init()  override;
    void Reset() override;
    void Prepare(const ParamSet& params) override;
    StereoFrame Process(float input, const ParamSet& params) override;
    const char* Name() const override { return "Dbucket"; }

private:
    Lfo        lfo_;
    ToneFilter filter_;
    DcBlocker  dc_;

    // Simple LCG for deterministic noise
    uint32_t noise_seed_ = 12345u;
    float    lfo_out_    = 0.0f;

    float next_noise();
};

} // namespace pedal

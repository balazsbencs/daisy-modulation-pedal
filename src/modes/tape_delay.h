#pragma once
#include "delay_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/tone_filter.h"
#include "../dsp/saturation.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

class TapeDelay : public DelayMode {
public:
    void Init()  override;
    void Reset() override;
    void Prepare(const ParamSet& params) override;
    StereoFrame Process(float input, const ParamSet& params) override;
    const char* Name() const override { return "Tape"; }

private:
    Lfo        lfo_;
    ToneFilter filter_;
    Saturation sat_;
    DcBlocker  dc_;
    float      lfo_out_ = 0.0f;
};

} // namespace pedal

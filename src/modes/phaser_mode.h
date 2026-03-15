#pragma once
#include "mod_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/allpass_filter.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

/// Classic phaser — 2/4/6/8/12/16-stage allpass chain with LFO + regen.
class PhaserMode : public ModMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const ParamSet& params) override;
    StereoFrame Process(float input, const ParamSet& params) override;
    const char* Name() const override { return "Phaser"; }

private:
    static constexpr int kMaxStages = 16;
    Lfo          lfo_;
    AllpassFilter stages_[kMaxStages];
    DcBlocker    dc_;
    float        lfo_coeff_ = 0.0f;  // cached allpass coefficient from LFO
    float        feedback_  = 0.0f;  // last output for regen
    int          num_stages_ = 4;    // active stage count
};

} // namespace pedal

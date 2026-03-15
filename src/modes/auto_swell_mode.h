#pragma once
#include "mod_mode.h"
#include "../dsp/envelope_follower.h"
#include "../dsp/delay_line_sdram.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

/// Envelope-triggered swell — soft attack volume ramp.
/// Speed repurposed as attack time (fast=short, slow=long).
/// P1 = release time; P2 = chorus blend (0=dry swell, 1=add chorus shimmer).
class AutoSwellMode : public ModMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const ParamSet& params) override;
    StereoFrame Process(float input, const ParamSet& params) override;
    const char* Name() const override { return "AutoSwell"; }

private:
    EnvelopeFollower env_;
    DcBlocker        dc_;

    float swell_gain_  = 0.0f;   // current swell gain 0..1
    float attack_coef_ = 0.001f;
    float release_coef_= 0.0001f;
    bool  swelling_    = false;
};

} // namespace pedal

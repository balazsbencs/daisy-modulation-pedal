#pragma once
#include "mod_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/svf.h"
#include "../dsp/envelope_follower.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

/// Resonant filter swept by LFO or envelope follower.
/// Tone knob: 0=LP, 0.5=Wah(BP), 1=HP.
/// P1: resonance Q (0.5–20).
/// P2: waveshape (0=Sine..5=S&H, 6=Env+, 7=Env-).
class FilterMode : public ModMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const ParamSet& params) override;
    StereoFrame Process(float input, const ParamSet& params) override;
    const char* Name() const override { return "Filter"; }

private:
    Lfo              lfo_;
    Svf              svf_;
    EnvelopeFollower env_;
    DcBlocker        dc_;

    float mod_val_           = 0.0f;     // cached modulation value 0..1
    float envelope_cutoff_hz_ = 1000.0f; // cutoff computed in Process(), applied in Prepare()
    int   ftype_    = 0;      // 0=LP, 1=BP(Wah), 2=HP
    bool  use_env_  = false;
    bool  env_inv_  = false;
};

} // namespace pedal

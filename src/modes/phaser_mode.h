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
    Lfo           lfo_;
    Lfo           lfo2_;                  // quadrature LFO for Barber Pole (π/2 offset)
    AllpassFilter stages_[kMaxStages];    // stages[0..3]=chainA, [4..7]=chainB (Barber Pole)
    DcBlocker     dc_;
    float         lfo_coeff_  = 0.0f;    // cached allpass coefficient (chain A / normal)
    float         lfo_coeff2_ = 0.0f;    // cached allpass coefficient for chain B
    float         lfo_val_    = 0.0f;    // stored LFO value for Barber Pole crossfade
    float         feedback_   = 0.0f;    // regen state for chain A / normal
    float         feedback2_  = 0.0f;    // regen state for chain B (Barber Pole)
    int           num_stages_  = 4;      // active stage count (normal modes)
    bool          barber_pole_ = false;  // true when sub-mode 6 is active
};

} // namespace pedal

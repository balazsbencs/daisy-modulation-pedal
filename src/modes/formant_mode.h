#pragma once
#include "mod_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/svf.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

/// Vowel formant filter — two SVF bandpass filters morphing between vowels.
/// P2 selects target vowel (AA/EE/EYE/OH/OOH).
/// LFO morphs toward that vowel; Speed controls morph rate.
class FormantMode : public ModMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const ParamSet& params) override;
    StereoFrame Process(float input, const ParamSet& params) override;
    const char* Name() const override { return "Formant"; }

private:
    Lfo      lfo_;
    Svf      f1_, f2_;   // formant 1 and 2 bandpass filters
    DcBlocker dc_;

    float f1_hz_ = 500.0f;
    float f2_hz_ = 1500.0f;
};

} // namespace pedal

#include "vintage_trem_mode.h"
#include "../config/constants.h"

namespace pedal {

void VintageTremMode::Init() {
    Reset();
}

void VintageTremMode::Reset() {
    lfo_.Init(4.0f, LfoWave::Sine);
    gain_ = 1.0f;
}

void VintageTremMode::Prepare(const ParamSet& params) {
    // sub-mode from p2: 0=Tube (sine), 1=Harmonic (triangle), 2=Photoresistor (exponential)
    const int sub = static_cast<int>(params.p2 * 2.999f);
    if (sub == 0)      lfo_.SetWave(LfoWave::Sine);
    else if (sub == 1) lfo_.SetWave(LfoWave::Triangle);
    else               lfo_.SetWave(LfoWave::Exponential);

    lfo_.SetRate(params.speed);
    const float lfo_val = lfo_.PrepareBlock(); // -1..+1
    // gain = 1 - depth * (0.5 + 0.5 * lfo_val):
    //   lfo_val=+1 → 1 - depth (minimum volume)
    //   lfo_val=-1 → 1 (maximum volume)
    gain_ = 1.0f - params.depth * (0.5f + 0.5f * lfo_val);
    if (gain_ < 0.0f) gain_ = 0.0f;
}

StereoFrame VintageTremMode::Process(float input, const ParamSet& /*params*/) {
    const float wet = input * gain_;
    return {wet, wet};
}

} // namespace pedal

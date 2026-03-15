#pragma once
#include "mod_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/saturation.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

/// Leslie rotating speaker simulation.
/// Horn (HF) rotates faster than drum (LF); both produce Doppler + AM.
class RotaryMode : public ModMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const ParamSet& params) override;
    StereoFrame Process(float input, const ParamSet& params) override;
    const char* Name() const override { return "Rotary"; }

private:
    Lfo       horn_lfo_;     // fast rotor
    Lfo       horn_lfo_q_;   // 90° offset for stereo spread
    Lfo       drum_lfo_;     // slow rotor
    Lfo       drum_lfo_q_;   // 90° offset
    Saturation drive_;
    DcBlocker  dc_l_, dc_r_;

    float horn_am_l_ = 1.0f;
    float horn_am_r_ = 1.0f;
    float drum_am_l_ = 1.0f;
    float drum_am_r_ = 1.0f;
    float horn_delay_ = 0.0f;
    float drum_delay_ = 0.0f;
};

} // namespace pedal

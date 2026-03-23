#include "formant_mode.h"
#include "../config/constants.h"

namespace pedal {

struct FormantFreqs { float f1, f2; };

static const FormantFreqs kVowels[5] = {
    {730.0f, 1090.0f},  // AA
    {270.0f, 2290.0f},  // EE
    {400.0f, 2050.0f},  // EYE
    {570.0f,  840.0f},  // OH
    {300.0f,  870.0f},  // OOH
};

void FormantMode::Init() {
    Reset();
}

void FormantMode::Reset() {
    lfo_.Init(0.5f, LfoWave::Sine);
    f1_.Reset(); f2_.Reset();
    f1_.SetQ(4.0f);
    f2_.SetQ(6.0f);
    dc_.Init();
    f1_hz_ = kVowels[0].f1;
    f2_hz_ = kVowels[0].f2;
}

void FormantMode::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.speed);

    // Target vowel from p2 (0..1 → 5 vowels)
    const int vowel = static_cast<int>(params.p2 * 4.999f);
    const float target_f1 = kVowels[vowel].f1;
    const float target_f2 = kVowels[vowel].f2;

    // Smooth morph toward target (slew rate proportional to depth and lfo)
    const float lfo_val = lfo_.PrepareBlock(); // -1..+1, used as morph modulation
    const float t = 0.5f + 0.5f * lfo_val;    // 0..1
    // Interpolate between current and target, scaled by depth
    const float morph = params.depth * t;
    f1_hz_ += (target_f1 - f1_hz_) * morph * 0.1f;
    f2_hz_ += (target_f2 - f2_hz_) * morph * 0.1f;

    f1_.SetFreq(f1_hz_);
    f2_.SetFreq(f2_hz_);

    // P1 controls formant bandwidth (Q: higher = narrower)
    const float q1 = 2.0f + params.p1 * 8.0f;   // 2..10
    const float q2 = 4.0f + params.p1 * 10.0f;  // 4..14
    f1_.SetQ(q1);
    f2_.SetQ(q2);
}

StereoFrame FormantMode::Process(StereoFrame input, const ParamSet& /*params*/) {
    const float mono = input.mono();
    f1_.Process(mono);
    f2_.Process(mono);
    // Sum bandpass outputs; scale down to prevent clipping
    float wet = (f1_.bp() + f2_.bp()) * 0.5f;
    wet = dc_.Process(wet);
    return {wet, wet};
}

} // namespace pedal

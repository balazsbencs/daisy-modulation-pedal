#include "phaser_mode.h"
#include "../config/constants.h"

namespace pedal {

// Stage counts per sub-mode. Barber Pole (6) uses two 4-stage chains — value unused in that path.
static const int kStageCounts[] = {2, 4, 6, 8, 12, 16, 4};

void PhaserMode::Init() {
    Reset();
}

void PhaserMode::Reset() {
    static constexpr float kHalfPi = 1.57079633f;
    lfo_.Init(0.5f, LfoWave::Sine);
    lfo2_.Init(0.5f, LfoWave::Sine);
    lfo2_.SetPhaseOffset(kHalfPi);   // 90° quadrature offset for Barber Pole
    for (auto& s : stages_) s.Reset();
    dc_.Init();
    lfo_coeff_  = 0.0f;
    lfo_coeff2_ = 0.0f;
    lfo_val_    = 0.0f;
    feedback_   = 0.0f;
    feedback2_  = 0.0f;
    num_stages_  = 4;
    barber_pole_ = false;
}

void PhaserMode::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.speed);
    lfo2_.SetRate(params.speed);

    // Sub-mode from p2: 0..6 → stage counts (6 = Barber Pole)
    const int sub = static_cast<int>(params.p2 * 6.999f);
    barber_pole_ = (sub == 6);
    num_stages_  = kStageCounts[sub];

    // Negative allpass coefficients place the phase-shift notch in the audio band.
    // For H(z)=(a+z⁻¹)/(1+a·z⁻¹) cascaded N times, the notch of (dry+wet)
    // falls at cos(ω) = -2a/(1+a²). Positive a pushes the notch near Nyquist;
    // negative a puts it in the 300 Hz – 12 kHz phaser sweet spot.
    // center: tone=0 → -0.95 (notch ~300 Hz), tone=1 → -0.10 (notch ~10 kHz)
    const float lfo_val = lfo_.PrepareBlock(); // -1..+1
    lfo_val_ = lfo_val;
    const float center  = -(0.95f - params.tone * 0.85f);
    lfo_coeff_ = center + params.depth * 0.4f * lfo_val;
    if (lfo_coeff_ > -0.01f) lfo_coeff_ = -0.01f;
    if (lfo_coeff_ < -0.99f) lfo_coeff_ = -0.99f;

    if (barber_pole_) {
        const float lfo_val2 = lfo2_.PrepareBlock(); // 90° offset
        lfo_coeff2_ = center + params.depth * 0.4f * lfo_val2;
        if (lfo_coeff2_ > -0.01f) lfo_coeff2_ = -0.01f;
        if (lfo_coeff2_ < -0.99f) lfo_coeff2_ = -0.99f;
    }
}

StereoFrame PhaserMode::Process(float input, const ParamSet& params) {
    const float regen = params.p1 * 0.95f;

    if (barber_pole_) {
        // Two independent 4-stage chains with quadrature LFOs.
        // Chain A: stages[0..3] with lfo_coeff_
        // Chain B: stages[4..7] with lfo_coeff2_ (90° offset)
        // Crossfade based on lfo_val_ so one chain's sweep hands off to the other,
        // creating the infinite rising/falling illusion.
        float xa = input + feedback_  * regen;
        float xb = input + feedback2_ * regen;
        for (int i = 0; i < 4; ++i) {
            stages_[i].SetCoeff(lfo_coeff_);
            xa = stages_[i].Process(xa);
        }
        for (int i = 4; i < 8; ++i) {
            stages_[i].SetCoeff(lfo_coeff2_);
            xb = stages_[i].Process(xb);
        }
        // lfo_val_ ∈ [-1,+1]; t=0 → all chain B, t=1 → all chain A
        const float t = (lfo_val_ + 1.0f) * 0.5f;
        const float x = xa * t + xb * (1.0f - t);
        const float out = dc_.Process(x);
        feedback_  = xa;
        feedback2_ = xb;
        return {out, out};
    }

    // Normal path: single chain with num_stages_ stages
    float x = input + feedback_ * regen;
    for (int i = 0; i < num_stages_; ++i) {
        stages_[i].SetCoeff(lfo_coeff_);
        x = stages_[i].Process(x);
    }
    x = dc_.Process(x);
    feedback_ = x;
    return {x, x};
}

} // namespace pedal

#include "audio_engine.h"
#include "util/scopedirqblocker.h"
#include "../dsp/fast_math.h"

using namespace daisy;

namespace pedal {

// ── Singleton ────────────────────────────────────────────────────────────────
AudioEngine* AudioEngine::instance_ = nullptr;

// ── Init ─────────────────────────────────────────────────────────────────────
void AudioEngine::Init(DaisySeed* hw) {
    hw_              = hw;
    mode_            = nullptr;
    bypassed_        = true;
    new_bypass_      = true;
    param_read_idx_  = 0;
    param_dirty_     = false;
    bypass_dirty_    = false;
    param_buf_[0]    = ParamSet::make_default();
    param_buf_[1]    = ParamSet::make_default();
    mono_count_      = 0;
    is_mono_         = true;
    instance_        = this;
}

// ── Main-loop API ─────────────────────────────────────────────────────────────

void AudioEngine::SetParams(const ParamSet& params) {
    // Prevent ISR from flipping param_read_idx_ while selecting write buffer.
    daisy::ScopedIrqBlocker irq_block;
    const int               write_idx = 1 - param_read_idx_;
    param_buf_[write_idx]             = params;
    // Publish: the ISR will flip param_read_idx_ on the next block entry.
    param_dirty_ = true;
}

void AudioEngine::SetMode(ModMode* mode) {
    daisy::ScopedIrqBlocker irq_block;
    mode_ = mode;
}

void AudioEngine::SetBypass(bool bypassed) {
    daisy::ScopedIrqBlocker irq_block;
    new_bypass_   = bypassed;
    bypass_dirty_ = true;
}

// ── ISR trampoline ────────────────────────────────────────────────────────────

void AudioEngine::AudioCallback(AudioHandle::InputBuffer  in,
                                AudioHandle::OutputBuffer out,
                                size_t                    size) {
    if (instance_ != nullptr) {
        instance_->ProcessBlock(in, out, size);
    }
}

// ── ProcessBlock (runs in ISR) ────────────────────────────────────────────────

void AudioEngine::ProcessBlock(AudioHandle::InputBuffer  in,
                               AudioHandle::OutputBuffer out,
                               size_t                    size) {
    // Consume pending parameter update: flip the read index so the next
    // iteration uses the freshly written buffer.
    if (param_dirty_) {
        param_read_idx_ ^= 1;
        param_dirty_     = false;
    }

    // Consume pending bypass change.
    if (bypass_dirty_) {
        bypassed_     = new_bypass_;
        bypass_dirty_ = false;
    }

    const ParamSet& params = param_buf_[param_read_idx_];
    ModMode*        mode   = mode_;

    // Block-level mono detection: compute mean-square energy of IN_R.
    // Codec grounds unconnected inputs, so IN_R ≈ 0 when mono cable plugged in.
    float in_r_energy = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        in_r_energy += IN_R[i] * IN_R[i];
    }
    in_r_energy /= static_cast<float>(size);

    // Hysteresis: require 3 consecutive blocks below threshold to declare mono,
    // or 1 block above threshold to declare stereo.
    static constexpr float kMonoThreshold = 1e-8f;  // -80 dBFS^2
    if (in_r_energy > kMonoThreshold) {
        is_mono_    = false;
        mono_count_ = 0;
    } else {
        if (mono_count_ < 3) ++mono_count_;
        if (mono_count_ >= 3) is_mono_ = true;
    }

    // Constant-power mixing: sin/cos mapping avoids the -3dB dip at 50/50 mix.
    // Recompute only when mix changes; trig is expensive on Cortex-M7.
    if (params.mix != last_mix_) {
        last_mix_ = params.mix;
        const float angle = params.mix * 1.57079632679f; // pi/2
        mix_dry_          = fast_cos(angle);
        mix_wet_          = fast_sin(angle);
    }

    if (mode != nullptr && !bypassed_) {
        mode->Prepare(params);
    }

    for (size_t i = 0; i < size; ++i) {
        const float in_l = IN_L[i];
        const float in_r = is_mono_ ? in_l : IN_R[i];

        if (bypassed_ || mode == nullptr) {
            OUT_L[i] = in_l;
            OUT_R[i] = in_r;
        } else {
            // Wet-only result from the mode; mix is applied here so modes
            // never need to know about the dry path.
            const StereoFrame wet = mode->Process(StereoFrame{in_l, in_r}, params);

            OUT_L[i] = (in_l * mix_dry_ + wet.left  * mix_wet_) * params.level;
            OUT_R[i] = (in_r * mix_dry_ + wet.right * mix_wet_) * params.level;
        }
    }
}

} // namespace pedal

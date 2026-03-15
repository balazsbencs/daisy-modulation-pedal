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
        const float dry = IN_L[i]; // pedal is mono-input

        if (bypassed_ || mode == nullptr) {
            // True bypass: route input directly to both outputs.
            OUT_L[i] = dry;
            OUT_R[i] = dry;
        } else {
            // Wet-only result from the mode; mix is applied here so modes
            // never need to know about the dry path.
            const StereoFrame wet = mode->Process(dry, params);

            OUT_L[i] = (dry * mix_dry_ + wet.left  * mix_wet_) * params.level;
            OUT_R[i] = (dry * mix_dry_ + wet.right * mix_wet_) * params.level;
        }
    }
}

} // namespace pedal

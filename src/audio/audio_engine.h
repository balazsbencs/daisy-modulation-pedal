#pragma once
#include "daisy_seed.h"
#include "stereo_frame.h"
#include "../params/param_set.h"
#include "../modes/delay_mode.h"
#include "bypass.h"

namespace pedal {

/// Owns the audio processing pipeline for the delay pedal.
///
/// Thread-safety model:
///   - The audio callback runs in interrupt context (ISR).
///   - The main loop communicates via double-buffered params and dirty flags.
///   - No mutexes are used; instead a single-writer / single-reader pattern
///     guarantees consistency on a Cortex-M7 without atomic overhead.
class AudioEngine {
public:
    /// Initialise the engine and register the singleton pointer.
    /// Must be called before hw->StartAudio().
    void Init(daisy::DaisySeed* hw);

    /// Push a new parameter snapshot from the main loop.
    /// Writes to the inactive buffer and sets the dirty flag.
    /// Safe to call any time from the main loop.
    void SetParams(const ParamSet& params);

    /// Swap the active delay mode pointer.
    /// The new mode must already be initialised; the engine starts using it
    /// on the next block boundary.
    void SetMode(DelayMode* mode);

    /// Signal bypass state to the audio callback.
    /// @param bypassed  true  → pass audio dry (no processing)
    ///                  false → run the active mode
    void SetBypass(bool bypassed);

    /// @return Current bypass state as seen by the audio callback.
    bool IsBypassed() const { return bypassed_; }

    /// Static trampoline registered with daisy::DaisySeed::StartAudio().
    static void AudioCallback(daisy::AudioHandle::InputBuffer  in,
                              daisy::AudioHandle::OutputBuffer out,
                              size_t                           size);

private:
    /// Singleton pointer; written by Init(), read only by the static callback.
    static AudioEngine* instance_;
    /// Per-block DSP dispatch; called exclusively from the ISR.
    void ProcessBlock(daisy::AudioHandle::InputBuffer  in,
                      daisy::AudioHandle::OutputBuffer out,
                      size_t                           size);

    daisy::DaisySeed* hw_   = nullptr;
    DelayMode*        mode_ = nullptr;

    // Double-buffered parameter set.
    // Index convention:
    //   param_read_idx_      → buffer being consumed by the ISR
    //   1 - param_read_idx_  → buffer available for main-loop writes
    ParamSet     param_buf_[2]{};
    volatile int param_read_idx_ = 0;
    volatile bool param_dirty_   = false;

    // Bypass double-flag: main loop writes new_bypass_ then sets dirty.
    bool         bypassed_     = true;   // starts bypassed until Init() done
    bool         new_bypass_   = true;
    volatile bool bypass_dirty_ = false;

    // Precalculated mixing gains (recomputed only when mix changes)
    float last_mix_ = -1.0f;  // sentinel: forces recompute on first block
    float mix_dry_  = 1.0f;
    float mix_wet_  = 0.0f;
    float mix_norm_ = 1.0f;
};

} // namespace pedal

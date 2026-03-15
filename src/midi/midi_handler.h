#pragma once
#include "daisy_seed.h"
#include "../config/constants.h"
#include <cstdint>

namespace pedal {

/// Aggregated MIDI state produced each main-loop poll.
/// Consumers should treat each field as a one-shot event unless noted.
struct MidiState {
    /// Normalised CC values [0, 1] indexed by parameter (0 = Speed … 6 = Level).
    float cc_normalized[NUM_PARAMS]{};

    /// True for each slot where a CC message was received this poll cycle.
    /// Use this (not cc_normalized > 0) to detect an incoming CC, since a
    /// value of 0 is a valid MIDI CC state.
    bool cc_received[NUM_PARAMS]{};

    /// -1 when no Program Change was received, otherwise 0..11.
    int  program_change = -1;

    /// True for exactly one poll when a MIDI Clock (0xF8) pulse arrives.
    bool clock_tick = false;

    /// True for exactly one poll when a MIDI Stop (0xFC) arrives.
    bool clock_stop = false;
};

/// Manages the runtime mapping of MIDI CC numbers to parameter indices.
/// A default mapping is applied at Init(); individual parameters can be
/// re-mapped via one-shot MIDI Learn.
class MidiLearn {
public:
    void Init();

    bool IsActive()    const { return active_; }
    int  ParamIndex()  const { return param_index_; }

    /// Begin a learn session for the given parameter index (0..6).
    void Start(int param_index);

    /// Attempt to record the incoming CC number as the mapping for the
    /// parameter currently in learn mode.
    /// @return true when the learn was completed successfully.
    bool TryLearn(uint8_t cc_num);

    /// @return The CC number currently mapped to the given parameter index.
    uint8_t GetMapping(int param) const { return cc_map_[param]; }

private:
    bool    active_      = false;
    int     param_index_ = -1;
    uint8_t cc_map_[NUM_PARAMS]
        = {CC_SPEED, CC_DEPTH, CC_MIX, CC_TONE, CC_P1, CC_P2, CC_LEVEL};
};

/// Polls both UART and USB MIDI interfaces and converts incoming events into
/// a MidiState snapshot.  Call Poll() once per main loop iteration.
class MidiHandlerPedal {
public:
    void Init();

    /// Drain all pending events from both interfaces and populate out_state.
    void Poll(MidiState& out_state);

    /// Expose learn control so the UI layer can trigger it.
    MidiLearn& Learn() { return learn_; }

private:
    daisy::MidiUartHandler uart_midi_;
    daisy::MidiUsbHandler  usb_midi_;
    MidiLearn              learn_;

    /// Dispatch a single MidiEvent into the shared MidiState accumulator.
    void ProcessEvent(const daisy::MidiEvent& ev, MidiState& state);
};

} // namespace pedal

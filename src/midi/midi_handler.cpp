#include "midi_handler.h"

using namespace daisy;

namespace pedal {

// ---------------------------------------------------------------------------
// MidiLearn
// ---------------------------------------------------------------------------

void MidiLearn::Init() {
    active_      = false;
    param_index_ = -1;
    // Restore default CC map.
    cc_map_[0] = CC_SPEED;
    cc_map_[1] = CC_DEPTH;
    cc_map_[2] = CC_MIX;
    cc_map_[3] = CC_TONE;
    cc_map_[4] = CC_P1;
    cc_map_[5] = CC_P2;
    cc_map_[6] = CC_LEVEL;
}

void MidiLearn::Start(int param_index) {
    if (param_index < 0 || param_index >= NUM_PARAMS) {
        active_      = false;
        param_index_ = -1;
        return;
    }
    active_      = true;
    param_index_ = param_index;
}

bool MidiLearn::TryLearn(uint8_t cc_num) {
    if (!active_ || param_index_ < 0 || param_index_ >= NUM_PARAMS) {
        return false;
    }
    cc_map_[param_index_] = cc_num;
    active_               = false;
    param_index_          = -1;
    return true;
}

// ---------------------------------------------------------------------------
// MidiHandlerPedal
// ---------------------------------------------------------------------------

void MidiHandlerPedal::Init() {
    // UART MIDI: default config uses USART1 (D14=RX, D13=TX) which matches
    // the hardware. DMA circular buffer — StartReceive() is non-blocking even
    // when no MIDI device is connected.
    MidiUartHandler::Config uart_cfg;
    uart_midi_.Init(uart_cfg);
    uart_midi_.StartReceive();

    // USB MIDI is intentionally excluded: D29/D30 (USB D-/D+) are wired to
    // the Tone encoder and cannot simultaneously serve USB. To add USB MIDI,
    // reroute Enc 4 to unused GPIO pins and update pin_map.h.

    learn_.Init();
}

void MidiHandlerPedal::Poll(MidiState& out_state) {
    out_state.program_change = -1;
    out_state.clock_tick     = false;
    out_state.clock_stop     = false;

    uart_midi_.Listen();
    while (uart_midi_.HasEvents()) {
        ProcessEvent(uart_midi_.PopEvent(), out_state);
    }
}

void MidiHandlerPedal::ProcessEvent(const MidiEvent& ev, MidiState& state) {
    switch (ev.type) {
        case ControlChange: {
            const uint8_t cc_num = ev.data[0];
            const float   val    = static_cast<float>(ev.data[1]) / 127.0f;

            // If MIDI learn is active, absorb the first CC as the new mapping.
            if (learn_.IsActive()) {
                learn_.TryLearn(cc_num);
                return;
            }

            // Dispatch to whichever parameter owns this CC number.
    for (int i = 0; i < NUM_PARAMS; ++i) {
        if (cc_num == learn_.GetMapping(i)) {
            state.cc_normalized[i] = val;
            state.cc_received[i]   = true;
        }
            }
            break;
        }

        case ProgramChange:
            // Map program numbers to modulation modes (0..11).
            if (ev.data[0] < NUM_MODES) {
                state.program_change = static_cast<int>(ev.data[0]);
            }
            break;

        case SystemRealTime:
            if (ev.data[0] == 0xF8) {        // MIDI Clock pulse
                state.clock_tick = true;
            } else if (ev.data[0] == 0xFC) { // MIDI Stop
                state.clock_stop = true;
            }
            break;

        default:
            break;
    }
}

} // namespace pedal

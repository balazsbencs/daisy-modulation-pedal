#include "controls.h"
#include "../config/pin_map.h"

namespace pedal {

void Controls::QuadEncoder::Init(daisy::Pin a, daisy::Pin b) {
    a_.Init(a, daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
    b_.Init(b, daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
    const uint8_t initial = ReadState();
    raw_prev_ = initial;
    stable_   = initial;
    accum_    = 0;
}

int Controls::QuadEncoder::Poll() {
    static const int8_t kTransitionTable[16] = {
        0, -1,  1,  0,
        1,  0,  0, -1,
       -1,  0,  0,  1,
        0,  1, -1,  0,
    };

    // Shift-register debounce: require two consecutive identical reads
    // before accepting a new state (filters contact bounce).
    const uint8_t raw = ReadState();
    if (raw != raw_prev_) {
        raw_prev_ = raw;
        return 0;  // state not yet stable
    }

    if (raw == stable_) {
        return 0;  // no change
    }

    const uint8_t index = static_cast<uint8_t>((stable_ << 2) | raw);
    stable_ = raw;
    accum_ += kTransitionTable[index];

    // Alps EC11: 2 transitions per detent → threshold ±2.
    if (accum_ >= 2) {
        accum_ = 0;
        return 1;
    }
    if (accum_ <= -2) {
        accum_ = 0;
        return -1;
    }
    return 0;
}

uint8_t Controls::QuadEncoder::ReadState() {
    return static_cast<uint8_t>((a_.Read() ? 1 : 0) | (b_.Read() ? 2 : 0));
}

void Controls::Init(daisy::DaisySeed& hw) {
    (void)hw;

    encoder_.Init(pins::ENC_A, pins::ENC_B, pins::ENC_SW);
    param_enc_[0].Init(pins::PARAM_ENC_0_A, pins::PARAM_ENC_0_B);
    param_enc_[1].Init(pins::PARAM_ENC_1_A, pins::PARAM_ENC_1_B);
    param_enc_[2].Init(pins::PARAM_ENC_2_A, pins::PARAM_ENC_2_B);
    param_enc_[3].Init(pins::PARAM_ENC_3_A, pins::PARAM_ENC_3_B);
    sw_bypass_.Init(pins::SW_BYPASS);
    sw_tap_.Init(pins::SW_TAP);
}

void Controls::Poll() {
    // Debounce switches
    encoder_.Debounce();
    sw_bypass_.Debounce();
    sw_tap_.Debounce();

    for (int i = 0; i < 4; ++i) {
        state_.param_encoder_increment[i] = param_enc_[i].Poll();
    }

    state_.mode_encoder_increment = encoder_.Increment();
    state_.mode_encoder_pressed   = encoder_.FallingEdge();
    state_.mode_encoder_held      = encoder_.Pressed();
    state_.bypass_pressed    = sw_bypass_.RisingEdge();
    state_.tap_pressed       = sw_tap_.RisingEdge();
    state_.tap_released      = sw_tap_.FallingEdge();
    state_.tap_held          = sw_tap_.Pressed();
    state_.tap_held_ms       = static_cast<uint32_t>(sw_tap_.TimeHeldMs());
    state_.bypass_held       = sw_bypass_.Pressed();
}

} // namespace pedal

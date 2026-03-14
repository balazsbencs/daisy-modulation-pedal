#pragma once
#include "daisy_seed.h"
#include "../params/param_set.h"
#include "../config/delay_mode_id.h"
#include "../config/constants.h"

namespace pedal {

struct ControlState {
    int  mode_encoder_increment;       // -1, 0, +1 since last poll
    int  param_encoder_increment[4]{}; // 4 parameter encoders
    bool mode_encoder_pressed;         // falling edge
    bool mode_encoder_held;            // currently held
    bool bypass_pressed;               // rising edge
    bool tap_pressed;                  // rising edge
    bool tap_released;                 // falling edge
    bool tap_held;                     // currently held
    uint32_t tap_held_ms;              // held duration while pressed
    bool bypass_held;                  // currently held
};

class Controls {
public:
    void Init(daisy::DaisySeed& hw);
    // Call each main loop iteration
    void Poll();
    const ControlState& state() const { return state_; }

private:
    class QuadEncoder {
      public:
        void Init(daisy::Pin a, daisy::Pin b);
        int  Poll();

      private:
        uint8_t ReadState();
        daisy::GPIO a_;
        daisy::GPIO b_;
        uint8_t prev_ = 0;
        int8_t  accum_ = 0;
    };

    daisy::Encoder    encoder_;
    QuadEncoder       param_enc_[4];
    daisy::Switch     sw_bypass_;
    daisy::Switch     sw_tap_;
    ControlState      state_{};
};

} // namespace pedal

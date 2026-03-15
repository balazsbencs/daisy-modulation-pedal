#pragma once
#include <cstdint>

namespace pedal {

enum class ParamId : uint8_t {
    Speed  = 0,  // LFO rate (Hz, or mode-specific)
    Depth  = 1,  // modulation depth 0..1
    Mix    = 2,  // wet/dry crossfade 0..1
    Tone   = 3,  // filter position 0..1 (0.5=flat)
    P1     = 4,  // mode-specific parameter 1
    P2     = 5,  // mode-specific parameter 2 / sub-mode
    Level  = 6,  // output gain 0..2 (unity at 1.0)
    COUNT  = 7,
};

} // namespace pedal

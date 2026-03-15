#pragma once
#include <cstdint>

namespace pedal {

enum class ModModeId : uint8_t {
    Chorus    = 0,
    Flanger   = 1,
    Rotary    = 2,
    Vibe      = 3,
    Phaser    = 4,
    Filter    = 5,
    Formant   = 6,
    VintTrem  = 7,
    PattTrem  = 8,
    AutoSwell = 9,
    Destroyer = 10,
    Quadrature = 11,
    COUNT      = 12,
};

} // namespace pedal

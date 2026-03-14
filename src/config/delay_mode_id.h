#pragma once
#include <cstdint>

namespace pedal {

enum class DelayModeId : uint8_t {
    Duck    = 0,
    Swell   = 1,
    Trem    = 2,
    Digital = 3,
    Dbucket = 4,
    Tape    = 5,
    Dual    = 6,
    Pattern = 7,
    Filter  = 8,
    Lofi    = 9,
    COUNT   = 10,
};

} // namespace pedal

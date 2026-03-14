#pragma once
#include <cstdint>

namespace pedal {

enum class ParamId : uint8_t {
    Time    = 0,
    Repeats = 1,
    Mix     = 2,
    Filter  = 3,
    Grit    = 4,
    ModSpd  = 5,
    ModDep  = 6,
    COUNT   = 7,
};

} // namespace pedal

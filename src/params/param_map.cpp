#include "param_map.h"

namespace pedal {

const ParamRange& get_param_range(ModModeId mode, ParamId param) {
    (void)mode; // per-mode overrides will be added as modes are implemented
    switch (param) {
        case ParamId::Speed: return default_ranges::SPEED;
        case ParamId::Depth: return default_ranges::DEPTH;
        case ParamId::Mix:   return default_ranges::MIX;
        case ParamId::Tone:  return default_ranges::TONE;
        case ParamId::P1:    return default_ranges::P1;
        case ParamId::P2:    return default_ranges::P2;
        case ParamId::Level: return default_ranges::LEVEL;
        default:             return default_ranges::MIX;
    }
}

} // namespace pedal

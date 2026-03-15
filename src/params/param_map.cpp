#include "param_map.h"

namespace pedal {

// Per-mode speed range overrides (curve matches default SPEED: 1.0f = log)
namespace mode_ranges {
    constexpr ParamRange SPEED_QUADRATURE = {0.5f,   2000.0f, 1.0f}; // audio-rate carrier Hz
    constexpr ParamRange SPEED_AUTOSWELL  = {0.010f, 0.500f,  0.0f}; // attack time, seconds (linear)
    constexpr ParamRange SPEED_DESTROYER  = {1.0f,   48.0f,   0.0f}; // decimation rate (linear)
    constexpr ParamRange SPEED_VINTTREM   = {1.0f,   15.0f,   1.0f}; // Hz, log
} // namespace mode_ranges

const ParamRange& get_param_range(ModModeId mode, ParamId param) {
    if (param == ParamId::Speed) {
        switch (mode) {
            case ModModeId::Quadrature: return mode_ranges::SPEED_QUADRATURE;
            case ModModeId::AutoSwell:  return mode_ranges::SPEED_AUTOSWELL;
            case ModModeId::Destroyer:  return mode_ranges::SPEED_DESTROYER;
            case ModModeId::VintTrem:   return mode_ranges::SPEED_VINTTREM;
            default: break;
        }
    }
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

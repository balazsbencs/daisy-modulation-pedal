#pragma once
#include "param_range.h"
#include "param_id.h"
#include "../config/mod_mode_id.h"

namespace pedal {

// Returns the physical range for a given parameter in a given mode.
// Allows modes to have different scaling for same pot.
const ParamRange& get_param_range(ModModeId mode, ParamId param);

// Default ranges used by most modes
namespace default_ranges {
    constexpr ParamRange SPEED = {0.05f, 10.0f, 1.0f};  // log, Hz
    constexpr ParamRange DEPTH = {0.0f,  1.0f,  0.0f};  // linear
    constexpr ParamRange MIX   = {0.0f,  1.0f,  0.0f};  // linear
    constexpr ParamRange TONE  = {0.0f,  1.0f,  0.0f};  // linear
    constexpr ParamRange P1    = {0.0f,  1.0f,  0.0f};  // linear
    constexpr ParamRange P2    = {0.0f,  1.0f,  0.0f};  // linear
    constexpr ParamRange LEVEL = {0.0f,  2.0f,  0.0f};  // linear, gain 0..2
}

} // namespace pedal

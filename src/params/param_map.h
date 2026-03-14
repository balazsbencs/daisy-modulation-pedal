#pragma once
#include "param_range.h"
#include "param_id.h"
#include "../config/delay_mode_id.h"

namespace pedal {

// Returns the physical range for a given parameter in a given mode.
// Allows modes to have different scaling for same pot.
const ParamRange& get_param_range(DelayModeId mode, ParamId param);

// Default ranges used by most modes
namespace default_ranges {
    constexpr ParamRange TIME      = {0.06f,  2.5f, 2.0f};  // log, 60ms–2500ms
    constexpr ParamRange TIME_LOFI = {0.002f, 2.5f, 2.0f};  // log, 2ms–2500ms
    constexpr ParamRange REPEATS = {0.0f,  0.98f, 0.0f};  // linear
    constexpr ParamRange MIX     = {0.0f,  1.0f,  0.0f};
    constexpr ParamRange FILTER  = {0.0f,  1.0f,  0.0f};
    constexpr ParamRange GRIT    = {0.0f,  1.0f,  0.0f};
    constexpr ParamRange MOD_SPD = {0.05f, 10.0f, 1.0f};  // slight log
    constexpr ParamRange MOD_DEP = {0.0f,  1.0f,  0.0f};
}

} // namespace pedal

#pragma once
#include "mod_mode.h"
#include "../config/mod_mode_id.h"
#include "../config/constants.h"

namespace pedal {

class ModeRegistry {
public:
    void Init();
    ModMode* get(ModModeId id);
    void Reset(ModModeId id);

private:
    // Pointers to statically allocated mode objects (no heap allocation)
    ModMode* modes_[NUM_MODES]{};
};

} // namespace pedal

#pragma once
#include "delay_mode.h"
#include "../config/delay_mode_id.h"
#include "../config/constants.h"

namespace pedal {

class ModeRegistry {
public:
    void Init();
    DelayMode* get(DelayModeId id);
    void Reset(DelayModeId id);

private:
    // Pointers to statically allocated mode objects (no heap allocation)
    DelayMode* modes_[NUM_MODES]{};
};

} // namespace pedal

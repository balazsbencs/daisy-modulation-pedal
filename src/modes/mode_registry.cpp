#include "mode_registry.h"
#include "duck_delay.h"
#include "swell_delay.h"
#include "trem_delay.h"
#include "digital_delay.h"
#include "dbucket_delay.h"
#include "tape_delay.h"
#include "dual_delay.h"
#include "pattern_delay.h"
#include "filter_delay.h"
#include "lofi_delay.h"

namespace pedal {

// Static instances: objects are small (DSP state only); SDRAM buffers are in
// their respective .cpp files.  No heap allocation anywhere.
static DuckDelay    s_duck;
static SwellDelay   s_swell;
static TremDelay    s_trem;
static DigitalDelay s_digital;
static DbucketDelay s_dbucket;
static TapeDelay    s_tape;
static DualDelay    s_dual;
static PatternDelay s_pattern;
static FilterDelay  s_filter;
static LofiDelay    s_lofi;

void ModeRegistry::Init() {
    modes_[static_cast<uint8_t>(DelayModeId::Duck)]    = &s_duck;
    modes_[static_cast<uint8_t>(DelayModeId::Swell)]   = &s_swell;
    modes_[static_cast<uint8_t>(DelayModeId::Trem)]    = &s_trem;
    modes_[static_cast<uint8_t>(DelayModeId::Digital)] = &s_digital;
    modes_[static_cast<uint8_t>(DelayModeId::Dbucket)] = &s_dbucket;
    modes_[static_cast<uint8_t>(DelayModeId::Tape)]    = &s_tape;
    modes_[static_cast<uint8_t>(DelayModeId::Dual)]    = &s_dual;
    modes_[static_cast<uint8_t>(DelayModeId::Pattern)] = &s_pattern;
    modes_[static_cast<uint8_t>(DelayModeId::Filter)]  = &s_filter;
    modes_[static_cast<uint8_t>(DelayModeId::Lofi)]    = &s_lofi;

    for (int i = 0; i < NUM_MODES; ++i) {
        modes_[i]->Init();
    }
}

DelayMode* ModeRegistry::get(DelayModeId id) {
    return modes_[static_cast<uint8_t>(id)];
}

void ModeRegistry::Reset(DelayModeId id) {
    modes_[static_cast<uint8_t>(id)]->Reset();
}

} // namespace pedal

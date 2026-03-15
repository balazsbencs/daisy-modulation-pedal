#include "mode_registry.h"
#include "chorus_mode.h"
#include "flanger_mode.h"
#include "rotary_mode.h"
#include "vibe_mode.h"
#include "phaser_mode.h"
#include "filter_mode.h"
#include "vintage_trem_mode.h"
#include "formant_mode.h"
#include "pattern_trem_mode.h"
#include "auto_swell_mode.h"
#include "destroyer_mode.h"
#include "stub_modes.h"

namespace pedal {

// Phase 1 modes
static ChorusMode      s_chorus;
static PhaserMode      s_phaser;
static VintageTremMode s_vint_trem;

// Phase 2 modes
static FlangerMode     s_flanger;
static RotaryMode      s_rotary;
static VibeMode        s_vibe;
static FilterMode      s_filter;

// Phase 3 modes
static FormantMode     s_formant;
static PatternTremMode s_patt_trem;
static AutoSwellMode   s_autoswell;
static DestroyerMode   s_destroyer;

// Placeholder passthrough mode (Phase 4)
static PassthroughMode s_quadrature("Quadrature");

void ModeRegistry::Init() {
    modes_[static_cast<uint8_t>(ModModeId::Chorus)]     = &s_chorus;
    modes_[static_cast<uint8_t>(ModModeId::Flanger)]    = &s_flanger;
    modes_[static_cast<uint8_t>(ModModeId::Rotary)]     = &s_rotary;
    modes_[static_cast<uint8_t>(ModModeId::Vibe)]       = &s_vibe;
    modes_[static_cast<uint8_t>(ModModeId::Phaser)]     = &s_phaser;
    modes_[static_cast<uint8_t>(ModModeId::Filter)]     = &s_filter;
    modes_[static_cast<uint8_t>(ModModeId::Formant)]    = &s_formant;
    modes_[static_cast<uint8_t>(ModModeId::VintTrem)]   = &s_vint_trem;
    modes_[static_cast<uint8_t>(ModModeId::PattTrem)]   = &s_patt_trem;
    modes_[static_cast<uint8_t>(ModModeId::AutoSwell)]  = &s_autoswell;
    modes_[static_cast<uint8_t>(ModModeId::Destroyer)]  = &s_destroyer;
    modes_[static_cast<uint8_t>(ModModeId::Quadrature)] = &s_quadrature;

    for (int i = 0; i < NUM_MODES; ++i) {
        modes_[i]->Init();
    }
}

ModMode* ModeRegistry::get(ModModeId id) {
    return modes_[static_cast<uint8_t>(id)];
}

void ModeRegistry::Reset(ModModeId id) {
    modes_[static_cast<uint8_t>(id)]->Reset();
}

} // namespace pedal

#pragma once
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"
#include "hid/disp/oled_display.h"
#include "../config/mod_mode_id.h"
#include "../params/param_set.h"
#include "../audio/bypass.h"
#include "../tempo/tempo_sync.h"
#include <cstdint>

namespace pedal {

// SSD1306 / SSD1309 128x64 over I2C.
using OledType = daisy::OledDisplay<daisy::SSD130xI2c128x64Driver>;

/// Owns the OLED hardware and exposes a single Update() call that rate-limits
/// redraws to approximately 30 fps (configurable via DISPLAY_UPDATE_MS).
class DisplayManager {
public:
    enum class PresetUiEvent {
        None,
        Loaded,
        Saved,
    };

    void Init();

    /// Call once per main loop iteration.
    /// Skips the redraw when called more frequently than DISPLAY_UPDATE_MS.
    /// @param mode    Currently active modulation mode.
    /// @param params  Immutable parameter snapshot for the current block.
    /// @param bypass  Bypass state.
    /// @param tempo   Tempo source state (MIDI / Tap).
    /// @param now_ms  Current time from System::GetNow().
    void Update(ModModeId       mode,
                const ParamSet& params,
                const Bypass&   bypass,
                const TempoSync& tempo,
                int             preset_slot,
                bool            shift_layer_active,
                PresetUiEvent   preset_event,
                uint32_t        now_ms);

private:
    /// Full-screen redraw — always sends a fresh frame to the display.
    void Render(ModModeId        mode,
                const ParamSet&  params,
                const Bypass&    bypass,
                const TempoSync& tempo,
                int              preset_slot,
                bool             shift_layer_active,
                PresetUiEvent    preset_event);

    /// Draw an outlined rectangle with an interior fill proportional to val [0, 1].
    /// @param x   Left edge of the bar.
    /// @param y   Top edge of the bar.
    /// @param w   Total width in pixels (includes 1-pixel border on each side).
    /// @param val Fill fraction in [0, 1]; clamped internally.
    void DrawParamBar(uint8_t x, uint8_t y, uint8_t w, float val);

    /// Map a ModModeId to its short display name.
    static const char* ModeName(ModModeId id);

    OledType oled_;
    uint32_t last_update_ms_ = 0;
};

} // namespace pedal

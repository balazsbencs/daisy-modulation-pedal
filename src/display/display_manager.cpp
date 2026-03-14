#include "display_manager.h"
#include "display_layout.h"
#include "../config/constants.h"

using namespace daisy;

namespace pedal {

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void DisplayManager::Init() {
    OledType::Config cfg;
    // I2C_1 defaults to PB8 (SCL) / PB9 (SDA) on the Daisy Seed.
    cfg.driver_config.transport_config.i2c_config.periph =
        I2CHandle::Config::Peripheral::I2C_1;
    cfg.driver_config.transport_config.i2c_address = OLED_I2C_ADDR;
    oled_.Init(cfg);

    oled_.Fill(false);
    oled_.Update();
    last_update_ms_ = 0;
}

// ---------------------------------------------------------------------------
// Update (rate-limited entry point)
// ---------------------------------------------------------------------------

void DisplayManager::Update(DelayModeId      mode,
                             const ParamSet&  params,
                             const Bypass&    bypass,
                             const TempoSync& tempo,
                             int              preset_slot,
                             bool             shift_layer_active,
                             PresetUiEvent    preset_event,
                             uint32_t         now_ms) {
    if (now_ms - last_update_ms_ < DISPLAY_UPDATE_MS) {
        return;
    }
    last_update_ms_ = now_ms;
    Render(mode, params, bypass, tempo, preset_slot, shift_layer_active, preset_event);
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void DisplayManager::Render(DelayModeId      mode,
                             const ParamSet&  params,
                             const Bypass&    bypass,
                             const TempoSync& tempo,
                             int              preset_slot,
                             bool             shift_layer_active,
                             PresetUiEvent    preset_event) {
    oled_.Fill(false);

    // --- Mode name (large font, top-left) ---
    oled_.SetCursor(layout::MODE_X, layout::MODE_Y);
    oled_.WriteString(ModeName(mode), Font_11x18, true);

    // --- Bypass indicator (top-right) ---
    oled_.SetCursor(layout::BYPASS_X, layout::BYPASS_Y);
    oled_.WriteString(bypass.IsActive() ? "ON" : "BY", Font_7x10, true);

    // --- Active preset slot (top-right) ---
    char slot_label[6] = {'P', static_cast<char>('1' + (preset_slot % 8)), 0, 0, 0, 0};
    oled_.SetCursor(114, 11);
    oled_.WriteString(slot_label, Font_6x8, true);

    // --- Horizontal separator ---
    oled_.DrawLine(0, layout::SEP_Y, layout::SCREEN_W - 1, layout::SEP_Y, true);

    // --- Row 1 labels: Time | Repeats | Mix ---
    oled_.SetCursor(0,  layout::PARAM_ROW1_Y);
    oled_.WriteString("Tm", Font_6x8, true);
    oled_.SetCursor(43, layout::PARAM_ROW1_Y);
    oled_.WriteString("Rp", Font_6x8, true);
    oled_.SetCursor(86, layout::PARAM_ROW1_Y);
    oled_.WriteString("Mx", Font_6x8, true);

    // --- Row 1 bars (width 38 px each) ---
    // time: 0..3 s → normalise to [0,1]
    DrawParamBar(0,  layout::BAR_Y1_TOP, 38, params.time / 3.0f);
    // repeats: 0..0.98 → normalise to [0,1]
    DrawParamBar(43, layout::BAR_Y1_TOP, 38, params.repeats / 0.98f);
    // mix: already [0,1]
    DrawParamBar(86, layout::BAR_Y1_TOP, 38, params.mix);

    // --- Row 2 labels: Filter | Grit | ModSpd | ModDep ---
    oled_.SetCursor(0,  layout::PARAM_ROW2_Y);
    oled_.WriteString("Fi", Font_6x8, true);
    oled_.SetCursor(32, layout::PARAM_ROW2_Y);
    oled_.WriteString("Gr", Font_6x8, true);
    oled_.SetCursor(64, layout::PARAM_ROW2_Y);
    oled_.WriteString("MS", Font_6x8, true);
    oled_.SetCursor(96, layout::PARAM_ROW2_Y);
    oled_.WriteString("MD", Font_6x8, true);

    // --- Row 2 bars (width 28 px each) ---
    // filter: [0,1]
    DrawParamBar(0,  layout::BAR_Y2_TOP, 28, params.filter);
    // grit: [0,1]
    DrawParamBar(32, layout::BAR_Y2_TOP, 28, params.grit);
    // mod_spd: 0.05..10 Hz → normalise to [0,1]
    DrawParamBar(64, layout::BAR_Y2_TOP, 28, params.mod_spd / 10.0f);
    // mod_dep: [0,1]
    DrawParamBar(96, layout::BAR_Y2_TOP, 28, params.mod_dep);

    // --- Tempo source label (bottom-left) ---
    oled_.SetCursor(layout::TEMPO_X, layout::TEMPO_Y);
    if (tempo.HasMidiClock()) {
        oled_.WriteString("MIDI", Font_6x8, true);
    } else if (tempo.HasTap()) {
        oled_.WriteString("TAP", Font_6x8, true);
    }

    // --- Shift-layer and preset feedback ---
    if (shift_layer_active) {
        oled_.SetCursor(88, layout::TEMPO_Y);
        oled_.WriteString("SHF", Font_6x8, true);
    }
    if (preset_event == PresetUiEvent::Loaded) {
        oled_.SetCursor(48, layout::MODE_Y);
        oled_.WriteString("LOAD", Font_6x8, true);
    } else if (preset_event == PresetUiEvent::Saved) {
        oled_.SetCursor(48, layout::MODE_Y);
        oled_.WriteString("SAVE", Font_6x8, true);
    }

    oled_.Update();
}

// ---------------------------------------------------------------------------
// DrawParamBar
// ---------------------------------------------------------------------------

void DisplayManager::DrawParamBar(uint8_t x, uint8_t y, uint8_t w, float val) {
    // Clamp to [0, 1] to guard against parameter normalisation overshoots.
    if (val < 0.0f) { val = 0.0f; }
    if (val > 1.0f) { val = 1.0f; }

    // Outline rectangle.
    oled_.DrawRect(x, y, x + w, y + layout::BAR_H, true, false);

    // Interior fill — leave 1-pixel border on all sides.
    const uint8_t inner_w    = static_cast<uint8_t>(w > 2u ? w - 2u : 0u);
    const uint8_t fill_width = static_cast<uint8_t>(val * static_cast<float>(inner_w));
    if (fill_width > 0) {
        oled_.DrawRect(x + 1u,
                       y + 1u,
                       x + 1u + fill_width,
                       y + layout::BAR_H - 1u,
                       true,
                       true);
    }
}

// ---------------------------------------------------------------------------
// ModeName
// ---------------------------------------------------------------------------

const char* DisplayManager::ModeName(DelayModeId id) {
    switch (id) {
        case DelayModeId::Duck:    return "Duck";
        case DelayModeId::Swell:   return "Swll";
        case DelayModeId::Trem:    return "Trem";
        case DelayModeId::Digital: return "Digi";
        case DelayModeId::Dbucket: return "BBD";
        case DelayModeId::Tape:    return "Tape";
        case DelayModeId::Dual:    return "Dual";
        case DelayModeId::Pattern: return "Patt";
        case DelayModeId::Filter:  return "Filt";
        case DelayModeId::Lofi:    return "LoFi";
        default:                   return "????";
    }
}

} // namespace pedal

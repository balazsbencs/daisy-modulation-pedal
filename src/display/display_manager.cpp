#include "display_manager.h"
#include "display_layout.h"
#include "../config/constants.h"

using namespace daisy;

namespace pedal {

// ---------------------------------------------------------------------------
// FormatSpeed — flash-conservative string formatter (no printf)
// Writes a null-terminated value+unit string into out[].
// buf must be at least 8 bytes.
// ---------------------------------------------------------------------------

namespace {

static uint8_t PutUInt(char* p, uint32_t v) {
    if (v == 0) { p[0] = '0'; return 1; }
    char tmp[6];
    uint8_t n = 0;
    while (v) { tmp[n++] = static_cast<char>('0' + v % 10); v /= 10; }
    for (uint8_t i = 0; i < n; ++i) p[i] = tmp[n - 1u - i];
    return n;
}

// Returns a formatted speed string: "3.5Hz", "440Hz", "120ms", "12x".
static void FormatSpeed(ModModeId mode, float speed, char* out) {
    uint8_t pos = 0;

    if (mode == ModModeId::AutoSwell) {
        // speed is attack time in seconds (0.010..0.500) → display as ms
        uint32_t ms = static_cast<uint32_t>(speed * 1000.0f + 0.5f);
        pos += PutUInt(out + pos, ms);
        out[pos++] = 'm'; out[pos++] = 's';

    } else if (mode == ModModeId::Destroyer) {
        // speed is decimation ratio (1..48)
        uint32_t r = static_cast<uint32_t>(speed + 0.5f);
        pos += PutUInt(out + pos, r);
        out[pos++] = 'x';

    } else if (speed >= 10.0f) {
        // 10 Hz and above — whole number
        pos += PutUInt(out + pos, static_cast<uint32_t>(speed + 0.5f));
        out[pos++] = 'H'; out[pos++] = 'z';

    } else {
        // below 10 Hz — one decimal place, e.g. "3.5Hz"
        uint32_t integer = static_cast<uint32_t>(speed);
        uint32_t frac    = static_cast<uint32_t>((speed - static_cast<float>(integer)) * 10.0f + 0.5f);
        if (frac >= 10) { integer++; frac = 0; }
        pos += PutUInt(out + pos, integer);
        out[pos++] = '.';
        out[pos++] = static_cast<char>('0' + frac);
        out[pos++] = 'H'; out[pos++] = 'z';
    }

    out[pos] = '\0';
}

} // namespace

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

void DisplayManager::Update(ModModeId        mode,
                             const ParamSet&  params,
                             const Bypass&    bypass,
                             const TempoSync& tempo,
                             int              preset_slot,
                             bool             shift_layer_active,
                             PresetUiEvent    preset_event,
                             int              mode_encoder_delta,
                             const int        param_encoder_delta[4],
                             uint32_t         now_ms) {
    pending_mode_delta_ += mode_encoder_delta;
    for (int i = 0; i < 4; ++i) {
        pending_param_delta_[i] += param_encoder_delta[i];
    }

    if (now_ms - last_update_ms_ < DISPLAY_UPDATE_MS) {
        return;
    }
    last_update_ms_ = now_ms;
    Render(mode,
           params,
           bypass,
           tempo,
           preset_slot,
           shift_layer_active,
           preset_event,
           pending_mode_delta_,
           pending_param_delta_,
           now_ms);

    pending_mode_delta_ = 0;
    for (int i = 0; i < 4; ++i) {
        pending_param_delta_[i] = 0;
    }
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void DisplayManager::Render(ModModeId        mode,
                             const ParamSet&  params,
                             const Bypass&    bypass,
                             const TempoSync& tempo,
                             int              preset_slot,
                             bool             shift_layer_active,
                             PresetUiEvent    preset_event,
                             int              mode_encoder_delta,
                             const int        param_encoder_delta[4],
                             uint32_t         now_ms) {
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

    // --- Row 1 labels: Speed | Depth | Mix ---
    oled_.SetCursor(0,  layout::PARAM_ROW1_Y);
    oled_.WriteString("Spd", Font_6x8, true);
    oled_.SetCursor(43, layout::PARAM_ROW1_Y);
    oled_.WriteString("Dpt", Font_6x8, true);
    oled_.SetCursor(86, layout::PARAM_ROW1_Y);
    oled_.WriteString("Mx", Font_6x8, true);

    // --- Row 1: speed value as text, depth/mix as bars ---
    char speed_buf[8];
    FormatSpeed(mode, params.speed, speed_buf);
    oled_.SetCursor(0, layout::BAR_Y1_TOP);
    oled_.WriteString(speed_buf, Font_6x8, true);
    // depth: already [0,1]
    DrawParamBar(43, layout::BAR_Y1_TOP, 38, params.depth);
    // mix: already [0,1]
    DrawParamBar(86, layout::BAR_Y1_TOP, 38, params.mix);

    // --- Row 2 labels: Tone | P1 | P2 | Level ---
    oled_.SetCursor(0,  layout::PARAM_ROW2_Y);
    oled_.WriteString("Tn", Font_6x8, true);
    oled_.SetCursor(32, layout::PARAM_ROW2_Y);
    oled_.WriteString("P1", Font_6x8, true);
    oled_.SetCursor(64, layout::PARAM_ROW2_Y);
    oled_.WriteString("P2", Font_6x8, true);
    oled_.SetCursor(96, layout::PARAM_ROW2_Y);
    oled_.WriteString("Lv", Font_6x8, true);

    // --- Row 2 bars (width 28 px each) ---
    // tone: [0,1]
    DrawParamBar(0,  layout::BAR_Y2_TOP, 28, params.tone);
    // p1: [0,1]
    DrawParamBar(32, layout::BAR_Y2_TOP, 28, params.p1);
    // p2: [0,1]
    DrawParamBar(64, layout::BAR_Y2_TOP, 28, params.p2);
    // level: 0..2 → normalise to [0,1]
    DrawParamBar(96, layout::BAR_Y2_TOP, 28, params.level / 2.0f);

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

const char* DisplayManager::ModeName(ModModeId id) {
    switch (id) {
        case ModModeId::Chorus:     return "Chorus";
        case ModModeId::Flanger:    return "Flanger";
        case ModModeId::Rotary:     return "Rotary";
        case ModModeId::Vibe:       return "Vibe";
        case ModModeId::Phaser:     return "Phaser";
        case ModModeId::Filter:     return "Filter";
        case ModModeId::Formant:    return "Formant";
        case ModModeId::VintTrem:   return "VinTrem";
        case ModModeId::PattTrem:   return "PattTrem";
        case ModModeId::AutoSwell:  return "AutoSwell";
        case ModModeId::Destroyer:  return "Destroy";
        case ModModeId::Quadrature: return "Quadrature";
        default:                    return "????";
    }
}

} // namespace pedal

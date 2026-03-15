#pragma once
#include <cstddef>
#include <cstdint>

namespace pedal {

constexpr float  SAMPLE_RATE     = 48000.0f;
constexpr size_t BLOCK_SIZE      = 48;
constexpr float  INV_SAMPLE_RATE = 1.0f / SAMPLE_RATE;

// Modulation delay lines are short (max 20ms for chorus/flanger)
constexpr size_t MAX_MOD_DELAY_SAMPLES = static_cast<size_t>(SAMPLE_RATE * 0.025f); // 25ms

// Number of modulation modes
constexpr int NUM_MODES = 12;

// Number of editable parameters.
constexpr int NUM_PARAMS = 7;
// Backward-compatible alias for older code paths.
constexpr int NUM_POTS = NUM_PARAMS;

// Smoothing coefficient for pot values (one-pole LP).
// α = 1 − e^(−T/τ), T = 1 ms loop period, τ = 30 ms → α ≈ 0.033
constexpr float POT_SMOOTH = 0.033f;

// Encoder parameter edit behavior.
constexpr float PARAM_STEP_SLOW = 0.005f;
constexpr float PARAM_STEP_FAST = 0.015f;
constexpr uint32_t ENCODER_FAST_WINDOW_MS = 40;

// Presets
constexpr int      PRESET_SLOT_COUNT   = 8;
constexpr uint32_t PRESET_HOLD_MS      = 700;
constexpr uint32_t PRESET_STATUS_MS    = 1000;

// MIDI CC defaults (14-20)
constexpr uint8_t CC_SPEED = 14;
constexpr uint8_t CC_DEPTH = 15;
constexpr uint8_t CC_MIX   = 16;
constexpr uint8_t CC_TONE  = 17;
constexpr uint8_t CC_P1    = 18;
constexpr uint8_t CC_P2    = 19;
constexpr uint8_t CC_LEVEL = 20;

// Display
constexpr uint32_t DISPLAY_UPDATE_MS = 33; // ~30fps
constexpr uint8_t  OLED_I2C_ADDR     = 0x3C;

// Tap tempo
constexpr int      TAP_MAX_TAPS   = 4;
constexpr float    TAP_MIN_BPM    = 40.0f;
constexpr float    TAP_MAX_BPM    = 240.0f;
constexpr uint32_t TAP_TIMEOUT_MS = 2000;

} // namespace pedal

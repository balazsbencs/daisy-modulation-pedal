#pragma once
#include <cstdint>

namespace pedal {
namespace layout {

// Physical screen dimensions (SSD1306 128x64).
constexpr uint8_t SCREEN_W = 128;
constexpr uint8_t SCREEN_H = 64;

// Mode name — top-left corner, rendered with Font_11x18.
constexpr uint8_t MODE_X = 0;
constexpr uint8_t MODE_Y = 0;

// Horizontal separator between the mode line and the parameter area.
constexpr uint8_t SEP_Y = 14;

// Parameter label rows (Font_6x8).
constexpr uint8_t PARAM_ROW1_Y = 18; // Time | Repeats | Mix
constexpr uint8_t PARAM_ROW2_Y = 38; // Filter | Grit | ModSpd | ModDep

// Top edges of the fill-bar rows (each bar is BAR_H pixels tall).
constexpr uint8_t BAR_H      = 6;
constexpr uint8_t BAR_Y1_TOP = 27;
constexpr uint8_t BAR_Y2_TOP = 47;

// Bypass status label — top-right area.
constexpr uint8_t BYPASS_X = 100;
constexpr uint8_t BYPASS_Y = 0;

// Tempo source label — bottom-left.
constexpr uint8_t TEMPO_X = 0;
constexpr uint8_t TEMPO_Y = 56;

} // namespace layout
} // namespace pedal

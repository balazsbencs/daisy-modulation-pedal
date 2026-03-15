# ── Daisy Modulation Pedal ────────────────────────────────────────────────────
TARGET = modulation

# ── Source files ──────────────────────────────────────────────────────────────
CPP_SOURCES = \
    src/main.cpp \
    src/hardware/controls.cpp \
    src/audio/audio_engine.cpp \
    src/audio/bypass.cpp \
    src/params/param_set.cpp \
    src/params/param_map.cpp \
    src/dsp/delay_line_sdram.cpp \
    src/dsp/envelope_follower.cpp \
    src/dsp/lfo.cpp \
    src/dsp/tone_filter.cpp \
    src/modes/chorus_mode.cpp \
    src/modes/flanger_mode.cpp \
    src/modes/rotary_mode.cpp \
    src/modes/vibe_mode.cpp \
    src/modes/phaser_mode.cpp \
    src/modes/filter_mode.cpp \
    src/modes/vintage_trem_mode.cpp \
    src/modes/mode_registry.cpp \
    src/midi/midi_handler.cpp \
    src/display/display_manager.cpp \
    src/presets/preset_manager.cpp \
    src/tempo/tap_tempo.cpp \
    src/tempo/tempo_sync.cpp

# ── Library paths ─────────────────────────────────────────────────────────────
LIBDAISY_DIR = third_party/libDaisy
DAISYSP_DIR  = third_party/DaisySP

# ── Additional include directories ───────────────────────────────────────────
# 'src' is added so that all #include "..." paths in source files resolve
# relative to the src/ root without needing ../.. traversal.
C_INCLUDES += -Isrc

# ── Compiler optimisation ─────────────────────────────────────────────────────
# Override libDaisy's default -O2 with size-optimised build.
# Suppresses loop unrolling and aggressive inlining that inflate flash.
OPT = -Os

# ── Pull in the libDaisy build system ─────────────────────────────────────────
# This Makefile fragment defines the compiler toolchain, linker script,
# flash/debug targets, and all required flags for the STM32H750.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# ── Daisy Delay Pedal ─────────────────────────────────────────────────────────
TARGET = delay

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
    src/modes/digital_delay.cpp \
    src/modes/duck_delay.cpp \
    src/modes/swell_delay.cpp \
    src/modes/trem_delay.cpp \
    src/modes/dbucket_delay.cpp \
    src/modes/tape_delay.cpp \
    src/modes/dual_delay.cpp \
    src/modes/pattern_delay.cpp \
    src/modes/filter_delay.cpp \
    src/modes/lofi_delay.cpp \
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

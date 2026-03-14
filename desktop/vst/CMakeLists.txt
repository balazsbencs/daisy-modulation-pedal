cmake_minimum_required(VERSION 3.22)

project(DaisyDelayPlugin VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

# JUCE is fetched at configure time.
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(DaisyDelayPlugin
  COMPANY_NAME "Daisy"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE Ddly
  PLUGIN_CODE Dly1
  FORMATS VST3 Standalone
  PRODUCT_NAME "Daisy Delay"
)

target_sources(DaisyDelayPlugin PRIVATE
  src/PluginProcessor.cpp
  src/PluginEditor.cpp

  ../../src/dsp/delay_line_sdram.cpp
  ../../src/dsp/envelope_follower.cpp
  ../../src/dsp/lfo.cpp
  ../../src/dsp/tone_filter.cpp

  ../../src/params/param_set.cpp
  ../../src/params/param_map.cpp

  ../../src/modes/mode_registry.cpp
  ../../src/modes/digital_delay.cpp
  ../../src/modes/duck_delay.cpp
  ../../src/modes/swell_delay.cpp
  ../../src/modes/trem_delay.cpp
  ../../src/modes/dbucket_delay.cpp
  ../../src/modes/tape_delay.cpp
  ../../src/modes/dual_delay.cpp
  ../../src/modes/pattern_delay.cpp
  ../../src/modes/filter_delay.cpp
  ../../src/modes/lofi_delay.cpp
)

target_include_directories(DaisyDelayPlugin PRIVATE
  include
  src
  ../..
  ../../src
)

target_compile_definitions(DaisyDelayPlugin PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)

target_link_libraries(DaisyDelayPlugin PRIVATE
  juce::juce_audio_utils
  juce::juce_dsp
)

# Recommended JUCE compile/link flags.
juce_generate_juce_header(DaisyDelayPlugin)

# User-local install targets (macOS)
set(USER_VST3_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
set(USER_APP_DIR "$ENV{HOME}/Applications")

add_custom_target(install-user-vst3
  COMMAND ${CMAKE_COMMAND} -E make_directory "${USER_VST3_DIR}"
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${USER_VST3_DIR}/Daisy Delay.vst3"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "$<TARGET_BUNDLE_DIR:DaisyDelayPlugin_VST3>"
          "${USER_VST3_DIR}/Daisy Delay.vst3"
  DEPENDS DaisyDelayPlugin_VST3
  COMMENT "Installing Daisy Delay.vst3 to ${USER_VST3_DIR}"
)

add_custom_target(install-user-standalone
  COMMAND ${CMAKE_COMMAND} -E make_directory "${USER_APP_DIR}"
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${USER_APP_DIR}/Daisy Delay.app"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "$<TARGET_BUNDLE_DIR:DaisyDelayPlugin_Standalone>"
          "${USER_APP_DIR}/Daisy Delay.app"
  DEPENDS DaisyDelayPlugin_Standalone
  COMMENT "Installing Daisy Delay.app to ${USER_APP_DIR}"
)

add_custom_target(install-user
  DEPENDS install-user-vst3 install-user-standalone
)

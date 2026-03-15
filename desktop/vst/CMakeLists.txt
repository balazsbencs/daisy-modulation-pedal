cmake_minimum_required(VERSION 3.22)

project(DaisyModulationPlugin VERSION 0.1.0 LANGUAGES C CXX)

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

juce_add_plugin(DaisyModulationPlugin
  COMPANY_NAME "Daisy"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE DMod
  PLUGIN_CODE Mod1
  FORMATS VST3 Standalone
  PRODUCT_NAME "Daisy Modulation"
)

target_sources(DaisyModulationPlugin PRIVATE
  src/PluginProcessor.cpp
  src/PluginEditor.cpp

  ../../src/dsp/delay_line_sdram.cpp
  ../../src/dsp/envelope_follower.cpp
  ../../src/dsp/lfo.cpp
  ../../src/dsp/tone_filter.cpp

  ../../src/params/param_set.cpp
  ../../src/params/param_map.cpp

  ../../src/modes/mode_registry.cpp
  ../../src/modes/chorus_mode.cpp
  ../../src/modes/phaser_mode.cpp
  ../../src/modes/vintage_trem_mode.cpp
)

target_include_directories(DaisyModulationPlugin PRIVATE
  include
  src
  ../..
  ../../src
)

target_compile_definitions(DaisyModulationPlugin PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)

target_link_libraries(DaisyModulationPlugin PRIVATE
  juce::juce_audio_utils
  juce::juce_dsp
)

# Recommended JUCE compile/link flags.
juce_generate_juce_header(DaisyModulationPlugin)

# User-local install targets (macOS)
set(USER_VST3_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
set(USER_APP_DIR "$ENV{HOME}/Applications")

add_custom_target(install-user-vst3
  COMMAND ${CMAKE_COMMAND} -E make_directory "${USER_VST3_DIR}"
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${USER_VST3_DIR}/Daisy Modulation.vst3"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "$<TARGET_BUNDLE_DIR:DaisyModulationPlugin_VST3>"
          "${USER_VST3_DIR}/Daisy Modulation.vst3"
  DEPENDS DaisyModulationPlugin_VST3
  COMMENT "Installing Daisy Modulation.vst3 to ${USER_VST3_DIR}"
)

add_custom_target(install-user-standalone
  COMMAND ${CMAKE_COMMAND} -E make_directory "${USER_APP_DIR}"
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${USER_APP_DIR}/Daisy Modulation.app"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "$<TARGET_BUNDLE_DIR:DaisyModulationPlugin_Standalone>"
          "${USER_APP_DIR}/Daisy Modulation.app"
  DEPENDS DaisyModulationPlugin_Standalone
  COMMENT "Installing Daisy Modulation.app to ${USER_APP_DIR}"
)

add_custom_target(install-user
  DEPENDS install-user-vst3 install-user-standalone
)

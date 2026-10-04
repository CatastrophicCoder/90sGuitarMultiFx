cmake_minimum_required(VERSION 3.25)

# Must be set before project(): JUCE bakes the deployment target into the plugin bundles.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(NinetiesMultiFx VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(A5_BUILD_TESTS "Build the unit and integration tests" ON)
option(A5_COPY_PLUGIN "Copy the built plugins into the user's plugin folders" OFF)

include(cmake/Dependencies.cmake)

# --- Engine -------------------------------------------------------------------
# Pure C++ with no JUCE dependency, so the signal chain can be tested and later wrapped for other
# plugin formats (CLAP) without touching DSP code. See docs/architecture.md.
add_library(a5_engine STATIC
    src/core/A5Processor.cpp)

target_include_directories(a5_engine PUBLIC src)
target_compile_features(a5_engine PUBLIC cxx_std_20)

# Linked into the plugins' shared libraries; Linux does not build static libraries this way by
# default, and the VST3 link fails without it.
set_target_properties(a5_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(a5_engine PRIVATE /W4)
else()
    target_compile_options(a5_engine PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

# --- Plugin -------------------------------------------------------------------
# The product name is a working title: the plan forbids reusing the original product's name or trade
# dress until naming has been reviewed. The manufacturer code is Catastrophic Audio's, shared with
# its other plugins; PLUGIN_CODE must be unique among them. Changing either after release breaks
# saved host sessions.
set(A5_PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND A5_PLUGIN_FORMATS AU)
endif()

juce_add_plugin(A5Plugin
    PRODUCT_NAME                "Nineties Multi-FX"
    COMPANY_NAME                "Catastrophic Audio"
    BUNDLE_ID                   "com.catastrophicaudio.ninetiesmultifx"
    PLUGIN_MANUFACTURER_CODE    Ctcd
    PLUGIN_CODE                 Nmf1
    FORMATS                     ${A5_PLUGIN_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${A5_COPY_PLUGIN}
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Distortion)

target_sources(A5Plugin PRIVATE
    src/core/ParameterLayout.cpp
    src/core/PresetState.cpp
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
    src/ui/MainPanel.cpp
    src/ui/EffectSection.cpp)

target_compile_definitions(A5Plugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(A5Plugin
    PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_warning_flags
    PUBLIC
        a5_engine
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# --- Tests --------------------------------------------------------------------
if(A5_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

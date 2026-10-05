cmake_minimum_required(VERSION 3.25)

# Must be set before project(): JUCE bakes the deployment target into the plugin bundles.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(FiveAMultiFx VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(A5_BUILD_TESTS "Build the unit and integration tests" ON)

# Only a Release build installs itself into ~/Library/Audio/Plug-Ins. Debug and Release install to
# the same place, so with both on, whichever built last is what a DAW loads, and a Debug plugin
# there looks like a performance problem rather than a misconfiguration. Debug plugins are still
# built, in build/A5Plugin_artefacts/Debug, for pluginval and for loading by hand.
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(a5_copy_default ON)
else()
    set(a5_copy_default OFF)
endif()
option(A5_COPY_PLUGIN "Install the built AU and VST3 into the user's plugin folders" ${a5_copy_default})

# Windows: link the C++ runtime statically, so the plugin does not depend on VCRUNTIME140.dll being
# installed beside it. A host that cannot find that DLL fails to load the plugin with no useful
# message. A variable rather than a per-target property so every target agrees, Catch2 included;
# mixing runtimes does not link.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE 9.0.2 and Catch2 v3.9.1 are git submodules pinned to their release tags.
# Clone with --recurse-submodules, or run `git submodule update --init` after cloning.
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt)
    message(FATAL_ERROR "external/JUCE is empty. Run: git submodule update --init")
endif()
add_subdirectory(external/JUCE)

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
# The manufacturer code is Catastrophic Audio's, shared with its other plugins; PLUGIN_CODE must be
# unique among them. Changing either, or the bundle ID, after release breaks saved host sessions.
# No standalone: the plugin is an effects unit for between an amp and a cabinet, and on its own it
# would play a raw, unamplified guitar (owner's decision 2026-10-05).
set(A5_PLUGIN_FORMATS VST3)
if(APPLE)
    list(APPEND A5_PLUGIN_FORMATS AU)
endif()

juce_add_plugin(A5Plugin
    PRODUCT_NAME                "Five-A MultiFX Processor"
    COMPANY_NAME                "Catastrophic Audio"
    BUNDLE_ID                   "com.catastrophicaudio.fivea"
    PLUGIN_MANUFACTURER_CODE    Ctcd
    PLUGIN_CODE                 Nmf1
    FORMATS                     ${A5_PLUGIN_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${A5_COPY_PLUGIN}   # Release only by default; see above
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
    add_subdirectory(external/Catch2)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/Catch2/extras)
    add_subdirectory(tests)
endif()

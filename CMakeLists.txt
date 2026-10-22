cmake_minimum_required(VERSION 3.22)

project(Cortex VERSION 0.8.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(CORTEX_BUILD_TESTS "Build unit tests" ON)

# Version + commit shown in the editor, so it's obvious which build a host loaded.
# Re-runs on every commit because CMake watches .git/logs/HEAD.
find_package(Git QUIET)
set(CORTEX_GIT_HASH "unknown")
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" OUTPUT_VARIABLE CORTEX_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(EXISTS "${PROJECT_SOURCE_DIR}/.git/logs/HEAD")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/.git/logs/HEAD")
    endif()
endif()
configure_file(cmake/Version.h.in "${PROJECT_BINARY_DIR}/generated/Version.h" @ONLY)

include(FetchContent)
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(juce)

set(CORTEX_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND CORTEX_FORMATS AU)
endif()

set(CORTEX_DSP_SOURCES
    ${PROJECT_SOURCE_DIR}/Source/dsp/STFT.cpp
    ${PROJECT_SOURCE_DIR}/Source/dsp/Cepstrum.cpp
    ${PROJECT_SOURCE_DIR}/Source/dsp/CepstralMorph.cpp
    ${PROJECT_SOURCE_DIR}/Source/dsp/PhaseBlend.cpp
    ${PROJECT_SOURCE_DIR}/Source/dsp/CutoffModulator.cpp
    ${PROJECT_SOURCE_DIR}/Source/brain/Block.cpp
    ${PROJECT_SOURCE_DIR}/Source/brain/Brain.cpp
    ${PROJECT_SOURCE_DIR}/Source/brain/BrainBuilder.cpp
    ${PROJECT_SOURCE_DIR}/Source/brain/Renderer.cpp
    ${PROJECT_SOURCE_DIR}/Source/state/BrainCache.cpp
    ${PROJECT_SOURCE_DIR}/Source/synth/MonoSynth.cpp)

set(CORTEX_UI_SOURCES
    ${PROJECT_SOURCE_DIR}/Source/ui/XYPad.cpp
    ${PROJECT_SOURCE_DIR}/Source/ui/CepstrumView.cpp
    ${PROJECT_SOURCE_DIR}/Source/ui/BrainMap.cpp
    ${PROJECT_SOURCE_DIR}/Source/ui/ParamPanel.cpp)

# Both plugins are built from the same sources; the processor picks effect or
# instrument behaviour from JucePlugin_IsSynth.
# Company name, website, bundle ID and manufacturer code are intentionally
# unset (JUCE defaults) until the publisher identity is decided.
function(cortex_add_plugin target)
    juce_add_plugin(${target}
        FORMATS ${CORTEX_FORMATS}
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        ${ARGN})

    target_sources(${target} PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Parameters.cpp
        ${CORTEX_DSP_SOURCES}
        ${CORTEX_UI_SOURCES})

    target_include_directories(${target} PRIVATE Source "${PROJECT_BINARY_DIR}/generated")

    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

cortex_add_plugin(Cortex
    PLUGIN_CODE Crtx
    PRODUCT_NAME "Cortex"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    VST3_CATEGORIES Fx Spectral
    AU_MAIN_TYPE kAudioUnitType_Effect)

cortex_add_plugin(CortexInstrument
    PLUGIN_CODE Crti
    PRODUCT_NAME "Cortex Instrument"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

if(CORTEX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

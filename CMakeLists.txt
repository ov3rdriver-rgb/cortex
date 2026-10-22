cmake_minimum_required(VERSION 3.22)

project(Cortex VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(CORTEX_BUILD_TESTS "Build unit tests" ON)

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

juce_add_plugin(Cortex
    # Company name, website, bundle ID and manufacturer code are intentionally
    # unset (JUCE defaults) until the publisher identity is decided.
    PLUGIN_CODE Crtx
    PRODUCT_NAME "Cortex"
    FORMATS ${CORTEX_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Spectral
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Cortex PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp)

target_include_directories(Cortex PRIVATE Source)

target_compile_definitions(Cortex PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Cortex
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(CORTEX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

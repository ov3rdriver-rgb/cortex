juce_add_console_app(CortexTests PRODUCT_NAME "CortexTests")

target_sources(CortexTests PRIVATE
    TestMain.cpp
    ProcessorTests.cpp
    ${PROJECT_SOURCE_DIR}/Source/PluginProcessor.cpp
    ${PROJECT_SOURCE_DIR}/Source/PluginEditor.cpp
    ${PROJECT_SOURCE_DIR}/Source/Parameters.cpp)

target_include_directories(CortexTests PRIVATE ${PROJECT_SOURCE_DIR}/Source)

target_compile_definitions(CortexTests PRIVATE
    JucePlugin_Name="Cortex"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_UNIT_TESTS=1)

target_link_libraries(CortexTests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

add_test(NAME CortexTests COMMAND CortexTests)

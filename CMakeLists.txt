cmake_minimum_required(VERSION 3.22)

project(SuperSimpleSampler VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to JUCE - adjust if needed
set(JUCE_PATH "$ENV{HOME}/JUCE")
add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)

juce_add_plugin(SuperSimpleSampler
    COMPANY_NAME "ALDEN HAMMERSMITH"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE AldH
    PLUGIN_CODE SSSa
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "ALDENHammersmith_Super Simple Sampler"
    BUNDLE_ID "com.aldenhammersmith.supersimplesampler"
)

target_sources(SuperSimpleSampler
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/InstrumentLoader.cpp
)

target_compile_definitions(SuperSimpleSampler
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SuperSimpleSampler
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

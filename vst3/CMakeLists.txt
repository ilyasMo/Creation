cmake_minimum_required(VERSION 3.22)
project(Afterglow VERSION 0.2.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(AFTERGLOW_CORE_TESTS_ONLY "Build the portable processing tests without JUCE" OFF)
if(AFTERGLOW_CORE_TESTS_ONLY)
    enable_testing()
    add_executable(afterglow_core_tests tests/GainStageTests.cpp)
    add_test(NAME afterglow_core_tests COMMAND afterglow_core_tests)
    add_executable(afterglow_delay_tests tests/DelayEngineTests.cpp)
    add_test(NAME afterglow_delay_tests COMMAND afterglow_delay_tests)
    add_executable(afterglow_feedback_tests tests/FeedbackColourTests.cpp)
    add_test(NAME afterglow_feedback_tests COMMAND afterglow_feedback_tests)
    return()
endif()
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
include(FetchContent)
FetchContent_Declare(JUCE
    URL https://codeload.github.com/juce-framework/JUCE/tar.gz/refs/tags/8.0.15
    URL_HASH SHA256=0eafd736286c6e7bb6958d79fce18d42cb654b8e231890d327b7cece379bd156
)
FetchContent_MakeAvailable(JUCE)
juce_add_plugin(Afterglow
    COMPANY_NAME "Creation Audio"
    BUNDLE_ID "audio.creation.afterglow"
    PLUGIN_MANUFACTURER_CODE CrAu
    PLUGIN_CODE Afgw
    FORMATS VST3 Standalone
    PRODUCT_NAME "Afterglow"
    DESCRIPTION "Afterglow VST3 development starter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
)
target_sources(Afterglow PRIVATE Source/PluginProcessor.cpp Source/PluginProcessor.h Source/GainStage.h Source/DelayEngine.h Source/FeedbackColour.h)
target_compile_definitions(Afterglow PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
)
target_link_libraries(Afterglow PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

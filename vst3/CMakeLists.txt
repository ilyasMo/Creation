cmake_minimum_required(VERSION 3.22)
project(Afterglow VERSION 0.5.0 LANGUAGES C CXX)
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
    add_executable(afterglow_tape_tests tests/TapeEngineTests.cpp)
    add_test(NAME afterglow_tape_tests COMMAND afterglow_tape_tests)
    add_executable(afterglow_tape_render tests/RenderTape.cpp)
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
juce_add_binary_data(AfterglowArtwork SOURCES Assets/Faceplate.png Assets/Caveat.ttf)
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
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
)
target_sources(Afterglow PRIVATE Source/PluginProcessor.cpp Source/PluginProcessor.h Source/GainStage.h Source/DelayEngine.h Source/TapeEngine.h Source/FeedbackColour.h Source/PluginEditor.cpp Source/PluginEditor.h Source/ParameterText.h)
target_compile_definitions(Afterglow PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
)
target_link_libraries(Afterglow PRIVATE juce::juce_audio_utils AfterglowArtwork
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

option(AFTERGLOW_EDITOR_CHECKS "Build native editor integration checks and screenshots" OFF)
if(AFTERGLOW_EDITOR_CHECKS)
    juce_add_console_app(AfterglowEditorChecks PRODUCT_NAME "AfterglowEditorChecks")
    target_sources(AfterglowEditorChecks PRIVATE tests/EditorChecks.cpp)
    target_include_directories(AfterglowEditorChecks PRIVATE Source $<TARGET_PROPERTY:Afterglow,INCLUDE_DIRECTORIES>)
    target_compile_definitions(AfterglowEditorChecks PRIVATE $<TARGET_PROPERTY:Afterglow,COMPILE_DEFINITIONS>)
    target_link_libraries(AfterglowEditorChecks PRIVATE Afterglow)
endif()

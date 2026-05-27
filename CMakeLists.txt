cmake_minimum_required(VERSION 3.22)
project(Pigasus VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

# JUCE dependency.
#   If a sibling ../JUCE folder exists (local development), use it.
#   Otherwise fetch JUCE 8.0.4 from GitHub so the repo builds out-of-the-box.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    message(STATUS "Pigasus: using local JUCE at ../JUCE")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE ${CMAKE_BINARY_DIR}/JUCE)
else()
    message(STATUS "Pigasus: fetching JUCE 8.0.4 from GitHub (first build only)")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Pigasus
    COMPANY_NAME "AnperAudio"
    BUNDLE_ID com.anperaudio.pigasus
    PLUGIN_MANUFACTURER_CODE Anpr
    PLUGIN_CODE Pgss
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Pigasus"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx Distortion Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Pigasus PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

juce_add_binary_data(PigasusData
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/pig_calm.png
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/pig_grumpy.png
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/pig_angry.png
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/pig_rage.png)

target_link_libraries(Pigasus PRIVATE PigasusData)

target_compile_definitions(Pigasus PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Pigasus PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_plugin_client
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

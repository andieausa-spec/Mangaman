cmake_minimum_required(VERSION 3.22)

project(Madonnator VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Auf dem Mac: Universal Binary (Apple Silicon + Intel), ab macOS 11
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# JUCE wird beim ersten CMake-Lauf automatisch von GitHub geladen.
# Alternativ: -DJUCE_DIR=/pfad/zu/JUCE angeben.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Madonnator
    COMPANY_NAME "Andreas Engel"
    COMPANY_COPYRIGHT "Andreas Engel, Darmstadt, Deutschland"
    BUNDLE_ID "com.andi.madonnator"
    PLUGIN_MANUFACTURER_CODE Andi
    PLUGIN_CODE Mdnr
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Synth Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "MADONNATOR")

juce_generate_juce_header(Madonnator)

target_sources(Madonnator PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Madonnator PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Madonnator
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optionaler Selbsttest (cmake -DMADONNATOR_TESTS=ON)
option(MADONNATOR_TESTS "Render-Selbsttest bauen" OFF)
if(MADONNATOR_TESTS)
    juce_add_console_app(MadonnatorRenderTest)
    target_sources(MadonnatorRenderTest PRIVATE Tests/RenderTest.cpp)
    target_include_directories(MadonnatorRenderTest PRIVATE
        $<TARGET_PROPERTY:Madonnator,INCLUDE_DIRECTORIES>)
    target_link_libraries(MadonnatorRenderTest PRIVATE Madonnator)
endif()

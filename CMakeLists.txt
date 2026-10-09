cmake_minimum_required(VERSION 3.22)

project(Mangaman VERSION 0.6.0)

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

juce_add_plugin(Mangaman
    COMPANY_NAME "Andreas Engel"
    COMPANY_COPYRIGHT "Andreas Engel, Darmstadt, Deutschland"
    BUNDLE_ID "com.andi.mangaman"
    PLUGIN_MANUFACTURER_CODE Andi
    PLUGIN_CODE Mgmn
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "TRENDY ANDY")

juce_generate_juce_header(Mangaman)

target_sources(Mangaman PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Mangaman PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Mangaman
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optionaler Selbsttest (cmake -DMANGAMAN_TESTS=ON)
option(MANGAMAN_TESTS "Render-Selbsttest bauen" OFF)
if(MANGAMAN_TESTS)
    juce_add_console_app(MangamanRenderTest)
    target_sources(MangamanRenderTest PRIVATE Tests/RenderTest.cpp)
    target_include_directories(MangamanRenderTest PRIVATE
        $<TARGET_PROPERTY:Mangaman,INCLUDE_DIRECTORIES>)
    target_link_libraries(MangamanRenderTest PRIVATE Mangaman)
endif()

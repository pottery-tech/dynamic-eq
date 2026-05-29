cmake_minimum_required(VERSION 3.22)
project(DynamicEQ VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch JUCE ──────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ── Plugin target ────────────────────────────────────────────────────────────
juce_add_plugin(DynamicEQ
    COMPANY_NAME            "YourName"
    PLUGIN_MANUFACTURER_CODE Joel
    PLUGIN_CODE             DyEQ
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "Dynamic EQ"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         "Fx" "EQ" "Dynamics"
    PLUGIN_MANUFACTURER_CODE JOel
)

juce_generate_juce_header(DynamicEQ)

target_sources(DynamicEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DynamicBand.cpp
    Source/SpectrumAnalyser.cpp
    Source/PresetManager.cpp
)

target_compile_definitions(DynamicEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DynamicEQ PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_opengl
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
)
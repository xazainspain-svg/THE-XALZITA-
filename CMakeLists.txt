cmake_minimum_required(VERSION 3.22)
project(XaLZa VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE, fetched automatically the first time you configure this project
# (CMake will clone it into build/_deps the first time — needs internet on
# the first configure only). Pinned to a real tagged release so the build
# doesn't shift under you.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# --- Embedded fonts --------------------------------------------------------
# Space Grotesk + IBM Plex Mono, the exact two typefaces the original web
# mockup uses (SIL OFL 1.1, free to embed/redistribute — see
# Source/Fonts/LICENSE-FONTS.txt). Compiled straight into the plugin binary
# via JUCE's BinaryData mechanism so the .vst3 never depends on the user
# having these fonts installed on their system.
juce_add_binary_data(XaLZaFonts SOURCES
    Source/Fonts/SpaceGrotesk-Regular.ttf
    Source/Fonts/SpaceGrotesk-SemiBold.ttf
    Source/Fonts/IBMPlexMono-Regular.ttf
    Source/Fonts/IBMPlexMono-Medium.ttf)

juce_add_plugin(XaLZa
    COMPANY_NAME "xaza"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Xaza
    PLUGIN_CODE Xlz1
    FORMATS VST3
    PRODUCT_NAME "The XaLZa")

target_sources(XaLZa PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(XaLZa PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# These only matter when building on Linux (they disable a few X11
# extensions JUCE can optionally use, for systems that don't have every
# X11 dev package installed). Harmless no-ops on Windows/macOS, where
# this whole block is skipped.
if (LINUX)
    target_compile_definitions(XaLZa PUBLIC
        JUCE_USE_XRANDR=0
        JUCE_USE_XINERAMA=0
        JUCE_USE_XCURSOR=0
        JUCE_USE_XRENDER=0
        JUCE_USE_XSHM=0
        JUCE_USE_XCOMPOSITE=0
        JUCE_USE_XINPUT=0
        JUCE_ALSA=0
        JUCE_JACK=0)
endif()

target_link_libraries(XaLZa PRIVATE
    XaLZaFonts
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)

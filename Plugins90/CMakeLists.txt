cmake_minimum_required(VERSION 3.22)
project(XazaFX90 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Filtros para compilar solo una parte (compilar los 90 tarda; JUCE se compila una vez por plugin):
#   -DXFX_ONLY=12              solo el plugin n.o 12 (ver plugins.txt)
#   -DXFX_CATEGORY=Dynamics    solo una categoria (Filters Dynamics Distortion Modulation Time Stereo Creative)
set(XFX_ONLY "" CACHE STRING "Indice (1-90) de un unico plugin a compilar")
set(XFX_CATEGORY "" CACHE STRING "Categoria a compilar")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Prueba del DSP sin JUCE:  cmake --build build --target xfx_dsp_test && ./build/xfx_dsp_test plugins.txt
add_executable(xfx_dsp_test EXCLUDE_FROM_ALL test/dsp_test.cpp)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/plugins.txt" XFX_LIST)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/plugins.txt")

set(idx 0)
foreach(line IN LISTS XFX_LIST)
    string(REPLACE "|" ";" parts "${line}")
    list(GET parts 0 pname)
    list(GET parts 1 pcat)
    math(EXPR n "${idx} + 1")
    set(want TRUE)
    if(NOT "${XFX_ONLY}" STREQUAL "" AND NOT "${XFX_ONLY}" STREQUAL "${n}")
        set(want FALSE)
    endif()
    if(NOT "${XFX_CATEGORY}" STREQUAL "" AND NOT "${XFX_CATEGORY}" STREQUAL "${pcat}")
        set(want FALSE)
    endif()
    if(want)
        if(n LESS 10)
            set(code "Xz0${n}")
        else()
            set(code "Xz${n}")
        endif()
        set(tgt "xfx_${n}")
        juce_add_plugin(${tgt}
            COMPANY_NAME "xaza"
            IS_SYNTH FALSE
            NEEDS_MIDI_INPUT FALSE
            NEEDS_MIDI_OUTPUT FALSE
            IS_MIDI_EFFECT FALSE
            COPY_PLUGIN_AFTER_BUILD FALSE
            PLUGIN_MANUFACTURER_CODE Xaza
            PLUGIN_CODE ${code}
            FORMATS VST3
            PRODUCT_NAME "Xaza ${pname}")
        target_sources(${tgt} PRIVATE Processor.cpp)
        target_compile_definitions(${tgt} PUBLIC
            XFX_INDEX=${idx}
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
        if(LINUX)
            target_compile_definitions(${tgt} PUBLIC
                JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0
                JUCE_USE_XSHM=0 JUCE_USE_XCOMPOSITE=0 JUCE_USE_XINPUT=0 JUCE_ALSA=0 JUCE_JACK=0)
        endif()
        target_link_libraries(${tgt} PRIVATE
            juce::juce_audio_utils
            juce::juce_recommended_config_flags)
    endif()
    math(EXPR idx "${idx} + 1")
endforeach()

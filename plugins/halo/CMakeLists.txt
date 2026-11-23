# LT HALO engine (no JUCE): analysis, composer, performance, WORLD re-synthesis, layers, choir planning
add_library(halo_engine STATIC
    Source/engine/Analysis.cpp
    Source/engine/Composer.cpp
    Source/engine/Render.cpp
    Source/engine/Fx.cpp
    Source/engine/Layers.cpp)
target_include_directories(halo_engine PUBLIC Source/engine)
target_link_libraries(halo_engine PUBLIC lt_world)
set_target_properties(halo_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(halo_engine PUBLIC cxx_std_17)

# ---------------------------------------------------------------- plugin
juce_add_plugin(LTHalo
    PRODUCT_NAME "LT HALO"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.halo
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Lthl
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Generator
    AU_MAIN_TYPE kAudioUnitType_Effect
    ICON_BIG ${CMAKE_SOURCE_DIR}/shared/brand/icon-halo.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LTHalo PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Views.cpp
    Source/Params.cpp
    Source/Session.cpp
    Source/Lyrics.cpp
    Source/AIHelper.cpp)

target_compile_definitions(LTHalo PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_USE_MP3AUDIOFORMAT=1)
target_link_libraries(LTHalo PRIVATE halo_engine lt_dsp juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp)
lt_configure_target(LTHalo)

# Lyrics (refuerzo follows the words): whisper.cpp, MIT. The model is downloaded by the user from Settings.
option(LT_HALO_WHISPER "LT HALO: read the lyrics with whisper.cpp" ON)
if(LT_HALO_WHISPER)
    include(FetchContent)
    set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
    set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
    set(GGML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GGML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    if(APPLE)
        set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)
    endif()
    set(LT_SAVED_SHARED ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
    FetchContent_Declare(whisper
        GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
        GIT_TAG v1.9.5
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(whisper)
    set(BUILD_SHARED_LIBS ${LT_SAVED_SHARED})
    target_link_libraries(LTHalo PRIVATE whisper)
    target_compile_definitions(LTHalo PRIVATE LT_HALO_WHISPER=1)
endif()

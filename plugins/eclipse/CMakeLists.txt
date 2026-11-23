juce_add_plugin(LTEclipse
    PRODUCT_NAME "LT ECLIPSE"
    VERSION 3.0.0
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.eclipse
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Ltec
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Drum Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(LTEclipseData
    HEADER_NAME EclipseData.h
    NAMESPACE EclipseData
    SOURCES
        Resources/thumb-negra.jpg Resources/thumb-blanca.jpg Resources/thumb-gris.jpg
        Resources/thumb-verde.jpg Resources/thumb-isla.jpg Resources/thumb-suite.jpg Resources/thumb-rosa.jpg
        Resources/thumb-disco.jpg Resources/thumb-neblina.jpg Resources/thumb-calle.jpg Resources/thumb-sabores.jpg
        Resources/thumb-espina.jpg Resources/thumb-billete.jpg Resources/thumb-santo.jpg Resources/thumb-infierno.jpg
        Resources/thumb-bombo.jpg)

# v2: neural drum separation (demucs.cpp, MIT; Eigen, MPL2)
set(DEMUCS_DIR ${CMAKE_SOURCE_DIR}/third_party/demucs.cpp)
if(NOT TARGET lt_demucs)
    add_library(lt_demucs STATIC
        ${DEMUCS_DIR}/src/crosstransformer.cpp ${DEMUCS_DIR}/src/dsp.cpp ${DEMUCS_DIR}/src/encdec.cpp
        ${DEMUCS_DIR}/src/layers.cpp ${DEMUCS_DIR}/src/lstm.cpp ${DEMUCS_DIR}/src/model_apply.cpp
        ${DEMUCS_DIR}/src/model_inference.cpp ${DEMUCS_DIR}/src/model_load.cpp)
    target_include_directories(lt_demucs PUBLIC ${DEMUCS_DIR}/src ${DEMUCS_DIR}/vendor/eigen)
    target_compile_features(lt_demucs PUBLIC cxx_std_17)
    if(MSVC)
        target_compile_options(lt_demucs PRIVATE /O2 /fp:fast /w)
        target_compile_definitions(lt_demucs PUBLIC EIGEN_MPL2_ONLY _USE_MATH_DEFINES NOMINMAX)
    else()
        target_compile_options(lt_demucs PRIVATE -O3 -ffast-math -fno-finite-math-only -w)
        target_compile_definitions(lt_demucs PUBLIC EIGEN_MPL2_ONLY)
    endif()
    if(APPLE)
        target_compile_definitions(lt_demucs PUBLIC EIGEN_USE_BLAS)
        target_link_libraries(lt_demucs PUBLIC "-framework Accelerate")
    endif()
endif()

target_sources(LTEclipse PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Look.cpp
    Source/Decor.cpp
    Source/Views.cpp
    Source/PatternEditor.cpp
    Source/SkinsPanel.cpp
    Source/SimilarPanel.cpp
    Source/SimilarSearch.cpp
    Source/CloneEngine.cpp
    Source/DrumSeparator.cpp
    Source/Classifier.cpp
    Source/Kit.cpp
    Source/Skins.cpp
    Source/Vibe.cpp
    Source/Sampler.cpp
    Source/Patterns.cpp
    Source/Params.cpp)

target_compile_definitions(LTEclipse PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LTEclipse PRIVATE LTEclipseData lt_dsp lt_demucs juce::juce_audio_utils juce::juce_dsp)
lt_configure_target(LTEclipse)

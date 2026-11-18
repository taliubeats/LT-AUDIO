juce_add_plugin(LTVocalSuite
    PRODUCT_NAME "LT VOCAL SUITE"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.vocalsuite
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Ltvs
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx Pitch Shift
    AU_MAIN_TYPE kAudioUnitType_Effect
    ICON_BIG ${CMAKE_SOURCE_DIR}/shared/brand/icon-vocalsuite.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(VocalSuiteData HEADER_NAME VocalSuiteData.h NAMESPACE VocalSuiteData SOURCES Resources/mascot.png Resources/mascot-light.png)

target_sources(LTVocalSuite PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Views.cpp
    Source/Params.cpp
    Source/Assistant.cpp
    Source/VocalChain.cpp
    ${CMAKE_SOURCE_DIR}/plugins/tune/Source/TuneEngine.cpp
    ${CMAKE_SOURCE_DIR}/plugins/tune/Source/PitchDetector.cpp
    ${CMAKE_SOURCE_DIR}/plugins/tune/Source/Scales.cpp
    ${CMAKE_SOURCE_DIR}/plugins/nova/Source/NovaEngine.cpp
    ${CMAKE_SOURCE_DIR}/plugins/orbit/Source/OrbitEngine.cpp
    ${CMAKE_SOURCE_DIR}/plugins/void/Source/VoidEngine.cpp
    ${CMAKE_SOURCE_DIR}/plugins/void/Source/ReverbTypes.cpp)

target_include_directories(LTVocalSuite PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/tune/Source
    ${CMAKE_SOURCE_DIR}/plugins/nova/Source
    ${CMAKE_SOURCE_DIR}/plugins/orbit/Source
    ${CMAKE_SOURCE_DIR}/plugins/void/Source)

target_compile_definitions(LTVocalSuite PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LTVocalSuite PRIVATE VocalSuiteData lt_dsp juce::juce_audio_utils juce::juce_dsp)
lt_configure_target(LTVocalSuite)

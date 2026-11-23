juce_add_plugin(LTSaturn
    PRODUCT_NAME "LT SATURN v2"
    VERSION 2.0.0
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.saturn
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Ltsa
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    ICON_BIG ${CMAKE_SOURCE_DIR}/shared/brand/icon-saturn.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LTSaturn PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Views.cpp
    Source/SaturnEngine.cpp
    Source/Params.cpp)

target_compile_definitions(LTSaturn PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LTSaturn PRIVATE lt_dsp juce::juce_audio_utils juce::juce_dsp)
lt_configure_target(LTSaturn)

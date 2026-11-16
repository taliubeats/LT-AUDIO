juce_add_plugin(LTOrbit
    PRODUCT_NAME "LT ORBIT"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.orbit
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Ltob
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Delay
    AU_MAIN_TYPE kAudioUnitType_Effect
    ICON_BIG ${CMAKE_SOURCE_DIR}/shared/brand/icon-orbit.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LTOrbit PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Views.cpp
    Source/OrbitEngine.cpp
    Source/Params.cpp)

target_compile_definitions(LTOrbit PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LTOrbit PRIVATE lt_dsp juce::juce_audio_utils juce::juce_dsp)
lt_configure_target(LTOrbit)

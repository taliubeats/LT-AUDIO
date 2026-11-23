juce_add_plugin(LTMeteor
    PRODUCT_NAME "LT METEOR"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.meteor
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Ltmt
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    ICON_BIG ${CMAKE_SOURCE_DIR}/shared/brand/icon-meteor.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LTMeteor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Views.cpp
    Source/MeteorEngine.cpp
    Source/Strokes.cpp
    Source/Params.cpp)

target_compile_definitions(LTMeteor PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LTMeteor PRIVATE lt_dsp juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp)
lt_configure_target(LTMeteor)

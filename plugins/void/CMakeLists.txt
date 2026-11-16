juce_add_plugin(LTVoid
    PRODUCT_NAME "LT VOID"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.void
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Ltvd
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect
    ICON_BIG ${CMAKE_SOURCE_DIR}/shared/brand/icon-void.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LTVoid PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Views.cpp
    Source/VoidEngine.cpp
    Source/ReverbTypes.cpp
    Source/Params.cpp)

target_compile_definitions(LTVoid PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LTVoid PRIVATE lt_dsp juce::juce_audio_utils juce::juce_dsp)
lt_configure_target(LTVoid)

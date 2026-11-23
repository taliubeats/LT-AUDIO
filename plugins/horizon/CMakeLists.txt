juce_add_plugin(LTHorizon
    PRODUCT_NAME "LT HORIZON"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.horizon
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Lthz
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Tools
    AU_MAIN_TYPE kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "LT HORIZON (standalone) records the audio input you choose."
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LTHorizon PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Recorder.cpp)

target_compile_definitions(LTHorizon PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LTHorizon PRIVATE juce::juce_audio_utils juce::juce_audio_formats)
lt_configure_target(LTHorizon)

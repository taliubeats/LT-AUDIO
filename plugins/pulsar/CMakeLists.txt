juce_add_plugin(LTPulsar
    PRODUCT_NAME "LT PULSAR"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.pulsar
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Ltps
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    ICON_BIG ${CMAKE_SOURCE_DIR}/shared/brand/icon-pulsar.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LTPulsar PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Views.cpp
    Source/DuckEngine.cpp
    Source/Curve.cpp
    Source/Presets.cpp)

target_compile_definitions(LTPulsar PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(LTPulsar PRIVATE
    lt_dsp
    juce::juce_audio_utils
    juce::juce_dsp)

lt_configure_target(LTPulsar)

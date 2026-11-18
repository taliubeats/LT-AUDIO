juce_add_plugin(LTTune
    PRODUCT_NAME "LT TUNE"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.tune
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Lttn
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Pitch Shift
    AU_MAIN_TYPE kAudioUnitType_Effect
    ICON_BIG ${CMAKE_SOURCE_DIR}/shared/brand/icon-tune.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LTTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Views.cpp
    Source/MatchView.cpp
    Source/TuneEngine.cpp
    Source/PitchDetector.cpp
    Source/MatchAnalysis.cpp
    Source/Scales.cpp
    Source/Params.cpp)

target_compile_definitions(LTTune PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_USE_MP3AUDIOFORMAT=1)
target_link_libraries(LTTune PRIVATE lt_dsp juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp)
lt_configure_target(LTTune)

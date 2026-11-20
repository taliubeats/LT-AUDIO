juce_add_plugin(LTLyra
    PRODUCT_NAME "LT LYRA"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.lyra
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Ltly
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Tools
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LTLyra PRIVATE
    Source/ChordEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(LTLyra PRIVATE Source)
target_compile_definitions(LTLyra PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LTLyra PRIVATE juce::juce_audio_utils)
lt_configure_target(LTLyra)

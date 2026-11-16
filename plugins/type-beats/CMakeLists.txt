juce_add_plugin(LTTypeBeats
    PRODUCT_NAME "LT TYPE BEATS"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.typebeats
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Ltyb
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx Analyzer
    AU_MAIN_TYPE kAudioUnitType_Effect
    ICON_BIG ${CMAKE_SOURCE_DIR}/shared/brand/icon-typebeats.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LTTypeBeats PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Views.cpp
    Source/Analysis.cpp
    Source/Downloader.cpp
    Source/MidiExport.cpp)

target_compile_definitions(LTTypeBeats PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_WINDOWS_MEDIA_FORMAT=1)

target_link_libraries(LTTypeBeats PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp)

lt_configure_target(LTTypeBeats)

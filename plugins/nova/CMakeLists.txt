juce_add_plugin(LTNova
    PRODUCT_NAME "LT NOVA"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.nova
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Ltnv
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    ICON_BIG ${CMAKE_SOURCE_DIR}/shared/brand/icon-nova.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LTNova PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Views.cpp
    Source/NovaEngine.cpp
    Source/Params.cpp)

target_compile_definitions(LTNova PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LTNova PRIVATE lt_dsp juce::juce_audio_utils juce::juce_dsp)
lt_configure_target(LTNova)

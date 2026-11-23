juce_add_plugin(LTCorona
    PRODUCT_NAME "LT CORONA"
    VERSION 1.0.0
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.corona
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Ltco
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(LTCoronaData
    HEADER_NAME CoronaData.h
    NAMESPACE CoronaData
    SOURCES Resources/corona-face.png)

target_sources(LTCorona PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/CoronaUI.cpp
    Source/CoronaEngine.cpp
    Source/Params.cpp)

target_compile_definitions(LTCorona PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LTCorona PRIVATE LTCoronaData lt_dsp juce::juce_audio_utils juce::juce_dsp)
lt_configure_target(LTCorona)

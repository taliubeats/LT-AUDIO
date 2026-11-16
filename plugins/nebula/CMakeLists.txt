juce_add_plugin(LTNebula
    PRODUCT_NAME "LT NEBULA"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.nebula
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Ltnb
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    ICON_BIG ${CMAKE_SOURCE_DIR}/shared/brand/icon-nebula.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LTNebula PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Views.cpp
    Source/NebulaEngine.cpp
    Source/NebulaFx.cpp
    Source/Modulation.cpp
    Source/ModViews.cpp
    Source/Banks.cpp
    Source/PresetBrowser.cpp
    Source/Params.cpp)

target_compile_definitions(LTNebula PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LTNebula PRIVATE lt_dsp juce::juce_audio_utils juce::juce_dsp)
lt_configure_target(LTNebula)

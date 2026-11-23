juce_add_plugin(LTCosmos
    PRODUCT_NAME "LT COSMOS"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.cosmos
    PLUGIN_MANUFACTURER_CODE Ltau
    PLUGIN_CODE Ltcs
    FORMATS ${LT_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Analyzer
    AU_MAIN_TYPE kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "LT COSMOS listens to an audio input to meter and visualise it."
    PLIST_TO_MERGE "<plist><dict><key>NSAudioCaptureUsageDescription</key><string>LT COSMOS listens to what your Mac plays to meter and visualise it. Nothing is recorded or sent anywhere.</string></dict></plist>"
    ICON_BIG ${CMAKE_SOURCE_DIR}/shared/brand/icon-cosmos.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LTCosmos PRIVATE
    Source/Loudness.cpp
    Source/Analysis.cpp
    Source/Visuals.cpp
    Source/PanelStrip.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

if(APPLE)
    target_sources(LTCosmos PRIVATE Source/SystemAudio.mm)
    set_source_files_properties(Source/SystemAudio.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(LTCosmos PRIVATE "-framework CoreAudio")
else()
    target_sources(LTCosmos PRIVATE Source/SystemAudioStub.cpp)
endif()

target_include_directories(LTCosmos PRIVATE Source)
target_compile_definitions(LTCosmos PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LTCosmos PRIVATE juce::juce_audio_utils juce::juce_dsp)
lt_configure_target(LTCosmos)

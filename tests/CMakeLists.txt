juce_add_console_app(lt_tests PRODUCT_NAME "LT Tests")
target_sources(lt_tests PRIVATE LicenseTests.cpp)
lt_configure_target(lt_tests)

juce_add_console_app(typebeats_analysis_test PRODUCT_NAME "TypeBeats Analysis Test")
target_sources(typebeats_analysis_test PRIVATE AnalysisTest.cpp ${CMAKE_SOURCE_DIR}/plugins/type-beats/Source/Analysis.cpp)
target_include_directories(typebeats_analysis_test PRIVATE ${CMAKE_SOURCE_DIR}/plugins/type-beats/Source)
target_link_libraries(typebeats_analysis_test PRIVATE juce::juce_audio_basics juce::juce_audio_formats juce::juce_dsp)
lt_configure_target(typebeats_analysis_test)

juce_add_console_app(pulsar_test PRODUCT_NAME "Pulsar Test")
target_sources(pulsar_test PRIVATE PulsarTest.cpp
    ${CMAKE_SOURCE_DIR}/plugins/pulsar/Source/Curve.cpp
    ${CMAKE_SOURCE_DIR}/plugins/pulsar/Source/Presets.cpp
    ${CMAKE_SOURCE_DIR}/plugins/pulsar/Source/DuckEngine.cpp)
target_include_directories(pulsar_test PRIVATE ${CMAKE_SOURCE_DIR}/plugins/pulsar/Source)
target_link_libraries(pulsar_test PRIVATE lt_dsp juce::juce_audio_basics juce::juce_dsp)
lt_configure_target(pulsar_test)

juce_add_console_app(void_test PRODUCT_NAME "Void Test")
target_sources(void_test PRIVATE VoidTest.cpp
    ${CMAKE_SOURCE_DIR}/plugins/void/Source/VoidEngine.cpp
    ${CMAKE_SOURCE_DIR}/plugins/void/Source/ReverbTypes.cpp)
target_include_directories(void_test PRIVATE ${CMAKE_SOURCE_DIR}/plugins/void/Source)
target_link_libraries(void_test PRIVATE juce::juce_audio_basics juce::juce_dsp juce::juce_graphics)
lt_configure_target(void_test)

juce_add_console_app(orbit_test PRODUCT_NAME "Orbit Test")
target_sources(orbit_test PRIVATE OrbitTest.cpp ${CMAKE_SOURCE_DIR}/plugins/orbit/Source/OrbitEngine.cpp)
target_include_directories(orbit_test PRIVATE ${CMAKE_SOURCE_DIR}/plugins/orbit/Source)
target_link_libraries(orbit_test PRIVATE juce::juce_audio_basics juce::juce_dsp)
lt_configure_target(orbit_test)

juce_add_console_app(knock_test PRODUCT_NAME "Knock Test")
target_sources(knock_test PRIVATE KnockTest.cpp ${CMAKE_SOURCE_DIR}/plugins/knock/Source/KnockEngine.cpp)
target_include_directories(knock_test PRIVATE ${CMAKE_SOURCE_DIR}/plugins/knock/Source)
target_link_libraries(knock_test PRIVATE lt_dsp juce::juce_audio_basics juce::juce_dsp)
lt_configure_target(knock_test)

juce_add_console_app(nebula_test PRODUCT_NAME "Nebula Test")
target_sources(nebula_test PRIVATE NebulaTest.cpp ${CMAKE_SOURCE_DIR}/plugins/nebula/Source/NebulaEngine.cpp ${CMAKE_SOURCE_DIR}/plugins/nebula/Source/NebulaFx.cpp ${CMAKE_SOURCE_DIR}/plugins/nebula/Source/Modulation.cpp ${CMAKE_SOURCE_DIR}/plugins/nebula/Source/Banks.cpp ${CMAKE_SOURCE_DIR}/plugins/nebula/Source/Params.cpp)
target_include_directories(nebula_test PRIVATE ${CMAKE_SOURCE_DIR}/plugins/nebula/Source)
target_link_libraries(nebula_test PRIVATE lt_dsp juce::juce_audio_basics juce::juce_audio_processors juce::juce_dsp)
lt_configure_target(nebula_test)

juce_add_console_app(tune_test PRODUCT_NAME "Tune Test")
target_sources(tune_test PRIVATE TuneTest.cpp
    ${CMAKE_SOURCE_DIR}/plugins/tune/Source/Scales.cpp
    ${CMAKE_SOURCE_DIR}/plugins/tune/Source/PitchDetector.cpp
    ${CMAKE_SOURCE_DIR}/plugins/tune/Source/TuneEngine.cpp
    ${CMAKE_SOURCE_DIR}/plugins/tune/Source/MatchAnalysis.cpp)
target_include_directories(tune_test PRIVATE ${CMAKE_SOURCE_DIR}/plugins/tune/Source)
target_link_libraries(tune_test PRIVATE juce::juce_audio_basics juce::juce_audio_formats juce::juce_dsp)
lt_configure_target(tune_test)

# Host-like stress tests (one per plugin). Build with -DLT_SANITIZE=ON to catch memory errors.
foreach(plug IN ITEMS LTTune LTNebula LTKnock LTOrbit LTVoid LTPulsar)
    if(TARGET ${plug})
        juce_add_console_app(${plug}_stress PRODUCT_NAME "${plug} Stress")
        target_sources(${plug}_stress PRIVATE stress/PluginStress.cpp)
        target_link_libraries(${plug}_stress PRIVATE ${plug} juce::juce_audio_processors juce::juce_gui_basics)
        target_compile_definitions(${plug}_stress PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
        set_target_properties(${plug}_stress PROPERTIES EXCLUDE_FROM_ALL TRUE)   # only built on request
    endif()
endforeach()

juce_add_console_app(nova_test PRODUCT_NAME "Nova Test")
target_sources(nova_test PRIVATE NovaTest.cpp ${CMAKE_SOURCE_DIR}/plugins/nova/Source/NovaEngine.cpp)
target_include_directories(nova_test PRIVATE ${CMAKE_SOURCE_DIR}/plugins/nova/Source)
target_link_libraries(nova_test PRIVATE juce::juce_audio_basics juce::juce_dsp)
lt_configure_target(nova_test)

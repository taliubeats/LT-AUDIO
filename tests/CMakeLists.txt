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

juce_add_console_app(lt_tests PRODUCT_NAME "LT Tests")
target_sources(lt_tests PRIVATE LicenseTests.cpp)
lt_configure_target(lt_tests)

juce_add_console_app(typebeats_analysis_test PRODUCT_NAME "TypeBeats Analysis Test")
target_sources(typebeats_analysis_test PRIVATE AnalysisTest.cpp ${CMAKE_SOURCE_DIR}/plugins/type-beats/Source/Analysis.cpp)
target_include_directories(typebeats_analysis_test PRIVATE ${CMAKE_SOURCE_DIR}/plugins/type-beats/Source)
target_link_libraries(typebeats_analysis_test PRIVATE juce::juce_audio_basics juce::juce_audio_formats juce::juce_dsp)
lt_configure_target(typebeats_analysis_test)

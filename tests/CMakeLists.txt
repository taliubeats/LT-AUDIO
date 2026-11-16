juce_add_console_app(lt_tests PRODUCT_NAME "LT Tests")
target_sources(lt_tests PRIVATE LicenseTests.cpp)
lt_configure_target(lt_tests)

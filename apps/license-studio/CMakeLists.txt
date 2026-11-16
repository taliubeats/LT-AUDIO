juce_add_gui_app(LTLicenseStudio
    PRODUCT_NAME "LT License Studio"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.licensestudio
    VERSION ${PROJECT_VERSION}
    ICON_BIG ${CMAKE_SOURCE_DIR}/shared/brand/icon-studio.png)

target_sources(LTLicenseStudio PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/StudioData.cpp)

lt_configure_target(LTLicenseStudio)

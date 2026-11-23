juce_add_gui_app(LTWarp
    PRODUCT_NAME "LT WARP"
    COMPANY_NAME "LT Audio"
    BUNDLE_ID com.ltaudio.warp
    VERSION ${PROJECT_VERSION}
    ICON_BIG ${CMAKE_SOURCE_DIR}/shared/brand/icon-warp.png
    DOCUMENT_EXTENSIONS mp3 wav flac aif aiff m4a opus ogg mp4 mov mkv webm jpg jpeg png webp tif tiff heic)

target_sources(LTWarp PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/Converter.cpp)

target_include_directories(LTWarp PRIVATE Source)
lt_configure_target(LTWarp)

# For a release, put LGPL builds of ffmpeg / ffprobe in tools/warp/<platform>/ and they are copied into the app.
set(WARP_TOOLS_DIR ${CMAKE_SOURCE_DIR}/tools/warp/${CMAKE_SYSTEM_NAME})
if(EXISTS ${WARP_TOOLS_DIR}/ffmpeg AND APPLE)
    add_custom_command(TARGET LTWarp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${WARP_TOOLS_DIR}/ffmpeg ${WARP_TOOLS_DIR}/ffprobe "$<TARGET_BUNDLE_CONTENT_DIR:LTWarp>/Resources/"
        COMMENT "Bundling ffmpeg into LT WARP")
endif()

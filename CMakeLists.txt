cmake_minimum_required(VERSION 3.22)

project(LT_AUDIO VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

option(LT_BUILD_STUDIO "Build LT License Studio (Mac app)" ON)
option(LT_BUILD_PLUGINS "Build the plugins" ON)
set(LT_JUCE_DIR "" CACHE PATH "Use a local JUCE checkout instead of downloading it")

# ---------------------------------------------------------------- JUCE
if(LT_JUCE_DIR)
    add_subdirectory(${LT_JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------- shared
add_library(lt_monocypher STATIC third_party/monocypher/monocypher.c)
target_include_directories(lt_monocypher PUBLIC third_party/monocypher)

juce_add_binary_data(lt_fonts
    HEADER_NAME LTFontsData.h
    NAMESPACE LTFontsData
    SOURCES
        shared/fonts/Montserrat-Light.ttf
        shared/fonts/Montserrat-Regular.ttf
        shared/fonts/Montserrat-SemiBold.ttf
        shared/fonts/Montserrat-Bold.ttf)

# The license public key is baked into every build. "NONE" = trial only.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/shared/license_public_key.txt LT_PUBLIC_KEY_HEX)
string(STRIP "${LT_PUBLIC_KEY_HEX}" LT_PUBLIC_KEY_HEX)
configure_file(shared/lt_shared/licensing/LTPublicKey.h.in ${CMAKE_BINARY_DIR}/generated/LTPublicKey.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shared/license_public_key.txt)

juce_add_module(shared/lt_shared)
target_include_directories(lt_shared INTERFACE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(lt_shared INTERFACE lt_monocypher lt_fonts)

# Audio pieces shared by the plugins (Frequency Focus)
juce_add_module(shared/lt_dsp)

# Common settings for every LT target
function(lt_configure_target target)
    target_compile_definitions(${target} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        LT_VERSION="${PROJECT_VERSION}")
    target_link_libraries(${target} PRIVATE
        lt_shared
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
endfunction()

# ---------------------------------------------------------------- products
if(APPLE)
    set(LT_PLUGIN_FORMATS VST3 AU Standalone)
else()
    set(LT_PLUGIN_FORMATS VST3 Standalone)
endif()

if(LT_BUILD_PLUGINS)
    add_subdirectory(plugins/type-beats)
    add_subdirectory(plugins/pulsar)
endif()

if(LT_BUILD_STUDIO)
    add_subdirectory(apps/license-studio)
endif()

add_subdirectory(tests)

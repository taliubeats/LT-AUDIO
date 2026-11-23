cmake_minimum_required(VERSION 3.22)

project(LT_AUDIO VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    add_compile_options(/bigobj /utf-8)
endif()
option(LT_BUILD_TESTS "Build the test programs" ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

option(LT_BUILD_STUDIO "Build LT License Studio (Mac app)" ON)
option(LT_BUILD_PLUGINS "Build the plugins" ON)
option(LT_SANITIZE "Build with address/undefined sanitizers (tests only)" OFF)
if(LT_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()
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
    add_subdirectory(plugins/void)
    add_subdirectory(plugins/orbit)
    add_subdirectory(plugins/knock)
    add_subdirectory(plugins/nebula)
    add_subdirectory(plugins/tune)
    add_subdirectory(plugins/nova)
    add_subdirectory(plugins/vocal-suite)
    add_subdirectory(plugins/quasar)
    add_subdirectory(plugins/cosmos)
    add_subdirectory(plugins/lyra)
    add_subdirectory(plugins/air)
    add_subdirectory(plugins/meteor)
    add_subdirectory(plugins/saturn)
    add_subdirectory(plugins/corona)
    add_subdirectory(plugins/horizon)
    add_subdirectory(plugins/eclipse)
    # LT HALO needs the WORLD vocoder (third_party/world, modified BSD)
    file(GLOB LT_WORLD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/third_party/world/src/*.cpp)
    add_library(lt_world STATIC ${LT_WORLD_SOURCES})
    target_include_directories(lt_world PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/world/src)
    set_target_properties(lt_world PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_subdirectory(plugins/halo)
endif()

if(LT_BUILD_STUDIO)
    add_subdirectory(apps/license-studio)
endif()

option(LT_BUILD_WARP "Build LT WARP (media converter app)" ON)
if(LT_BUILD_WARP)
    add_subdirectory(apps/warp)
endif()

if(LT_BUILD_TESTS)
    add_subdirectory(tests)
endif()

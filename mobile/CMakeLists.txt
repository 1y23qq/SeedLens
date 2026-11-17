cmake_minimum_required(VERSION 3.22)
project(SeedLensMobile LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJCXX)
endif()
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
add_compile_options(-fwrapv -fno-strict-aliasing -ffp-contract=off
    -ffunction-sections -fdata-sections "-ffile-prefix-map=${ROOT}=.")
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.zip
    URL_HASH SHA256=9850235fa6c9e99631dbe6f30dd620e4fba2e9c879b551a8c479e505e9e3f617)
FetchContent_MakeAvailable(SDL3)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${ROOT}/scripts/embed_assets.py"
    "${CMAKE_CURRENT_BINARY_DIR}/resources.cpp" COMMAND_ERROR_IS_FATAL ANY)
file(GLOB_RECURSE JAVA_SOURCES "${ROOT}/third_party/cubiomes_java/*.c")
list(FILTER JAVA_SOURCES EXCLUDE REGEX "/tests/")
list(FILTER JAVA_SOURCES EXCLUDE REGEX "/tests.c$")
file(GLOB BEDROCK_SOURCES "${ROOT}/third_party/cubiomes_bedrock/*.c")
list(FILTER BEDROCK_SOURCES EXCLUDE REGEX "/tests.c$")
add_library(java_engine STATIC ${JAVA_SOURCES} "${ROOT}/src/backend_java.c")
add_library(bedrock_engine STATIC ${BEDROCK_SOURCES} "${ROOT}/src/backend_bedrock.c")
target_compile_options(bedrock_engine PRIVATE -include "${ROOT}/src/bedrock_symbols.h")
set(IMGUI "${ROOT}/third_party/imgui")
add_library(imgui_mobile STATIC
    "${IMGUI}/imgui.cpp" "${IMGUI}/imgui_draw.cpp"
    "${IMGUI}/imgui_tables.cpp" "${IMGUI}/imgui_widgets.cpp"
    "${IMGUI}/backends/imgui_impl_sdl3.cpp"
    "${IMGUI}/backends/imgui_impl_sdlrenderer3.cpp")
target_include_directories(imgui_mobile PUBLIC "${IMGUI}" "${IMGUI}/backends")
target_link_libraries(imgui_mobile PUBLIC SDL3::SDL3-static)
set(SOURCES
    "${ROOT}/src/engine.cpp" "${ROOT}/src/persistence.cpp"
    "${ROOT}/src/export_common.cpp" "${ROOT}/src/export_sdl.cpp"
    "${ROOT}/src/ui.cpp" "${ROOT}/src/mobile_main.cpp"
    "${ROOT}/src/mobile_platform.cpp" "${ROOT}/tests/core_tests.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/resources.cpp")
set_source_files_properties("${ROOT}/tests/core_tests.cpp"
    PROPERTIES COMPILE_DEFINITIONS "main=seedlens_core_tests_main")
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(SeedLens MACOSX_BUNDLE ${SOURCES}
        ios/platform_ios.mm ios/LaunchScreen.storyboard ios/Assets.xcassets)
    set_target_properties(SeedLens PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.seedlens.app"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        XCODE_ATTRIBUTE_ENABLE_BITCODE "NO")
    set_source_files_properties(ios/LaunchScreen.storyboard ios/Assets.xcassets
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_compile_options(SeedLens PRIVATE "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")
    target_link_libraries(SeedLens PRIVATE "-framework UIKit")
else()
    add_executable(SeedLens ${SOURCES})
endif()
target_include_directories(SeedLens PRIVATE "${ROOT}/src")
target_link_libraries(SeedLens PRIVATE java_engine bedrock_engine imgui_mobile SDL3::SDL3-static)

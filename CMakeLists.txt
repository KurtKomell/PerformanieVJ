cmake_minimum_required(VERSION 3.24)

# vcpkg presets set CMAKE_TOOLCHAIN_FILE; CMake applies it before project(), so
# CMakeLists never reads it otherwise — CMake 3.25+ then warns it was unused.
if(DEFINED CACHE{CMAKE_TOOLCHAIN_FILE})
    get_property(_pvj_toolchain_value CACHE CMAKE_TOOLCHAIN_FILE PROPERTY VALUE)
    unset(_pvj_toolchain_value)
endif()

project(PerformanieVJ
    VERSION 0.1.0
    DESCRIPTION "PerformanieVJ - Visual Mats for Video Performance"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(VJ_FEATURE_DXV    "Enable DXV codec decoder"                 ON)
option(VJ_FEATURE_SPOUT  "Enable Spout input (Windows only)"        ON)
option(VJ_FEATURE_SYPHON "Enable Syphon input (macOS only)"         ON)
option(VJ_FEATURE_NDI    "Enable NDI input (cross-platform)"        OFF)
option(VJ_FEATURE_ASIO   "Enable ASIO audio driver (Windows only)"  OFF)
option(VJ_BUILD_TESTS    "Build unit tests"                         ON)

if(NOT WIN32)
    set(VJ_FEATURE_SPOUT OFF CACHE BOOL "" FORCE)
    set(VJ_FEATURE_ASIO  OFF CACHE BOOL "" FORCE)
endif()
if(NOT APPLE)
    set(VJ_FEATURE_SYPHON OFF CACHE BOOL "" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Qt 6.7 is required because M5 uses QRhiWidget (public API since 6.7).
# GuiPrivate gives us the QRhi headers; ShaderTools provides the qt6_add_shaders
# CMake helper used to precompile .vert/.frag into .qsb artifacts.
# We accept the private-module coupling; silence CMake's reminder on every configure.
set(QT_NO_PRIVATE_MODULE_WARNING ON)
set(_qt_components Core Concurrent Gui GuiPrivate Widgets ShaderTools)
if(VJ_BUILD_TESTS)
    list(APPEND _qt_components Test)
endif()
find_package(Qt6 6.7 REQUIRED COMPONENTS ${_qt_components})

include(FindFFmpegLibs)

qt_standard_project_setup()

add_subdirectory(src)

if(VJ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "PerformanieVJ ${PROJECT_VERSION}")
message(STATUS "  Qt version   : ${Qt6_VERSION}")
message(STATUS "  DXV          : ${VJ_FEATURE_DXV}")
message(STATUS "  Spout        : ${VJ_FEATURE_SPOUT}")
message(STATUS "  Syphon       : ${VJ_FEATURE_SYPHON}")
message(STATUS "  NDI          : ${VJ_FEATURE_NDI}")
message(STATUS "  ASIO         : ${VJ_FEATURE_ASIO}")

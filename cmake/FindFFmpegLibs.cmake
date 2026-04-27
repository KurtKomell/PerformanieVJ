# Lightweight locator for the FFmpeg C libraries we use (avformat, avcodec,
# swscale, swresample, avutil). Prefers vcpkg's unofficial FFMPEG package when
# it is available, then falls back to pkg-config, then to manual
# find_library / find_path calls.
#
# On success it creates an interface target `pvj::ffmpeg` carrying all
# include directories and link libraries.

if(TARGET pvj::ffmpeg)
    return()
endif()

# 1) vcpkg-provided FFMPEG config (recommended)
find_package(FFMPEG QUIET)
if(FFMPEG_FOUND AND FFMPEG_LIBRARIES AND FFMPEG_INCLUDE_DIRS)
    add_library(pvj_ffmpeg INTERFACE)
    target_include_directories(pvj_ffmpeg INTERFACE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(pvj_ffmpeg INTERFACE ${FFMPEG_LIBRARIES})
    add_library(pvj::ffmpeg ALIAS pvj_ffmpeg)
    message(STATUS "FFmpeg: using vcpkg/system package (${FFMPEG_INCLUDE_DIRS})")
    return()
endif()

# 2) pkg-config (Linux/macOS system)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FF_PKG QUIET libavformat libavcodec libswscale libswresample libavutil)
    if(FF_PKG_FOUND)
        add_library(pvj_ffmpeg INTERFACE)
        target_include_directories(pvj_ffmpeg INTERFACE ${FF_PKG_INCLUDE_DIRS})
        target_link_libraries(pvj_ffmpeg INTERFACE ${FF_PKG_LIBRARIES})
        target_link_directories(pvj_ffmpeg INTERFACE ${FF_PKG_LIBRARY_DIRS})
        add_library(pvj::ffmpeg ALIAS pvj_ffmpeg)
        message(STATUS "FFmpeg: using pkg-config")
        return()
    endif()
endif()

# 3) manual search as last resort
find_path(FF_INCLUDE_DIR libavformat/avformat.h)
find_library(FF_AVFORMAT_LIB   avformat)
find_library(FF_AVCODEC_LIB    avcodec)
find_library(FF_SWSCALE_LIB    swscale)
find_library(FF_SWRESAMPLE_LIB swresample)
find_library(FF_AVUTIL_LIB     avutil)

if(FF_INCLUDE_DIR AND FF_AVFORMAT_LIB AND FF_AVCODEC_LIB AND FF_SWSCALE_LIB
   AND FF_SWRESAMPLE_LIB AND FF_AVUTIL_LIB)
    add_library(pvj_ffmpeg INTERFACE)
    target_include_directories(pvj_ffmpeg INTERFACE ${FF_INCLUDE_DIR})
    target_link_libraries(pvj_ffmpeg INTERFACE
        ${FF_AVFORMAT_LIB}
        ${FF_AVCODEC_LIB}
        ${FF_SWSCALE_LIB}
        ${FF_SWRESAMPLE_LIB}
        ${FF_AVUTIL_LIB}
    )
    add_library(pvj::ffmpeg ALIAS pvj_ffmpeg)
    message(STATUS "FFmpeg: using manual lookup (${FF_INCLUDE_DIR})")
    return()
endif()

message(FATAL_ERROR
    "FFmpeg libraries (avformat/avcodec/swscale/swresample/avutil) were not "
    "found. Install via vcpkg (see vcpkg.json) or your distro's ffmpeg-dev "
    "package and retry.")

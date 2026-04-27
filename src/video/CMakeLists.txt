set(_pvj_video_sources
    MediaProbe.h
    MediaProbe.cpp
    ThumbnailExtractor.h
    ThumbnailExtractor.cpp
    VideoDecoder.h
    VideoDecoder.cpp
)

if(VJ_FEATURE_DXV)
    list(APPEND _pvj_video_sources
        HapFrameDecoder.h
        HapFrameDecoder.cpp
    )
endif()

add_library(pvj_video STATIC ${_pvj_video_sources})

target_link_libraries(pvj_video
    PUBLIC
        Qt6::Core
        Qt6::Gui
    PRIVATE
        pvj::ffmpeg
)

if(VJ_FEATURE_DXV)
    find_package(lz4 CONFIG REQUIRED)
    target_link_libraries(pvj_video PRIVATE lz4::lz4)
    target_compile_definitions(pvj_video PRIVATE VJ_FEATURE_DXV=1)
endif()

target_include_directories(pvj_video
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set_target_properties(pvj_video PROPERTIES
    AUTOMOC ON
    AUTORCC OFF
    AUTOUIC OFF
)

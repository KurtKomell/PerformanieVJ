find_path(MINIAUDIO_INCLUDE_DIR "miniaudio.h" REQUIRED)

set(_pvj_audio_sources
    AudioRingBuffer.cpp
    AudioRingBuffer.h
    AudioEngine.cpp
    AudioEngine.h
    FfmpegAudioDecoder.cpp
    FfmpegAudioDecoder.h
)

add_library(pvj_audio STATIC ${_pvj_audio_sources})

target_include_directories(pvj_audio
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${MINIAUDIO_INCLUDE_DIR}
)

target_link_libraries(pvj_audio
    PUBLIC
        Qt6::Core
    PRIVATE
        pvj::ffmpeg
)

set_target_properties(pvj_audio PROPERTIES
    AUTOMOC ON
    AUTORCC OFF
    AUTOUIC OFF
)

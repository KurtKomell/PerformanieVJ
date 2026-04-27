add_library(pvj_input STATIC
    IInputSource.h
    IOscInput.h
    InputTypes.h
    InputRouter.h
    InputRouter.cpp
    MidiInput.h
    MidiInput.cpp
)

find_package(RtMidi CONFIG REQUIRED)
target_link_libraries(pvj_input
    PUBLIC
        Qt6::Core
        Qt6::Gui
        pvj_core
    PRIVATE
        RtMidi::rtmidi
)

# vcpkg + MSVC: IMPORTED target may not propagate INTERFACE_INCLUDE_DIRECTORIES to the
# compile line for this static library. Add the include root explicitly when found.
set(_pvj_rtmidi_incdir "")
if(DEFINED RtMidi_DIR)
    cmake_path(GET RtMidi_DIR PARENT_PATH _pvj_rtmidi_share)
    cmake_path(GET _pvj_rtmidi_share PARENT_PATH _pvj_vcpkg_installed)
    if(EXISTS "${_pvj_vcpkg_installed}/include/RtMidi.h")
        set(_pvj_rtmidi_incdir "${_pvj_vcpkg_installed}/include")
    elseif(EXISTS "${_pvj_vcpkg_installed}/include/rtmidi/RtMidi.h")
        set(_pvj_rtmidi_incdir "${_pvj_vcpkg_installed}/include")
    endif()
endif()
if(NOT _pvj_rtmidi_incdir)
    foreach(_pvj_prefix IN LISTS CMAKE_PREFIX_PATH)
        if(EXISTS "${_pvj_prefix}/include/RtMidi.h")
            set(_pvj_rtmidi_incdir "${_pvj_prefix}/include")
            break()
        elseif(EXISTS "${_pvj_prefix}/include/rtmidi/RtMidi.h")
            set(_pvj_rtmidi_incdir "${_pvj_prefix}/include")
            break()
        endif()
    endforeach()
endif()
if(_pvj_rtmidi_incdir)
    target_include_directories(pvj_input PRIVATE "${_pvj_rtmidi_incdir}")
endif()

target_include_directories(pvj_input
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set_target_properties(pvj_input PROPERTIES
    AUTOMOC ON
)

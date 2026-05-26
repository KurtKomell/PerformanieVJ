# Find NVIDIA Maxine Video Effects SDK (open-source tree + runtime DLL install).

# Set MAXINE_SDK_ROOT to the extracted SDK root (e.g. C:/Maxine-VFX-SDK).

# Headers: <root>/nvvfx/include/nvVideoEffects.h

# Link:    nvvfx/src/NVVideoEffectsProxy.cpp + nvCVImageProxy.cpp (no import .lib)

# Runtime: C:/Program Files/NVIDIA Corporation/NVIDIA Video Effects/NVVideoEffects.dll



include(FindPackageHandleStandardArgs)



set(_maxine_roots "")

if(DEFINED MAXINE_SDK_ROOT)

    list(APPEND _maxine_roots "${MAXINE_SDK_ROOT}")

endif()

if(DEFINED ENV{MAXINE_SDK_ROOT})

    list(APPEND _maxine_roots "$ENV{MAXINE_SDK_ROOT}")

endif()

list(APPEND _maxine_roots

    "C:/Maxine-VFX-SDK"

    "C:/Program Files/NVIDIA Corporation/Maxine Video Effects"

    "C:/MaxineVFX"

)



set(_maxine_search_paths ${_maxine_roots})

foreach(_root IN LISTS _maxine_roots)

    list(APPEND _maxine_search_paths

        "${_root}"

        "${_root}/nvvfx"

        "${_root}/NvVFX"

    )

endforeach()



find_path(MAXINE_VFX_INCLUDE_DIR

    NAMES nvVideoEffects.h nvvfx.h NvVFX.h

    PATHS ${_maxine_search_paths}

    PATH_SUFFIXES

        include Include

        nvvfx/include nvvfx/Include

        NvVFX/include NvVFX/Include

)



find_file(MAXINE_VFX_PROXY_EFFECTS_CPP

    NAMES NVVideoEffectsProxy.cpp

    PATHS ${_maxine_search_paths}

    PATH_SUFFIXES nvvfx/src nvvfx/Src src Src

)



find_file(MAXINE_VFX_PROXY_IMAGE_CPP

    NAMES nvCVImageProxy.cpp

    PATHS ${_maxine_search_paths}

    PATH_SUFFIXES nvvfx/src nvvfx/Src src Src

)



# Legacy SDKs ship NvVFX.lib; the GitHub / NGC tree does not.

find_library(MAXINE_VFX_LIBRARY

    NAMES NvVFX nvvfx

    PATHS ${_maxine_search_paths}

    PATH_SUFFIXES

        lib lib/x64 x64

        nvvfx/lib nvvfx/lib/x64

)



find_library(MAXINE_NVCV_LIBRARY

    NAMES nvcv nvcvlib NvCV NVCVImage

    PATHS ${_maxine_search_paths}

    PATH_SUFFIXES

        lib lib/x64 x64

        nvvfx/lib nvvfx/lib/x64

)



find_path(MAXINE_VFX_BIN_DIR

    NAMES NVVideoEffects.dll

    PATHS

        "C:/Program Files/NVIDIA Corporation/NVIDIA Video Effects"

        "C:/Program Files/NVIDIA Corporation/Maxine Video Effects"

        ${_maxine_search_paths}

    PATH_SUFFIXES

        bin bin/x64 x64

        nvvfx/bin nvvfx/bin/x64

)



find_package(CUDAToolkit QUIET)



set(_maxine_have_proxy FALSE)

set(MAXINE_VFX_PROXY_SOURCES "")

if(MAXINE_VFX_PROXY_EFFECTS_CPP AND MAXINE_VFX_PROXY_IMAGE_CPP)

    set(_maxine_have_proxy TRUE)

    set(MAXINE_VFX_PROXY_SOURCES

        "${MAXINE_VFX_PROXY_EFFECTS_CPP}"

        "${MAXINE_VFX_PROXY_IMAGE_CPP}"

    )

endif()



find_package_handle_standard_args(MaxineVFX
    REQUIRED_VARS MAXINE_VFX_INCLUDE_DIR
)

if(MaxineVFX_FOUND AND NOT _maxine_have_proxy AND NOT MAXINE_VFX_LIBRARY)
    set(MaxineVFX_FOUND FALSE)
endif()



if(MaxineVFX_FOUND)

    set(MAXINE_VFX_INCLUDE_DIRS "${MAXINE_VFX_INCLUDE_DIR}")

    set(MAXINE_VFX_LIBRARIES "")

    if(_maxine_have_proxy)

        set(MAXINE_VFX_USE_PROXY TRUE)

    else()

        set(MAXINE_VFX_USE_PROXY FALSE)

        set(MAXINE_VFX_LIBRARIES ${MAXINE_VFX_LIBRARY})

        if(MAXINE_NVCV_LIBRARY)

            list(APPEND MAXINE_VFX_LIBRARIES ${MAXINE_NVCV_LIBRARY})

        endif()

    endif()

    if(CUDAToolkit_FOUND)

        list(APPEND MAXINE_VFX_LIBRARIES CUDA::cudart)

    endif()

endif()



mark_as_advanced(

    MAXINE_VFX_INCLUDE_DIR

    MAXINE_VFX_LIBRARY

    MAXINE_NVCV_LIBRARY

    MAXINE_VFX_BIN_DIR

    MAXINE_VFX_PROXY_EFFECTS_CPP

    MAXINE_VFX_PROXY_IMAGE_CPP

)



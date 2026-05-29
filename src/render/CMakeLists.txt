set(_pvj_render_sources
    RhiPreviewWidget.h
    RhiPreviewWidget.cpp
    RhiMixerWidget.h
    RhiMixerWidget.cpp
    FullscreenOutputWindow.h
    FullscreenOutputWindow.cpp
    ShaderLibrary.h
    ShaderLibrary.cpp
    FilterEffectShaderMap.h
    FilterEffectShaderMap.cpp
    FilterUniformPacker.h
    FilterUniformPacker.cpp
)

set(_pvj_shader_files
    ${CMAKE_SOURCE_DIR}/shaders/textured_quad.vert
    ${CMAKE_SOURCE_DIR}/shaders/textured_quad.frag
    ${CMAKE_SOURCE_DIR}/shaders/mixer.vert
    ${CMAKE_SOURCE_DIR}/shaders/mixer.frag
    ${CMAKE_SOURCE_DIR}/shaders/layer_feedback.vert
    ${CMAKE_SOURCE_DIR}/shaders/layer_feedback.frag
    ${CMAKE_SOURCE_DIR}/shaders/layer_feedback_hist.frag
    ${CMAKE_SOURCE_DIR}/shaders/stack_combine.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_blur.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_color.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_kaleido.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_identity.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_distort.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_transform.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_generate.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_stylize.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_pattern.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_utility.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_key.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_blend.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_mask.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_light.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_revival.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_temporal.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_film.frag
    ${CMAKE_SOURCE_DIR}/shaders/transition.vert
    ${CMAKE_SOURCE_DIR}/shaders/transition_crossfade.frag
    ${CMAKE_SOURCE_DIR}/shaders/transition_luma.frag
    ${CMAKE_SOURCE_DIR}/shaders/transition_slide.frag
)
set(_pvj_shader_outputs
    textured_quad.vert.qsb
    textured_quad.frag.qsb
    mixer.vert.qsb
    mixer.frag.qsb
    layer_feedback.vert.qsb
    layer_feedback.frag.qsb
    layer_feedback_hist.frag.qsb
    stack_combine.frag.qsb
    effect_blur.frag.qsb
    effect_color.frag.qsb
    effect_kaleido.frag.qsb
    effect_identity.frag.qsb
    effect_distort.frag.qsb
    effect_transform.frag.qsb
    effect_generate.frag.qsb
    effect_stylize.frag.qsb
    effect_pattern.frag.qsb
    effect_utility.frag.qsb
    effect_key.frag.qsb
    effect_blend.frag.qsb
    effect_mask.frag.qsb
    effect_light.frag.qsb
    effect_revival.frag.qsb
    effect_temporal.frag.qsb
    effect_film.frag.qsb
    transition.vert.qsb
    transition_crossfade.frag.qsb
    transition_luma.frag.qsb
    transition_slide.frag.qsb
)

if(VJ_FEATURE_DXV)
    list(APPEND _pvj_render_sources
        RhiBc3YCoCgWidget.h
        RhiBc3YCoCgWidget.cpp
    )
    list(APPEND _pvj_shader_files
        ${CMAKE_SOURCE_DIR}/shaders/dxv_ycocg.vert
        ${CMAKE_SOURCE_DIR}/shaders/dxv_ycocg.frag
    )
    list(APPEND _pvj_shader_outputs
        dxv_ycocg.vert.qsb
        dxv_ycocg.frag.qsb
    )
endif()

list(APPEND _pvj_render_sources
    maxine/MaxineTextureBridge.cpp
)

if(VJ_FEATURE_MAXINE)
    find_package(MaxineVFX)
    if(MaxineVFX_FOUND)
        list(APPEND _pvj_render_sources maxine/MaxineFilterBackend.cpp)
        if(MAXINE_VFX_PROXY_SOURCES)
            list(APPEND _pvj_render_sources ${MAXINE_VFX_PROXY_SOURCES})
        endif()
        set(_pvj_maxine_sdk_linked TRUE)
    else()
        message(WARNING "VJ_FEATURE_MAXINE is ON but Maxine SDK was not found. Set MAXINE_SDK_ROOT "
                "(e.g. C:/Maxine-VFX-SDK with nvvfx/include/nvVideoEffects.h). "
                "NVIDIA filters appear in the UI but run as passthrough until the SDK is linked.")
        list(APPEND _pvj_render_sources maxine/MaxineFilterBackend_stub.cpp)
    endif()
else()
    list(APPEND _pvj_render_sources maxine/MaxineFilterBackend_stub.cpp)
endif()

add_library(pvj_render STATIC ${_pvj_render_sources})

# Precompile the GLSL shaders into .qsb bundles and ship them in a qrc
# resource with the `pvj_render` library. Qt's QRhi runtime picks the right
# variant (SPIR-V, HLSL, MSL, GLSL ES) for the active backend automatically.
# Absolute shader paths alone make Qt place outputs under .qsb/<fullpath>, which
# breaks MSVC/MSBuild; explicit OUTPUTS keep build + :/shaders/... URLs stable.
qt6_add_shaders(pvj_render "pvj_render_shaders"
    PREFIX
        "/shaders"
    FILES ${_pvj_shader_files}
    OUTPUTS ${_pvj_shader_outputs}
)

target_link_libraries(pvj_render
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        pvj_core
    PRIVATE
        Qt6::GuiPrivate
)

if(WIN32)
    target_link_libraries(pvj_render PRIVATE d3d11)
endif()

# MSVC C4702 in Qt6/QtCore/qnumeric.h (unreachable-code analysis) — third-party noise, not app bugs.
if(MSVC)
    target_compile_options(pvj_render PRIVATE /wd4702)
endif()

if(VJ_FEATURE_DXV)
    target_compile_definitions(pvj_render PRIVATE VJ_FEATURE_DXV=1)
endif()

if(VJ_FEATURE_MAXINE)
    target_compile_definitions(pvj_render PRIVATE VJ_FEATURE_MAXINE=1)
    if(_pvj_maxine_sdk_linked)
        target_compile_definitions(pvj_render PRIVATE PVJ_MAXINE_SDK_LINKED=1)
        target_include_directories(pvj_render PRIVATE ${MAXINE_VFX_INCLUDE_DIRS})
        if(MAXINE_VFX_LIBRARIES)
            target_link_libraries(pvj_render PRIVATE ${MAXINE_VFX_LIBRARIES})
        endif()
        if(MAXINE_VFX_BIN_DIR)
            set(PVJ_MAXINE_BIN_DIR "${MAXINE_VFX_BIN_DIR}" CACHE INTERNAL "Maxine DLL directory")
        endif()
        if(MAXINE_VFX_USE_PROXY)
            message(STATUS "Maxine VFX SDK (proxy): ${MAXINE_VFX_INCLUDE_DIR}")
        else()
            message(STATUS "Maxine VFX SDK: ${MAXINE_VFX_INCLUDE_DIR}")
        endif()
    endif()
else()
    target_compile_definitions(pvj_render PRIVATE VJ_FEATURE_MAXINE=0)
endif()

target_include_directories(pvj_render
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set_target_properties(pvj_render PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC OFF
)

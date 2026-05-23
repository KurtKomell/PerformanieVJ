set(_pvj_render_sources
    RhiPreviewWidget.h
    RhiPreviewWidget.cpp
    RhiMixerWidget.h
    RhiMixerWidget.cpp
    FullscreenOutputWindow.h
    FullscreenOutputWindow.cpp
    ShaderLibrary.h
    ShaderLibrary.cpp
)

set(_pvj_shader_files
    ${CMAKE_SOURCE_DIR}/shaders/textured_quad.vert
    ${CMAKE_SOURCE_DIR}/shaders/textured_quad.frag
    ${CMAKE_SOURCE_DIR}/shaders/mixer.vert
    ${CMAKE_SOURCE_DIR}/shaders/mixer.frag
    ${CMAKE_SOURCE_DIR}/shaders/layer_feedback.vert
    ${CMAKE_SOURCE_DIR}/shaders/layer_feedback.frag
    ${CMAKE_SOURCE_DIR}/shaders/stack_combine.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_blur.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_color.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_kaleido.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_identity.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_distort.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_key.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_blend.frag
    ${CMAKE_SOURCE_DIR}/shaders/effect_mask.frag
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
    stack_combine.frag.qsb
    effect_blur.frag.qsb
    effect_color.frag.qsb
    effect_kaleido.frag.qsb
    effect_identity.frag.qsb
    effect_distort.frag.qsb
    effect_key.frag.qsb
    effect_blend.frag.qsb
    effect_mask.frag.qsb
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

# MSVC C4702 in Qt6/QtCore/qnumeric.h (unreachable-code analysis) — third-party noise, not app bugs.
if(MSVC)
    target_compile_options(pvj_render PRIVATE /wd4702)
endif()

if(VJ_FEATURE_DXV)
    target_compile_definitions(pvj_render PRIVATE VJ_FEATURE_DXV=1)
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

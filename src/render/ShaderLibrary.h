#pragma once

#include <QString>

namespace pvj::render {

/// Built-in single-pass cell effects (fragment + `textured_quad.vert`).
enum class EffectShaderId {
    Blur,
    ColorCorrection,
    Kaleido,
    Mask,
};

/// A/B transition pairs (`transition.vert` + fragment).
enum class TransitionShaderId {
    Crossfade,
    LumaWipe,
    Slide,
};

// --- std140 UBO layouts (match shaders; safe to memcpy into dynamic UBOs) ---

/// Shared with `textured_quad.vert` + most `effect_*.frag` (64 bytes).
struct alignas(16) EffectQuadUbo {
    float scaleOffset[4] = { 1, 1, 0, 0 };
    float rotation[4] = {}; ///< radians in .x (vertex rotation)
    float params[4] = {};
    float params2[4] = {};
};

/// Alias for filter-chain UBO (same 64-byte layout as EffectQuadUbo).
struct alignas(16) EffectQuadUbo2 {
    float scaleOffset[4] = { 1, 1, 0, 0 };
    float rotation[4] = {};
    float params[4] = {};
    float params2[4] = {};
};

/// `transition_*.frag` + `transition.vert` (32 bytes).
struct alignas(16) TransitionUbo {
    float scaleOffset[4] = { 1, 1, 0, 0 };
    /// x = progress [0,1], y = extra (e.g. luma softness), zw = slide direction
    float transition[4] = {};
};

QString effectFragmentShaderResource(EffectShaderId id);
QString effectFragmentShaderForType(const QString& typeId);
bool effectUsesTwoTextures(const QString& typeId);
QString transitionVertexShaderResource();
QString transitionFragmentShaderResource(TransitionShaderId id);
QString sharedTexturedQuadVertexResource();

/// True if the `:/shaders/...qsb` resource exists and is non-empty (for tests / CI).
bool shaderBundleIsValid(const QString& qsbResourcePath);

} // namespace pvj::render

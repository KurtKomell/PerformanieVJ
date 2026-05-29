#include "FilterUniformPacker.h"

#include "core/FilterParamSchema.h"

#include <QtGlobal>

namespace pvj::render {
namespace {

double namedParam(const pvj::core::CellFilterNode& node, const QString& name, double fallback = 0.0)
{
    for (const auto& p : node.params) {
        if (p.name == name) {
            return p.value;
        }
    }
    return fallback;
}

float strengthToUvRadius(double strength, const QSize& pixelSize)
{
    const double s = qBound(0.0, strength, 1.0);
    const double minDim = double(qMax(1, qMin(pixelSize.width(), pixelSize.height())));
    return float(qBound(0.003, s * 48.0 / minDim, 0.35));
}

float optionalStrengthToUvRadius(double strength, const QSize& pixelSize)
{
    if (strength <= 0.001) {
        return 0.0f;
    }
    return strengthToUvRadius(strength, pixelSize);
}

/// Resolve-style glow blur radius: glow_size sets base reach, spread scales it (never zero).
float glowBlurUvRadius(double glowSize, double spread, const QSize& pixelSize)
{
    const float base = strengthToUvRadius(glowSize, pixelSize);
    const float spreadMul = 0.35f + float(qBound(0.0, spread, 1.0)) * 2.65f;
    return qMax(base * spreadMul, 0.004f);
}

float halationBlurUvRadius(double spread, const QSize& pixelSize)
{
    return glowBlurUvRadius(spread, 0.5, pixelSize);
}

void applyHalationColorSpacePreset(float rgb[3], int preset)
{
    switch (preset) {
    case 1: // Sony S-Gamut3
        rgb[0] *= 0.92f;
        rgb[1] *= 1.03f;
        rgb[2] *= 0.96f;
        break;
    case 2: // Rec.709
        break;
    case 3: // P3 D65
        rgb[0] *= 1.04f;
        rgb[1] *= 0.98f;
        rgb[2] *= 1.05f;
        break;
    case 4: // Film
        rgb[0] *= 1.12f;
        rgb[1] *= 0.92f;
        rgb[2] *= 0.78f;
        break;
    default:
        break;
    }
}

void packGenericParams(EffectQuadUbo2& ubo, const pvj::core::CellFilterNode& node)
{
    const pvj::core::FilterNodeSpec schema = pvj::core::filterParamSchemas().value(node.typeId);
    for (int i = 0; i < schema.params.size() && i < 4; ++i) {
        const auto& spec = schema.params[i];
        double v = spec.defaultV;
        for (const auto& p : node.params) {
            if (p.name == spec.name) {
                v = p.value;
                break;
            }
        }
        if (spec.kind == pvj::core::FilterParamKind::Angle) {
            v = v * 3.14159265358979323846 / 180.0;
        } else if (spec.kind == pvj::core::FilterParamKind::Bool) {
            v = v >= 0.5 ? 1.0 : 0.0;
        }
        ubo.params[i] = float(v);
    }
}

void packBlurUniforms(EffectQuadUbo2& ubo, const pvj::core::CellFilterNode& node,
                      const QSize& pixelSize, int /*internalPass*/)
{
    const float minDim = float(qMax(1, qMin(pixelSize.width(), pixelSize.height())));
    const float blend = float(qBound(0.0, namedParam(node, QStringLiteral("blend"), 1.0), 1.0));
    const QString type = node.typeId.toLower();
    const float blurType = (type == QStringLiteral("gaussian_blur"))
                               ? 0.0f
                               : float(namedParam(node, QStringLiteral("blur_type"), 0.0));
    const float borderType = float(namedParam(node, QStringLiteral("border_type"), 0.0));

    ubo.params[3] = blend;
    // rotation.x is vertex rotation (radians) in textured_quad.vert — keep 0 for filters.
    ubo.rotation[0] = 0.0f;
    ubo.rotation[1] = borderType;
    ubo.params2[1] = blurType;
    ubo.params2[2] = 1.0f / minDim;

    if (type == QStringLiteral("gaussian_blur")) {
        ubo.params[0] = strengthToUvRadius(namedParam(node, QStringLiteral("horizontal_strength"), 0.5),
                                           pixelSize);
        ubo.params[1] = strengthToUvRadius(namedParam(node, QStringLiteral("vertical_strength"), 0.5),
                                           pixelSize);
    } else if (type == QStringLiteral("box_blur")) {
        const bool sameHv =
            namedParam(node, QStringLiteral("same_horizontal_vertical"), 1.0) >= 0.5;
        double hStrength = namedParam(node, QStringLiteral("horizontal_strength"), 0.5);
        double vStrength = namedParam(node, QStringLiteral("vertical_strength"), 0.5);
        if (sameHv) {
            vStrength = hStrength;
        }
        ubo.params[0] = optionalStrengthToUvRadius(hStrength, pixelSize);
        ubo.params[1] = optionalStrengthToUvRadius(vStrength, pixelSize);
        ubo.params2[3] = float(qBound(1.0, namedParam(node, QStringLiteral("iterations"), 1.0), 6.0));
    } else if (type == QStringLiteral("directional_blur")) {
        const double angleDeg = namedParam(node, QStringLiteral("blur_angle"), 0.0);
        ubo.params[0] = strengthToUvRadius(namedParam(node, QStringLiteral("blur_strength"), 0.5), pixelSize);
        ubo.params[2] = float(angleDeg * 3.14159265358979323846 / 180.0);
        ubo.rotation[2] = float(namedParam(node, QStringLiteral("symmetric_blur"), 0.0) >= 0.5 ? 1.0 : 0.0);
    } else if (type == QStringLiteral("radial_blur")) {
        const double smooth = namedParam(node, QStringLiteral("smooth_strength"),
                                         namedParam(node, QStringLiteral("blur_strength"), 0.5));
        ubo.params[0] = strengthToUvRadius(smooth, pixelSize);
        ubo.params[2] = float(namedParam(node, QStringLiteral("blur_symmetry"), 0.0));
        ubo.rotation[2] = float(namedParam(node, QStringLiteral("quality"), 1.0));
    } else if (type == QStringLiteral("zoom_blur")) {
        ubo.params[0] = float(qBound(0.0, namedParam(node, QStringLiteral("zoom_amount"), 0.5), 1.0));
        ubo.params[1] = float(qBound(0.0, namedParam(node, QStringLiteral("smooth_strength"), 0.5), 1.0));
        ubo.params[2] = float(qBound(0.0, namedParam(node, QStringLiteral("center_exclusion"), 0.0), 1.0));
        ubo.rotation[2] = float(namedParam(node, QStringLiteral("quality"), 1.0));
    } else if (type == QStringLiteral("mosaic_blur")) {
        ubo.params[0] = float(qBound(1.0, namedParam(node, QStringLiteral("pixel_frequency"), 100.0), 500.0));
        ubo.params[1] = float(namedParam(node, QStringLiteral("cell_shape"), 0.0));
        ubo.params[2] = float(qBound(0.0, namedParam(node, QStringLiteral("aliasing"), 1.0), 1.0));
    } else if (type == QStringLiteral("lens_blur")) {
        ubo.params[0] = float(qBound(0.0, namedParam(node, QStringLiteral("blur_size"), 4.0) / 64.0, 1.0));
        ubo.params[1] = float(qBound(0.0, namedParam(node, QStringLiteral("highlights"), 0.35), 1.0));
    } else if (type == QStringLiteral("sharpen")) {
        ubo.params[0] = float(qBound(0.0, namedParam(node, QStringLiteral("sharpen_amount"), 1.8), 5.0));
        ubo.params[1] = float(qBound(0.0, namedParam(node, QStringLiteral("fine_detail_size"), 0.05), 1.0));
        ubo.params[2] = float(qBound(0.0, namedParam(node, QStringLiteral("fine_detail"), 1.0), 2.0));
        ubo.params2[1] = float(qBound(0.0, namedParam(node, QStringLiteral("medium_details"), 1.0), 2.0));
        ubo.params2[3] = float(qBound(0.0, namedParam(node, QStringLiteral("large_details"), 1.0), 2.0));
    } else if (type == QStringLiteral("sharpen_edges")) {
        ubo.params[0] = float(qBound(0.0, namedParam(node, QStringLiteral("sharpen_amount"), 0.5), 2.0));
        ubo.params[1] = float(qBound(0.0, namedParam(node, QStringLiteral("sharpen_radius"), 0.5), 1.0));
        ubo.params[2] =
            float(qBound(0.0, namedParam(node, QStringLiteral("edge_detect_threshold"), 0.2), 1.0));
        ubo.params2[1] =
            float(qBound(0.0, namedParam(node, QStringLiteral("edge_mask_strength"), 0.5), 1.0));
        ubo.params2[3] = float(qBound(0.0, namedParam(node, QStringLiteral("edge_blur"), 0.5), 1.0));
        ubo.params3[0] = float(qBound(0.0, namedParam(node, QStringLiteral("pre_denoise"), 0.0), 1.0));
        ubo.params3[1] =
            float(namedParam(node, QStringLiteral("display_edges"), 0.0) >= 0.5 ? 1.0 : 0.0);
    } else if (type == QStringLiteral("soften_sharpen")) {
        ubo.params[0] = float(qBound(-1.0, namedParam(node, QStringLiteral("small_texture"), 0.0), 1.0));
        ubo.params[1] = float(qBound(-1.0, namedParam(node, QStringLiteral("medium_texture"), -0.8), 1.0));
        ubo.params[2] = float(qBound(-1.0, namedParam(node, QStringLiteral("large_texture"), -0.3), 1.0));
        ubo.params2[1] =
            float(qBound(0.0, namedParam(node, QStringLiteral("small_texture_size"), 0.5), 1.0));
        ubo.params2[3] =
            float(qBound(0.0, namedParam(node, QStringLiteral("coring_softness"), 0.0), 1.0));
    }
}

float kelvinToNorm(double k)
{
    return float(qBound(0.0, (k - 1000.0) / 19000.0, 1.0));
}

void packColorUniforms(EffectQuadUbo2& ubo, const pvj::core::CellFilterNode& node)
{
    const float blend = float(qBound(0.0, namedParam(node, QStringLiteral("blend"), 1.0), 1.0));
    const QString type = node.typeId.toLower();

    ubo.rotation[0] = 0.0f;
    ubo.rotation[1] = 0.0f;
    ubo.rotation[2] = 0.0f;

    if (type == QStringLiteral("invert")) {
        packGenericParams(ubo, node);
        return;
    }

    ubo.params[0] = blend;

    if (type == QStringLiteral("chromatic_adaptation")) {
        ubo.params[1] = kelvinToNorm(namedParam(node, QStringLiteral("source_temp"), 6500.0));
        ubo.params[2] = float(qBound(-1.0, namedParam(node, QStringLiteral("source_tint"), 0.0), 1.0));
        ubo.params[3] = kelvinToNorm(namedParam(node, QStringLiteral("target_temp"), 6500.0));
        // rotation.x rotates the quad in textured_quad.vert — never use it for effect data.
        ubo.rotation[1] = float(qBound(-1.0, namedParam(node, QStringLiteral("target_tint"), 0.0), 1.0));
        ubo.rotation[2] = float(namedParam(node, QStringLiteral("method"), 0.0));
        return;
    }

    if (type == QStringLiteral("color_compressor")) {
        const double hueDeg = namedParam(node, QStringLiteral("target_hue"), 0.0);
        ubo.params[1] = float(hueDeg * 3.14159265358979323846 / 180.0);
        ubo.params[2] = float(qBound(0.0, namedParam(node, QStringLiteral("compress_hue"), 0.5), 1.0));
        ubo.params[3] = float(qBound(0.0, namedParam(node, QStringLiteral("compress_saturation"), 0.5), 1.0));
        ubo.params2[3] = float(qBound(0.0, namedParam(node, QStringLiteral("compress_luminance"), 0.5), 1.0));
        return;
    }

    if (type == QStringLiteral("color_stabilizer")) {
        ubo.params[1] = float(qBound(0.0, namedParam(node, QStringLiteral("strength"), 0.5), 1.0));
        ubo.params[2] = float(namedParam(node, QStringLiteral("match"), 2.0));
        return;
    }

    if (type == QStringLiteral("contrast_pop")) {
        ubo.params[1] = float(qBound(-1.0, namedParam(node, QStringLiteral("detail_amount"), 0.0), 1.0));
        ubo.params[2] = float(qBound(0.0, namedParam(node, QStringLiteral("detail_size"), 0.5), 1.0));
        ubo.params[3] = float(qBound(0.0, namedParam(node, QStringLiteral("low_threshold"), 0.0), 1.0));
        ubo.rotation[1] = float(qBound(0.0, namedParam(node, QStringLiteral("high_threshold"), 1.0), 1.0));
        ubo.rotation[2] = float(qBound(0.0, namedParam(node, QStringLiteral("softness"), 0.5), 1.0));
        return;
    }

    if (type == QStringLiteral("dehaze")) {
        const double hueDeg = namedParam(node, QStringLiteral("haze_hue"), 0.0);
        ubo.params[1] = float(qBound(-1.0, namedParam(node, QStringLiteral("dehaze_strength"), 0.5), 1.0));
        ubo.params[2] = float(hueDeg * 3.14159265358979323846 / 180.0);
        return;
    }

    packGenericParams(ubo, node);
}

void packFilmUniforms(EffectQuadUbo2& ubo, const pvj::core::CellFilterNode& node)
{
    const float blend = float(qBound(0.0, namedParam(node, QStringLiteral("blend"), 1.0), 1.0));
    const QString type = node.typeId.toLower();

    ubo.rotation[0] = 0.0f;
    ubo.rotation[1] = 0.0f;
    ubo.rotation[2] = 0.0f;

    if (type == QStringLiteral("film_look")) {
        ubo.params[0] = float(namedParam(node, QStringLiteral("film_look"), 0.0));
        ubo.params[1] = blend;
        return;
    }

    if (type == QStringLiteral("film_color")) {
        ubo.params[0] = float(qBound(-1.0, namedParam(node, QStringLiteral("exposure"), 0.0), 1.0));
        ubo.params[1] = float(qBound(-1.0, namedParam(node, QStringLiteral("contrast"), 0.0), 1.0));
        ubo.params[2] = float(qBound(0.0, namedParam(node, QStringLiteral("highlights_fade"), 0.0), 1.0));
        ubo.params[3] = float(qBound(0.0, namedParam(node, QStringLiteral("fade_rolloff"), 0.5), 1.0));
        ubo.params2[1] = kelvinToNorm(namedParam(node, QStringLiteral("temperature"), 6500.0));
        ubo.params2[2] = float(qBound(-1.0, namedParam(node, QStringLiteral("tint"), 0.0), 1.0));
        ubo.params2[3] =
            float(qBound(0.0, namedParam(node, QStringLiteral("subtractive_saturation"), 0.0), 1.0));
        ubo.params3[0] = float(qBound(-1.0, namedParam(node, QStringLiteral("saturation"), 0.0), 1.0));
        ubo.params3[1] = float(qBound(0.0, namedParam(node, QStringLiteral("richness"), 0.0), 1.0));
        ubo.params3[2] = blend;
        return;
    }

    if (type == QStringLiteral("film_split_tone")) {
        ubo.params[0] = float(qBound(0.0, namedParam(node, QStringLiteral("amount"), 0.0), 1.0));
        const double hueDeg = namedParam(node, QStringLiteral("hue_angle"), 23.0);
        ubo.params[1] = float(hueDeg * 3.14159265358979323846 / 180.0);
        ubo.params[2] = float(qBound(0.0, namedParam(node, QStringLiteral("balance"), 0.5), 1.0));
        ubo.params[3] = blend;
        return;
    }

    if (type == QStringLiteral("film_vignette")) {
        ubo.params[0] = float(qBound(0.0, namedParam(node, QStringLiteral("amount"), 0.5), 1.0));
        ubo.params[1] = float(qBound(0.1, namedParam(node, QStringLiteral("size"), 0.5), 1.0));
        ubo.params[2] = float(qBound(0.0, namedParam(node, QStringLiteral("softness"), 0.5), 1.0));
        ubo.params2[1] = float(qBound(0.0, namedParam(node, QStringLiteral("roundness"), 0.5), 1.0));
        ubo.params2[2] = blend;
        return;
    }

    if (type == QStringLiteral("film_halation")) {
        ubo.params[0] = float(qBound(0.0, namedParam(node, QStringLiteral("amount"), 0.5), 1.0));
        ubo.params[1] = float(qBound(0.0, namedParam(node, QStringLiteral("threshold"), 0.5), 1.0));
        ubo.params[2] = float(qBound(0.0, namedParam(node, QStringLiteral("size"), 0.5), 1.0));
        const double hueDeg = namedParam(node, QStringLiteral("hue"), 0.0);
        ubo.rotation[1] = float(hueDeg * 3.14159265358979323846 / 180.0);
        ubo.params2[1] = blend;
        return;
    }

    if (type == QStringLiteral("film_bloom")) {
        ubo.params[0] = float(qBound(0.0, namedParam(node, QStringLiteral("amount"), 0.5), 1.0));
        ubo.params[1] = float(qBound(0.0, namedParam(node, QStringLiteral("threshold"), 0.5), 1.0));
        ubo.params[2] = float(qBound(0.0, namedParam(node, QStringLiteral("size"), 0.5), 1.0));
        ubo.params2[1] = blend;
        return;
    }

    if (type == QStringLiteral("film_grain")) {
        ubo.params[0] = float(qBound(0.0, namedParam(node, QStringLiteral("size"), 0.5), 1.0));
        ubo.params[1] = float(qBound(0.0, namedParam(node, QStringLiteral("strength"), 0.35), 1.0));
        ubo.rotation[1] = float(namedParam(node, QStringLiteral("monochrome"), 1.0) >= 0.5 ? 1.0 : 0.0);
        ubo.params[2] = blend;
        return;
    }

    if (type == QStringLiteral("film_flicker")) {
        ubo.params[0] = float(qBound(0.0, namedParam(node, QStringLiteral("amount"), 0.3), 1.0));
        ubo.params[1] = float(qBound(0.0, namedParam(node, QStringLiteral("speed"), 0.5), 1.0));
        ubo.params[2] = blend;
        return;
    }

    if (type == QStringLiteral("film_gate_weave")) {
        ubo.params[0] = float(qBound(0.0, namedParam(node, QStringLiteral("amount_h"), 0.3), 1.0));
        ubo.params[1] = float(qBound(0.0, namedParam(node, QStringLiteral("amount_v"), 0.3), 1.0));
        ubo.params[2] = float(qBound(0.0, namedParam(node, QStringLiteral("speed"), 0.5), 1.0));
        ubo.params[3] = blend;
        return;
    }

    if (type == QStringLiteral("film_gate")) {
        ubo.params[0] = float(namedParam(node, QStringLiteral("ratio"), 2.0));
        ubo.params[1] = float(qBound(0.0, namedParam(node, QStringLiteral("padding"), 0.0), 1.0));
        ubo.params[2] = float(qBound(0.0, namedParam(node, QStringLiteral("softness"), 0.5), 1.0));
        ubo.params2[1] = blend;
        return;
    }

    packGenericParams(ubo, node);
}

void unpackColorRgb(double packed, float rgb[3])
{
    const int v = int(qBound(0.0, packed, 16777215.0));
    rgb[0] = float((v >> 16) & 0xFF) / 255.0f;
    rgb[1] = float((v >> 8) & 0xFF) / 255.0f;
    rgb[2] = float(v & 0xFF) / 255.0f;
}

void packGhostSlot(EffectQuadUbo2& ubo, int slot, const pvj::core::CellFilterNode& node,
                   const QString& prefix)
{
    const float shape = float(namedParam(node, prefix + QStringLiteral("_shape"), 0.0));
    ubo.light[5 + slot][0] = shape;
    ubo.light[5 + slot][1] = float(namedParam(node, prefix + QStringLiteral("_position"), 0.3));
    ubo.light[5 + slot][2] = float(namedParam(node, prefix + QStringLiteral("_size"), 0.2));
    ubo.light[5 + slot][3] =
        float(namedParam(node, prefix + QStringLiteral("_center_brightness"), 0.5));
    float rgb[3] = { 1, 1, 1 };
    unpackColorRgb(namedParam(node, prefix + QStringLiteral("_color"), 0xFFFFFF), rgb);
    ubo.light[9 + slot][0] = rgb[0];
    ubo.light[9 + slot][1] = rgb[1];
    ubo.light[9 + slot][2] = rgb[2];
    ubo.light[9 + slot][3] =
        float(namedParam(node, prefix + QStringLiteral("_edge_brightness"), 0.6));
    // Ringing/chromatic stored in params3 for slot 0-1 only; reuse light vec4 .y/.z via edge slot
    // Ghost extra: softness in params3 if needed — pack into light[9].w as edge, use rotation for soft
}

void packLightUniforms(EffectQuadUbo2& ubo, const pvj::core::CellFilterNode& node,
                         const QSize& pixelSize, int /*internalPass*/)
{
    const float blend = float(qBound(0.0, namedParam(node, QStringLiteral("blend"), 1.0), 1.0));
    const QString type = node.typeId.toLower();

    ubo.rotation[0] = 0.0f;

    if (type == QStringLiteral("aperture_diffraction")) {
        ubo.params[0] = blend;
        ubo.params[1] = float(namedParam(node, QStringLiteral("source_threshold"), 0.65));
        ubo.params[2] = float(namedParam(node, QStringLiteral("result_gamma"), 0.5));
        ubo.params[3] = float(namedParam(node, QStringLiteral("result_scale"), 0.5));
        ubo.rotation[1] = float(namedParam(node, QStringLiteral("quality"), 0.0));
        ubo.light[0][0] = float(namedParam(node, QStringLiteral("iris_shape"), 3.0));
        ubo.light[0][1] = float(namedParam(node, QStringLiteral("aperture_size"), 0.5));
        ubo.light[0][2] = float(namedParam(node, QStringLiteral("blade_curvature"), 0.35));
        ubo.light[0][3] = float(namedParam(node, QStringLiteral("rotation"), 0.0));
        ubo.light[1][0] = float(namedParam(node, QStringLiteral("hv_ratio"), 0.0));
        ubo.light[1][1] = float(namedParam(node, QStringLiteral("chroma_shift"), 0.25));
        ubo.light[1][2] = float(namedParam(node, QStringLiteral("angle"), 0.0));
        return;
    }

    if (type == QStringLiteral("glow") || type == QStringLiteral("halation")) {
        ubo.params[0] = blend;
        ubo.params[1] = float(qBound(0.0, namedParam(node, QStringLiteral("threshold"), 0.65), 1.0));
        if (type == QStringLiteral("glow")) {
            const double glowSize = namedParam(node, QStringLiteral("glow_size"), 0.5);
            const double spread = namedParam(node, QStringLiteral("spread"), 0.5);
            const float blurR = glowBlurUvRadius(glowSize, spread, pixelSize);
            ubo.params[2] = blurR;
            ubo.params[3] = blurR;
            ubo.light[0][0] = float(namedParam(node, QStringLiteral("brightness"), 0.5));
            ubo.light[0][1] = float(namedParam(node, QStringLiteral("composite_type"), 0.0));
            ubo.light[0][2] =
                float(namedParam(node, QStringLiteral("source_of_glow"), 0.0) >= 0.5 ? 1.0 : 0.0);
            float rgb[3] = { 1, 1, 1 };
            unpackColorRgb(namedParam(node, QStringLiteral("glow_color"), 0xFFFFFF), rgb);
            ubo.light[1][0] = rgb[0];
            ubo.light[1][1] = rgb[1];
            ubo.light[1][2] = rgb[2];
            ubo.light[1][3] = 1.0f;
        } else {
            const double spread = namedParam(node, QStringLiteral("spread"), 0.45);
            const double strength = namedParam(node, QStringLiteral("strength"), 0.66);
            // Spread 0..1 maps directly to shader blur size (Resolve: spread = highlight reach).
            ubo.params[2] = float(qBound(0.0, spread, 1.0));
            ubo.params[3] = float(qBound(0.0, spread, 1.0));
            const int colorSpace = int(namedParam(node, QStringLiteral("processing_color_space"), 0.0));
            ubo.light[0][0] = float(qBound(0.0, strength, 1.0));
            ubo.light[0][1] = float(colorSpace);
            ubo.light[0][2] =
                float(qBound(0.0, namedParam(node, QStringLiteral("film_saturation_level"), 0.5), 1.0));
            float rgb[3] = { 1.0f, 0.4f, 0.2f };
            unpackColorRgb(namedParam(node, QStringLiteral("halation_color"), 0xFF6633), rgb);
            applyHalationColorSpacePreset(rgb, colorSpace);
            ubo.light[1][0] = rgb[0];
            ubo.light[1][1] = rgb[1];
            ubo.light[1][2] = rgb[2];
            ubo.light[1][3] = float(qBound(0.05, namedParam(node, QStringLiteral("gamma"), 0.5), 1.0));
        }
        return;
    }

    if (type == QStringLiteral("lens_flare")) {
        ubo.params[0] = blend;
        ubo.params[1] = float(namedParam(node, QStringLiteral("position_x"), 0.65));
        ubo.params[2] = float(namedParam(node, QStringLiteral("position_y"), 0.35));
        ubo.params[3] = float(namedParam(node, QStringLiteral("global_scaling"), 0.5));
        ubo.light[0][0] = float(namedParam(node, QStringLiteral("lens_center_x"), 0.5));
        ubo.light[0][1] = float(namedParam(node, QStringLiteral("lens_center_y"), 0.5));
        ubo.light[0][2] = float(namedParam(node, QStringLiteral("global_scaling"), 0.5));
        ubo.light[0][3] = float(namedParam(node, QStringLiteral("anamorphism"), 0.0));
        ubo.light[1][0] = float(namedParam(node, QStringLiteral("global_defocus"), 0.0));
        ubo.light[1][1] = float(namedParam(node, QStringLiteral("global_brightness"), 0.5));
        ubo.light[1][2] = float(namedParam(node, QStringLiteral("global_saturation"), 0.5));
        ubo.light[1][3] = float(namedParam(node, QStringLiteral("colorise_result"), 0.0));
        float rgb[3] = { 1, 1, 1 };
        unpackColorRgb(namedParam(node, QStringLiteral("colorization_color"), 0xFFFFFF), rgb);
        ubo.light[2][0] = rgb[0];
        ubo.light[2][1] = rgb[1];
        ubo.light[2][2] = rgb[2];
        ubo.params3[0] = float(namedParam(node, QStringLiteral("glare_brightness"), 0.5));
        ubo.params3[1] = float(namedParam(node, QStringLiteral("aperture_blades"), 6.0));
        ubo.params3[2] = float(namedParam(node, QStringLiteral("aperture_angle"), 0.0));
        unpackColorRgb(namedParam(node, QStringLiteral("glare_color"), 0xFFDD88), rgb);
        ubo.light[3][0] = float(namedParam(node, QStringLiteral("glare_brightness"), 0.5));
        ubo.light[3][1] = rgb[0];
        ubo.light[3][2] = rgb[1];
        ubo.light[3][3] = rgb[2];
        ubo.light[4][0] = float(namedParam(node, QStringLiteral("starburst_size"), 0.5));
        unpackColorRgb(namedParam(node, QStringLiteral("starburst_color"), 0xFFFFFF), rgb);
        ubo.light[4][1] = rgb[0];
        ubo.light[4][2] = rgb[1];
        ubo.light[4][3] = float(namedParam(node, QStringLiteral("flare_size"), 0.35));
        packGhostSlot(ubo, 0, node, QStringLiteral("ghost1"));
        packGhostSlot(ubo, 1, node, QStringLiteral("ghost2"));
        packGhostSlot(ubo, 2, node, QStringLiteral("ghost3"));
        packGhostSlot(ubo, 3, node, QStringLiteral("ghost4"));
        return;
    }

    if (type == QStringLiteral("lens_reflections")) {
        ubo.params[0] = blend;
        ubo.params[1] = float(namedParam(node, QStringLiteral("threshold"), 0.7));
        ubo.params[2] = float(namedParam(node, QStringLiteral("brightness"), 0.5));
        ubo.params[3] = float(qMax(0.01, namedParam(node, QStringLiteral("gamma"), 0.5)));
        float rgb[3] = { 1, 0.93f, 0.8f };
        unpackColorRgb(namedParam(node, QStringLiteral("color"), 0xFFEECC), rgb);
        ubo.light[0][0] = rgb[0];
        ubo.light[0][1] = rgb[1];
        ubo.light[0][2] = rgb[2];
        ubo.light[0][3] = float(namedParam(node, QStringLiteral("smooth"), 1.0));
        ubo.light[1][0] = float(namedParam(node, QStringLiteral("eclipse_position"), 0.0));
        ubo.light[1][1] = float(namedParam(node, QStringLiteral("eclipse_size"), 0.0));
        ubo.light[1][2] = float(namedParam(node, QStringLiteral("eclipse_softness"), 0.5));
        ubo.light[1][3] = float(namedParam(node, QStringLiteral("eclipse_chromatic_shift"), 0.0));
        return;
    }

    if (type == QStringLiteral("light_rays")) {
        ubo.params[0] = blend;
        ubo.params[1] = float(namedParam(node, QStringLiteral("source_threshold"), 0.65));
        ubo.params[2] = float(namedParam(node, QStringLiteral("length"), 0.5));
        ubo.params[3] = float(namedParam(node, QStringLiteral("soften"), 0.35));
        ubo.rotation[1] = float(namedParam(node, QStringLiteral("source_of_rays"), 0.0));
        ubo.rotation[2] = float(namedParam(node, QStringLiteral("ray_directions"), 0.0));
        const bool atAngle = namedParam(node, QStringLiteral("ray_directions"), 0.0) >= 0.5;
        if (atAngle) {
            ubo.light[0][0] = 0.5f;
            ubo.light[0][1] = 0.5f;
            ubo.light[0][2] = float(namedParam(node, QStringLiteral("ray_angle"), 0.0));
        } else {
            ubo.light[0][0] = float(namedParam(node, QStringLiteral("ray_location_x"), 0.5));
            ubo.light[0][1] = float(namedParam(node, QStringLiteral("ray_location_y"), 0.5));
            ubo.light[0][2] = 0.0f;
        }
        ubo.light[0][3] = float(namedParam(node, QStringLiteral("brightness"), 0.5));
        ubo.light[1][0] = float(namedParam(node, QStringLiteral("saturation"), 0.5));
        ubo.light[1][1] = float(namedParam(node, QStringLiteral("ccd_bloom"), 0.0));
        ubo.light[1][2] = float(namedParam(node, QStringLiteral("composite_type"), 0.0));
        return;
    }

    packGenericParams(ubo, node);
}

} // namespace

void packFilterUniformBuffer(EffectQuadUbo2& ubo, const pvj::core::CellFilterNode& node,
                             const QSize& pixelSize, float elapsedSec, int internalPass,
                             const float keyChannelRgb[3])
{
    ubo = EffectQuadUbo2{};
    ubo.scaleOffset[0] = 1.0f;
    ubo.scaleOffset[1] = 1.0f;
    ubo.scaleOffset[2] = elapsedSec;
    ubo.scaleOffset[3] = float(pixelSize.width()) / float(qMax(pixelSize.height(), 1));
    ubo.params2[0] = float(internalPass);

    const pvj::core::FilterEffectMeta meta = pvj::core::filterEffectMeta(node.typeId);
    ubo.rotation[3] = float(meta.familyId);

    switch (meta.family) {
    case pvj::core::FilterEffectFamily::Blur:
        packBlurUniforms(ubo, node, pixelSize, internalPass);
        return;
    case pvj::core::FilterEffectFamily::Key:
        packGenericParams(ubo, node);
        ubo.scaleOffset[2] = keyChannelRgb ? keyChannelRgb[0] : 1.0f;
        ubo.rotation[1] = keyChannelRgb ? keyChannelRgb[1] : 1.0f;
        ubo.rotation[2] = keyChannelRgb ? keyChannelRgb[2] : 1.0f;
        ubo.rotation[3] = (meta.familyId == 1) ? 1.0f : 0.0f;
        if (ubo.params[3] < 0.04f) {
            ubo.params[3] = 0.04f;
        }
        return;
    case pvj::core::FilterEffectFamily::Mask:
        packGenericParams(ubo, node);
        return;
    case pvj::core::FilterEffectFamily::Color:
        packColorUniforms(ubo, node);
        ubo.rotation[0] = 0.0f;
        return;
    case pvj::core::FilterEffectFamily::Film:
        packFilmUniforms(ubo, node);
        ubo.rotation[0] = 0.0f;
        return;
    case pvj::core::FilterEffectFamily::Light:
        packLightUniforms(ubo, node, pixelSize, internalPass);
        ubo.rotation[0] = 0.0f;
        return;
    default:
        packGenericParams(ubo, node);
        return;
    }
}

} // namespace pvj::render

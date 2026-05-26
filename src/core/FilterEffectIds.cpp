#include "FilterEffectIds.h"

#include "FilterCatalog.h"
#include "FilterParamSchema.h"

#include <QHash>

namespace pvj::core {
namespace {

struct RawEntry {
    const char* typeId;
    FilterEffectFamily family;
    int familyId;
    int internalPasses;
    int blendModeOverride;
    FilterExecutionBackend backend = FilterExecutionBackend::Shader;
};

// Family-local effect indices; algorithms live in matching `effect_*.frag`.
constexpr RawEntry kEntries[] = {
    // Blur & sharpen (11)
    { "blur", FilterEffectFamily::Blur, 0, 2, -1 },
    { "fast_blur", FilterEffectFamily::Blur, 1, 1, -1 },
    { "gaussian_blur", FilterEffectFamily::Blur, 2, 2, -1 },
    { "box_blur", FilterEffectFamily::Blur, 3, 2, -1 },
    { "radial_blur", FilterEffectFamily::Blur, 4, 1, -1 },
    { "directional_blur", FilterEffectFamily::Blur, 5, 1, -1 },
    { "motion_blur", FilterEffectFamily::Blur, 6, 1, -1 },
    { "zoom_blur", FilterEffectFamily::Blur, 7, 1, -1 },
    { "temporal_blur", FilterEffectFamily::Blur, 8, 1, -1 },
    { "sharpen", FilterEffectFamily::Blur, 9, 1, -1 },
    { "denoise", FilterEffectFamily::Blur, 10, 1, -1 },

    // Color & levels (25)
    { "color", FilterEffectFamily::Color, 0, 1, -1 },
    { "color_correction", FilterEffectFamily::Color, 1, 1, -1 },
    { "color_intensity", FilterEffectFamily::Color, 2, 1, -1 },
    { "color_balance", FilterEffectFamily::Color, 3, 1, -1 },
    { "brightness", FilterEffectFamily::Color, 4, 1, -1 },
    { "contrast", FilterEffectFamily::Color, 5, 1, -1 },
    { "gamma", FilterEffectFamily::Color, 6, 1, -1 },
    { "exposure", FilterEffectFamily::Color, 7, 1, -1 },
    { "saturation", FilterEffectFamily::Color, 8, 1, -1 },
    { "hue", FilterEffectFamily::Color, 9, 1, -1 },
    { "hsl", FilterEffectFamily::Color, 10, 1, -1 },
    { "curves", FilterEffectFamily::Color, 11, 1, -1 },
    { "levels", FilterEffectFamily::Color, 12, 1, -1 },
    { "invert", FilterEffectFamily::Color, 13, 1, -1 },
    { "solarize", FilterEffectFamily::Color, 14, 1, -1 },
    { "tint", FilterEffectFamily::Color, 15, 1, -1 },
    { "black_white", FilterEffectFamily::Color, 16, 1, -1 },
    { "posterize", FilterEffectFamily::Color, 17, 1, -1 },
    { "threshold", FilterEffectFamily::Color, 18, 1, -1 },
    { "colorize", FilterEffectFamily::Color, 19, 1, -1 },
    { "chromatic_aberration", FilterEffectFamily::Color, 20, 1, -1 },
    { "hue_rotate", FilterEffectFamily::Color, 21, 1, -1 },
    { "selective_color", FilterEffectFamily::Color, 22, 1, -1 },
    { "channel_mixer", FilterEffectFamily::Color, 23, 1, -1 },
    { "color_lookup", FilterEffectFamily::Color, 24, 1, -1 },

    // Transform (13)
    { "transform", FilterEffectFamily::Transform, 0, 1, -1 },
    { "scale", FilterEffectFamily::Transform, 1, 1, -1 },
    { "rotate", FilterEffectFamily::Transform, 2, 1, -1 },
    { "flip", FilterEffectFamily::Transform, 3, 1, -1 },
    { "flip_horizontal", FilterEffectFamily::Transform, 4, 1, -1 },
    { "flip_vertical", FilterEffectFamily::Transform, 5, 1, -1 },
    { "tile", FilterEffectFamily::Transform, 6, 1, -1 },
    { "motion_tile", FilterEffectFamily::Transform, 7, 1, -1 },
    { "smooth_transform", FilterEffectFamily::Transform, 8, 1, -1 },
    { "zoom", FilterEffectFamily::Transform, 9, 1, -1 },
    { "polar", FilterEffectFamily::Transform, 10, 1, -1 },
    { "polarizer", FilterEffectFamily::Transform, 11, 1, -1 },
    { "pixelate", FilterEffectFamily::Transform, 12, 1, -1 },

    // Kaleido (6)
    { "mirror", FilterEffectFamily::Kaleido, 0, 1, -1 },
    { "mirror_quad", FilterEffectFamily::Kaleido, 1, 1, -1 },
    { "mirror_stripes", FilterEffectFamily::Kaleido, 2, 1, -1 },
    { "multi_mirror", FilterEffectFamily::Kaleido, 3, 1, -1 },
    { "kaleido", FilterEffectFamily::Kaleido, 4, 1, -1 },
    { "kaleidoscope", FilterEffectFamily::Kaleido, 5, 1, -1 },

    // Distort (17)
    { "ripple", FilterEffectFamily::Distort, 0, 1, -1 },
    { "wave", FilterEffectFamily::Distort, 1, 1, -1 },
    { "twirl", FilterEffectFamily::Distort, 2, 1, -1 },
    { "bulge", FilterEffectFamily::Distort, 3, 1, -1 },
    { "fisheye", FilterEffectFamily::Distort, 4, 1, -1 },
    { "distortion", FilterEffectFamily::Distort, 5, 1, -1 },
    { "bend", FilterEffectFamily::Distort, 6, 1, -1 },
    { "warp", FilterEffectFamily::Distort, 7, 1, -1 },
    { "displacement", FilterEffectFamily::Distort, 8, 1, -1 },
    { "screen_shake", FilterEffectFamily::Distort, 9, 1, -1 },
    { "space_warper", FilterEffectFamily::Distort, 10, 1, -1 },
    { "shifty", FilterEffectFamily::Distort, 11, 1, -1 },
    { "mesh_warp", FilterEffectFamily::Distort, 12, 1, -1 },
    { "liquify", FilterEffectFamily::Distort, 13, 1, -1 },
    { "spherize", FilterEffectFamily::Distort, 14, 1, -1 },
    { "cylinder", FilterEffectFamily::Distort, 15, 1, -1 },
    { "cube", FilterEffectFamily::Distort, 16, 1, -1 },

    // Generate (18)
    { "glow", FilterEffectFamily::Generate, 0, 1, -1 },
    { "bloom", FilterEffectFamily::Generate, 1, 2, -1 },
    { "god_rays", FilterEffectFamily::Generate, 2, 1, -1 },
    { "strobe", FilterEffectFamily::Generate, 3, 1, -1 },
    { "trails", FilterEffectFamily::Generate, 4, 1, -1 },
    { "light_leak", FilterEffectFamily::Generate, 5, 1, -1 },
    { "noise", FilterEffectFamily::Generate, 6, 1, -1 },
    { "rgb_noise", FilterEffectFamily::Generate, 7, 1, -1 },
    { "video_noise", FilterEffectFamily::Generate, 8, 1, -1 },
    { "halftone", FilterEffectFamily::Generate, 9, 1, -1 },
    { "vignette", FilterEffectFamily::Generate, 10, 1, -1 },
    { "spotlight", FilterEffectFamily::Generate, 11, 1, -1 },
    { "drop_shadow", FilterEffectFamily::Generate, 12, 1, -1 },
    { "rainbow", FilterEffectFamily::Generate, 13, 1, -1 },
    { "prismatic", FilterEffectFamily::Generate, 14, 1, -1 },
    { "replicate", FilterEffectFamily::Generate, 15, 1, -1 },
    { "echo", FilterEffectFamily::Generate, 16, 1, -1 },
    { "slit_scanner", FilterEffectFamily::Generate, 17, 1, -1 },

    // Stylize (24)
    { "edges", FilterEffectFamily::Stylize, 0, 1, -1 },
    { "emboss", FilterEffectFamily::Stylize, 1, 1, -1 },
    { "find_edges", FilterEffectFamily::Stylize, 2, 1, -1 },
    { "glow_edges", FilterEffectFamily::Stylize, 3, 1, -1 },
    { "crt", FilterEffectFamily::Stylize, 4, 1, -1 },
    { "vhs", FilterEffectFamily::Stylize, 5, 1, -1 },
    { "vhsifyer", FilterEffectFamily::Stylize, 6, 1, -1 },
    { "film_grain", FilterEffectFamily::Stylize, 7, 1, -1 },
    { "scanlines", FilterEffectFamily::Stylize, 8, 1, -1 },
    { "broadcast", FilterEffectFamily::Stylize, 9, 1, -1 },
    { "reducto", FilterEffectFamily::Stylize, 10, 1, -1 },
    { "total_visual_annihilation", FilterEffectFamily::Stylize, 11, 1, -1 },
    { "cartoon", FilterEffectFamily::Stylize, 12, 1, -1 },
    { "watercolor", FilterEffectFamily::Stylize, 13, 1, -1 },
    { "oil_paint", FilterEffectFamily::Stylize, 14, 1, -1 },
    { "night_vision", FilterEffectFamily::Stylize, 15, 1, -1 },
    { "thermal", FilterEffectFamily::Stylize, 16, 1, -1 },
    { "x_ray", FilterEffectFamily::Stylize, 17, 1, -1 },
    { "duotone", FilterEffectFamily::Stylize, 18, 1, -1 },
    { "tritone", FilterEffectFamily::Stylize, 19, 1, -1 },
    { "gradient_map", FilterEffectFamily::Stylize, 20, 1, -1 },
    { "stroke", FilterEffectFamily::Stylize, 21, 1, -1 },
    { "erode", FilterEffectFamily::Stylize, 22, 1, -1 },
    { "dilate", FilterEffectFamily::Stylize, 23, 1, -1 },

    // Key (2)
    { "chroma_key", FilterEffectFamily::Key, 0, 1, -1 },
    { "luma_key", FilterEffectFamily::Key, 1, 1, -1 },

    // Mask & crop (4)
    { "linear_mask", FilterEffectFamily::Mask, 0, 1, -1 },
    { "mask", FilterEffectFamily::Mask, 1, 1, -1 },
    { "crop", FilterEffectFamily::Mask, 2, 1, -1 },
    { "crop_rectangle", FilterEffectFamily::Mask, 3, 1, -1 },

    // Blend (15) — blendModeOverride matches W3C / Photoshop ordering in effect_blend.frag
    { "blend_normal", FilterEffectFamily::Blend, 0, 1, 0 },
    { "blend_add", FilterEffectFamily::Blend, 1, 1, 1 },
    { "blend_subtract", FilterEffectFamily::Blend, 2, 1, 2 },
    { "blend_multiply", FilterEffectFamily::Blend, 3, 1, 3 },
    { "blend_screen", FilterEffectFamily::Blend, 4, 1, 4 },
    { "blend_overlay", FilterEffectFamily::Blend, 5, 1, 5 },
    { "blend_soft_light", FilterEffectFamily::Blend, 6, 1, 6 },
    { "blend_hard_light", FilterEffectFamily::Blend, 7, 1, 7 },
    { "blend_color_dodge", FilterEffectFamily::Blend, 8, 1, 8 },
    { "blend_color_burn", FilterEffectFamily::Blend, 9, 1, 9 },
    { "blend_darken", FilterEffectFamily::Blend, 10, 1, 10 },
    { "blend_lighten", FilterEffectFamily::Blend, 11, 1, 11 },
    { "blend_difference", FilterEffectFamily::Blend, 12, 1, 12 },
    { "blend_exclusion", FilterEffectFamily::Blend, 13, 1, 13 },
    { "blend_mode", FilterEffectFamily::Blend, 14, 1, -1 },

    // Patterns (5)
    { "radar", FilterEffectFamily::Pattern, 0, 1, -1 },
    { "polka_dot", FilterEffectFamily::Pattern, 1, 1, -1 },
    { "stripes", FilterEffectFamily::Pattern, 2, 1, -1 },
    { "checkerboard", FilterEffectFamily::Pattern, 3, 1, -1 },
    { "dots", FilterEffectFamily::Pattern, 4, 1, -1 },

    // Utility (8)
    { "add_subtract", FilterEffectFamily::Utility, 0, 1, -1 },
    { "mix", FilterEffectFamily::Utility, 1, 1, -1 },
    { "fade", FilterEffectFamily::Utility, 2, 1, -1 },
    { "rgb_shift", FilterEffectFamily::Utility, 3, 1, -1 },
    { "shift", FilterEffectFamily::Utility, 4, 1, -1 },
    { "tilt_shift", FilterEffectFamily::Utility, 5, 1, -1 },
    { "opacity", FilterEffectFamily::Utility, 6, 1, -1 },
    { "transform_3d", FilterEffectFamily::Utility, 7, 1, -1 },

    // DaVinci Resolve Effects Library
    { "mosaic_blur", FilterEffectFamily::Blur, 11, 1, -1 },
    { "lens_blur", FilterEffectFamily::Blur, 12, 1, -1 },
    { "sharpen_edges", FilterEffectFamily::Blur, 13, 1, -1 },
    { "soften_sharpen", FilterEffectFamily::Blur, 14, 1, -1 },
    { "aces_transform", FilterEffectFamily::Color, 25, 1, -1 },
    { "chromatic_adaptation", FilterEffectFamily::Color, 26, 1, -1 },
    { "color_compressor", FilterEffectFamily::Color, 27, 1, -1 },
    { "color_space_transform", FilterEffectFamily::Color, 28, 1, -1 },
    { "color_stabilizer", FilterEffectFamily::Color, 29, 1, -1 },
    { "contrast_pop", FilterEffectFamily::Color, 30, 1, -1 },
    { "dehaze", FilterEffectFamily::Color, 31, 1, -1 },
    { "false_color", FilterEffectFamily::Color, 32, 1, -1 },
    { "flicker_addition", FilterEffectFamily::Color, 33, 1, -1 },
    { "gamut_limiter", FilterEffectFamily::Color, 34, 1, -1 },
    { "gamut_mapping", FilterEffectFamily::Color, 35, 1, -1 },
    { "color_generator", FilterEffectFamily::Generate, 18, 1, -1 },
    { "color_palette", FilterEffectFamily::Generate, 19, 1, -1 },
    { "grid", FilterEffectFamily::Pattern, 5, 1, -1 },
    { "key_3d", FilterEffectFamily::Key, 2, 1, -1 },
    { "hsl_keyer", FilterEffectFamily::Key, 3, 1, -1 },
    { "alpha_matte_shrink_glow", FilterEffectFamily::Key, 4, 1, -1 },
    { "aperture_diffraction", FilterEffectFamily::Light, 0, 1, -1 },
    { "halation", FilterEffectFamily::Light, 1, 1, -1 },
    { "lens_flare", FilterEffectFamily::Light, 2, 1, -1 },
    { "lens_reflections", FilterEffectFamily::Light, 3, 1, -1 },
    { "light_rays", FilterEffectFamily::Light, 4, 1, -1 },
    { "beauty", FilterEffectFamily::Revival, 6, 1, -1 },
    { "automatic_dirt_removal", FilterEffectFamily::Revival, 0, 1, -1 },
    { "chromatic_aberration_removal", FilterEffectFamily::Revival, 1, 1, -1 },
    { "dead_pixel_fixer", FilterEffectFamily::Revival, 2, 1, -1 },
    { "deband", FilterEffectFamily::Revival, 3, 1, -1 },
    { "deflicker", FilterEffectFamily::Revival, 4, 1, -1 },
    { "frame_replacer", FilterEffectFamily::Revival, 5, 1, -1 },
    { "patch_replacer", FilterEffectFamily::Revival, 7, 1, -1 },
    { "noise_reduction", FilterEffectFamily::Maxine, 3, 1, -1, FilterExecutionBackend::Maxine },
    { "abstraction", FilterEffectFamily::Stylize, 24, 1, -1 },
    { "blanking_fill", FilterEffectFamily::Stylize, 25, 1, -1 },
    { "pencil_sketch", FilterEffectFamily::Stylize, 26, 1, -1 },
    { "prism_blur", FilterEffectFamily::Stylize, 27, 1, -1 },
    { "stylize", FilterEffectFamily::Stylize, 28, 1, -1 },
    { "motion_trails", FilterEffectFamily::Temporal, 0, 1, -1 },
    { "smear", FilterEffectFamily::Temporal, 1, 1, -1 },
    { "stop_motion", FilterEffectFamily::Temporal, 2, 1, -1 },
    { "analog_damage", FilterEffectFamily::Stylize, 29, 1, -1 },
    { "film_damage", FilterEffectFamily::Stylize, 30, 1, -1 },
    { "jpeg_damage", FilterEffectFamily::Stylize, 31, 1, -1 },
    { "texture_pop", FilterEffectFamily::Stylize, 32, 1, -1 },
    { "camera_shake", FilterEffectFamily::Transform, 13, 1, -1 },
    { "video_collage", FilterEffectFamily::Transform, 14, 1, -1 },
    { "dent", FilterEffectFamily::Distort, 17, 1, -1 },
    { "lens_distortion", FilterEffectFamily::Distort, 18, 1, -1 },
    { "ripples", FilterEffectFamily::Distort, 19, 1, -1 },
    { "vortex", FilterEffectFamily::Distort, 20, 1, -1 },
    { "warper", FilterEffectFamily::Distort, 21, 1, -1 },
    { "waviness", FilterEffectFamily::Distort, 22, 1, -1 },
    { "binoculars", FilterEffectFamily::Stylize, 33, 1, -1 },
    { "cctv", FilterEffectFamily::Stylize, 34, 1, -1 },
    { "colored_border", FilterEffectFamily::Stylize, 35, 1, -1 },
    { "digital_glitch", FilterEffectFamily::Stylize, 36, 1, -1 },
    { "drone_overlay", FilterEffectFamily::Stylize, 37, 1, -1 },
    { "dslr", FilterEffectFamily::Stylize, 38, 1, -1 },
    { "dve", FilterEffectFamily::Stylize, 39, 1, -1 },
    { "video_call", FilterEffectFamily::Stylize, 40, 1, -1 },
    { "video_camera", FilterEffectFamily::Stylize, 41, 1, -1 },
    { "background", FilterEffectFamily::Generate, 20, 1, -1 },
    { "fast_noise", FilterEffectFamily::Generate, 21, 1, -1 },
    { "plasma", FilterEffectFamily::Generate, 22, 1, -1 },
    { "mandelbrot", FilterEffectFamily::Generate, 23, 1, -1 },
    { "day_sky", FilterEffectFamily::Generate, 24, 1, -1 },
    { "dctl", FilterEffectFamily::Utility, 8, 1, -1 },

    // NVIDIA Maxine (4)
    { "nvidia_artifact_reduction", FilterEffectFamily::Maxine, 0, 1, -1, FilterExecutionBackend::Maxine },
    { "nvidia_super_resolution", FilterEffectFamily::Maxine, 1, 1, -1, FilterExecutionBackend::Maxine },
    { "nvidia_upscale", FilterEffectFamily::Maxine, 2, 1, -1, FilterExecutionBackend::Maxine },
};

const QHash<QString, FilterEffectMeta>& metaMap()
{
    static const QHash<QString, FilterEffectMeta> kMap = [] {
        QHash<QString, FilterEffectMeta> m;
        m.reserve(static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0])) + 8);
        for (const RawEntry& e : kEntries) {
            FilterEffectMeta meta;
            meta.family = e.family;
            meta.familyId = e.familyId;
            meta.internalPasses = e.internalPasses;
            meta.blendModeOverride = e.blendModeOverride;
            meta.backend = e.backend;
            m.insert(QString::fromLatin1(e.typeId), meta);
        }
        return m;
    }();
    return kMap;
}

} // namespace

FilterEffectMeta filterEffectMeta(const QString& typeId)
{
    return metaMap().value(typeId.toLower());
}

int filterEffectInternalPasses(const QString& typeId)
{
    return filterEffectMeta(typeId).internalPasses;
}

int filterEffectCatalogCount()
{
    return static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0]));
}

bool filterUsesMaxineBackend(const QString& typeId)
{
    return filterEffectMeta(typeId).backend == FilterExecutionBackend::Maxine;
}

QString maxineFilterCategoryKey()
{
    return QStringLiteral("NVIDIA");
}

QString feedbackMarkerTypeId()
{
    return QStringLiteral("feedback_marker");
}

bool isFeedbackMarkerNode(const QString& typeId)
{
    return typeId.compare(feedbackMarkerTypeId(), Qt::CaseInsensitive) == 0;
}

bool splitFilterChainAtFeedbackMarker(const QList<CellFilterNode>& chain,
                                      QList<CellFilterNode>* pre,
                                      QList<CellFilterNode>* post)
{
    if (pre) {
        pre->clear();
    }
    if (post) {
        post->clear();
    }

    bool foundMarker = false;
    for (const CellFilterNode& node : chain) {
        if (!foundMarker && isFeedbackMarkerNode(node.typeId)) {
            foundMarker = true;
            continue;
        }
        if (!foundMarker) {
            if (pre) {
                pre->append(node);
            }
        } else if (post) {
            post->append(node);
        }
    }

    if (!foundMarker && post) {
        *post = chain;
    }
    return foundMarker;
}

bool isOutputAllowedFilter(const QString& typeId)
{
    const QString lower = typeId.toLower();
    if (lower.isEmpty()) {
        return false;
    }
    return filterUsesMaxineBackend(lower)
        && filterCatalogCategory(lower) == maxineFilterCategoryKey();
}

void sanitizeOutputFilterChain(QList<CellFilterNode>& chain)
{
    QList<CellFilterNode> kept;
    kept.reserve(chain.size());
    for (const CellFilterNode& node : chain) {
        if (!isOutputAllowedFilter(node.typeId)) {
            continue;
        }
        CellFilterNode copy = node;
        copy.params = defaultParamsFor(copy.typeId);
        for (const EffectParam& saved : node.params) {
            for (EffectParam& p : copy.params) {
                if (p.name == saved.name) {
                    p.value = saved.value;
                    break;
                }
            }
        }
        kept.append(copy);
    }
    chain = std::move(kept);
}

} // namespace pvj::core

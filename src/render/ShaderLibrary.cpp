#include "ShaderLibrary.h"

#include <QFile>
#include <QHash>
#include <QtGlobal>

namespace pvj::render {

QString effectFragmentShaderResource(EffectShaderId id)
{
    switch (id) {
    case EffectShaderId::Blur:
        return QStringLiteral(":/shaders/effect_blur.frag.qsb");
    case EffectShaderId::ColorCorrection:
        return QStringLiteral(":/shaders/effect_color.frag.qsb");
    case EffectShaderId::Kaleido:
        return QStringLiteral(":/shaders/effect_kaleido.frag.qsb");
    case EffectShaderId::Mask:
        return QStringLiteral(":/shaders/effect_mask.frag.qsb");
    }
    return {};
}

QString effectFragmentShaderForType(const QString& typeId)
{
    static const QHash<QString, QString> kByType = [] {
        QHash<QString, QString> m;
        const auto add = [&m](const QStringList& ids, const QString& path) {
            for (const auto& id : ids) {
                m.insert(id, path);
            }
        };
        add({ QStringLiteral("blur"), QStringLiteral("fast_blur"), QStringLiteral("gaussian_blur"),
              QStringLiteral("box_blur") }, QStringLiteral(":/shaders/effect_blur.frag.qsb"));
        add({ QStringLiteral("color"), QStringLiteral("color_correction"), QStringLiteral("brightness"),
              QStringLiteral("contrast"), QStringLiteral("gamma"), QStringLiteral("saturation"),
              QStringLiteral("hue"), QStringLiteral("hue_rotate"), QStringLiteral("invert"),
              QStringLiteral("black_white"), QStringLiteral("exposure"), QStringLiteral("threshold"),
              QStringLiteral("posterize"), QStringLiteral("solarize"), QStringLiteral("tint"),
              QStringLiteral("colorize"), QStringLiteral("duotone"), QStringLiteral("tritone"),
              QStringLiteral("gradient_map"), QStringLiteral("night_vision"), QStringLiteral("thermal"),
              QStringLiteral("x_ray"), QStringLiteral("hsl"), QStringLiteral("curves"), QStringLiteral("levels"),
              QStringLiteral("color_intensity"), QStringLiteral("color_balance") },
            QStringLiteral(":/shaders/effect_color.frag.qsb"));
        add({ QStringLiteral("kaleido"), QStringLiteral("kaleidoscope"), QStringLiteral("mirror"),
              QStringLiteral("mirror_quad"), QStringLiteral("mirror_stripes"), QStringLiteral("multi_mirror") },
            QStringLiteral(":/shaders/effect_kaleido.frag.qsb"));
        add({ QStringLiteral("pixelate"), QStringLiteral("rgb_shift"), QStringLiteral("vignette"),
              QStringLiteral("scanlines"), QStringLiteral("film_grain"), QStringLiteral("glow"),
              QStringLiteral("bloom"), QStringLiteral("edges"), QStringLiteral("emboss"),
              QStringLiteral("ripple"), QStringLiteral("wave"), QStringLiteral("twirl"),
              QStringLiteral("bulge"), QStringLiteral("fisheye"), QStringLiteral("zoom"),
              QStringLiteral("halftone"), QStringLiteral("crop_rectangle"), QStringLiteral("tile"), QStringLiteral("strobe"),
              QStringLiteral("cartoon"), QStringLiteral("watercolor"), QStringLiteral("oil_paint"),
              QStringLiteral("god_rays"), QStringLiteral("mesh_warp"), QStringLiteral("liquify"),
              QStringLiteral("slit_scanner"), QStringLiteral("space_warper"), QStringLiteral("shifty"),
              QStringLiteral("light_leak"), QStringLiteral("total_visual_annihilation"),
              QStringLiteral("radial_blur"), QStringLiteral("directional_blur"), QStringLiteral("motion_blur"),
              QStringLiteral("zoom_blur"), QStringLiteral("temporal_blur"), QStringLiteral("sharpen"),
              QStringLiteral("denoise"), QStringLiteral("transform"), QStringLiteral("scale"),
              QStringLiteral("rotate"), QStringLiteral("flip"), QStringLiteral("flip_horizontal"),
              QStringLiteral("flip_vertical"), QStringLiteral("distortion"), QStringLiteral("bend"),
              QStringLiteral("warp"), QStringLiteral("polar"), QStringLiteral("polarizer"),
              QStringLiteral("displacement"), QStringLiteral("smooth_transform"), QStringLiteral("screen_shake"),
              QStringLiteral("trails"), QStringLiteral("noise"), QStringLiteral("rgb_noise"),
              QStringLiteral("video_noise"), QStringLiteral("spotlight"), QStringLiteral("drop_shadow"),
              QStringLiteral("rainbow"), QStringLiteral("prismatic"), QStringLiteral("replicate"),
              QStringLiteral("echo"), QStringLiteral("find_edges"), QStringLiteral("glow_edges"),
              QStringLiteral("crt"), QStringLiteral("vhs"), QStringLiteral("vhsifyer"),
              QStringLiteral("broadcast"), QStringLiteral("reducto"), QStringLiteral("linear_mask"),
              QStringLiteral("mask"), QStringLiteral("crop"), QStringLiteral("add_subtract"),
              QStringLiteral("mix"), QStringLiteral("fade"), QStringLiteral("shift"),
              QStringLiteral("tilt_shift"), QStringLiteral("radar"), QStringLiteral("polka_dot"),
              QStringLiteral("stripes"), QStringLiteral("checkerboard"), QStringLiteral("dots"),
              QStringLiteral("selective_color"), QStringLiteral("channel_mixer"), QStringLiteral("color_lookup"),
              QStringLiteral("motion_tile"), QStringLiteral("spherize"), QStringLiteral("cylinder"),
              QStringLiteral("cube"), QStringLiteral("opacity"), QStringLiteral("transform_3d"),
              QStringLiteral("chromatic_aberration"), QStringLiteral("stroke"), QStringLiteral("erode"),
              QStringLiteral("dilate") },
            QStringLiteral(":/shaders/effect_distort.frag.qsb"));
        add({ QStringLiteral("chroma_key"), QStringLiteral("luma_key") },
            QStringLiteral(":/shaders/effect_key.frag.qsb"));
        add({ QStringLiteral("blend_normal"), QStringLiteral("blend_add"), QStringLiteral("blend_subtract"),
              QStringLiteral("blend_multiply"), QStringLiteral("blend_screen"), QStringLiteral("blend_overlay"),
              QStringLiteral("blend_soft_light"), QStringLiteral("blend_hard_light"),
              QStringLiteral("blend_color_dodge"), QStringLiteral("blend_color_burn"),
              QStringLiteral("blend_darken"), QStringLiteral("blend_lighten"),
              QStringLiteral("blend_difference"), QStringLiteral("blend_exclusion"),
              QStringLiteral("blend_mode") }, QStringLiteral(":/shaders/effect_blend.frag.qsb"));
        add({ QStringLiteral("feedback"), QStringLiteral("feedback_rotated"),
              QStringLiteral("feedback_pro"), QStringLiteral("warped_feedback") },
            QStringLiteral(":/shaders/layer_feedback.frag.qsb"));
        return m;
    }();
    return kByType.value(typeId.toLower(), QStringLiteral(":/shaders/effect_identity.frag.qsb"));
}

bool effectUsesTwoTextures(const QString& typeId)
{
    const QString id = typeId.toLower();
    return id.contains(QStringLiteral("displacement"))
        || id.contains(QStringLiteral("mask"))
        || id.contains(QStringLiteral("feedback"))
        || id.startsWith(QStringLiteral("blend_"))
        || id == QStringLiteral("blend_mode");
}

QString transitionVertexShaderResource()
{
    return QStringLiteral(":/shaders/transition.vert.qsb");
}

QString transitionFragmentShaderResource(TransitionShaderId id)
{
    switch (id) {
    case TransitionShaderId::Crossfade:
        return QStringLiteral(":/shaders/transition_crossfade.frag.qsb");
    case TransitionShaderId::LumaWipe:
        return QStringLiteral(":/shaders/transition_luma.frag.qsb");
    case TransitionShaderId::Slide:
        return QStringLiteral(":/shaders/transition_slide.frag.qsb");
    }
    return {};
}

QString sharedTexturedQuadVertexResource()
{
    return QStringLiteral(":/shaders/textured_quad.vert.qsb");
}

bool shaderBundleIsValid(const QString& qsbResourcePath)
{
    QFile f(qsbResourcePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    return f.size() > 0;
}

} // namespace pvj::render

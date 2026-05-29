#include "FilterParamSchema.h"

#include "FilterCatalog.h"
#include "FilterEffectIds.h"

#include <QSet>

#include <QtGlobal>
#include <utility>

namespace pvj::core {
namespace {

FilterParamSpec floatParam(
    const char* name, const char* label, double minV, double maxV, double defaultV,
    const char* section = "")
{
    FilterParamSpec p;
    p.name = QString::fromLatin1(name);
    p.label = QString::fromLatin1(label);
    p.section = QString::fromLatin1(section);
    p.kind = FilterParamKind::Float;
    p.minV = minV;
    p.maxV = maxV;
    p.defaultV = defaultV;
    return p;
}

FilterParamSpec angleParam(const char* name, const char* label, double defaultV = 0.0,
                           const char* section = "")
{
    FilterParamSpec p = floatParam(name, label, -180.0, 180.0, defaultV, section);
    p.kind = FilterParamKind::Angle;
    return p;
}

FilterParamSpec percentParam(const char* name, const char* label, double defaultV = 0.5,
                             const char* section = "")
{
    FilterParamSpec p = floatParam(name, label, 0.0, 1.0, defaultV, section);
    p.kind = FilterParamKind::Percent;
    return p;
}

FilterParamSpec boolParam(const char* name, const char* label, bool defaultV = false,
                          const char* section = "")
{
    FilterParamSpec p = floatParam(name, label, 0.0, 1.0, defaultV ? 1.0 : 0.0, section);
    p.kind = FilterParamKind::Bool;
    return p;
}

FilterParamSpec enumParam(const char* name, const char* label,
                          std::initializer_list<const char*> labels, int defaultIndex = 0,
                          const char* section = "")
{
    FilterParamSpec p = floatParam(name, label, 0.0, static_cast<double>(labels.size() - 1),
                                   defaultIndex, section);
    p.kind = FilterParamKind::EnumIndex;
    for (const char* item : labels) {
        p.enumLabels.push_back(QString::fromLatin1(item));
    }
    return p;
}

FilterParamSpec colorParam(const char* name, const char* label, int defaultRgb = 0xFFFFFF,
                           const char* section = "")
{
    FilterParamSpec p = floatParam(name, label, 0.0, 16777215.0, double(defaultRgb), section);
    p.kind = FilterParamKind::Color;
    return p;
}

bool isBlurTypeId(const QString& typeId)
{
    static const QSet<QString> k = {
        QStringLiteral("gaussian_blur"),
        QStringLiteral("box_blur"),
        QStringLiteral("directional_blur"),
        QStringLiteral("radial_blur"),
        QStringLiteral("zoom_blur"),
        QStringLiteral("mosaic_blur"),
        QStringLiteral("lens_blur"),
        QStringLiteral("sharpen"),
        QStringLiteral("sharpen_edges"),
        QStringLiteral("soften_sharpen"),
    };
    return k.contains(typeId.toLower());
}

bool isLightTypeId(const QString& typeId)
{
    static const QSet<QString> k = {
        QStringLiteral("aperture_diffraction"),
        QStringLiteral("glow"),
        QStringLiteral("halation"),
        QStringLiteral("lens_flare"),
        QStringLiteral("lens_reflections"),
        QStringLiteral("light_rays"),
    };
    return k.contains(typeId.toLower());
}

bool isFilmTypeId(const QString& typeId)
{
    static const QSet<QString> k = {
        QStringLiteral("film_look"),
        QStringLiteral("film_color"),
        QStringLiteral("film_split_tone"),
        QStringLiteral("film_vignette"),
        QStringLiteral("film_halation"),
        QStringLiteral("film_bloom"),
        QStringLiteral("film_grain"),
        QStringLiteral("film_flicker"),
        QStringLiteral("film_gate_weave"),
        QStringLiteral("film_gate"),
    };
    return k.contains(typeId.toLower());
}

QVector<FilterParamSpec> gaussianBlurParams()
{
    return {
        percentParam("horizontal_strength", "Horizontal Strength", 0.5, "Controls"),
        percentParam("vertical_strength", "Vertical Strength", 0.5, "Controls"),
        enumParam("border_type", "Border Type",
                  { "Black", "Replicate", "Reflect", "Wrap Around" }, 0, "Controls"),
        percentParam("blend", "Blend", 1.0, "Global Blend"),
        boolParam("use_alpha", "Use Alpha", true, "Global Blend"),
    };
}

QVector<FilterParamSpec> boxBlurParams()
{
    return {
        floatParam("iterations", "Iterations", 1.0, 6.0, 1.0, "Controls"),
        percentParam("horizontal_strength", "Horizontal Strength", 0.5, "Controls"),
        percentParam("vertical_strength", "Vertical Strength", 0.5, "Controls"),
        boolParam("same_horizontal_vertical", "Same Horizontal/Vertical", true, "Controls"),
        enumParam("blur_type", "Blur Type", { "Realistic", "Stylized" }, 0, "Controls"),
        enumParam("border_type", "Border Type",
                  { "Black", "Replicate", "Reflect", "Wrap Around" }, 0, "Controls"),
        percentParam("blend", "Blend", 1.0, "Global Blend"),
        boolParam("use_alpha", "Use Alpha", true, "Global Blend"),
    };
}

QVector<FilterParamSpec> directionalBlurParams()
{
    return {
        percentParam("blur_strength", "Blur Strength", 0.5, "Controls"),
        angleParam("blur_angle", "Blur Angle", 0.0, "Controls"),
        boolParam("symmetric_blur", "Symmetric Blur", false, "Controls"),
        enumParam("blur_type", "Blur Type", { "Realistic", "Stylized" }, 0, "Controls"),
        enumParam("border_type", "Border Type",
                  { "Black", "Replicate", "Reflect", "Wrap Around" }, 0, "Controls"),
        percentParam("blend", "Blend", 1.0, "Global Blend"),
        boolParam("use_alpha", "Use Alpha", true, "Global Blend"),
    };
}

QVector<FilterParamSpec> radialBlurParams()
{
    return {
        percentParam("smooth_strength", "Smooth Strength", 0.5, "Controls"),
        enumParam("blur_symmetry", "Blur Symmetry",
                  { "Symmetric", "Clockwise", "Anti-Clockwise" }, 0, "Controls"),
        enumParam("blur_type", "Blur Type", { "Realistic", "Stylized" }, 0, "Controls"),
        enumParam("border_type", "Border Type",
                  { "Black", "Replicate", "Reflect", "Wrap Around" }, 0, "Controls"),
        enumParam("quality", "Quality", { "Faster", "Better", "Best" }, 1, "Controls"),
        percentParam("blend", "Blend", 1.0, "Global Blend"),
        boolParam("use_alpha", "Use Alpha", true, "Global Blend"),
    };
}

QVector<FilterParamSpec> zoomBlurParams()
{
    return {
        enumParam("blur_type", "Blur Type", { "Realistic", "Stylized" }, 0, "Controls"),
        percentParam("zoom_amount", "Zoom Amount", 0.5, "Controls"),
        percentParam("smooth_strength", "Smooth Strength", 0.5, "Controls"),
        percentParam("center_exclusion", "Center Exclusion", 0.0, "Controls"),
        enumParam("border_type", "Border Type",
                  { "Black", "Replicate", "Reflect", "Wrap Around" }, 0, "Controls"),
        enumParam("quality", "Quality", { "Faster", "Better", "Best" }, 1, "Controls"),
        percentParam("blend", "Blend", 1.0, "Global Blend"),
        boolParam("use_alpha", "Use Alpha", true, "Global Blend"),
    };
}

QVector<FilterParamSpec> mosaicBlurParams()
{
    return {
        floatParam("pixel_frequency", "Pixel Frequency", 1.0, 500.0, 100.0, "Controls"),
        enumParam("cell_shape", "Cell Shape", { "Square", "Hexagon", "Triangle" }, 0, "Controls"),
        floatParam("aliasing", "Aliasing", 0.0, 1.0, 1.0, "Controls"),
        percentParam("blend", "Blend", 1.0, "Global Blend"),
        boolParam("use_alpha", "Use Alpha", true, "Global Blend"),
    };
}

QVector<FilterParamSpec> lensBlurParams()
{
    return {
        floatParam("blur_size", "Blur Size", 0.0, 64.0, 4.0, "Controls"),
        percentParam("highlights", "Highlights", 0.35, "Controls"),
        percentParam("blend", "Blend", 1.0, "Global Blend"),
        boolParam("use_alpha", "Use Alpha", true, "Global Blend"),
    };
}

QVector<FilterParamSpec> sharpenParams()
{
    return {
        floatParam("sharpen_amount", "Sharpen Amount", 0.0, 5.0, 1.8, "Main Controls"),
        floatParam("fine_detail_size", "Fine Detail Size", 0.0, 1.0, 0.05, "Detail Levels"),
        floatParam("fine_detail", "Fine Detail", 0.0, 2.0, 1.0, "Detail Levels"),
        floatParam("medium_details", "Medium Details", 0.0, 2.0, 1.0, "Detail Levels"),
        floatParam("large_details", "Large Details", 0.0, 2.0, 1.0, "Detail Levels"),
        percentParam("blend", "Blend", 1.0, "Global Blend"),
    };
}

QVector<FilterParamSpec> sharpenEdgesParams()
{
    return {
        floatParam("sharpen_amount", "Sharpen Amount", 0.0, 2.0, 0.5, "Main Controls"),
        percentParam("sharpen_radius", "Sharpen Radius", 0.5, "Main Controls"),
        boolParam("display_edges", "Display Edges", false, "Edge Detection"),
        percentParam("pre_denoise", "Pre Denoise", 0.0, "Edge Detection"),
        percentParam("edge_detect_threshold", "Edge Detect Threshold", 0.2, "Edge Detection"),
        percentParam("edge_mask_strength", "Edge Mask Strength", 0.5, "Edge Detection"),
        percentParam("edge_blur", "Edge Blur", 0.5, "Edge Detection"),
        percentParam("blend", "Blend", 1.0, "Global Blend"),
    };
}

QVector<FilterParamSpec> softenSharpenParams()
{
    return {
        floatParam("small_texture", "Small Texture", -1.0, 1.0, 0.0, "Texture"),
        floatParam("medium_texture", "Medium Texture", -1.0, 1.0, -0.8, "Texture"),
        floatParam("large_texture", "Large Texture", -1.0, 1.0, -0.3, "Texture"),
        floatParam("small_texture_size", "Small Texture Size", 0.0, 1.0, 0.5, "Texture"),
        percentParam("coring_softness", "Coring Softness", 0.0, "Coring"),
        percentParam("blend", "Blend", 1.0, "Global Blend"),
    };
}

QHash<QString, FilterNodeSpec> buildSchemas()
{
    QHash<QString, FilterNodeSpec> schema;
    schema.reserve(40);

    auto insertSpec = [&](const QString& typeId, QVector<FilterParamSpec> params) {
        FilterNodeSpec spec;
        spec.typeId = typeId;
        spec.params = std::move(params);
        schema.insert(spec.typeId, spec);
    };

    insertSpec(QStringLiteral("gaussian_blur"), gaussianBlurParams());
    insertSpec(QStringLiteral("box_blur"), boxBlurParams());
    insertSpec(QStringLiteral("directional_blur"), directionalBlurParams());
    insertSpec(QStringLiteral("radial_blur"), radialBlurParams());
    insertSpec(QStringLiteral("zoom_blur"), zoomBlurParams());
    insertSpec(QStringLiteral("mosaic_blur"), mosaicBlurParams());
    insertSpec(QStringLiteral("lens_blur"), lensBlurParams());
    insertSpec(QStringLiteral("sharpen"), sharpenParams());
    insertSpec(QStringLiteral("sharpen_edges"), sharpenEdgesParams());
    insertSpec(QStringLiteral("soften_sharpen"), softenSharpenParams());

    insertSpec(QStringLiteral("invert"),
        { boolParam("invert_red", "Invert Red", true, "Channels"),
          boolParam("invert_green", "Invert Green", true, "Channels"),
          boolParam("invert_blue", "Invert Blue", true, "Channels"),
          boolParam("invert_alpha", "Invert Alpha", false, "Channels") });

    insertSpec(QStringLiteral("chromatic_adaptation"),
        { enumParam("method", "Method",
                    { "CAT02", "Bradford Linear", "Von Kries", "Sharp", "CMCCAT2000" }, 0, "Controls"),
          floatParam("source_temp", "Source Temperature", 1000.0, 20000.0, 6500.0, "Source"),
          floatParam("source_tint", "Source Tint", -1.0, 1.0, 0.0, "Source"),
          floatParam("target_temp", "Target Temperature", 1000.0, 20000.0, 6500.0, "Target"),
          floatParam("target_tint", "Target Tint", -1.0, 1.0, 0.0, "Target"),
          percentParam("blend", "Blend", 1.0, "Global") });

    insertSpec(QStringLiteral("color_compressor"),
        { angleParam("target_hue", "Target Hue", 0.0, "Compressor"),
          percentParam("compress_hue", "Compress Hue", 0.5, "Compressor"),
          percentParam("compress_saturation", "Compress Saturation", 0.5, "Compressor"),
          percentParam("compress_luminance", "Compress Luminance", 0.5, "Compressor"),
          percentParam("blend", "Blend", 1.0, "Global") });

    insertSpec(QStringLiteral("color_stabilizer"),
        { enumParam("match", "Match", { "Luminance", "Color", "Both" }, 2, "Controls"),
          percentParam("strength", "Strength", 0.5, "Controls"),
          percentParam("blend", "Blend", 1.0, "Global") });

    insertSpec(QStringLiteral("contrast_pop"),
        { floatParam("detail_amount", "Detail Amount", -1.0, 1.0, 0.0, "Controls"),
          percentParam("detail_size", "Detail Size", 0.5, "Controls"),
          percentParam("low_threshold", "Low Threshold", 0.0, "Controls"),
          percentParam("high_threshold", "High Threshold", 1.0, "Controls"),
          percentParam("softness", "Softness", 0.5, "Controls"),
          percentParam("blend", "Blend", 1.0, "Global") });

    insertSpec(QStringLiteral("dehaze"),
        { floatParam("dehaze_strength", "Dehaze Strength", -1.0, 1.0, 0.5, "Controls"),
          angleParam("haze_hue", "Haze Color", 0.0, "Controls"),
          percentParam("blend", "Blend", 1.0, "Global") });

    // Resolve 21 Film Emulation (Film Look Creator sections)
    insertSpec(QStringLiteral("film_look"),
        { enumParam("film_look", "Film Look",
                    { "Default 65mm", "Default 35mm", "Cinematic", "Nostalgic", "Bleach Bypass",
                      "Rochester", "Akasaka", "Elated", "Vintage", "Aurora" },
                    0, "Film Look"),
          percentParam("blend", "Film Look Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("film_color"),
        { floatParam("exposure", "Exposure", -1.0, 1.0, 0.0, "Color Settings"),
          floatParam("contrast", "Contrast", -1.0, 1.0, 0.0, "Color Settings"),
          percentParam("highlights_fade", "Highlights Fade", 0.0, "Color Settings"),
          percentParam("fade_rolloff", "Fade Rolloff", 0.5, "Color Settings"),
          floatParam("temperature", "White Balance", 1000.0, 20000.0, 6500.0, "Color Settings"),
          floatParam("tint", "Tint", -1.0, 1.0, 0.0, "Color Settings"),
          percentParam("subtractive_saturation", "Subtractive Sat", 0.0, "Color Settings"),
          floatParam("saturation", "Saturation", -1.0, 1.0, 0.0, "Color Settings"),
          percentParam("richness", "Richness", 0.0, "Color Settings"),
          percentParam("blend", "Color Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("film_split_tone"),
        { percentParam("amount", "Amount", 0.0, "Split Tone"),
          angleParam("hue_angle", "Hue Angle", 23.0, "Split Tone"),
          percentParam("balance", "Balance", 0.5, "Split Tone"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("film_vignette"),
        { percentParam("amount", "Amount", 0.5, "Vignette"),
          percentParam("size", "Size", 0.5, "Vignette"),
          percentParam("softness", "Softness", 0.5, "Vignette"),
          percentParam("roundness", "Roundness", 0.5, "Vignette"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("film_halation"),
        { percentParam("amount", "Amount", 0.5, "Halation"),
          percentParam("threshold", "Threshold", 0.5, "Halation"),
          percentParam("size", "Size", 0.5, "Halation"),
          angleParam("hue", "Hue", 0.0, "Halation"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("film_bloom"),
        { percentParam("amount", "Amount", 0.5, "Bloom"),
          percentParam("threshold", "Threshold", 0.5, "Bloom"),
          percentParam("size", "Size", 0.5, "Bloom"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("film_grain"),
        { percentParam("size", "Size", 0.5, "Grain"),
          percentParam("strength", "Strength", 0.35, "Grain"),
          boolParam("monochrome", "Monochrome", true, "Grain"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("film_flicker"),
        { percentParam("amount", "Amount", 0.3, "Flicker"),
          percentParam("speed", "Speed", 0.5, "Flicker"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("film_gate_weave"),
        { percentParam("amount_h", "Amount H", 0.3, "Gate Weave"),
          percentParam("amount_v", "Amount V", 0.3, "Gate Weave"),
          percentParam("speed", "Speed", 0.5, "Gate Weave"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("film_gate"),
        { enumParam("ratio", "Ratio",
                    { "4:3", "16:10", "16:9", "1.85", "2.39", "Super 16", "Super 8" }, 2,
                    "Film Gate"),
          percentParam("padding", "Padding", 0.0, "Film Gate"),
          percentParam("softness", "Softness", 0.5, "Film Gate"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    // ResolveFX Light (Resolve 21 manual parameters)
    insertSpec(QStringLiteral("aperture_diffraction"),
        { enumParam("quality", "Quality", { "Full", "Half", "Quarter" }, 0, "Quality"),
          percentParam("source_threshold", "Source Threshold", 0.65, "Controls"),
          enumParam("iris_shape", "Iris Shape",
                    { "Triangle", "Square", "Pentagon", "Hexagon", "Heptagon", "Octagon" }, 3,
                    "Aperture"),
          percentParam("aperture_size", "Aperture Size", 0.5, "Aperture"),
          percentParam("blade_curvature", "Blade Curvature", 0.35, "Aperture"),
          angleParam("rotation", "Rotation", 0.0, "Aperture"),
          percentParam("hv_ratio", "H/V Ratio", 0.0, "Aperture"),
          angleParam("angle", "Angle", 0.0, "Aperture"),
          percentParam("chroma_shift", "Chroma Shift", 0.25, "Aperture"),
          percentParam("result_gamma", "Result Gamma", 0.5, "Diffraction"),
          percentParam("result_scale", "Result Scale", 0.5, "Diffraction"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("glow"),
        { enumParam("source_of_glow", "Source of Glow", { "Bright Regions", "Alpha" }, 0,
                    "Controls"),
          percentParam("glow_size", "Glow Size", 0.5, "Controls"),
          percentParam("spread", "Spread", 0.5, "Controls"),
          percentParam("brightness", "Brightness", 0.5, "Controls"),
          percentParam("threshold", "Threshold", 0.65, "Controls"),
          colorParam("glow_color", "Glow Color", 0xFFFFFF, "Color Scale"),
          enumParam("composite_type", "Composite Type",
                    { "Add", "Screen", "Overlay", "Luminosity" }, 0, "Controls"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("halation"),
        { enumParam("processing_color_space", "Processing Color Space",
                    { "Default", "Sony S-Gamut3", "Rec.709", "P3 D65", "Film" }, 0, "Processing"),
          colorParam("halation_color", "Halation Color", 0xFF6633, "Secondary Glow"),
          percentParam("strength", "Strength", 0.66, "Secondary Glow"),
          percentParam("gamma", "Gamma", 0.5, "Secondary Glow"),
          percentParam("spread", "Spread", 0.45, "Secondary Glow"),
          percentParam("threshold", "Threshold", 0.65, "Isolation"),
          percentParam("film_saturation_level", "Film Saturation Level", 0.5, "Isolation"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("lens_flare"),
        { percentParam("global_scaling", "Global Scaling", 0.5, "Global Corrections"),
          percentParam("anamorphism", "Anamorphism", 0.0, "Global Corrections"),
          percentParam("lens_center_x", "Lens Center X", 0.5, "Global Corrections"),
          percentParam("lens_center_y", "Lens Center Y", 0.5, "Global Corrections"),
          percentParam("global_defocus", "Global Defocus", 0.0, "Global Corrections"),
          percentParam("global_brightness", "Global Brightness", 0.5, "Global Corrections"),
          percentParam("global_saturation", "Global Saturation", 0.5, "Global Corrections"),
          percentParam("colorise_result", "Colorise Result", 0.0, "Global Corrections"),
          colorParam("colorization_color", "Colorization Color", 0xFFFFFF, "Global Corrections"),
          percentParam("position_x", "Position X", 0.65, "Position"),
          percentParam("position_y", "Position Y", 0.35, "Position"),
          floatParam("aperture_blades", "Aperture Blades", 3.0, 16.0, 6.0, "Aperture"),
          angleParam("aperture_angle", "Angle", 0.0, "Aperture"),
          percentParam("glare_brightness", "Glare Brightness", 0.5, "Full-Screen Glare"),
          colorParam("glare_color", "Glare Color", 0xFFDD88, "Full-Screen Glare"),
          percentParam("flare_size", "Flare Size", 0.35, "Flare Spot"),
          percentParam("flare_irregularity", "Flare Irregularity", 0.2, "Flare Spot"),
          percentParam("flare_softness", "Flare Softness", 0.4, "Flare Spot"),
          colorParam("flare_color", "Flare Color", 0xFFFFFF, "Flare Spot"),
          percentParam("starburst_size", "Starburst Size", 0.5, "Starburst"),
          percentParam("starburst_softness", "Starburst Softness", 0.4, "Starburst"),
          angleParam("starburst_split_angle", "Starburst Split Angle", 15.0, "Starburst"),
          percentParam("starburst_split_balance", "Starburst Split Balance", 0.5, "Starburst"),
          colorParam("starburst_color", "Starburst Color", 0xFFFFFF, "Starburst"),
          enumParam("ghost1_shape", "Ghost 1 Shape",
                    { "None", "Aperture", "Anamorphic Streak", "Disc", "Bubble", "Corona" }, 2,
                    "Ghost 1"),
          colorParam("ghost1_color", "Ghost 1 Color", 0x88AAFF, "Ghost 1"),
          percentParam("ghost1_position", "Ghost 1 Position", 0.3, "Ghost 1"),
          percentParam("ghost1_size", "Ghost 1 Size", 0.25, "Ghost 1"),
          percentParam("ghost1_center_brightness", "Ghost 1 Center Brightness", 0.5, "Ghost 1"),
          percentParam("ghost1_edge_brightness", "Ghost 1 Edge Brightness", 0.7, "Ghost 1"),
          percentParam("ghost1_softness", "Ghost 1 Softness", 0.4, "Ghost 1"),
          percentParam("ghost1_ringing", "Ghost 1 Ringing", 0.3, "Ghost 1"),
          percentParam("ghost1_chromatic_shift", "Ghost 1 Chromatic Shift", 0.1, "Ghost 1"),
          enumParam("ghost2_shape", "Ghost 2 Shape",
                    { "None", "Aperture", "Anamorphic Streak", "Disc", "Bubble", "Corona" }, 3,
                    "Ghost 2"),
          colorParam("ghost2_color", "Ghost 2 Color", 0xFFCC88, "Ghost 2"),
          percentParam("ghost2_position", "Ghost 2 Position", 0.55, "Ghost 2"),
          percentParam("ghost2_size", "Ghost 2 Size", 0.18, "Ghost 2"),
          percentParam("ghost2_center_brightness", "Ghost 2 Center Brightness", 0.4, "Ghost 2"),
          percentParam("ghost2_edge_brightness", "Ghost 2 Edge Brightness", 0.6, "Ghost 2"),
          percentParam("ghost2_softness", "Ghost 2 Softness", 0.35, "Ghost 2"),
          percentParam("ghost2_ringing", "Ghost 2 Ringing", 0.25, "Ghost 2"),
          percentParam("ghost2_chromatic_shift", "Ghost 2 Chromatic Shift", 0.15, "Ghost 2"),
          enumParam("ghost3_shape", "Ghost 3 Shape",
                    { "None", "Aperture", "Anamorphic Streak", "Disc", "Bubble", "Corona" }, 0,
                    "Ghost 3"),
          colorParam("ghost3_color", "Ghost 3 Color", 0xFFFFFF, "Ghost 3"),
          percentParam("ghost3_position", "Ghost 3 Position", 0.75, "Ghost 3"),
          percentParam("ghost3_size", "Ghost 3 Size", 0.12, "Ghost 3"),
          percentParam("ghost3_center_brightness", "Ghost 3 Center Brightness", 0.3, "Ghost 3"),
          percentParam("ghost3_edge_brightness", "Ghost 3 Edge Brightness", 0.5, "Ghost 3"),
          percentParam("ghost3_softness", "Ghost 3 Softness", 0.3, "Ghost 3"),
          percentParam("ghost3_ringing", "Ghost 3 Ringing", 0.2, "Ghost 3"),
          percentParam("ghost3_chromatic_shift", "Ghost 3 Chromatic Shift", 0.05, "Ghost 3"),
          enumParam("ghost4_shape", "Ghost 4 Shape",
                    { "None", "Aperture", "Anamorphic Streak", "Disc", "Bubble", "Corona" }, 4,
                    "Ghost 4"),
          colorParam("ghost4_color", "Ghost 4 Color", 0xAADDFF, "Ghost 4"),
          percentParam("ghost4_position", "Ghost 4 Position", 0.9, "Ghost 4"),
          percentParam("ghost4_size", "Ghost 4 Size", 0.1, "Ghost 4"),
          percentParam("ghost4_center_brightness", "Ghost 4 Center Brightness", 0.25, "Ghost 4"),
          percentParam("ghost4_edge_brightness", "Ghost 4 Edge Brightness", 0.45, "Ghost 4"),
          percentParam("ghost4_softness", "Ghost 4 Softness", 0.35, "Ghost 4"),
          percentParam("ghost4_ringing", "Ghost 4 Ringing", 0.15, "Ghost 4"),
          percentParam("ghost4_chromatic_shift", "Ghost 4 Chromatic Shift", 0.08, "Ghost 4"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("lens_reflections"),
        { percentParam("threshold", "Threshold", 0.7, "Isolation"),
          percentParam("brightness", "Brightness", 0.5, "Global"),
          percentParam("gamma", "Gamma", 0.5, "Global"),
          colorParam("color", "Color", 0xFFEECC, "Global"),
          percentParam("smooth", "Smooth", 1.0, "Global"),
          percentParam("eclipse_position", "Eclipse Position", 0.0, "Eclipse"),
          percentParam("eclipse_size", "Eclipse Size", 0.0, "Eclipse"),
          percentParam("eclipse_softness", "Eclipse Softness", 0.5, "Eclipse"),
          percentParam("eclipse_chromatic_shift", "Eclipse Chromatic Shift", 0.0, "Eclipse"),
          percentParam("repeat", "Repeat", 0.0, "Repeat"),
          percentParam("repeat_position_seed", "Repeat Position Seed", 0.5, "Repeat"),
          percentParam("repeat_size_seed", "Repeat Size Seed", 0.5, "Repeat"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("light_rays"),
        { enumParam("source_of_rays", "Source of Rays", { "Bright Regions", "Edges" }, 0,
                    "Main"),
          percentParam("source_threshold", "Source Threshold", 0.65, "Main"),
          enumParam("ray_directions", "Ray Directions", { "From A Location", "At an Angle" }, 0,
                    "Position"),
          percentParam("ray_location_x", "Ray Location X", 0.5, "Position"),
          percentParam("ray_location_y", "Ray Location Y", 0.5, "Position"),
          angleParam("ray_angle", "Ray Angle", 0.0, "Position"),
          percentParam("length", "Length", 0.5, "Appearance"),
          percentParam("soften", "Soften", 0.35, "Appearance"),
          percentParam("brightness", "Brightness", 0.5, "Appearance"),
          percentParam("saturation", "Saturation", 0.5, "Appearance"),
          enumParam("ccd_bloom", "CCD Bloom", { "Off", "Harsh", "Soft" }, 0, "Appearance"),
          enumParam("composite_type", "Composite Type",
                    { "Add", "Screen", "Overlay", "Luminosity" }, 0, "Appearance"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(feedbackMarkerTypeId(), {});

    // Keying-only (not in catalog picker).
    insertSpec(QStringLiteral("chroma_key"),
        { enumParam("mode", "Key mode",
                    { "Chroma (hue)", "Chroma (hue) inverted", "RGB distance", "RGB distance inverted",
                      "RGB max delta", "RGB max delta inverted" },
                    0),
          percentParam("hue", "Key hue", 0.33),
          percentParam("threshold", "Threshold", 0.2),
          percentParam("softness", "Softness", 0.2) });
    insertSpec(QStringLiteral("luma_key"),
        { enumParam("mode", "Key source",
                    { "Weighted RGB (inspector)", "Weighted RGB inverted", "Luma BT.709",
                      "Luma BT.709 inverted", "Red channel", "Green channel", "Blue channel",
                      "Max RGB", "Min RGB", "Max RGB inverted" },
                    0),
          percentParam("brightness", "Level / center", 0.5),
          percentParam("threshold", "Tolerance", 0.25),
          percentParam("softness", "Feather", 0.12) });
    insertSpec(QStringLiteral("mask"),
        { enumParam("mode", "Mask mode",
                    { "None", "Rectangle", "Circle", "Soft edge", "Ellipse", "Custom" }, 0),
          percentParam("sizeX", "Size X", 1.0),
          percentParam("sizeY", "Size Y", 1.0),
          percentParam("feather", "Feather", 0.1) });

#ifndef NDEBUG
    for (const FilterCatalogEntry& entry : filterCatalogEntries()) {
        Q_ASSERT(schema.contains(entry.typeId));
    }
#endif

    return schema;
}

} // namespace

bool isColorTypeId(const QString& typeId)
{
    static const QSet<QString> k = {
        QStringLiteral("chromatic_adaptation"),
        QStringLiteral("color_compressor"),
        QStringLiteral("contrast_pop"),
    };
    return k.contains(typeId.toLower());
}

bool filterAllowsExtendedParams(const QString& typeId)
{
    return isBlurTypeId(typeId) || isColorTypeId(typeId) || isFilmTypeId(typeId)
        || isLightTypeId(typeId);
}

const QHash<QString, FilterNodeSpec>& filterParamSchemas()
{
    static const QHash<QString, FilterNodeSpec> kSchema = buildSchemas();
    return kSchema;
}

QList<EffectParam> defaultParamsFor(const QString& typeId)
{
    const FilterNodeSpec spec = filterParamSchemas().value(typeId);
    QList<EffectParam> out;
    out.reserve(spec.params.size());
    for (const FilterParamSpec& p : spec.params) {
        out.append({ p.name, p.defaultV });
    }
    return out;
}

} // namespace pvj::core

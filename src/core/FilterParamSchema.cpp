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

FilterParamSpec intParam(const char* name, const char* label, double minV, double maxV,
                         double defaultV, const char* section = "")
{
    FilterParamSpec p = floatParam(name, label, minV, maxV, defaultV, section);
    p.kind = FilterParamKind::Float;
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

bool isTemporalTypeId(const QString& typeId)
{
    static const QSet<QString> k = {
        QStringLiteral("motion_trails"),
        QStringLiteral("smear"),
        QStringLiteral("stop_motion"),
        QStringLiteral("motion_blur"),
    };
    return k.contains(typeId.toLower());
}

bool isTextureTypeId(const QString& typeId)
{
    static const QSet<QString> k = {
        QStringLiteral("analog_damage"),
        QStringLiteral("film_damage"),
        QStringLiteral("jpeg_damage"),
        QStringLiteral("texture_pop"),
    };
    return k.contains(typeId.toLower());
}

#define PVJ_SCRATCH_PARAMS(N, DEFPOS, MOVING)                                                       \
    colorParam("scratch" #N "_color", "Scratch Color", 0xE8E8E8, "Add Scratch " #N),               \
        percentParam("scratch" #N "_position", "Scratch Position", DEFPOS, "Add Scratch " #N),    \
        percentParam("scratch" #N "_width", "Scratch Width", 0.002, "Add Scratch " #N),             \
        percentParam("scratch" #N "_strength", "Scratch Strength", 0.4, "Add Scratch " #N),         \
        percentParam("scratch" #N "_blur", "Scratch Blur", 0.3, "Add Scratch " #N),                \
        boolParam("scratch" #N "_moving", "Moving Scratch", MOVING, "Add Scratch " #N),             \
        percentParam("scratch" #N "_moving_amplitude", "Moving Amplitude", 0.3, "Add Scratch " #N), \
        percentParam("scratch" #N "_moving_speed", "Moving Speed", 0.5, "Add Scratch " #N),         \
        percentParam("scratch" #N "_moving_randomness", "Moving Randomness", 0.4,                   \
                     "Add Scratch " #N),                                                            \
        percentParam("scratch" #N "_flickering_speed", "Flickering Speed", 0.5,                   \
                     "Add Scratch " #N)

QVector<FilterParamSpec> jpegDamageParams()
{
    return {
        percentParam("quality", "Quality", 1.0, "Controls"),
        percentParam("resolution", "Resolution", 0.5, "Controls"),
        percentParam("block_aspect_ratio", "Block Aspect Ratio", 0.5, "Controls"),
        percentParam("frequency_scale", "Frequency Scale", 0.5, "Controls"),
        enumParam("scale_component", "Scale Component", { "All Frequencies", "X-Frequency", "Y-Frequency" },
                  0, "Controls"),
        percentParam("blend", "Blend", 1.0, "Global Blend"),
    };
}

QVector<FilterParamSpec> texturePopParams()
{
    return {
        enumParam("mode", "Mode", { "Simple", "Advanced" }, 0, "Mode"),
        floatParam("details", "Details", -1.0, 1.0, 0.0, "Simple"),
        floatParam("rough", "Rough", -1.0, 1.0, 0.0, "Advanced"),
        floatParam("coarse", "Coarse", -1.0, 1.0, 0.0, "Advanced"),
        floatParam("medium", "Medium", -1.0, 1.0, 0.0, "Advanced"),
        floatParam("small", "Small", -1.0, 1.0, 0.0, "Advanced"),
        floatParam("fine", "Fine", -1.0, 1.0, 0.0, "Advanced"),
        floatParam("tiny", "Tiny", -1.0, 1.0, 0.0, "Advanced"),
        floatParam("strength", "Strength", 0.0, 2.0, 1.0, "Controls"),
        percentParam("shadows", "Shadows", 1.0, "Tonal Range"),
        percentParam("midtones", "Midtones", 1.0, "Tonal Range"),
        percentParam("highlights", "Highlights", 1.0, "Tonal Range"),
        percentParam("blend", "Blend", 1.0, "Global Blend"),
    };
}

QVector<FilterParamSpec> filmDamageParams()
{
    QVector<FilterParamSpec> p;
    p.append({
        percentParam("film_blur", "Film Blur", 0.15, "Blur and Shift"),
        floatParam("temp_shift", "Temp Shift", -1.0, 1.0, 0.12, "Blur and Shift"),
        floatParam("tint_shift", "Tint Shift", -1.0, 1.0, 0.18, "Blur and Shift"),
        percentParam("focal_factor", "Focal Factor", 0.55, "Add Vignetting"),
        percentParam("geometry_factor", "Geometry Factor", 0.5, "Add Vignetting"),
        percentParam("tilt_amount", "Tilt Amount", 0.0, "Add Vignetting"),
        angleParam("tilt_angle", "Tilt Angle", 0.0, "Add Vignetting"),
        colorParam("dirt_color", "Dirt Color", 0x1A1A1A, "Add Dirt"),
        boolParam("changing_dirt", "Changing Dirt", true, "Add Dirt"),
        percentParam("dirt_density", "Dirt Density", 0.25, "Add Dirt"),
        percentParam("dirt_size", "Dirt Size", 0.4, "Add Dirt"),
        percentParam("dirt_blur", "Dirt Blur", 0.35, "Add Dirt"),
        floatParam("dirt_seed", "Dirt Seed", 0.0, 100.0, 1.0, "Add Dirt"),
    });
    p.append({ PVJ_SCRATCH_PARAMS(1, 0.15, true), PVJ_SCRATCH_PARAMS(2, 0.35, true),
               PVJ_SCRATCH_PARAMS(3, 0.55, false), PVJ_SCRATCH_PARAMS(4, 0.72, false),
               PVJ_SCRATCH_PARAMS(5, 0.88, false) });
    p.append(percentParam("blend", "Blend", 1.0, "Global Blend"));
    return p;
}

QVector<FilterParamSpec> analogDamageParams()
{
    return {
        enumParam("preset", "Preset",
                  { "Custom", "VHS", "Bad Reception", "Old TV", "Damaged Tape", "Security Cam" }, 0,
                  "Preset"),
        percentParam("vignetting", "Vignetting", 0.35, "Telecine Source"),
        percentParam("vignette_aspect", "Vignette Aspect", 0.5, "Telecine Source"),
        percentParam("shutter_weave", "Shutter Weave", 0.2, "Telecine Source"),
        percentParam("noise_scale", "Noise Scale", 0.5, "Broadcast Signal"),
        percentParam("signal_noise", "Signal Noise", 0.25, "Broadcast Signal"),
        percentParam("chroma_noise", "Chroma Noise", 0.2, "Broadcast Signal"),
        percentParam("detail_loss", "Detail Loss", 0.2, "Broadcast Signal"),
        percentParam("chroma_detail_loss", "Chroma Detail Loss", 0.15, "Broadcast Signal"),
        percentParam("ghosting", "Ghosting", 0.15, "Broadcast Signal"),
        percentParam("ghost_offset", "Ghost Offset", 0.3, "Broadcast Signal"),
        percentParam("chroma_misalignment", "Chroma Misalignment", 0.2, "Broadcast Signal"),
        percentParam("brightness", "Brightness", 0.5, "Color Dials"),
        percentParam("contrast", "Contrast", 0.5, "Color Dials"),
        percentParam("color", "Color", 0.5, "Color Dials"),
        percentParam("tint", "Tint", 0.5, "Color Dials"),
        percentParam("image_aspect", "Image Aspect", 0.5, "Scan"),
        percentParam("h_shift", "H-Shift", 0.0, "Scan"),
        percentParam("v_shift", "V-Shift", 0.0, "Scan"),
        percentParam("v_hold", "V-Hold", 0.0, "Scan"),
        boolParam("v_hold_latch", "V-Hold Latch", false, "Scan"),
        percentParam("overscan", "Overscan", 0.0, "Scan"),
        percentParam("v_scale", "V-Scale", 0.0, "Scan"),
        percentParam("vertical_blanking", "Vertical Blanking", 0.0, "Scan"),
        percentParam("line_sharpness", "Line Sharpness", 0.5, "Scan Lines"),
        floatParam("line_frequency", "Line Frequency", 100.0, 800.0, 400.0, "Scan Lines"),
        boolParam("colored_lines", "Colored Lines", false, "Scan Lines"),
        percentParam("phosphor_brightness", "Phosphor Brightness", 0.05, "TV Construction"),
        percentParam("phosphor_tint", "Phosphor Tint", 0.1, "TV Construction"),
        percentParam("defocus", "Defocus", 0.15, "TV Construction"),
        percentParam("screen_curvature", "Screen Curvature", 0.25, "TV Construction"),
        boolParam("edge_mask", "Edge Mask", false, "TV Construction"),
        boolParam("edges_transparent", "Edges Transparent", false, "TV Construction"),
        percentParam("mask_curvature", "Mask Curvature", 0.5, "TV Construction"),
        percentParam("mask_aspect", "Mask Aspect", 0.5, "TV Construction"),
        percentParam("restless_foot_height", "Restless Foot Height", 0.0, "VHS"),
        percentParam("restless_foot_offset", "Restless Foot Offset", 0.0, "VHS"),
        percentParam("restless_foot_jitter", "Restless Foot Jitter", 0.3, "VHS"),
        percentParam("blend", "Blend", 1.0, "Global Blend"),
    };
}

bool isStylizeTypeId(const QString& typeId)
{
    static const QSet<QString> k = {
        QStringLiteral("abstraction"),
        QStringLiteral("blanking_fill"),
        QStringLiteral("drop_shadow"),
        QStringLiteral("edge_detect"),
        QStringLiteral("emboss"),
        QStringLiteral("mirrors"),
        QStringLiteral("pencil_sketch"),
        QStringLiteral("prism_blur"),
        QStringLiteral("scanlines"),
        QStringLiteral("stylize"),
        QStringLiteral("tilt_shift"),
        QStringLiteral("vignette"),
        QStringLiteral("watercolor"),
    };
    return k.contains(typeId.toLower());
}

QVector<FilterParamSpec> mirrorsParams()
{
    return {
        enumParam("mirror_placement", "Mirror Placement",
                  { "Individual", "Rosette", "Kaleidoscope" }, 0, "Main"),
        boolParam("reflect_at_borders", "Reflect at Borders", false, "Main"),
        boolParam("mirror1_enable", "Mirror 1 Enable", true, "Individual Mirrors"),
        percentParam("mirror1_x", "Mirror 1 X", 0.5, "Individual Mirrors"),
        percentParam("mirror1_y", "Mirror 1 Y", 0.5, "Individual Mirrors"),
        angleParam("mirror1_angle", "Mirror 1 Angle", 0.0, "Individual Mirrors"),
        boolParam("mirror1_flip", "Mirror 1 Flip", false, "Individual Mirrors"),
        boolParam("mirror2_enable", "Mirror 2 Enable", false, "Mirror 2"),
        percentParam("mirror2_x", "Mirror 2 X", 0.5, "Mirror 2"),
        percentParam("mirror2_y", "Mirror 2 Y", 0.5, "Mirror 2"),
        angleParam("mirror2_angle", "Mirror 2 Angle", 0.0, "Mirror 2"),
        boolParam("mirror2_flip", "Mirror 2 Flip", false, "Mirror 2"),
        boolParam("mirror3_enable", "Mirror 3 Enable", false, "Mirror 3"),
        percentParam("mirror3_x", "Mirror 3 X", 0.5, "Mirror 3"),
        percentParam("mirror3_y", "Mirror 3 Y", 0.5, "Mirror 3"),
        angleParam("mirror3_angle", "Mirror 3 Angle", 0.0, "Mirror 3"),
        boolParam("mirror3_flip", "Mirror 3 Flip", false, "Mirror 3"),
        boolParam("mirror4_enable", "Mirror 4 Enable", false, "Mirror 4"),
        percentParam("mirror4_x", "Mirror 4 X", 0.5, "Mirror 4"),
        percentParam("mirror4_y", "Mirror 4 Y", 0.5, "Mirror 4"),
        angleParam("mirror4_angle", "Mirror 4 Angle", 0.0, "Mirror 4"),
        boolParam("mirror4_flip", "Mirror 4 Flip", false, "Mirror 4"),
        boolParam("mirror5_enable", "Mirror 5 Enable", false, "Mirror 5"),
        percentParam("mirror5_x", "Mirror 5 X", 0.5, "Mirror 5"),
        percentParam("mirror5_y", "Mirror 5 Y", 0.5, "Mirror 5"),
        angleParam("mirror5_angle", "Mirror 5 Angle", 0.0, "Mirror 5"),
        boolParam("mirror5_flip", "Mirror 5 Flip", false, "Mirror 5"),
        boolParam("mirror6_enable", "Mirror 6 Enable", false, "Mirror 6"),
        percentParam("mirror6_x", "Mirror 6 X", 0.5, "Mirror 6"),
        percentParam("mirror6_y", "Mirror 6 Y", 0.5, "Mirror 6"),
        angleParam("mirror6_angle", "Mirror 6 Angle", 0.0, "Mirror 6"),
        boolParam("mirror6_flip", "Mirror 6 Flip", false, "Mirror 6"),
        percentParam("rosette_x", "Rosette X", 0.5, "Rosette"),
        percentParam("rosette_y", "Rosette Y", 0.5, "Rosette"),
        angleParam("rosette_angle", "Rosette Angle", 0.0, "Rosette"),
        percentParam("rosette_wedge_width", "Wedge Width", 0.5, "Rosette"),
        percentParam("kaleido_x", "Kaleidoscope X", 0.5, "Kaleidoscope"),
        percentParam("kaleido_y", "Kaleidoscope Y", 0.5, "Kaleidoscope"),
        percentParam("kaleido_center_size", "Center Size", 0.5, "Kaleidoscope"),
        angleParam("kaleido_angle", "Kaleidoscope Angle", 0.0, "Kaleidoscope"),
        floatParam("kaleido_sides", "Number of Sides", 3.0, 8.0, 4.0, "Kaleidoscope"),
        percentParam("blend", "Blend", 1.0, "Global Blend"),
    };
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
    schema.reserve(60);

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

    // Resolve FX Stylize (Resolve 21)
    insertSpec(QStringLiteral("abstraction"),
        { percentParam("pre_blur", "Pre Blur", 0.15, "Main Controls"),
          percentParam("abstraction_strength", "Abstraction Strength", 0.65, "Main Controls"),
          percentParam("iterate_abstraction", "Iterate Abstraction", 0.35, "Main Controls"),
          boolParam("quantization", "Quantization", true, "Quantization"),
          floatParam("steps", "Steps", 2.0, 32.0, 6.0, "Quantization"),
          percentParam("softness", "Softness", 0.1, "Quantization"),
          boolParam("draw_edge", "Draw Edge", true, "Draw Edge"),
          percentParam("edge_strength", "Edge Strength", 0.55, "Draw Edge"),
          percentParam("edge_detection_threshold", "Edge Detection Threshold", 0.22, "Draw Edge"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("blanking_fill"),
        { enumParam("zoom_mode", "Zoom Mode", { "Auto", "Manual", "Warp Top Layer" }, 0, "Main"),
          percentParam("expand", "Expand", 0.5, "Manual"),
          percentParam("aspect", "Aspect", 0.5, "Manual"),
          percentParam("blend_edges", "Blend Edges", 0.5, "Fill Appearance"),
          percentParam("blur_background", "Blur Background", 0.3, "Fill Appearance"),
          percentParam("fade_amount", "Fade Amount", 0.0, "Fill Appearance"),
          colorParam("fade_color", "Fade Color", 0x000000, "Fill Appearance"),
          percentParam("shadow_strength", "Shadow Strength", 0.5, "Drop Shadow"),
          angleParam("drop_angle", "Drop Angle", 135.0, "Drop Shadow"),
          percentParam("drop_distance", "Drop Distance", 0.05, "Drop Shadow"),
          percentParam("drop_blur", "Blur", 0.3, "Drop Shadow"),
          colorParam("drop_color", "Color", 0x000000, "Drop Shadow"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("drop_shadow"),
        { percentParam("shadow_strength", "Shadow Strength", 0.5, "Controls"),
          angleParam("drop_angle", "Drop Angle", 135.0, "Controls"),
          percentParam("drop_distance", "Drop Distance", 0.05, "Controls"),
          percentParam("blur", "Blur", 0.3, "Controls"),
          colorParam("color", "Color", 0x000000, "Controls"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("edge_detect"),
        { enumParam("mode", "Mode", { "RGB", "Grayscale" }, 0, "Main"),
          colorParam("edge_color", "Edge Color", 0xFFFFFF, "Main"),
          percentParam("edge_thickness", "Edge Thickness", 0.5, "Main"),
          percentParam("threshold", "Threshold", 0.2, "Main"),
          percentParam("glow", "Glow", 0.0, "Main"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("emboss"),
        { enumParam("emboss_style", "Emboss Style",
                    { "Relief", "Emboss Over", "Sobel", "Laplacian" }, 0, "Controls"),
          percentParam("power", "Power", 0.5, "Controls"),
          angleParam("angle", "Angle", 45.0, "Controls"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("mirrors"), mirrorsParams());

    insertSpec(QStringLiteral("pencil_sketch"),
        { boolParam("color_sketch", "Color Sketch", false, "Main"),
          percentParam("stroke_thickness", "Stroke Thickness", 0.5, "Sketch Stroke"),
          percentParam("stroke_threshold", "Stroke Threshold", 0.5, "Sketch Stroke"),
          percentParam("stroke_length", "Stroke Length", 0.5, "Sketch Stroke"),
          floatParam("tone_levels", "Tone Levels", 2.0, 16.0, 6.0, "Sketch Tone"),
          percentParam("tone_shadows", "Shadows", 0.5, "Sketch Tone"),
          percentParam("tone_midtones", "Midtones", 0.5, "Sketch Tone"),
          percentParam("tone_highlights", "Highlights", 0.5, "Sketch Tone"),
          percentParam("texture_amount", "Texture Amount", 0.3, "Sketch Texture"),
          percentParam("texture_scale", "Texture Scale", 0.5, "Sketch Texture"),
          boolParam("auto_animate", "Auto Animate", false, "Sketch Texture"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("prism_blur"),
        { percentParam("blur_strength", "Blur Strength", 0.5, "Controls"),
          percentParam("aberration_distance", "Aberration Distance", 0.25, "Controls"),
          percentParam("vignette_size", "Vignette Size", 0.5, "Controls"),
          percentParam("vignette_sharpness", "Vignette Sharpness", 0.5, "Controls"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("scanlines"),
        { floatParam("line_frequency", "Line Frequency", 1.0, 100.0, 10.0, "Controls"),
          percentParam("line_sharpness", "Line Sharpness", 0.5, "Controls"),
          angleParam("line_angle", "Line Angle", 0.0, "Controls"),
          percentParam("line_width", "Line Width", 0.5, "Controls"),
          percentParam("line_shift", "Line Shift", 0.0, "Controls"),
          colorParam("color1", "Color 1", 0x000000, "Controls"),
          colorParam("color2", "Color 2", 0x000000, "Controls"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("stylize"),
        { enumParam("style", "Styles",
                    { "Antimonocromatismo", "Asheville", "Brush Stroke", "Candy", "Chinese Brush",
                      "Kandinsky Composition", "Dance", "Edtaonisl", "Feather", "Illustrated Portrait",
                      "La Muse", "Mondrian", "Stained Glass", "Cafe Terrace", "Impressionist Reservoir",
                      "Scream", "Protocubist Portrait", "Udnie", "Great Wave", "Fauvist Portrait" },
                    0, "Controls"),
          percentParam("style_scale", "Style Scale", 0.5, "Controls"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("tilt_shift"),
        { enumParam("blur_type", "Blur Type", { "Fast Blur", "Lens Blur" }, 1, "Main"),
          percentParam("blur_strength", "Blur Strength", 0.5, "Main"),
          enumParam("iris_shape", "Iris Shape",
                    { "Circle", "Triangle", "Square", "Pentagon", "Hexagon", "Heptagon", "Octagon" },
                    0, "Depth of Field"),
          floatParam("iris_blades", "Iris Blades", 3.0, 12.0, 6.0, "Depth of Field"),
          percentParam("focus_center", "Focus Center", 0.5, "Depth of Field"),
          percentParam("focus_width", "Focus Width", 0.3, "Depth of Field"),
          angleParam("focus_angle", "Focus Angle", 0.0, "Depth of Field"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("vignette"),
        { percentParam("size", "Size", 0.5, "Controls"),
          percentParam("softness", "Softness", 0.5, "Controls"),
          percentParam("strength", "Strength", 0.5, "Controls"),
          colorParam("color", "Color", 0x000000, "Controls"),
          percentParam("roundness", "Roundness", 0.5, "Controls"),
          percentParam("center_x", "Center X", 0.5, "Controls"),
          percentParam("center_y", "Center Y", 0.5, "Controls"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("watercolor"),
        { percentParam("detail", "Detail", 0.5, "Controls"),
          percentParam("brush_size", "Brush Size", 0.5, "Controls"),
          percentParam("edge_darkening", "Edge Darkening", 0.3, "Controls"),
          percentParam("paper_texture", "Paper Texture", 0.2, "Controls"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    // Resolve FX Temporal (Resolve 21 handbook)
    insertSpec(QStringLiteral("motion_trails"),
        { intParam("trail_length", "Trail Length", 1.0, 16.0, 6.0, "General"),
          percentParam("dropoff", "Dropoff", 0.5, "General"),
          enumParam("composite_gamma", "Composite Gamma",
                    { "Timeline", "Rec.709", "Linear", "Custom" }, 0, "Advanced"),
          percentParam("composite_gamma_custom", "Composite Gamma", 0.6, "Advanced"),
          percentParam("pan", "Pan", 0.02, "Move Trail"),
          angleParam("pan_angle", "Pan Angle", 0.0, "Move Trail"),
          percentParam("zoom", "Zoom", 0.0, "Move Trail"),
          angleParam("rotate", "Rotate", 0.0, "Move Trail"),
          boolParam("reuse_current_frame", "Reuse Current Frame", false, "Move Trail"),
          enumParam("border_type", "Border Type",
                    { "Black", "Soften", "Replicate", "Reflect", "Wrap-Around" }, 0, "Advanced"),
          enumParam("input_alpha", "Input Alpha", { "Ignore", "Use in Compositing" }, 0,
                    "Advanced"),
          boolParam("use_alpha", "Use Alpha", true, "Advanced"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("smear"),
        { intParam("frames_either_side", "Frames Either Side", 0.0, 8.0, 2.0, "General"),
          percentParam("luma_threshold", "Luma Threshold", 0.5, "General"),
          percentParam("chroma_threshold", "Chroma Threshold", 0.5, "General"),
          enumParam("input_alpha", "Input Alpha", { "Ignore", "Use in Compositing" }, 0,
                    "Advanced"),
          boolParam("use_alpha", "Use Alpha", true, "Advanced"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("stop_motion"),
        { intParam("frame_hold", "Frame Hold", 1.0, 24.0, 2.0, "General"),
          enumParam("input_alpha", "Input Alpha", { "Ignore", "Use in Compositing" }, 0,
                    "Advanced"),
          boolParam("use_alpha", "Use Alpha", true, "Advanced"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    insertSpec(QStringLiteral("motion_blur"),
        { enumParam("motion_est_type", "Motion Est. Type", { "Better", "Faster" }, 0, "General"),
          percentParam("motion_range", "Motion Range", 0.5, "General"),
          percentParam("motion_blur", "Motion Blur", 0.5, "General"),
          enumParam("blur_direction", "Blur Direction",
                    { "Both Directions", "From Previous Frame", "Towards Next Frame" }, 0,
                    "General"),
          percentParam("granularity", "Granularity", 0.5, "General"),
          percentParam("blend", "Blend", 1.0, "Global Blend") });

    // Resolve FX Texture (Resolve 21 handbook)
    insertSpec(QStringLiteral("jpeg_damage"), jpegDamageParams());
    insertSpec(QStringLiteral("texture_pop"), texturePopParams());
    insertSpec(QStringLiteral("film_damage"), filmDamageParams());
    insertSpec(QStringLiteral("analog_damage"), analogDamageParams());

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
        || isLightTypeId(typeId) || isStylizeTypeId(typeId) || isTemporalTypeId(typeId)
        || isTextureTypeId(typeId);
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

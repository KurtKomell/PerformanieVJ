#include "FilterParamSchema.h"

#include "FilterCatalog.h"

#include <QtGlobal>
#include <utility>

namespace pvj::core {
namespace {

FilterParamSpec floatParam(
    const char* name, const char* label, double minV, double maxV, double defaultV)
{
    FilterParamSpec p;
    p.name = QString::fromLatin1(name);
    p.label = QString::fromLatin1(label);
    p.kind = FilterParamKind::Float;
    p.minV = minV;
    p.maxV = maxV;
    p.defaultV = defaultV;
    return p;
}

FilterParamSpec angleParam(const char* name, const char* label, double defaultV = 0.0)
{
    FilterParamSpec p = floatParam(name, label, -180.0, 180.0, defaultV);
    p.kind = FilterParamKind::Angle;
    return p;
}

FilterParamSpec percentParam(const char* name, const char* label, double defaultV = 0.5)
{
    FilterParamSpec p = floatParam(name, label, 0.0, 1.0, defaultV);
    p.kind = FilterParamKind::Percent;
    return p;
}

FilterParamSpec colorParam(const char* name, const char* label, double defaultV = 1.0)
{
    FilterParamSpec p = floatParam(name, label, 0.0, 1.0, defaultV);
    p.kind = FilterParamKind::Color;
    return p;
}

FilterParamSpec boolParam(const char* name, const char* label, bool defaultV = false)
{
    FilterParamSpec p = floatParam(name, label, 0.0, 1.0, defaultV ? 1.0 : 0.0);
    p.kind = FilterParamKind::Bool;
    return p;
}

FilterParamSpec enumParam(
    const char* name, const char* label, std::initializer_list<const char*> labels, int defaultIndex = 0)
{
    FilterParamSpec p = floatParam(name, label, 0.0, static_cast<double>(labels.size() - 1), defaultIndex);
    p.kind = FilterParamKind::EnumIndex;
    for (const char* item : labels) {
        p.enumLabels.push_back(QString::fromLatin1(item));
    }
    return p;
}

QHash<QString, FilterNodeSpec> buildSchemas()
{
    QHash<QString, FilterNodeSpec> schema;
    const auto& catalog = filterCatalogEntries();
    schema.reserve(catalog.size() + 32);

    auto setParams = [&](const char* typeId, QVector<FilterParamSpec> params) {
        FilterNodeSpec spec;
        spec.typeId = QString::fromLatin1(typeId);
        spec.params = std::move(params);
        if (spec.params.size() > 4) {
            spec.params.resize(4);
        }
        schema.insert(spec.typeId, spec);
    };

    auto setParamsById = [&](const QString& typeId, QVector<FilterParamSpec> params) {
        FilterNodeSpec spec;
        spec.typeId = typeId;
        spec.params = std::move(params);
        if (spec.params.size() > 4) {
            spec.params.resize(4);
        }
        schema.insert(spec.typeId, spec);
    };

    // Baseline schema for every catalog typeId (guarantees full coverage).
    // Resolume docs describe video-effect "Opacity" as universal mix control.
    // Keep key "amount" for compatibility with existing mappings/tests.
    for (const FilterCatalogEntry& entry : catalog) {
        setParamsById(entry.typeId, { percentParam("amount", "Opacity", 1.0) });
    }

    // Blur family
    for (const char* id : { "blur", "fast_blur", "gaussian_blur", "box_blur", "temporal_blur" }) {
        setParams(id,
            { percentParam("amount", "Amount", 0.75),
                floatParam("radius", "Radius", 0.0, 128.0, 12.0) });
    }
    for (const char* id : { "radial_blur", "directional_blur", "motion_blur", "zoom_blur" }) {
        setParams(id,
            { percentParam("amount", "Amount", 0.75),
                floatParam("radius", "Radius", 0.0, 128.0, 10.0),
                angleParam("angle", "Angle", 0.0) });
    }
    setParams("sharpen", { percentParam("amount", "Amount", 0.4) });
    setParams("denoise", { percentParam("amount", "Amount", 0.3) });

    // Color family
    for (const char* id :
        { "color", "color_correction", "hsl", "curves", "levels", "selective_color", "channel_mixer", "color_lookup" }) {
        setParams(id,
            { percentParam("amount", "Amount", 1.0),
                floatParam("brightness", "Brightness", -1.0, 1.0, 0.0),
                floatParam("contrast", "Contrast", 0.0, 2.0, 1.0),
                floatParam("saturation", "Saturation", 0.0, 2.0, 1.0) });
    }
    setParams("color_intensity", { floatParam("intensity", "Intensity", 0.0, 2.0, 1.0) });
    setParams("color_balance",
        { colorParam("red", "Red", 1.0), colorParam("green", "Green", 1.0), colorParam("blue", "Blue", 1.0) });
    setParams("brightness", { floatParam("value", "Brightness", -1.0, 1.0, 0.0) });
    setParams("contrast", { floatParam("value", "Contrast", 0.0, 2.0, 1.0) });
    setParams("gamma", { floatParam("value", "Gamma", 0.1, 4.0, 1.0) });
    setParams("exposure", { floatParam("value", "Exposure", -4.0, 4.0, 0.0) });
    setParams("saturation", { floatParam("value", "Saturation", 0.0, 2.0, 1.0) });
    setParams("hue", { angleParam("angle", "Hue", 0.0) });
    setParams("hue_rotate", { angleParam("angle", "Hue Rotate", 0.0) });
    setParams("invert", { boolParam("enabled", "Enabled", true) });
    setParams("solarize", { floatParam("threshold", "Threshold", 0.0, 1.0, 0.5) });
    setParams("tint", { angleParam("hue", "Hue", 0.0), floatParam("strength", "Strength", 0.0, 1.0, 0.5) });
    setParams("black_white", { floatParam("mix", "Mix", 0.0, 1.0, 1.0) });
    setParams("posterize", { floatParam("levels", "Levels", 2.0, 64.0, 8.0) });
    setParams("threshold", { floatParam("value", "Threshold", 0.0, 1.0, 0.5) });
    // Manual examples describe Colorize with hue/brightness style controls.
    setParams("colorize",
        { angleParam("hue", "Hue", 0.0),
          floatParam("brightness", "Brightness", -1.0, 1.0, 0.0),
          floatParam("saturation", "Saturation", 0.0, 1.0, 1.0) });
    setParams("chromatic_aberration",
        { floatParam("distance", "Distance", 0.0, 100.0, 2.0), angleParam("angle", "Angle", 0.0) });

    // Distort / transform family
    setParams("transform",
        { floatParam("scale", "Scale", 0.0, 4.0, 1.0), angleParam("rotation", "Rotation", 0.0),
            floatParam("offset_x", "Offset X", -1.0, 1.0, 0.0), floatParam("offset_y", "Offset Y", -1.0, 1.0, 0.0) });
    setParams("scale", { floatParam("value", "Scale", 0.0, 4.0, 1.0) });
    setParams("rotate", { angleParam("angle", "Angle", 0.0) });
    setParams("flip", { enumParam("mode", "Mode", { "Horizontal", "Vertical", "Both" }, 0) });
    setParams("flip_horizontal", { boolParam("enabled", "Enabled", true) });
    setParams("flip_vertical", { boolParam("enabled", "Enabled", true) });
    // Resolume docs/tutorials refer to this control as "Divisions" (Bendoscope/Kaleido style).
    for (const char* id : { "mirror", "mirror_quad", "mirror_stripes", "multi_mirror", "kaleido", "kaleidoscope" }) {
        setParams(id,
            { floatParam("divisions", "Divisions", 1.0, 32.0, 4.0), angleParam("angle", "Angle", 0.0),
                percentParam("mix", "Mix", 1.0) });
    }
    for (const char* id : { "tile", "motion_tile" }) {
        setParams(id,
            { floatParam("repeat_x", "Repeat X", 1.0, 32.0, 2.0), floatParam("repeat_y", "Repeat Y", 1.0, 32.0, 2.0),
                percentParam("mirror", "Mirror", 0.0) });
    }
    for (const char* id : { "ripple", "wave", "twirl", "bulge", "fisheye", "bend", "warp", "spherize", "cylinder", "cube", "liquify", "mesh_warp" }) {
        setParams(id,
            { percentParam("amount", "Amount", 0.5), floatParam("frequency", "Frequency", 0.0, 32.0, 4.0),
                floatParam("speed", "Speed", -10.0, 10.0, 0.0) });
    }
    // Resolume Distortion naming in older manuals: Distort + Radius.
    setParams("distortion",
        { percentParam("distort", "Distort", 0.5),
          floatParam("radius", "Radius", 0.0, 1.0, 0.5) });
    setParams("zoom", { floatParam("amount", "Amount", 0.0, 4.0, 1.0) });
    setParams("polar", { percentParam("amount", "Amount", 1.0) });
    setParams("polarizer", { angleParam("angle", "Angle", 0.0), percentParam("amount", "Amount", 1.0) });
    // Resolume-style naming: horizontal/vertical displacement factors.
    setParams("displacement",
        { floatParam("horizontal", "Horizontal", -1.0, 1.0, 0.0),
          floatParam("vertical", "Vertical", -1.0, 1.0, 0.0),
          percentParam("amount", "Amount", 0.5) });
    setParams("pixelate", { floatParam("size", "Size", 1.0, 256.0, 8.0) });
    setParams("smooth_transform", { percentParam("smoothness", "Smoothness", 0.5) });
    setParams("screen_shake",
        { percentParam("amount", "Amount", 0.3), floatParam("frequency", "Frequency", 0.1, 30.0, 8.0) });
    setParams("space_warper",
        { percentParam("amount", "Amount", 0.6), floatParam("frequency", "Frequency", 0.0, 16.0, 2.0) });
    setParams("shifty", { percentParam("amount", "Amount", 0.5), angleParam("direction", "Direction", 0.0) });

    // Generate / blend
    for (const char* id : { "glow", "bloom", "god_rays" }) {
        setParams(id,
            { percentParam("amount", "Amount", 0.6), floatParam("threshold", "Threshold", 0.0, 1.0, 0.5),
                floatParam("radius", "Radius", 0.0, 128.0, 16.0) });
    }
    setParams("strobe", { floatParam("rate", "Rate", 0.0, 20.0, 8.0), percentParam("duty", "Duty", 0.5) });
    setParams("trails", { percentParam("strength", "Strength", 0.75), percentParam("decay", "Decay", 0.2) });
    setParams("light_leak", { percentParam("amount", "Amount", 0.6), angleParam("angle", "Angle", 0.0) });
    for (const char* id : { "noise", "rgb_noise", "video_noise", "film_grain" }) {
        setParams(id, { percentParam("amount", "Amount", 0.2), floatParam("speed", "Speed", 0.0, 10.0, 1.0) });
    }
    setParams("halftone", { floatParam("size", "Size", 1.0, 128.0, 8.0), angleParam("angle", "Angle", 0.0) });
    setParams("vignette", { percentParam("amount", "Amount", 0.5), percentParam("softness", "Softness", 0.5) });
    setParams("spotlight",
        { percentParam("amount", "Amount", 0.6), floatParam("size", "Size", 0.0, 2.0, 0.5),
            floatParam("falloff", "Falloff", 0.0, 1.0, 0.5) });
    setParams("drop_shadow",
        { percentParam("opacity", "Opacity", 0.5), floatParam("distance", "Distance", 0.0, 128.0, 8.0),
            angleParam("angle", "Angle", 45.0) });
    setParams("rainbow", { percentParam("amount", "Amount", 1.0), floatParam("speed", "Speed", -4.0, 4.0, 0.5) });
    setParams("prismatic", { percentParam("amount", "Amount", 0.5), floatParam("samples", "Samples", 1.0, 16.0, 4.0) });
    setParams("replicate", { floatParam("count", "Count", 1.0, 32.0, 4.0), percentParam("spread", "Spread", 0.25) });
    setParams("echo", { percentParam("amount", "Amount", 0.5), floatParam("delay", "Delay", 0.0, 2.0, 0.2) });
    setParams("slit_scanner", { floatParam("speed", "Speed", -4.0, 4.0, 0.5), enumParam("axis", "Axis", { "Horizontal", "Vertical" }, 0) });
    // Stylize / film
    for (const char* id : { "edges", "emboss", "find_edges", "glow_edges", "cartoon", "watercolor", "oil_paint" }) {
        setParams(id, { percentParam("amount", "Amount", 0.6), floatParam("detail", "Detail", 0.0, 4.0, 1.0) });
    }
    for (const char* id : { "crt", "vhs", "vhsifyer", "scanlines", "broadcast", "reducto", "night_vision", "thermal", "x_ray", "duotone", "tritone", "gradient_map", "stroke", "erode", "dilate", "total_visual_annihilation" }) {
        setParams(id, { percentParam("amount", "Amount", 0.7) });
    }

    // Key and masks (GPU: effect_key.frag; cell keyChannelR/G/B weights in UBO rotation.xyz)
    setParams("chroma_key",
        { enumParam("mode", "Key mode",
                { "Chroma (hue)", "Chroma (hue) inverted", "RGB distance", "RGB distance inverted",
                  "RGB max delta", "RGB max delta inverted" },
                0),
            colorParam("hue", "Key hue", 0.33), percentParam("threshold", "Threshold", 0.2),
            percentParam("softness", "Softness", 0.2) });
    setParams("luma_key",
        { enumParam("mode", "Key source",
                { "Weighted RGB (inspector)", "Weighted RGB inverted", "Luma BT.709", "Luma BT.709 inverted",
                  "Red channel", "Green channel", "Blue channel", "Max RGB", "Min RGB", "Max RGB inverted" },
                0),
            percentParam("brightness", "Level / center", 0.5), percentParam("threshold", "Tolerance", 0.25),
            percentParam("softness", "Feather", 0.12) });
    setParams("linear_mask", { angleParam("angle", "Angle", 0.0), percentParam("softness", "Softness", 0.2) });
    setParams("mask",
        { enumParam("mode", "Mask mode", { "None", "Rectangle", "Circle", "Soft edge", "Ellipse", "Custom" }, 0),
          percentParam("sizeX", "Size X", 1.0),
          percentParam("sizeY", "Size Y", 1.0),
          percentParam("feather", "Feather", 0.1) });
    setParams("crop", { percentParam("left", "Left", 0.0), percentParam("top", "Top", 0.0), percentParam("right", "Right", 1.0), percentParam("bottom", "Bottom", 1.0) });
    setParams("crop_rectangle", { percentParam("x", "X", 0.0), percentParam("y", "Y", 0.0), percentParam("width", "Width", 1.0), percentParam("height", "Height", 1.0) });

    // Mix / utility / patterns / blend modes
    setParams("add_subtract", { percentParam("mix", "Mix", 0.5), enumParam("mode", "Mode", { "Add", "Subtract" }, 0) });
    for (const char* id : { "mix", "fade", "opacity" }) {
        setParams(id, { percentParam("amount", "Amount", 1.0) });
    }
    setParams("rgb_shift", { floatParam("distance", "Distance", 0.0, 100.0, 2.0), angleParam("angle", "Angle", 0.0) });
    setParams("shift", { floatParam("x", "X", -1.0, 1.0, 0.0), floatParam("y", "Y", -1.0, 1.0, 0.0) });
    setParams("tilt_shift", { percentParam("amount", "Amount", 0.5), angleParam("angle", "Angle", 0.0) });

    for (const char* id : { "radar", "polka_dot", "stripes", "checkerboard", "dots" }) {
        setParams(id,
            { percentParam("amount", "Amount", 1.0), floatParam("scale", "Scale", 0.01, 10.0, 1.0),
                floatParam("speed", "Speed", -10.0, 10.0, 0.0) });
    }

    for (const char* id :
        { "blend_normal", "blend_add", "blend_subtract", "blend_multiply", "blend_screen", "blend_overlay",
            "blend_soft_light", "blend_hard_light", "blend_color_dodge", "blend_color_burn", "blend_darken",
            "blend_lighten", "blend_difference", "blend_exclusion", "blend_mode" }) {
        setParams(id,
            { enumParam("mode", "Blend Mode",
                        { "Normal", "Add", "Subtract", "Multiply", "Screen", "Overlay",
                          "Soft Light", "Hard Light", "Difference", "Exclusion" }, 0),
              percentParam("mix", "Opacity", 1.0) });
    }
    setParams("transform_3d",
        { angleParam("rotate_x", "Rotate X", 0.0), angleParam("rotate_y", "Rotate Y", 0.0), angleParam("rotate_z", "Rotate Z", 0.0), floatParam("depth", "Depth", 0.0, 2.0, 0.5) });

    // Validate against catalog in debug builds.
#ifndef NDEBUG
    for (const FilterCatalogEntry& entry : catalog) {
        Q_ASSERT(schema.contains(entry.typeId));
        Q_ASSERT(schema.value(entry.typeId).params.size() <= 4);
    }
#endif

    return schema;
}

} // namespace

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

#include "FilterCatalog.h"

#include <QHash>

namespace pvj::core {
namespace {

// Categories mirror Resolume’s Effects panel groupings (English labels; translated in UI).
QVector<FilterCatalogEntry> buildCatalog()
{
    QVector<FilterCatalogEntry> e;

    auto add = [&](const char* cat, const char* id, const char* name) {
        e.append({ QString::fromLatin1(id), QString::fromLatin1(cat), QString::fromLatin1(name) });
    };

    // --- Blur & sharpen (built-in shader: blur) ---
    add("Blur & sharpen", "blur", "Blur");
    add("Blur & sharpen", "fast_blur", "Fast Blur");
    add("Blur & sharpen", "gaussian_blur", "Gaussian Blur");
    add("Blur & sharpen", "box_blur", "Box Blur");
    add("Blur & sharpen", "radial_blur", "Radial Blur");
    add("Blur & sharpen", "directional_blur", "Directional Blur");
    add("Blur & sharpen", "motion_blur", "Motion Blur");
    add("Blur & sharpen", "zoom_blur", "Zoom Blur");
    add("Blur & sharpen", "temporal_blur", "Temporal Blur");
    add("Blur & sharpen", "sharpen", "Sharpen");
    add("Blur & sharpen", "denoise", "Denoise");

    // --- Color & levels (built-in shader: color) ---
    add("Color & levels", "color", "Color");
    add("Color & levels", "color_correction", "Color Correction");
    add("Color & levels", "color_intensity", "Color Intensity");
    add("Color & levels", "color_balance", "Color Balance");
    add("Color & levels", "brightness", "Brightness");
    add("Color & levels", "contrast", "Contrast");
    add("Color & levels", "gamma", "Gamma");
    add("Color & levels", "exposure", "Exposure");
    add("Color & levels", "saturation", "Saturation");
    add("Color & levels", "hue", "Hue");
    add("Color & levels", "hsl", "HSL Adjust");
    add("Color & levels", "curves", "Curves");
    add("Color & levels", "levels", "Levels");
    add("Color & levels", "invert", "Invert");
    add("Color & levels", "solarize", "Solarize");
    add("Color & levels", "tint", "Tint");
    add("Color & levels", "black_white", "Black & White");
    add("Color & levels", "posterize", "Posterize");
    add("Color & levels", "threshold", "Threshold");
    add("Color & levels", "colorize", "Colorize");
    add("Color & levels", "chromatic_aberration", "Chromatic Aberration");

    // --- Distort & transform (kaleido / mirror family) ---
    add("Distort & transform", "transform", "Transform");
    add("Distort & transform", "scale", "Scale");
    add("Distort & transform", "rotate", "Rotate");
    add("Distort & transform", "flip", "Flip");
    add("Distort & transform", "flip_horizontal", "Flip Horizontal");
    add("Distort & transform", "flip_vertical", "Flip Vertical");
    add("Distort & transform", "mirror", "Mirror");
    add("Distort & transform", "mirror_quad", "Mirror Quad");
    add("Distort & transform", "mirror_stripes", "Mirror Stripes");
    add("Distort & transform", "multi_mirror", "Multi Mirror");
    add("Distort & transform", "tile", "Tile");
    add("Distort & transform", "kaleido", "Kaleido");
    add("Distort & transform", "kaleidoscope", "Kaleidoscope");
    add("Distort & transform", "ripple", "Ripple");
    add("Distort & transform", "wave", "Wave");
    add("Distort & transform", "twirl", "Twirl");
    add("Distort & transform", "bulge", "Bulge");
    add("Distort & transform", "fisheye", "Fisheye");
    add("Distort & transform", "distortion", "Distortion");
    add("Distort & transform", "bend", "Bend");
    add("Distort & transform", "warp", "Warp");
    add("Distort & transform", "zoom", "Zoom");
    add("Distort & transform", "polar", "Polar");
    add("Distort & transform", "polarizer", "Polarizer");
    add("Distort & transform", "displacement", "Displacement");
    add("Distort & transform", "pixelate", "Pixelate");
    add("Distort & transform", "smooth_transform", "Smooth Transform");
    add("Distort & transform", "screen_shake", "Screen Shake");
    add("Distort & transform", "space_warper", "Space Warper");
    add("Distort & transform", "shifty", "Shifty");

    // --- Generate & blend ---
    add("Generate & blend", "glow", "Glow");
    add("Generate & blend", "bloom", "Bloom");
    add("Generate & blend", "god_rays", "God Rays");
    add("Generate & blend", "strobe", "Strobe");
    add("Generate & blend", "trails", "Trails");
    add("Generate & blend", "light_leak", "Light Leak");
    add("Generate & blend", "noise", "Noise");
    add("Generate & blend", "rgb_noise", "RGB Noise");
    add("Generate & blend", "video_noise", "Video Noise");
    add("Generate & blend", "halftone", "Halftone");
    add("Generate & blend", "vignette", "Vignette");
    add("Generate & blend", "spotlight", "Spotlight");
    add("Generate & blend", "drop_shadow", "Drop Shadow");
    add("Generate & blend", "rainbow", "Rainbow");
    add("Generate & blend", "prismatic", "Prismatic");
    add("Generate & blend", "replicate", "Replicate");
    add("Generate & blend", "echo", "Echo");
    add("Generate & blend", "slit_scanner", "Slit Scanner");
    add("Generate & blend", "feedback", "Feedback");
    add("Generate & blend", "feedback_rotated", "Feedback Rotated");
    add("Generate & blend", "feedback_pro", "Feedback Pro");
    add("Generate & blend", "warped_feedback", "Warped Feedback");

    // --- Stylize & film ---
    add("Stylize & film", "edges", "Edges");
    add("Stylize & film", "emboss", "Emboss");
    add("Stylize & film", "find_edges", "Find Edges");
    add("Stylize & film", "glow_edges", "Glow Edges");
    add("Stylize & film", "crt", "CRT");
    add("Stylize & film", "vhs", "VHS");
    add("Stylize & film", "vhsifyer", "VHSifyer");
    add("Stylize & film", "film_grain", "Film Grain");
    add("Stylize & film", "scanlines", "Scanlines");
    add("Stylize & film", "broadcast", "Broadcast");
    add("Stylize & film", "reducto", "Reducto");
    add("Stylize & film", "total_visual_annihilation", "Total Visual Annihilation");

    // --- Key & mask ---
    add("Key & mask", "chroma_key", "Chroma Key");
    add("Key & mask", "luma_key", "Luma Key");
    add("Key & mask", "linear_mask", "Linear Mask");
    add("Key & mask", "mask", "Mask");
    add("Key & mask", "crop", "Crop");
    add("Key & mask", "crop_rectangle", "Crop Rectangle");

    // --- Mix & utility (composition-style; same panel in Resolume) ---
    add("Mix & utility", "add_subtract", "Add Subtract");
    add("Mix & utility", "mix", "Mix");
    add("Mix & utility", "fade", "Fade");
    add("Mix & utility", "rgb_shift", "RGB Shift");
    add("Mix & utility", "shift", "Shift");
    add("Mix & utility", "tilt_shift", "Tilt Shift");

    // --- Radar / pattern (named in Resolume docs examples) ---
    add("Patterns", "radar", "Radar");
    add("Patterns", "polka_dot", "PolkaDot");
    add("Patterns", "stripes", "Stripes");
    add("Patterns", "checkerboard", "Checkerboard");
    add("Patterns", "dots", "Dots");

    // --- Blend modes (often exposed as clip / layer effects in Resolume) ---
    add("Blend modes", "blend_normal", "Normal");
    add("Blend modes", "blend_add", "Add");
    add("Blend modes", "blend_subtract", "Subtract");
    add("Blend modes", "blend_multiply", "Multiply Blend");
    add("Blend modes", "blend_screen", "Screen");
    add("Blend modes", "blend_overlay", "Overlay");
    add("Blend modes", "blend_soft_light", "Soft Light");
    add("Blend modes", "blend_hard_light", "Hard Light");
    add("Blend modes", "blend_color_dodge", "Color Dodge");
    add("Blend modes", "blend_color_burn", "Color Burn");
    add("Blend modes", "blend_darken", "Darken");
    add("Blend modes", "blend_lighten", "Lighten");
    add("Blend modes", "blend_difference", "Difference");
    add("Blend modes", "blend_exclusion", "Exclusion");

    // --- Stylize (extra) ---
    add("Stylize & film", "cartoon", "Cartoon");
    add("Stylize & film", "watercolor", "Watercolor");
    add("Stylize & film", "oil_paint", "Oil Paint");
    add("Stylize & film", "night_vision", "Night Vision");
    add("Stylize & film", "thermal", "Thermal");
    add("Stylize & film", "x_ray", "X-Ray");
    add("Stylize & film", "duotone", "Duotone");
    add("Stylize & film", "tritone", "Tritone");
    add("Stylize & film", "gradient_map", "Gradient Map");
    add("Stylize & film", "stroke", "Stroke");
    add("Stylize & film", "erode", "Erode");
    add("Stylize & film", "dilate", "Dilate");

    // --- Color (extra) ---
    add("Color & levels", "hue_rotate", "Hue Rotate");
    add("Color & levels", "selective_color", "Selective Color");
    add("Color & levels", "channel_mixer", "Channel Mixer");
    add("Color & levels", "color_lookup", "Color Lookup");

    // --- Distort (extra) ---
    add("Distort & transform", "mesh_warp", "Mesh Warp");
    add("Distort & transform", "liquify", "Liquify");
    add("Distort & transform", "motion_tile", "Motion Tile");
    add("Distort & transform", "spherize", "Spherize");
    add("Distort & transform", "cylinder", "Cylinder");
    add("Distort & transform", "cube", "Cube");

    // --- Utility / meta ---
    add("Utility", "opacity", "Opacity");
    add("Utility", "blend_mode", "Blend Mode");
    add("Utility", "transform_3d", "3D Transform");

    return e;
}

} // namespace

const QVector<FilterCatalogEntry>& filterCatalogEntries()
{
    static const QVector<FilterCatalogEntry> k = buildCatalog();
    return k;
}

QString filterCatalogEnglishName(const QString& typeId)
{
    static const QHash<QString, QString> kMap = [] {
        QHash<QString, QString> m;
        m.reserve(256);
        for (const auto& e : filterCatalogEntries()) {
            m.insert(e.typeId, e.englishName);
        }
        return m;
    }();
    return kMap.value(typeId);
}

} // namespace pvj::core

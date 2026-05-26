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

    // --- NVIDIA Maxine (Video Effects SDK) ---
    add("NVIDIA", "nvidia_artifact_reduction", "Encoder Artifact Reduction");
    add("NVIDIA", "nvidia_super_resolution", "Super Resolution");
    add("NVIDIA", "nvidia_upscale", "Upscale");
    add("NVIDIA", "noise_reduction", "Video Denoise");

    // --- DaVinci Resolve Effects Library (missing filters only) ---
    // Resolve FX Blur
    add("Resolve FX Blur", "mosaic_blur", "Mosaic Blur");
    add("Resolve FX Blur", "lens_blur", "Lens Blur");
    // Resolve FX Sharpen
    add("Resolve FX Sharpen", "sharpen_edges", "Sharpen Edges");
    add("Resolve FX Sharpen", "soften_sharpen", "Soften & Sharpen");
    // Resolve FX Color
    add("Resolve FX Color", "aces_transform", "ACES Transform");
    add("Resolve FX Color", "chromatic_adaptation", "Chromatic Adaptation");
    add("Resolve FX Color", "color_compressor", "Color Compressor");
    add("Resolve FX Color", "color_space_transform", "Color Space Transform");
    add("Resolve FX Color", "color_stabilizer", "Color Stabilizer");
    add("Resolve FX Color", "contrast_pop", "Contrast Pop");
    add("Resolve FX Color", "dehaze", "Dehaze");
    add("Resolve FX Color", "false_color", "False Color");
    add("Resolve FX Color", "flicker_addition", "Flicker Addition");
    add("Resolve FX Color", "gamut_limiter", "Gamut Limiter");
    add("Resolve FX Color", "gamut_mapping", "Gamut Mapping");
    add("Resolve FX Color", "dctl", "DCTL");
    // Resolve FX Generate
    add("Resolve FX Generate", "color_generator", "Color Generator");
    add("Resolve FX Generate", "color_palette", "Color Palette");
    add("Resolve FX Generate", "grid", "Grid");
    // Resolve FX Key
    add("Resolve FX Key", "key_3d", "3D Keyer");
    add("Resolve FX Key", "hsl_keyer", "HSL Keyer");
    add("Resolve FX Key", "alpha_matte_shrink_glow", "Alpha Matte Shrink and Glow");
    // Resolve FX Light
    add("Resolve FX Light", "aperture_diffraction", "Aperture Diffraction");
    add("Resolve FX Light", "halation", "Halation");
    add("Resolve FX Light", "lens_flare", "Lens Flare");
    add("Resolve FX Light", "lens_reflections", "Lens Reflections");
    add("Resolve FX Light", "light_rays", "Light Rays");
    // Resolve FX Refine
    add("Resolve FX Refine", "beauty", "Beauty");
    // Resolve FX Revival
    add("Resolve FX Revival", "automatic_dirt_removal", "Automatic Dirt Removal");
    add("Resolve FX Revival", "chromatic_aberration_removal", "Chromatic Aberration Removal");
    add("Resolve FX Revival", "dead_pixel_fixer", "Dead Pixel Fixer");
    add("Resolve FX Revival", "deband", "Deband");
    add("Resolve FX Revival", "deflicker", "Deflicker");
    add("Resolve FX Revival", "frame_replacer", "Frame Replacer");
    add("Resolve FX Revival", "patch_replacer", "Patch Replacer");
    // Resolve FX Stylize
    add("Resolve FX Stylize", "abstraction", "Abstraction");
    add("Resolve FX Stylize", "blanking_fill", "Blanking Fill");
    add("Resolve FX Stylize", "pencil_sketch", "Pencil Sketch");
    add("Resolve FX Stylize", "prism_blur", "Prism Blur");
    add("Resolve FX Stylize", "stylize", "Stylize");
    // Resolve FX Temporal
    add("Resolve FX Temporal", "motion_trails", "Motion Trails");
    add("Resolve FX Temporal", "smear", "Smear");
    add("Resolve FX Temporal", "stop_motion", "Stop Motion");
    // Resolve FX Texture
    add("Resolve FX Texture", "analog_damage", "Analog Damage");
    add("Resolve FX Texture", "film_damage", "Film Damage");
    add("Resolve FX Texture", "jpeg_damage", "JPEG Damage");
    add("Resolve FX Texture", "texture_pop", "Texture Pop");
    // Resolve FX Transform
    add("Resolve FX Transform", "camera_shake", "Camera Shake");
    add("Resolve FX Transform", "video_collage", "Video Collage");
    // Resolve FX Warp
    add("Resolve FX Warp", "dent", "Dent");
    add("Resolve FX Warp", "lens_distortion", "Lens Distortion");
    add("Resolve FX Warp", "ripples", "Ripples");
    add("Resolve FX Warp", "vortex", "Vortex");
    add("Resolve FX Warp", "warper", "Warper");
    add("Resolve FX Warp", "waviness", "Waviness");
    // Fusion Effects
    add("Fusion Effects", "binoculars", "Binoculars");
    add("Fusion Effects", "cctv", "CCTV");
    add("Fusion Effects", "colored_border", "Colored Border");
    add("Fusion Effects", "digital_glitch", "Digital Glitch");
    add("Fusion Effects", "drone_overlay", "Drone Overlay");
    add("Fusion Effects", "dslr", "DSLR");
    add("Fusion Effects", "dve", "DVE");
    add("Fusion Effects", "video_call", "Video Call");
    add("Fusion Effects", "video_camera", "Video Camera");
    // Fusion Generators
    add("Fusion Generators", "background", "Background");
    add("Fusion Generators", "fast_noise", "Fast Noise");
    add("Fusion Generators", "plasma", "Plasma");
    add("Fusion Generators", "mandelbrot", "Mandelbrot");
    add("Fusion Generators", "day_sky", "Day Sky");

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

QString filterCatalogCategory(const QString& typeId)
{
    static const QHash<QString, QString> kMap = [] {
        QHash<QString, QString> m;
        m.reserve(256);
        for (const auto& e : filterCatalogEntries()) {
            m.insert(e.typeId, e.category);
        }
        return m;
    }();
    return kMap.value(typeId.toLower());
}

} // namespace pvj::core

#include "FilterCatalog.h"

#include <QHash>

namespace pvj::core {
namespace {

QVector<FilterCatalogEntry> buildCatalog()
{
    QVector<FilterCatalogEntry> e;
    e.reserve(54);

    auto add = [&](const char* cat, const char* id, const char* name) {
        e.append({ QString::fromLatin1(id), QString::fromLatin1(cat), QString::fromLatin1(name) });
    };

    // Phase 1: Resolve-style blur filters only (rebuilt from scratch).
    add("Blur & sharpen", "gaussian_blur", "Gaussian Blur");
    add("Blur & sharpen", "box_blur", "Box Blur");
    add("Blur & sharpen", "directional_blur", "Directional Blur");
    add("Blur & sharpen", "radial_blur", "Radial Blur");
    add("Blur & sharpen", "zoom_blur", "Zoom Blur");
    add("Blur & sharpen", "mosaic_blur", "Mosaic Blur");
    add("Blur & sharpen", "lens_blur", "Lens Blur");
    add("Blur & sharpen", "sharpen", "Sharpen");
    add("Blur & sharpen", "sharpen_edges", "Sharpen Edges");
    add("Blur & sharpen", "soften_sharpen", "Soften & Sharpen");

    // Resolve FX Color (Phase 2)
    add("Color & levels", "invert", "Invert Color");
    add("Color & levels", "chromatic_adaptation", "Chromatic Adaptation");
    add("Color & levels", "color_compressor", "Color Compressor");
    add("Color & levels", "color_stabilizer", "Color Stabilizer");
    add("Color & levels", "contrast_pop", "Contrast Pop");
    add("Color & levels", "dehaze", "Dehaze");

    // Resolve 21 Film Emulation (Film Look Creator)
    add("Film Emulation", "film_look", "Film Look");
    add("Film Emulation", "film_color", "Film Color");
    add("Film Emulation", "film_split_tone", "Film Split Tone");
    add("Film Emulation", "film_vignette", "Film Vignette");
    add("Film Emulation", "film_halation", "Film Halation");
    add("Film Emulation", "film_bloom", "Film Bloom");
    add("Film Emulation", "film_grain", "Film Grain");
    add("Film Emulation", "film_flicker", "Film Flicker");
    add("Film Emulation", "film_gate_weave", "Film Gate Weave");
    add("Film Emulation", "film_gate", "Film Gate");

    // ResolveFX Light (Resolve 21)
    add("Light FX", "aperture_diffraction", "Aperture Diffraction");
    add("Light FX", "glow", "Glow");
    add("Light FX", "halation", "Halation");
    add("Light FX", "lens_flare", "Lens Flare");
    add("Light FX", "lens_reflections", "Lens Reflections");
    add("Light FX", "light_rays", "Light Rays");

    // Resolve FX Stylize (Resolve 21)
    add("Stylize", "abstraction", "Abstraction");
    add("Stylize", "blanking_fill", "Blanking Fill");
    add("Stylize", "drop_shadow", "Drop Shadow");
    add("Stylize", "edge_detect", "Edge Detect");
    add("Stylize", "emboss", "Emboss");
    add("Stylize", "mirrors", "Mirrors");
    add("Stylize", "pencil_sketch", "Pencil Sketch");
    add("Stylize", "prism_blur", "Prism Blur");
    add("Stylize", "scanlines", "Scanlines");
    add("Stylize", "stylize", "Stylize");
    add("Stylize", "tilt_shift", "Tilt-Shift Blur");
    add("Stylize", "vignette", "Vignette");
    add("Stylize", "watercolor", "Watercolor");

    // Resolve FX Temporal (Resolve 21 handbook)
    add("Resolve FX Temporal", "motion_trails", "Motion Trails");
    add("Resolve FX Temporal", "smear", "Smear");
    add("Resolve FX Temporal", "stop_motion", "Stop Motion");
    add("Resolve FX Temporal", "motion_blur", "Motion Blur");

    // Resolve FX Texture (Resolve 21 handbook)
    add("Resolve FX Texture", "analog_damage", "Analog Damage");
    add("Resolve FX Texture", "film_damage", "Film Damage");
    add("Resolve FX Texture", "jpeg_damage", "JPEG Damage");
    add("Resolve FX Texture", "texture_pop", "Texture Pop");

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
        m.reserve(32);
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
        m.reserve(32);
        for (const auto& e : filterCatalogEntries()) {
            m.insert(e.typeId, e.category);
        }
        return m;
    }();
    return kMap.value(typeId.toLower());
}

bool filterCatalogMatchesSearch(const QString& query, const QString& displayName,
                                const QString& englishName, const QString& typeId,
                                const QString& category)
{
    const QString q = query.trimmed();
    if (q.isEmpty()) {
        return true;
    }
    const auto contains = [&](const QString& field) {
        return field.contains(q, Qt::CaseInsensitive);
    };
    return contains(displayName) || contains(englishName) || contains(typeId) || contains(category);
}

} // namespace pvj::core

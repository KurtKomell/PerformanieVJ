#include "FilterCatalog.h"

#include <QHash>

namespace pvj::core {
namespace {

QVector<FilterCatalogEntry> buildCatalog()
{
    QVector<FilterCatalogEntry> e;
    e.reserve(30);

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

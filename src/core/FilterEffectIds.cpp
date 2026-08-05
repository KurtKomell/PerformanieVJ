#include "FilterEffectIds.h"

#include "FilterCatalog.h"
#include "FilterParamSchema.h"

#include <QHash>
#include <QSet>

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

// Blur family indices match effect_blur.frag pvjEffectId().
constexpr RawEntry kEntries[] = {
    { "gaussian_blur", FilterEffectFamily::Blur, 0, 1, -1 },
    { "box_blur", FilterEffectFamily::Blur, 1, 1, -1 },
    { "directional_blur", FilterEffectFamily::Blur, 2, 1, -1 },
    { "radial_blur", FilterEffectFamily::Blur, 3, 1, -1 },
    { "zoom_blur", FilterEffectFamily::Blur, 4, 1, -1 },
    { "mosaic_blur", FilterEffectFamily::Blur, 5, 1, -1 },
    { "lens_blur", FilterEffectFamily::Blur, 6, 1, -1 },
    { "sharpen", FilterEffectFamily::Blur, 7, 1, -1 },
    { "sharpen_edges", FilterEffectFamily::Blur, 8, 1, -1 },
    { "soften_sharpen", FilterEffectFamily::Blur, 9, 1, -1 },

    { "invert", FilterEffectFamily::Color, 13, 1, -1 },
    { "chromatic_adaptation", FilterEffectFamily::Color, 26, 1, -1 },
    { "color_compressor", FilterEffectFamily::Color, 27, 1, -1 },
    { "color_stabilizer", FilterEffectFamily::Color, 29, 1, -1 },
    { "contrast_pop", FilterEffectFamily::Color, 30, 1, -1 },
    { "dehaze", FilterEffectFamily::Color, 31, 1, -1 },

    // Film Emulation (Resolve 21 Film Look Creator sections).
    { "film_look", FilterEffectFamily::Film, 0, 1, -1 },
    { "film_color", FilterEffectFamily::Film, 1, 1, -1 },
    { "film_split_tone", FilterEffectFamily::Film, 2, 1, -1 },
    { "film_vignette", FilterEffectFamily::Film, 3, 1, -1 },
    { "film_halation", FilterEffectFamily::Film, 4, 1, -1 },
    { "film_bloom", FilterEffectFamily::Film, 5, 1, -1 },
    { "film_grain", FilterEffectFamily::Film, 6, 1, -1 },
    { "film_flicker", FilterEffectFamily::Film, 7, 1, -1 },
    { "film_gate_weave", FilterEffectFamily::Film, 8, 1, -1 },
    { "film_gate", FilterEffectFamily::Film, 9, 1, -1 },

    // ResolveFX Light (familyId matches effect_light.frag)
    { "aperture_diffraction", FilterEffectFamily::Light, 0, 1, -1 },
    { "halation", FilterEffectFamily::Light, 1, 1, -1 },
    { "lens_flare", FilterEffectFamily::Light, 2, 1, -1 },
    { "lens_reflections", FilterEffectFamily::Light, 3, 1, -1 },
    { "light_rays", FilterEffectFamily::Light, 4, 1, -1 },
    { "glow", FilterEffectFamily::Light, 5, 4, -1 },

    // Resolve FX Stylize (familyId matches effect_stylize.frag pvjEffectId())
    { "abstraction", FilterEffectFamily::Stylize, 0, 1, -1 },
    { "blanking_fill", FilterEffectFamily::Stylize, 1, 1, -1 },
    { "drop_shadow", FilterEffectFamily::Stylize, 2, 1, -1 },
    { "edge_detect", FilterEffectFamily::Stylize, 3, 1, -1 },
    { "emboss", FilterEffectFamily::Stylize, 4, 1, -1 },
    { "mirrors", FilterEffectFamily::Stylize, 5, 1, -1 },
    { "pencil_sketch", FilterEffectFamily::Stylize, 6, 1, -1 },
    { "prism_blur", FilterEffectFamily::Stylize, 7, 1, -1 },
    { "scanlines", FilterEffectFamily::Stylize, 8, 1, -1 },
    { "stylize", FilterEffectFamily::Stylize, 9, 1, -1 },
    { "tilt_shift", FilterEffectFamily::Stylize, 10, 1, -1 },
    { "vignette", FilterEffectFamily::Stylize, 11, 1, -1 },
    { "watercolor", FilterEffectFamily::Stylize, 12, 1, -1 },

    // Resolve FX Temporal (familyId matches effect_temporal.frag)
    { "motion_trails", FilterEffectFamily::Temporal, 0, 1, -1 },
    { "smear", FilterEffectFamily::Temporal, 1, 1, -1 },
    { "stop_motion", FilterEffectFamily::Temporal, 2, 1, -1 },
    { "motion_blur", FilterEffectFamily::Temporal, 3, 1, -1 },

    // Resolve FX Texture (familyId matches effect_texture.frag)
    { "jpeg_damage", FilterEffectFamily::Texture, 0, 1, -1 },
    { "texture_pop", FilterEffectFamily::Texture, 1, 1, -1 },
    { "film_damage", FilterEffectFamily::Texture, 2, 1, -1 },
    { "analog_damage", FilterEffectFamily::Texture, 3, 1, -1 },

    // Injected by layer keying (not listed in FilterCatalog picker).
    { "chroma_key", FilterEffectFamily::Key, 0, 1, -1 },
    { "luma_key", FilterEffectFamily::Key, 1, 1, -1 },
    { "mask", FilterEffectFamily::Mask, 1, 1, -1 },
};

const QHash<QString, FilterEffectMeta>& metaMap()
{
    static const QHash<QString, FilterEffectMeta> kMap = [] {
        QHash<QString, FilterEffectMeta> m;
        m.reserve(static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0])) + 4);
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

QSet<QString> registeredTypeIds()
{
    static const QSet<QString> k = [] {
        QSet<QString> s;
        for (const RawEntry& e : kEntries) {
            s.insert(QString::fromLatin1(e.typeId));
        }
        s.insert(feedbackMarkerTypeId());
        return s;
    }();
    return k;
}

} // namespace

bool filterEffectIsRegistered(const QString& typeId)
{
    if (isFeedbackMarkerNode(typeId)) {
        return true;
    }
    return registeredTypeIds().contains(typeId.toLower());
}

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

bool filterEffectNeedsTemporalHistory(const QString& typeId)
{
    return filterEffectMeta(typeId).family == FilterEffectFamily::Temporal;
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
    Q_UNUSED(typeId);
    return false;
}

void sanitizeOutputFilterChain(QList<CellFilterNode>& chain)
{
    chain.clear();
}

void sanitizeCellFilterChain(QList<CellFilterNode>& chain)
{
    QList<CellFilterNode> kept;
    kept.reserve(chain.size());
    for (const CellFilterNode& node : chain) {
        if (isFeedbackMarkerNode(node.typeId)) {
            kept.append(node);
            continue;
        }
        if (!filterEffectIsRegistered(node.typeId)) {
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

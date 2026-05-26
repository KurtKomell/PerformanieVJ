#include "FilterUniformPacker.h"

#include "core/FilterParamSchema.h"

#include <QtGlobal>

namespace pvj::render {
namespace {

double paramValue(const pvj::core::CellFilterNode& node, const pvj::core::FilterNodeSpec& schema,
                  int index, double fallback = 0.0)
{
    if (index < 0 || index >= schema.params.size()) {
        return fallback;
    }
    const auto& spec = schema.params[index];
    double v = spec.defaultV;
    for (const auto& p : node.params) {
        if (p.name == spec.name) {
            v = p.value;
            break;
        }
    }
    if (spec.kind == pvj::core::FilterParamKind::Angle) {
        v = v * 3.14159265358979323846 / 180.0;
    } else if (spec.kind == pvj::core::FilterParamKind::Bool) {
        v = v >= 0.5 ? 1.0 : 0.0;
    }
    return v;
}

double namedParam(const pvj::core::CellFilterNode& node, const QString& name, double fallback = 0.0)
{
    for (const auto& p : node.params) {
        if (p.name == name) {
            return p.value;
        }
    }
    return fallback;
}

bool usesResolveBlendParams(const pvj::core::CellFilterNode& node)
{
    const pvj::core::FilterNodeSpec schema = pvj::core::filterParamSchemas().value(node.typeId);
    return !schema.params.isEmpty() && schema.params.front().name == QStringLiteral("blend");
}

float uvRadiusFromBlurParams(const pvj::core::CellFilterNode& node, const QSize& pixelSize)
{
    const double amount = qBound(0.0, namedParam(node, QStringLiteral("amount"), 0.75), 1.0);
    double radiusPx = namedParam(node, QStringLiteral("radius"), -1.0);
    if (radiusPx < 0.0) {
        radiusPx = amount * 16.0;
    }
    const double minDim = double(qMax(1, qMin(pixelSize.width(), pixelSize.height())));
    // Ensure at least ~2 px effective radius so blur is visible at preview sizes.
    const double uvRadius = qBound(0.002, (radiusPx / minDim) * amount * 2.5, 0.25);
    return float(uvRadius);
}

void packGenericParams(EffectQuadUbo2& ubo, const pvj::core::CellFilterNode& node)
{
    const pvj::core::FilterNodeSpec schema = pvj::core::filterParamSchemas().value(node.typeId);
    for (int i = 0; i < schema.params.size() && i < 4; ++i) {
        ubo.params[i] = float(paramValue(node, schema, i, schema.params[i].defaultV));
    }
}

} // namespace

void packFilterUniformBuffer(EffectQuadUbo2& ubo, const pvj::core::CellFilterNode& node,
                             const QSize& pixelSize, float elapsedSec, int internalPass,
                             const float keyChannelRgb[3])
{
    ubo = EffectQuadUbo2{};
    ubo.scaleOffset[0] = 1.0f;
    ubo.scaleOffset[1] = 1.0f;
    ubo.scaleOffset[2] = elapsedSec;
    ubo.scaleOffset[3] = float(pixelSize.width()) / float(qMax(pixelSize.height(), 1));
    ubo.params2[0] = float(internalPass);

    const pvj::core::FilterEffectMeta meta = pvj::core::filterEffectMeta(node.typeId);
    ubo.rotation[3] = float(meta.familyId);

    const bool resolveBlend = usesResolveBlendParams(node);
    if (resolveBlend) {
        packGenericParams(ubo, node);
        switch (meta.family) {
        case pvj::core::FilterEffectFamily::Key:
            ubo.scaleOffset[2] = keyChannelRgb ? keyChannelRgb[0] : 1.0f;
            ubo.rotation[1] = keyChannelRgb ? keyChannelRgb[1] : 1.0f;
            ubo.rotation[2] = keyChannelRgb ? keyChannelRgb[2] : 1.0f;
            break;
        case pvj::core::FilterEffectFamily::Generate:
            if (meta.familyId == 1 && internalPass == 1) {
                ubo.params[1] = ubo.params[1] * 2.0f;
            }
            break;
        default:
            break;
        }
        return;
    }

    packGenericParams(ubo, node);

    switch (meta.family) {
    case pvj::core::FilterEffectFamily::Blur: {
        const float minDim = float(qMax(1, qMin(pixelSize.width(), pixelSize.height())));
        const float r = uvRadiusFromBlurParams(node, pixelSize);
        const float amount = float(qBound(0.0, namedParam(node, QStringLiteral("amount"), 0.75), 1.0));
        const float radiusPx = float(namedParam(node, QStringLiteral("radius"), 12.0));
        ubo.params[0] = r;
        ubo.params[1] = radiusPx;
        ubo.params[2] = float(namedParam(node, QStringLiteral("angle"), 0.0)
                               * 3.14159265358979323846 / 180.0);
        ubo.params[3] = amount;
        // params2.y = blur axis (0 horizontal, 1 vertical); params2.z = 1/minDim (texel size).
        ubo.params2[1] = (internalPass % 2 == 1) ? 1.0f : 0.0f;
        ubo.params2[2] = 1.0f / minDim;
        ubo.rotation[0] = 0.0f;
        break;
    }
    case pvj::core::FilterEffectFamily::Kaleido:
        ubo.params[0] = float(qMax(2.0, namedParam(node, QStringLiteral("divisions"), 4.0)));
        ubo.params[1] = float(namedParam(node, QStringLiteral("angle"), 0.0) * 3.14159265358979323846 / 180.0);
        ubo.params[2] = float(namedParam(node, QStringLiteral("mix"), 1.0));
        break;
    case pvj::core::FilterEffectFamily::Key:
        ubo.scaleOffset[2] = keyChannelRgb ? keyChannelRgb[0] : 1.0f;
        ubo.rotation[1] = keyChannelRgb ? keyChannelRgb[1] : 1.0f;
        ubo.rotation[2] = keyChannelRgb ? keyChannelRgb[2] : 1.0f;
        ubo.rotation[3] = (meta.familyId == 1) ? 1.0f : 0.0f;
        if (ubo.params[3] < 0.04f) {
            ubo.params[3] = 0.04f;
        }
        break;
    case pvj::core::FilterEffectFamily::Blend:
        if (meta.blendModeOverride >= 0) {
            ubo.params[0] = float(meta.blendModeOverride);
        }
        break;
    case pvj::core::FilterEffectFamily::Generate:
        if (meta.familyId == 1 && internalPass == 1) {
            ubo.params[0] = ubo.params[0] * 2.0f;
        }
        break;
    default:
        break;
    }
}

} // namespace pvj::render

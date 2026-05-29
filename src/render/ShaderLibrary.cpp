#include "ShaderLibrary.h"

#include "FilterEffectShaderMap.h"

#include "core/FilterCatalog.h"
#include "core/FilterEffectIds.h"

#include <QFile>
#include <QSet>

namespace pvj::render {
namespace {

bool isKnownTypeId(const QString& lower)
{
    return pvj::core::filterEffectIsRegistered(lower);
}

} // namespace

QString effectFragmentShaderResource(EffectShaderId id)
{
    switch (id) {
    case EffectShaderId::Blur:
        return filterFamilyShaderResource(pvj::core::FilterEffectFamily::Blur);
    case EffectShaderId::ColorCorrection:
        return filterFamilyShaderResource(pvj::core::FilterEffectFamily::Color);
    case EffectShaderId::Kaleido:
        return filterFamilyShaderResource(pvj::core::FilterEffectFamily::Kaleido);
    case EffectShaderId::Mask:
        return filterFamilyShaderResource(pvj::core::FilterEffectFamily::Mask);
    }
    return {};
}

QString effectFragmentShaderForType(const QString& typeId)
{
    const QString lower = typeId.toLower();
    if (!isKnownTypeId(lower)) {
        return QStringLiteral(":/shaders/effect_identity.frag.qsb");
    }
    const pvj::core::FilterEffectMeta meta = pvj::core::filterEffectMeta(lower);
    return filterFamilyShaderResource(meta.family);
}

bool effectUsesTwoTextures(const QString& typeId)
{
    const pvj::core::FilterEffectMeta meta = pvj::core::filterEffectMeta(typeId);
    return meta.family == pvj::core::FilterEffectFamily::Blend
        || meta.family == pvj::core::FilterEffectFamily::Light;
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

#include "FilterEffectShaderMap.h"

namespace pvj::render {

QString filterFamilyShaderResource(pvj::core::FilterEffectFamily family)
{
    switch (family) {
    case pvj::core::FilterEffectFamily::Blur:
        return QStringLiteral(":/shaders/effect_blur.frag.qsb");
    case pvj::core::FilterEffectFamily::Color:
        return QStringLiteral(":/shaders/effect_color.frag.qsb");
    case pvj::core::FilterEffectFamily::Transform:
        return QStringLiteral(":/shaders/effect_transform.frag.qsb");
    case pvj::core::FilterEffectFamily::Distort:
        return QStringLiteral(":/shaders/effect_distort.frag.qsb");
    case pvj::core::FilterEffectFamily::Kaleido:
        return QStringLiteral(":/shaders/effect_kaleido.frag.qsb");
    case pvj::core::FilterEffectFamily::Generate:
        return QStringLiteral(":/shaders/effect_generate.frag.qsb");
    case pvj::core::FilterEffectFamily::Stylize:
        return QStringLiteral(":/shaders/effect_stylize.frag.qsb");
    case pvj::core::FilterEffectFamily::Key:
        return QStringLiteral(":/shaders/effect_key.frag.qsb");
    case pvj::core::FilterEffectFamily::Mask:
        return QStringLiteral(":/shaders/effect_mask.frag.qsb");
    case pvj::core::FilterEffectFamily::Blend:
        return QStringLiteral(":/shaders/effect_blend.frag.qsb");
    case pvj::core::FilterEffectFamily::Pattern:
        return QStringLiteral(":/shaders/effect_pattern.frag.qsb");
    case pvj::core::FilterEffectFamily::Utility:
        return QStringLiteral(":/shaders/effect_utility.frag.qsb");
    case pvj::core::FilterEffectFamily::Light:
        return QStringLiteral(":/shaders/effect_light.frag.qsb");
    case pvj::core::FilterEffectFamily::Revival:
        return QStringLiteral(":/shaders/effect_revival.frag.qsb");
    case pvj::core::FilterEffectFamily::Temporal:
        return QStringLiteral(":/shaders/effect_temporal.frag.qsb");
    case pvj::core::FilterEffectFamily::Maxine:
        return QStringLiteral(":/shaders/effect_utility.frag.qsb");
    }
    return QStringLiteral(":/shaders/effect_identity.frag.qsb");
}

} // namespace pvj::render

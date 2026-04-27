#include "render/ShaderLibrary.h"

#include <QtTest>

class TestShaderLibrary : public QObject
{
    Q_OBJECT

private slots:
    void effect_fragments_load();
    void transition_pairs_load();
};

void TestShaderLibrary::effect_fragments_load()
{
    using pvj::render::EffectShaderId;
    using pvj::render::shaderBundleIsValid;
    using pvj::render::sharedTexturedQuadVertexResource;
    using pvj::render::effectFragmentShaderResource;

    QVERIFY(shaderBundleIsValid(sharedTexturedQuadVertexResource()));

    QVERIFY(shaderBundleIsValid(effectFragmentShaderResource(EffectShaderId::Blur)));
    QVERIFY(shaderBundleIsValid(effectFragmentShaderResource(EffectShaderId::ColorCorrection)));
    QVERIFY(shaderBundleIsValid(effectFragmentShaderResource(EffectShaderId::Kaleido)));
    QVERIFY(shaderBundleIsValid(effectFragmentShaderResource(EffectShaderId::Mask)));
}

void TestShaderLibrary::transition_pairs_load()
{
    using pvj::render::TransitionShaderId;
    using pvj::render::shaderBundleIsValid;
    using pvj::render::transitionFragmentShaderResource;
    using pvj::render::transitionVertexShaderResource;

    QVERIFY(shaderBundleIsValid(transitionVertexShaderResource()));

    QVERIFY(shaderBundleIsValid(transitionFragmentShaderResource(TransitionShaderId::Crossfade)));
    QVERIFY(shaderBundleIsValid(transitionFragmentShaderResource(TransitionShaderId::LumaWipe)));
    QVERIFY(shaderBundleIsValid(transitionFragmentShaderResource(TransitionShaderId::Slide)));
}

QTEST_MAIN(TestShaderLibrary)

#include "test_shader_library.moc"

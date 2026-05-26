#include <QtTest/QtTest>

#include "core/FilterCatalog.h"
#include "core/FilterParamSchema.h"
#include "core/Model.h"
#include "render/RhiMixerWidget.h"
#include "render/ShaderLibrary.h"

class TestRhiFilterChain : public QObject
{
    Q_OBJECT

private slots:
    void shader_mapping_exists_for_common_types();
    void mixer_accepts_filter_chains();
    void mixer_accepts_output_filter_chain();
    /// Every catalog typeId resolves to a loadable :/shaders/*.qsb (including identity pass-through).
    void every_catalog_type_has_resolvable_shader_bundle();
};

void TestRhiFilterChain::shader_mapping_exists_for_common_types()
{
    using pvj::render::effectFragmentShaderForType;
    using pvj::render::shaderBundleIsValid;

    QVERIFY(shaderBundleIsValid(effectFragmentShaderForType(QStringLiteral("blur"))));
    QVERIFY(shaderBundleIsValid(effectFragmentShaderForType(QStringLiteral("color_correction"))));
    QVERIFY(shaderBundleIsValid(effectFragmentShaderForType(QStringLiteral("kaleidoscope"))));
    QVERIFY(shaderBundleIsValid(effectFragmentShaderForType(QStringLiteral("pixelate"))));
    QVERIFY(shaderBundleIsValid(effectFragmentShaderForType(QStringLiteral("cartoon"))));
}

void TestRhiFilterChain::mixer_accepts_filter_chains()
{
    pvj::render::RhiMixerWidget mixer;
    QList<pvj::core::CellFilterNode> chain;
    pvj::core::CellFilterNode n;
    n.typeId = QStringLiteral("blur");
    n.params.append({ QStringLiteral("amount"), 0.75 });
    chain.append(n);
    mixer.setLayerActive(0, true);
    mixer.setLayerFilterChain(0, chain);
    QVERIFY(mixer.layerActive(0));
}

void TestRhiFilterChain::mixer_accepts_output_filter_chain()
{
    pvj::render::RhiMixerWidget mixer;
    QList<pvj::core::CellFilterNode> chain;
    pvj::core::CellFilterNode n;
    n.typeId = QStringLiteral("nvidia_upscale");
    n.params = pvj::core::defaultParamsFor(n.typeId);
    chain.append(n);
    mixer.setOutputFilterChain(chain);
    QCOMPARE(mixer.outputFilterChain().size(), 1);
    QCOMPARE(mixer.outputFilterChain()[0].typeId, QStringLiteral("nvidia_upscale"));
}

void TestRhiFilterChain::every_catalog_type_has_resolvable_shader_bundle()
{
    using pvj::core::filterCatalogEntries;
    using pvj::render::effectFragmentShaderForType;
    using pvj::render::shaderBundleIsValid;

    for (const auto& entry : filterCatalogEntries()) {
        const QString path = effectFragmentShaderForType(entry.typeId);
        QVERIFY2(shaderBundleIsValid(path),
                 qPrintable(QStringLiteral("typeId=%1 path=%2").arg(entry.typeId, path)));
    }
}

QTEST_MAIN(TestRhiFilterChain)
#include "test_rhi_filter_chain.moc"

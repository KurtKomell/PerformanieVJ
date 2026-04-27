#include <QtTest/QtTest>

#include "core/FilterCatalog.h"
#include "core/Model.h"
#include "render/RhiMixerWidget.h"
#include "render/ShaderLibrary.h"

class TestRhiFilterChain : public QObject
{
    Q_OBJECT

private slots:
    void shader_mapping_exists_for_common_types();
    void mixer_accepts_filter_chains();
    /// Every catalog typeId resolves to a loadable :/shaders/*.qsb (including identity pass-through).
    void every_catalog_type_has_resolvable_shader_bundle();
    /// `feedback` still uses a dedicated 2-tex / 64B-UBO path; keep explicit until the mixer prepass supports it.
    void identity_fallback_is_only_expected_for_feedback();
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

void TestRhiFilterChain::identity_fallback_is_only_expected_for_feedback()
{
    using pvj::core::filterCatalogEntries;
    using pvj::render::effectFragmentShaderForType;

    static const QString kIdentity = QStringLiteral(":/shaders/effect_identity.frag.qsb");
    QStringList stillIdentity;
    for (const auto& entry : filterCatalogEntries()) {
        if (effectFragmentShaderForType(entry.typeId) == kIdentity) {
            stillIdentity.append(entry.typeId);
        }
    }
    stillIdentity.sort();
    // `feedback` maps to identity in the current filter prepass implementation.
    const QStringList knownIdentity = { QStringLiteral("feedback") };
    for (const QString& id : stillIdentity) {
        QVERIFY2(knownIdentity.contains(id),
                 qPrintable(QStringLiteral("unexpected identity for %1; map it in ShaderLibrary or document here").arg(id)));
    }
}

QTEST_MAIN(TestRhiFilterChain)
#include "test_rhi_filter_chain.moc"

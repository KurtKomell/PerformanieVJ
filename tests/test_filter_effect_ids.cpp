#include <QtTest/QtTest>

#include <QSet>

#include "core/FilterCatalog.h"
#include "core/FilterEffectIds.h"
#include "core/FilterParamSchema.h"
#include "render/FilterEffectShaderMap.h"
#include "render/ShaderLibrary.h"

class TestFilterEffectIds : public QObject
{
    Q_OBJECT

private slots:
    void catalog_matches_effect_table();
    void family_ids_unique_within_family();
    void no_identity_shader_for_catalog_entries();
    void all_family_shaders_load();
    void resolve_entries_are_new();
    void resolve_schemas_use_blend_param();
};

void TestFilterEffectIds::catalog_matches_effect_table()
{
    QCOMPARE(pvj::core::filterEffectCatalogCount(), pvj::core::filterCatalogEntries().size());
    for (const auto& entry : pvj::core::filterCatalogEntries()) {
        const pvj::core::FilterEffectMeta meta = pvj::core::filterEffectMeta(entry.typeId);
        QVERIFY(meta.familyId >= 0);
    }
}

void TestFilterEffectIds::family_ids_unique_within_family()
{
    QHash<int, QSet<int>> idsByFamily;
    for (const auto& entry : pvj::core::filterCatalogEntries()) {
        const pvj::core::FilterEffectMeta meta = pvj::core::filterEffectMeta(entry.typeId);
        const int fam = static_cast<int>(meta.family);
        QVERIFY(!idsByFamily[fam].contains(meta.familyId));
        idsByFamily[fam].insert(meta.familyId);
    }
}

void TestFilterEffectIds::no_identity_shader_for_catalog_entries()
{
    using pvj::render::effectFragmentShaderForType;
    const QString identity = QStringLiteral(":/shaders/effect_identity.frag.qsb");
    for (const auto& entry : pvj::core::filterCatalogEntries()) {
        QVERIFY2(effectFragmentShaderForType(entry.typeId) != identity,
                 qPrintable(entry.typeId));
    }
}

void TestFilterEffectIds::all_family_shaders_load()
{
    using pvj::render::filterFamilyShaderResource;
    using pvj::render::shaderBundleIsValid;
    for (int f = 0; f <= static_cast<int>(pvj::core::FilterEffectFamily::Maxine); ++f) {
        const auto family = static_cast<pvj::core::FilterEffectFamily>(f);
        QVERIFY(shaderBundleIsValid(filterFamilyShaderResource(family)));
    }
}

void TestFilterEffectIds::resolve_entries_are_new()
{
    static const QSet<QString> kAlreadyCovered = {
        QStringLiteral("gaussian_blur"), QStringLiteral("box_blur"), QStringLiteral("radial_blur"),
        QStringLiteral("directional_blur"), QStringLiteral("motion_blur"), QStringLiteral("zoom_blur"),
        QStringLiteral("sharpen"), QStringLiteral("denoise"), QStringLiteral("glow"),
        QStringLiteral("god_rays"), QStringLiteral("vignette"), QStringLiteral("drop_shadow"),
        QStringLiteral("emboss"), QStringLiteral("find_edges"), QStringLiteral("film_grain"),
        QStringLiteral("vhs"), QStringLiteral("chroma_key"), QStringLiteral("luma_key"),
        QStringLiteral("tilt_shift"), QStringLiteral("night_vision"), QStringLiteral("chromatic_aberration"),
        QStringLiteral("invert"), QStringLiteral("watercolor"), QStringLiteral("cartoon"),
        QStringLiteral("screen_shake"),
    };
    int resolveCount = 0;
    for (const auto& entry : pvj::core::filterCatalogEntries()) {
        if (!entry.category.startsWith(QStringLiteral("Resolve FX"))
            && !entry.category.startsWith(QStringLiteral("Fusion"))) {
            continue;
        }
        ++resolveCount;
        QVERIFY2(!kAlreadyCovered.contains(entry.typeId), qPrintable(entry.typeId));
        const pvj::core::FilterEffectMeta meta = pvj::core::filterEffectMeta(entry.typeId);
        QVERIFY2(meta.familyId >= 0, qPrintable(entry.typeId));
    }
    QVERIFY2(resolveCount >= 65, qPrintable(QString::number(resolveCount)));
}

void TestFilterEffectIds::resolve_schemas_use_blend_param()
{
    for (const auto& entry : pvj::core::filterCatalogEntries()) {
        if (!entry.category.startsWith(QStringLiteral("Resolve FX"))
            && !entry.category.startsWith(QStringLiteral("Fusion"))) {
            continue;
        }
        const pvj::core::FilterNodeSpec spec = pvj::core::filterParamSchemas().value(entry.typeId);
        QVERIFY2(!spec.params.isEmpty(), qPrintable(entry.typeId));
        QCOMPARE(spec.params.front().name, QStringLiteral("blend"));
        QVERIFY(spec.params.size() <= 4);
    }
}

QTEST_MAIN(TestFilterEffectIds)
#include "test_filter_effect_ids.moc"

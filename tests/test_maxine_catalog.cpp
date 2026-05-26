#include "core/FilterCatalog.h"
#include "core/FilterEffectIds.h"

#include <QtTest>

class TestMaxineCatalog : public QObject
{
    Q_OBJECT

private slots:
    void maxine_filters_use_maxine_backend();
    void maxine_category_key();
};

void TestMaxineCatalog::maxine_filters_use_maxine_backend()
{
    for (const char* id : { "nvidia_artifact_reduction", "nvidia_super_resolution", "nvidia_upscale",
                            "nvidia_video_denoise", "noise_reduction" }) {
        const pvj::core::FilterEffectMeta meta = pvj::core::filterEffectMeta(QString::fromLatin1(id));
        QCOMPARE(meta.family, pvj::core::FilterEffectFamily::Maxine);
        QVERIFY(pvj::core::filterUsesMaxineBackend(QString::fromLatin1(id)));
    }
}

void TestMaxineCatalog::maxine_category_key()
{
    QCOMPARE(pvj::core::maxineFilterCategoryKey(), QStringLiteral("NVIDIA"));
}

QTEST_APPLESS_MAIN(TestMaxineCatalog)
#include "test_maxine_catalog.moc"

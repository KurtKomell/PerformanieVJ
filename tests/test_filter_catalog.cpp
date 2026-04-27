#include <QtTest/QtTest>

#include <QSet>

#include "core/FilterCatalog.h"

using namespace pvj::core;

class TestFilterCatalog : public QObject
{
    Q_OBJECT

private slots:
    void entries_are_unique_and_named();
    void catalog_covers_resolume_style_set();
};

void TestFilterCatalog::entries_are_unique_and_named()
{
    QSet<QString> ids;
    for (const auto& e : filterCatalogEntries()) {
        QVERIFY2(!e.typeId.isEmpty(), "typeId");
        QVERIFY2(!e.category.isEmpty(), "category");
        QVERIFY2(!e.englishName.isEmpty(), "englishName");
        QVERIFY2(!ids.contains(e.typeId), qPrintable(QStringLiteral("duplicate: ") + e.typeId));
        ids.insert(e.typeId);
        QCOMPARE(filterCatalogEnglishName(e.typeId), e.englishName);
    }
}

void TestFilterCatalog::catalog_covers_resolume_style_set()
{
    QVERIFY(filterCatalogEntries().size() >= 120);
}

QTEST_MAIN(TestFilterCatalog)
#include "test_filter_catalog.moc"

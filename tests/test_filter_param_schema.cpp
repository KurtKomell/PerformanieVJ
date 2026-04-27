#include <QtTest/QtTest>

#include "core/FilterCatalog.h"
#include "core/FilterParamSchema.h"

using namespace pvj::core;

class TestFilterParamSchema : public QObject
{
    Q_OBJECT

private slots:
    void schema_covers_all_catalog_entries();
};

void TestFilterParamSchema::schema_covers_all_catalog_entries()
{
    const auto& schema = filterParamSchemas();
    for (const auto& entry : filterCatalogEntries()) {
        QVERIFY2(schema.contains(entry.typeId), qPrintable(entry.typeId));
        const auto spec = schema.value(entry.typeId);
        QVERIFY(spec.params.size() <= 4);
        for (const auto& p : spec.params) {
            QVERIFY2(!p.name.isEmpty(), "param name");
            QVERIFY2(!p.label.isEmpty(), "param label");
            QVERIFY2(p.defaultV >= p.minV && p.defaultV <= p.maxV, qPrintable(entry.typeId + ":" + p.name));
        }
    }
}

QTEST_MAIN(TestFilterParamSchema)
#include "test_filter_param_schema.moc"

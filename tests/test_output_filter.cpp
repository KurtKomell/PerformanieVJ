#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "core/FilterEffectIds.h"
#include "core/FilterParamSchema.h"
#include "core/Project.h"
#include "core/PvjSerializer.h"

class TestOutputFilter : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void nvidia_filters_allowed_on_output_chain();
    void resolve_and_shader_filters_rejected();
    void sanitize_strips_invalid_nodes();
    void output_chain_roundtrips_in_pvj();

private:
    QTemporaryDir m_tmp;
};

void TestOutputFilter::initTestCase()
{
    QVERIFY(m_tmp.isValid());
}

void TestOutputFilter::nvidia_filters_allowed_on_output_chain()
{
    using pvj::core::isOutputAllowedFilter;

    QVERIFY(isOutputAllowedFilter(QStringLiteral("nvidia_artifact_reduction")));
    QVERIFY(isOutputAllowedFilter(QStringLiteral("nvidia_super_resolution")));
    QVERIFY(isOutputAllowedFilter(QStringLiteral("nvidia_upscale")));
    QVERIFY(isOutputAllowedFilter(QStringLiteral("noise_reduction")));
}

void TestOutputFilter::resolve_and_shader_filters_rejected()
{
    using pvj::core::isOutputAllowedFilter;

    QVERIFY(!isOutputAllowedFilter(QStringLiteral("blur")));
    QVERIFY(!isOutputAllowedFilter(QStringLiteral("color_correction")));
    QVERIFY(!isOutputAllowedFilter(QStringLiteral("")));
    QVERIFY(!isOutputAllowedFilter(QStringLiteral("cartoon")));
}

void TestOutputFilter::sanitize_strips_invalid_nodes()
{
    using pvj::core::CellFilterNode;
    using pvj::core::sanitizeOutputFilterChain;

    QList<CellFilterNode> chain;
    CellFilterNode blur;
    blur.typeId = QStringLiteral("blur");
    blur.params = pvj::core::defaultParamsFor(blur.typeId);
    chain.append(blur);

    CellFilterNode nvidia;
    nvidia.typeId = QStringLiteral("nvidia_upscale");
    nvidia.params = pvj::core::defaultParamsFor(nvidia.typeId);
    chain.append(nvidia);

    sanitizeOutputFilterChain(chain);
    QCOMPARE(chain.size(), 1);
    QCOMPARE(chain[0].typeId, QStringLiteral("nvidia_upscale"));
}

void TestOutputFilter::output_chain_roundtrips_in_pvj()
{
    using pvj::core::CellFilterNode;
    using pvj::core::Project;
    using pvj::core::PvjSerializer;

    Project p;
    p.initializeDefault();

    CellFilterNode n;
    n.typeId = QStringLiteral("nvidia_upscale");
    n.params = pvj::core::defaultParamsFor(n.typeId);
    if (!n.params.isEmpty()) {
        n.params[0].value = 0.66;
    }
    p.settings.output.filterChain.append(n);

    const QString path = m_tmp.filePath(QStringLiteral("output_chain.pvj"));
    QVERIFY(PvjSerializer::save(p, path).ok);

    Project loaded;
    const auto res = PvjSerializer::load(loaded, path);
    QVERIFY2(res.ok, qPrintable(res.errorMessage));
    QCOMPARE(loaded.settings.output.filterChain.size(), 1);
    QCOMPARE(loaded.settings.output.filterChain[0].typeId, QStringLiteral("nvidia_upscale"));
    if (!loaded.settings.output.filterChain[0].params.isEmpty()) {
        QCOMPARE(loaded.settings.output.filterChain[0].params[0].value, 0.66);
    }
}

QTEST_MAIN(TestOutputFilter)
#include "test_output_filter.moc"

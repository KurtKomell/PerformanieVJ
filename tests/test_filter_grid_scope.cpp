#include <QtTest/QtTest>

#include "core/FilterParamSchema.h"
#include "core/Project.h"

using namespace pvj::core;

namespace {

const Cell* cellAtBankSlot(const Project& p, int bankSet, int bank, int cell)
{
    if (bankSet < 0 || bankSet >= p.bankSets.size()) {
        return nullptr;
    }
    const auto& set = p.bankSets[bankSet];
    if (bank < 0 || bank >= set.banks.size()) {
        return nullptr;
    }
    const auto& b = set.banks[bank];
    if (cell < 0 || cell >= b.cells.size()) {
        return nullptr;
    }
    return &b.cells[cell];
}

} // namespace

class TestFilterGridScope : public QObject
{
    Q_OBJECT

private slots:
    void filterChainsAreIsolatedPerCell();
    void deckResolutionMatchesBankCoordinates();
};

void TestFilterGridScope::filterChainsAreIsolatedPerCell()
{
    Project p;
    p.initializeDefault();
    QVERIFY(!p.bankSets.isEmpty());
    Cell& a = p.bankSets[0].banks[0].cells[0];
    Cell& b = p.bankSets[0].banks[0].cells[1];
    a.filterChain.clear();
    b.filterChain.clear();

    CellFilterNode n;
    n.typeId = QStringLiteral("blur");
    n.params = defaultParamsFor(n.typeId);
    a.filterChain.append(n);

    QCOMPARE(a.filterChain.size(), 1);
    QCOMPARE(b.filterChain.size(), 0);
    QVERIFY(cellAtBankSlot(p, 0, 0, 0)->filterChain.size() == 1);
    QVERIFY(cellAtBankSlot(p, 0, 0, 1)->filterChain.isEmpty());
}

void TestFilterGridScope::deckResolutionMatchesBankCoordinates()
{
    Project p;
    p.initializeDefault();
    Cell& c00 = p.bankSets[0].banks[3].cells[5];
    c00.filterChain.clear();
    CellFilterNode n;
    n.typeId = QStringLiteral("invert");
    n.params = defaultParamsFor(n.typeId);
    c00.filterChain.append(n);

    const Cell* resolved = cellAtBankSlot(p, 0, 3, 5);
    QVERIFY(resolved);
    QCOMPARE(resolved->filterChain.size(), 1);
    QCOMPARE(resolved->filterChain[0].typeId, QStringLiteral("invert"));
}

QTEST_MAIN(TestFilterGridScope)
#include "test_filter_grid_scope.moc"

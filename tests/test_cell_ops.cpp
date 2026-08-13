#include "core/CellOps.h"
#include "core/Model.h"

#include <QtTest>

using pvj::core::Cell;
using pvj::core::CellPasteMode;
using pvj::core::InputType;
using pvj::core::PropertyMapping;
using pvj::core::VisualType;
using pvj::core::pasteCellContent;

class TestCellOps : public QObject
{
    Q_OBJECT

private slots:
    void pasteFullWithMidi_copiesVisualAndMappings();
    void pasteFullWithoutMidi_clearsMappings();
    void pasteParamsWithMidi_keepsVisual();
};

namespace {

PropertyMapping makeCcMapping(const QString& property, int cc)
{
    PropertyMapping m;
    m.property = property;
    m.input    = InputType::MidiCC;
    m.channel  = 0;
    m.number   = cc;
    return m;
}

Cell makeSourceCell()
{
    Cell src;
    src.index = 3;
    src.name  = QStringLiteral("Src");
    src.visual.type = VisualType::Media;
    src.props.transparency = 0.4;
    src.propertyMappings.append(makeCcMapping(QStringLiteral("transparency"), 7));
    return src;
}

} // namespace

void TestCellOps::pasteFullWithMidi_copiesVisualAndMappings()
{
    Cell src = makeSourceCell();
    Cell dst;
    dst.index = 9;
    dst.name  = QStringLiteral("Dst");
    dst.visual.type = VisualType::Empty;
    dst.propertyMappings.append(makeCcMapping(QStringLiteral("fade"), 1));

    pasteCellContent(dst, src, CellPasteMode::FullWithMidi);

    QCOMPARE(dst.index, 9);
    QCOMPARE(dst.name, QStringLiteral("Src"));
    QCOMPARE(dst.visual.type, VisualType::Media);
    QCOMPARE(dst.props.transparency, 0.4);
    QCOMPARE(dst.propertyMappings.size(), 1);
    QCOMPARE(dst.propertyMappings[0].property, QStringLiteral("transparency"));
    QCOMPARE(dst.propertyMappings[0].number, 7);
}

void TestCellOps::pasteFullWithoutMidi_clearsMappings()
{
    Cell src = makeSourceCell();
    Cell dst;
    dst.index = 9;
    dst.name  = QStringLiteral("Dst");
    dst.visual.type = VisualType::Empty;
    dst.propertyMappings.append(makeCcMapping(QStringLiteral("fade"), 1));

    pasteCellContent(dst, src, CellPasteMode::FullWithoutMidi);

    QCOMPARE(dst.name, QStringLiteral("Src"));
    QCOMPARE(dst.visual.type, VisualType::Media);
    QCOMPARE(dst.props.transparency, 0.4);
    QVERIFY(dst.propertyMappings.isEmpty());
}

void TestCellOps::pasteParamsWithMidi_keepsVisual()
{
    Cell src = makeSourceCell();
    Cell dst;
    dst.index = 9;
    dst.name  = QStringLiteral("Dst");
    dst.visual.type = VisualType::Empty;
    dst.props.transparency = 1.0;

    pasteCellContent(dst, src, CellPasteMode::ParamsWithMidi);

    QCOMPARE(dst.name, QStringLiteral("Dst"));
    QCOMPARE(dst.visual.type, VisualType::Empty);
    QCOMPARE(dst.props.transparency, 0.4);
    QCOMPARE(dst.propertyMappings.size(), 1);
    QCOMPARE(dst.propertyMappings[0].number, 7);
}

QTEST_MAIN(TestCellOps)
#include "test_cell_ops.moc"

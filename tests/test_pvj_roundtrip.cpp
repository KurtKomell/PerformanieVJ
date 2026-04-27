#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "core/Project.h"
#include "core/FilterParamSchema.h"
#include "core/PvjSerializer.h"

using namespace pvj::core;

class TestPvjRoundtrip : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void emptyDefaultRoundtrips();
    void richProjectRoundtrips();

private:
    QTemporaryDir m_tmp;
};

void TestPvjRoundtrip::initTestCase()
{
    QVERIFY(m_tmp.isValid());
}

void TestPvjRoundtrip::emptyDefaultRoundtrips()
{
    Project p;
    p.initializeDefault();

    const QString path = m_tmp.filePath(QStringLiteral("empty.pvj"));
    QVERIFY(PvjSerializer::save(p, path).ok);

    Project loaded;
    const auto res = PvjSerializer::load(loaded, path);
    QVERIFY2(res.ok, qPrintable(res.errorMessage));

    QCOMPARE(loaded.bankSets.size(), 1);
    QCOMPARE(loaded.bankSets[0].type, BankSetType::TypeA);
    QCOMPARE(loaded.bankSets[0].banks.size(), 64);
    QCOMPARE(loaded.bankSets[0].banks[0].cells.size(), 4 * 12);
    QCOMPARE(loaded.settings.matrix.width, 1920);
    QCOMPARE(loaded.settings.matrix.height, 1080);
    QCOMPARE(loaded.sourceFormat, QStringLiteral("pvj"));
}

void TestPvjRoundtrip::richProjectRoundtrips()
{
    Project p;
    p.initializeDefault();

    MediaItem m;
    m.id = QUuid::createUuid();
    m.path = QStringLiteral("/videos/clip.mp4");
    m.displayName = QStringLiteral("clip.mp4");
    p.mediaLibrary.append(m);

    auto& cell = p.bankSets[0].banks[2].cells[5];
    cell.visual.type = VisualType::Media;
    cell.visual.mediaId = m.id;
    cell.props.transparency = 0.5;
    cell.props.movieSpeed   = 1.25;
    cell.props.rotationZ    = 0.75;
    cell.props.copyMode     = CopyMode::Add;
    cell.props.maskType     = MaskType::Circle;
    cell.props.maskWidth    = 0.8;

    Effect fx;
    fx.name = QStringLiteral("blur");
    fx.params.append({QStringLiteral("amount"), 0.42});
    fx.params.append({QStringLiteral("quality"), 2.0});
    cell.effect = fx;

    PropertyMapping pm;
    pm.property = QStringLiteral("transparency");
    pm.input = InputType::MidiCC;
    pm.channel = 1;
    pm.number = 7;
    pm.minValue = 0.0;
    pm.maxValue = 1.0;
    cell.propertyMappings.append(pm);

    PropertyMapping pmNote;
    pmNote.property = QStringLiteral("playMode");
    pmNote.input = InputType::MidiNote;
    pmNote.channel = 2;
    pmNote.number = 40;
    pmNote.buttonMode = PropertyButtonMode::SetOnPress;
    pmNote.buttonValue = 0.5; // normalized → mid play-mode index via applyEnumFromNormalized
    cell.propertyMappings.append(pmNote);

    cell.filterChain.clear();
    pvj::core::CellFilterNode n1;
    n1.id = QUuid::createUuid();
    n1.typeId = QStringLiteral("blur");
    n1.params = pvj::core::defaultParamsFor(n1.typeId);
    if (!n1.params.isEmpty()) {
        n1.params[0].value = 0.25;
    }
    pvj::core::CellFilterNode n2;
    n2.id = QUuid::createUuid();
    n2.typeId = QStringLiteral("color_correction");
    n2.params = pvj::core::defaultParamsFor(n2.typeId);
    if (n2.params.size() > 1) {
        n2.params[1].value = -0.15;
    }
    cell.filterChain.append(n1);
    cell.filterChain.append(n2);

    TriggerMapping tm;
    tm.input = InputType::MidiNote;
    tm.channel = 1;
    tm.number = 36;
    tm.target = TriggerTarget::Cell;
    tm.bankSetIndex = 0;
    tm.bankIndex = 2;
    tm.cellIndex = 5;
    p.triggerMappings.append(tm);

    p.settings.audio.driver = QStringLiteral("wasapi");
    p.settings.audio.bufferSize = 256;
    p.settings.matrix.width = 3840;
    p.settings.matrix.height = 2160;

    const QString path = m_tmp.filePath(QStringLiteral("rich.pvj"));
    const auto saveRes = PvjSerializer::save(p, path);
    QVERIFY2(saveRes.ok, qPrintable(saveRes.errorMessage));

    Project loaded;
    const auto loadRes = PvjSerializer::load(loaded, path);
    QVERIFY2(loadRes.ok, qPrintable(loadRes.errorMessage));

    QCOMPARE(loaded.mediaLibrary.size(), 1);
    QCOMPARE(loaded.mediaLibrary[0].path, m.path);
    QCOMPARE(loaded.mediaLibrary[0].id, m.id);

    const auto& c = loaded.bankSets[0].banks[2].cells[5];
    QCOMPARE(c.visual.type, VisualType::Media);
    QCOMPARE(c.visual.mediaId, m.id);
    QCOMPARE(c.props.transparency, 0.5);
    QCOMPARE(c.props.movieSpeed, 1.25);
    QCOMPARE(c.props.rotationZ, 0.75);
    QCOMPARE(c.props.copyMode, CopyMode::Add);
    QCOMPARE(c.props.maskType, MaskType::Circle);
    QCOMPARE(c.props.maskWidth, 0.8);
    QVERIFY(c.effect.has_value());
    QCOMPARE(c.effect->name, QStringLiteral("blur"));
    QCOMPARE(c.effect->params.size(), 2);
    QCOMPARE(c.effect->params[0].name, QStringLiteral("amount"));
    QCOMPARE(c.effect->params[0].value, 0.42);

    QCOMPARE(c.propertyMappings.size(), 2);
    QCOMPARE(c.propertyMappings[0].property, QStringLiteral("transparency"));
    QCOMPARE(c.propertyMappings[0].input, InputType::MidiCC);
    QCOMPARE(c.propertyMappings[0].number, 7);
    QCOMPARE(c.propertyMappings[1].property, QStringLiteral("playMode"));
    QCOMPARE(c.propertyMappings[1].input, InputType::MidiNote);
    QCOMPARE(c.propertyMappings[1].channel, 2);
    QCOMPARE(c.propertyMappings[1].number, 40);
    QCOMPARE(c.propertyMappings[1].buttonMode, PropertyButtonMode::SetOnPress);
    QCOMPARE(c.propertyMappings[1].buttonValue, 0.5);
    QCOMPARE(c.filterChain.size(), 2);
    QCOMPARE(c.filterChain[0].typeId, QStringLiteral("blur"));
    QVERIFY(!c.filterChain[0].params.isEmpty());
    QCOMPARE(c.filterChain[0].params[0].name, QStringLiteral("amount"));
    QCOMPARE(c.filterChain[0].params[0].value, 0.25);
    QCOMPARE(c.filterChain[1].typeId, QStringLiteral("color_correction"));
    QVERIFY(c.filterChain[1].params.size() >= 2);
    QCOMPARE(c.filterChain[1].params[1].value, -0.15);

    QCOMPARE(loaded.triggerMappings.size(), 1);
    QCOMPARE(loaded.triggerMappings[0].input, InputType::MidiNote);
    QCOMPARE(loaded.triggerMappings[0].number, 36);

    QCOMPARE(loaded.settings.audio.driver, QStringLiteral("wasapi"));
    QCOMPARE(loaded.settings.audio.bufferSize, 256);
    QCOMPARE(loaded.settings.matrix.width, 3840);
    QCOMPARE(loaded.settings.matrix.height, 2160);
}

QTEST_MAIN(TestPvjRoundtrip)
#include "test_pvj_roundtrip.moc"

#include <QtTest/QtTest>

#include "core/Model.h"
#include "core/PropertyRegistry.h"

using namespace pvj::core;

class TestPropertyRegistry : public QObject
{
    Q_OBJECT

private slots:
    void enum_roundtrip_play_mode_normalized();
    void toggle_inverts_transparency();
};

void TestPropertyRegistry::enum_roundtrip_play_mode_normalized()
{
    Cell c;
    c.props.playMode = PlayMode::LoopForward;

    QVERIFY(PropertyRegistry::applyEnumFromNormalized(c, QStringLiteral("playMode"), 0.5));
    // 11 modes: floor(0.5 * 11) = 5 → PlayMode enum value 5
    QCOMPARE(int(c.props.playMode), 5);
    double readBack = 0.0;
    QVERIFY(PropertyRegistry::readValue(c, QStringLiteral("playMode"), &readBack));
    QCOMPARE(readBack, 5.0);
}

void TestPropertyRegistry::toggle_inverts_transparency()
{
    Cell c;
    c.props.transparency = 0.2;
    QVERIFY(PropertyRegistry::toggleValue(c, QStringLiteral("transparency")));
    QVERIFY(qAbs(c.props.transparency - 0.8) < 1e-6);
}

QTEST_MAIN(TestPropertyRegistry)
#include "test_property_registry.moc"

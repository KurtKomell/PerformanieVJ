#include "core/Model.h"
#include "core/Project.h"
#include "core/PropertyRegistry.h"
#include "input/InputRouter.h"

#include <QSignalSpy>
#include <QtTest>

using pvj::core::InputType;
using pvj::core::Project;
using pvj::core::PropertyMapping;
using pvj::input::InputRouter;

class TestInputRouter : public QObject
{
    Q_OBJECT

private slots:
    void midiCc_updatesMappedTransparency();
    void midiBuffer_multipleCcMessages();
    void midiThreeByteCc_parses();
    void resolvePropertyId_mapsGrandVjTokens();
};

void TestInputRouter::midiCc_updatesMappedTransparency()
{
    Project project;
    project.initializeDefault();
    project.ensureSingleBankSet();
    auto& cell = project.bankSets[0].banks[0].cells[0];

    PropertyMapping pm;
    pm.property = QStringLiteral("transparency");
    pm.input    = InputType::MidiCC;
    pm.channel  = 0;
    pm.number   = 7;
    pm.minValue = 0.0;
    pm.maxValue = 1.0;
    cell.propertyMappings.append(pm);

    InputRouter router;
    router.setProject(&project);

    QSignalSpy spy(&router, &InputRouter::propertyValueChanged);
    const unsigned char msg[] = {0xB0, 0x07, 0x40};
    router.handleMidiBytes(msg, sizeof(msg));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(3).toString(), QStringLiteral("transparency"));
    QVERIFY(qAbs(spy.at(0).at(4).toDouble() - (64.0 / 127.0)) < 1e-6);
}

void TestInputRouter::midiBuffer_multipleCcMessages()
{
    Project project;
    project.initializeDefault();
    project.ensureSingleBankSet();
    auto& cell = project.bankSets[0].banks[0].cells[0];

    PropertyMapping pm;
    pm.property = QStringLiteral("transparency");
    pm.input    = InputType::MidiCC;
    pm.channel  = 0;
    pm.number   = 1;
    pm.minValue = 0.0;
    pm.maxValue = 1.0;
    cell.propertyMappings.append(pm);

    InputRouter router;
    router.setProject(&project);

    QSignalSpy spy(&router, &InputRouter::propertyValueChanged);
    const unsigned char buf[] = {0xB0, 0x01, 0x10, 0xB0, 0x01, 0x20};
    router.handleMidiBytes(buf, sizeof(buf));

    QCOMPARE(spy.count(), 2);
}

void TestInputRouter::midiThreeByteCc_parses()
{
    Project project;
    project.initializeDefault();
    project.ensureSingleBankSet();
    auto& cell = project.bankSets[0].banks[0].cells[0];

    PropertyMapping pm;
    pm.property = QStringLiteral("transparency");
    pm.input    = InputType::MidiCC;
    pm.channel  = 0;
    pm.number   = 7;
    pm.minValue = 0.0;
    pm.maxValue = 1.0;
    cell.propertyMappings.append(pm);

    InputRouter router;
    router.setProject(&project);

    QSignalSpy spy(&router, &InputRouter::propertyValueChanged);
    const unsigned char msg[] = {0xB0, 0x07, 0x40};
    router.handleMidiBytes(msg, sizeof(msg));

    QCOMPARE(spy.count(), 1);
}

void TestInputRouter::resolvePropertyId_mapsGrandVjTokens()
{
    using pvj::core::PropertyRegistry::resolvePropertyId;
    QCOMPARE(resolvePropertyId(QStringLiteral("TRSP")), QStringLiteral("transparency"));
    QCOMPARE(resolvePropertyId(QStringLiteral("fade")), QStringLiteral("fade"));
}

QTEST_MAIN(TestInputRouter)
#include "test_input_router.moc"

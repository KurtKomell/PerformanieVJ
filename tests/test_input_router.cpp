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
    void midiAftertouch_updatesMappedProperty();
    void learnPropertyCc_holdNoteThenAftertouch();
    void learnPropertyNote_shortPressMapsNote();
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

void TestInputRouter::midiAftertouch_updatesMappedProperty()
{
    Project project;
    project.initializeDefault();
    project.ensureSingleBankSet();
    auto& cell = project.bankSets[0].banks[0].cells[0];

    PropertyMapping pm;
    pm.property = QStringLiteral("transparency");
    pm.input    = InputType::MidiAftertouch;
    pm.channel  = 0;
    pm.number   = 60;
    pm.minValue = 0.0;
    pm.maxValue = 1.0;
    cell.propertyMappings.append(pm);

    InputRouter router;
    router.setProject(&project);

    QSignalSpy spy(&router, &InputRouter::propertyValueChanged);
    const unsigned char msg[] = {0xA0, 0x3C, 0x40};
    router.handleMidiBytes(msg, sizeof(msg));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(3).toString(), QStringLiteral("transparency"));
    QVERIFY(qAbs(spy.at(0).at(4).toDouble() - (64.0 / 127.0)) < 1e-6);
}

void TestInputRouter::learnPropertyCc_holdNoteThenAftertouch()
{
    Project project;
    project.initializeDefault();
    project.ensureSingleBankSet();

    InputRouter router;
    router.setProject(&project);
    router.beginLearnPropertyCc(0, 0, 0, QStringLiteral("transparency"));

    QSignalSpy finished(&router, &InputRouter::learnFinished);
    const unsigned char noteOn[] = {0x90, 0x3C, 0x7F};
    router.handleMidiBytes(noteOn, sizeof(noteOn));
    QCOMPARE(finished.count(), 0);

    const unsigned char at[] = {0xA0, 0x3C, 0x50};
    router.handleMidiBytes(at, sizeof(at));
    QCOMPARE(finished.count(), 1);
    QVERIFY(finished.at(0).at(0).toString().contains(QStringLiteral("aftertouch")));

    const auto& mappings = project.bankSets[0].banks[0].cells[0].propertyMappings;
    QCOMPARE(mappings.size(), 1);
    QCOMPARE(mappings[0].input, InputType::MidiAftertouch);
    QCOMPARE(mappings[0].number, 60);
}

void TestInputRouter::learnPropertyNote_shortPressMapsNote()
{
    Project project;
    project.initializeDefault();
    project.ensureSingleBankSet();

    InputRouter router;
    router.setProject(&project);
    router.beginLearnPropertyNote(0, 0, 0, QStringLiteral("fade"),
                                  pvj::core::PropertyButtonMode::Toggle, 0.0);

    QSignalSpy finished(&router, &InputRouter::learnFinished);
    const unsigned char noteOn[] = {0x90, 0x40, 0x7F};
    const unsigned char noteOff[] = {0x80, 0x40, 0x00};
    router.handleMidiBytes(noteOn, sizeof(noteOn));
    QCOMPARE(finished.count(), 0);
    router.handleMidiBytes(noteOff, sizeof(noteOff));
    QCOMPARE(finished.count(), 1);
    QVERIFY(finished.at(0).at(0).toString().contains(QStringLiteral("note")));

    const auto& mappings = project.bankSets[0].banks[0].cells[0].propertyMappings;
    QCOMPARE(mappings.size(), 1);
    QCOMPARE(mappings[0].input, InputType::MidiNote);
    QCOMPARE(mappings[0].number, 64);
}

void TestInputRouter::resolvePropertyId_mapsGrandVjTokens()
{
    using pvj::core::PropertyRegistry::resolvePropertyId;
    QCOMPARE(resolvePropertyId(QStringLiteral("TRSP")), QStringLiteral("transparency"));
    QCOMPARE(resolvePropertyId(QStringLiteral("fade")), QStringLiteral("fade"));
}

QTEST_MAIN(TestInputRouter)
#include "test_input_router.moc"

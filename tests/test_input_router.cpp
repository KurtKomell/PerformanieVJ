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
    void midiCcPropertyFader_doesNotTriggerCell();
    void midiCcCellTrigger_stillFiresWithoutPropertyMapping();
    void midiBuffer_multipleCcMessages();
    void midiThreeByteCc_parses();
    void midiAftertouch_updatesMappedProperty();
    void learnPropertyCc_holdNoteThenAftertouch();
    void learnPropertyNote_shortPressMapsNote();
    void resolvePropertyId_mapsGrandVjTokens();
    void lastContinuousValue_remembersCcAndAftertouch();
    void scaleContinuousMapping_hueIsUnipolar01();
    void scaleContinuousMapping_respectsLearnMinMax();
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

void TestInputRouter::midiCcPropertyFader_doesNotTriggerCell()
{
    Project project;
    project.initializeDefault();
    project.ensureSingleBankSet();
    auto& cell = project.bankSets[0].banks[0].cells[0];

    PropertyMapping pm;
    pm.property = QStringLiteral("feedbackInHueShift");
    pm.input    = InputType::MidiCC;
    pm.channel  = 0;
    pm.number   = 32;
    pm.minValue = -1.0;
    pm.maxValue = 1.0;
    cell.propertyMappings.append(pm);

    pvj::core::TriggerMapping trig;
    trig.input        = InputType::MidiCC;
    trig.channel      = 0;
    trig.number       = 32;
    trig.target       = pvj::core::TriggerTarget::Cell;
    trig.bankSetIndex = 0;
    trig.bankIndex    = 0;
    trig.cellIndex    = 0;
    project.triggerMappings.append(trig);

    InputRouter router;
    router.setProject(&project);

    QSignalSpy propSpy(&router, &InputRouter::propertyValueChanged);
    QSignalSpy cellSpy(&router, &InputRouter::triggerCell);

    // Cross the CC 64 threshold that would otherwise launch the cell.
    const unsigned char below[] = {0xB0, 32, 10};
    const unsigned char above[] = {0xB0, 32, 80};
    router.handleMidiBytes(below, sizeof(below));
    router.handleMidiBytes(above, sizeof(above));

    QCOMPARE(propSpy.count(), 2);
    QCOMPARE(cellSpy.count(), 0);
}

void TestInputRouter::midiCcCellTrigger_stillFiresWithoutPropertyMapping()
{
    Project project;
    project.initializeDefault();
    project.ensureSingleBankSet();

    pvj::core::TriggerMapping trig;
    trig.input        = InputType::MidiCC;
    trig.channel      = 0;
    trig.number       = 32;
    trig.target       = pvj::core::TriggerTarget::Cell;
    trig.bankSetIndex = 0;
    trig.bankIndex    = 0;
    trig.cellIndex    = 0;
    project.triggerMappings.append(trig);

    InputRouter router;
    router.setProject(&project);

    QSignalSpy cellSpy(&router, &InputRouter::triggerCell);
    const unsigned char below[] = {0xB0, 32, 10};
    const unsigned char above[] = {0xB0, 32, 80};
    router.handleMidiBytes(below, sizeof(below));
    router.handleMidiBytes(above, sizeof(above));

    QCOMPARE(cellSpy.count(), 1);
    QCOMPARE(cellSpy.at(0).at(2).toInt(), 0);
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

void TestInputRouter::lastContinuousValue_remembersCcAndAftertouch()
{
    Project project;
    project.initializeDefault();
    project.ensureSingleBankSet();

    InputRouter router;
    router.setProject(&project);
    QVERIFY(!router.lastContinuousValue(InputType::MidiCC, 0, 7).has_value());

    const unsigned char cc[] = {0xB0, 0x07, 0x40};
    router.handleMidiBytes(cc, sizeof(cc));
    QCOMPARE(router.lastContinuousValue(InputType::MidiCC, 0, 7).value_or(-1), 0x40);

    const unsigned char at[] = {0xA0, 0x3C, 0x55};
    router.handleMidiBytes(at, sizeof(at));
    QCOMPARE(router.lastContinuousValue(InputType::MidiAftertouch, 0, 60).value_or(-1), 0x55);
}

void TestInputRouter::scaleContinuousMapping_hueIsUnipolar01()
{
    PropertyMapping pm;
    pm.property = QStringLiteral("feedbackHueShift");
    pm.input = InputType::MidiCC;
    pm.channel = 0;
    pm.number = 30;
    pm.minValue = -1.0;
    pm.maxValue = 1.0;

    InputRouter router;
    QString prop;
    double value = -99.0;
    QVERIFY(router.scaleContinuousMapping(pm, 0, &prop, &value));
    QCOMPARE(prop.toLower(), QStringLiteral("feedbackhueshift"));
    QCOMPARE(value, 0.0);
    QVERIFY(router.scaleContinuousMapping(pm, 127, &prop, &value));
    QCOMPARE(value, 1.0);
    QVERIFY(router.scaleContinuousMapping(pm, 64, &prop, &value));
    QVERIFY(qAbs(value - (64.0 / 127.0)) < 1e-9);
}

void TestInputRouter::scaleContinuousMapping_respectsLearnMinMax()
{
    PropertyMapping pm;
    pm.property = QStringLiteral("feedbackZoom");
    pm.input = InputType::MidiCC;
    pm.channel = 0;
    pm.number = 30;
    // Stored mapping range ignored for zoom; PropertyRegistry learnMinMax wins via apply path —
    // scaleContinuousMapping uses pm.min/max unless overridden. Use explicit zoom range.
    pm.minValue = pvj::core::kFeedbackZoomMin;
    pm.maxValue = pvj::core::kFeedbackZoomMax;

    InputRouter router;
    QString prop;
    double value = -99.0;
    QVERIFY(router.scaleContinuousMapping(pm, 0, &prop, &value));
    QCOMPARE(prop.toLower(), QStringLiteral("feedbackzoom"));
    QCOMPARE(value, pvj::core::kFeedbackZoomMin);
    QVERIFY(router.scaleContinuousMapping(pm, 127, &prop, &value));
    QCOMPARE(value, pvj::core::kFeedbackZoomMax);
}

QTEST_MAIN(TestInputRouter)
#include "test_input_router.moc"

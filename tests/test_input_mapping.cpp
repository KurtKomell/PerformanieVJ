#include <QtTest/QtTest>

#include <QtMath>

#include "core/Model.h"
#include "core/FilterParamSchema.h"
#include "core/Project.h"
#include "input/InputRouter.h"

using pvj::core::InputType;
using pvj::core::Project;
using pvj::core::PropertyButtonMode;
using pvj::core::PropertyMapping;
using pvj::core::TriggerTarget;
using pvj::input::InputRouter;

class TestInputMapping : public QObject
{
    Q_OBJECT

private slots:
    void learn_cell_trigger_adds_trigger_mapping();
    void playback_cc_updates_property_mapping();
    void playback_note_toggle_emits_property_toggle();
    void playback_cc_updates_filter_property_mapping();
};

void TestInputMapping::learn_cell_trigger_adds_trigger_mapping()
{
    Project p;
    p.initializeDefault();

    InputRouter r;
    r.setProject(&p);
    r.beginLearnCellTrigger(0, 1, 2);

    const unsigned char noteOn[] = {0x95, 60, 100}; // ch 6, note 60
    r.handleMidiBytes(noteOn, sizeof(noteOn));

    QCOMPARE(p.triggerMappings.size(), 1);
    QCOMPARE(p.triggerMappings[0].input, InputType::MidiNote);
    QCOMPARE(p.triggerMappings[0].channel, 5); // 0x95 & 0x0F
    QCOMPARE(p.triggerMappings[0].number, 60);
    QCOMPARE(p.triggerMappings[0].target, TriggerTarget::Cell);
    QCOMPARE(p.triggerMappings[0].bankSetIndex, 0);
    QCOMPARE(p.triggerMappings[0].bankIndex, 1);
    QCOMPARE(p.triggerMappings[0].cellIndex, 2);
}

void TestInputMapping::playback_cc_updates_property_mapping()
{
    Project p;
    p.initializeDefault();

    auto& cell = p.bankSets[0].banks[0].cells[0];
    cell.propertyMappings.clear();
    pvj::core::PropertyMapping pm;
    pm.property = QStringLiteral("transparency");
    pm.input    = InputType::MidiCC;
    pm.channel  = 0;
    pm.number   = 7;
    pm.minValue = 0.0;
    pm.maxValue = 1.0;
    cell.propertyMappings.append(pm);

    InputRouter r;
    r.setProject(&p);

    bool got = false;
    double gotV = 0.0;
    QObject::connect(&r, &InputRouter::propertyValueChanged, this,
                     [&](int bs, int bk, int ci, const QString& prop, double v) {
                         got = true;
                         QCOMPARE(bs, 0);
                         QCOMPARE(bk, 0);
                         QCOMPARE(ci, 0);
                         QCOMPARE(prop, QStringLiteral("transparency"));
                         gotV = v;
                     });

    const unsigned char cc[] = {0xB0, 7, 127}; // CC 7 full
    r.handleMidiBytes(cc, sizeof(cc));

    QVERIFY(got);
    QVERIFY(qAbs(gotV - 1.0) < 1e-9);
}

void TestInputMapping::playback_note_toggle_emits_property_toggle()
{
    Project p;
    p.initializeDefault();

    auto& cell = p.bankSets[0].banks[0].cells[0];
    cell.propertyMappings.clear();
    PropertyMapping pm;
    pm.property    = QStringLiteral("clipPaused");
    pm.input       = InputType::MidiNote;
    pm.channel     = 0;
    pm.number      = 50;
    pm.buttonMode  = PropertyButtonMode::Toggle;
    cell.propertyMappings.append(pm);

    InputRouter r;
    r.setProject(&p);

    bool got = false;
    QObject::connect(&r, &InputRouter::propertyToggleRequested, this,
                     [&](int bs, int bk, int ci, const QString& prop) {
                         got = true;
                         QCOMPARE(bs, 0);
                         QCOMPARE(bk, 0);
                         QCOMPARE(ci, 0);
                         QCOMPARE(prop, QStringLiteral("clipPaused"));
                     });

    const unsigned char noteOn[] = {0x90, 50, 100}; // ch 1, note 50
    r.handleMidiBytes(noteOn, sizeof(noteOn));

    QVERIFY(got);
}

void TestInputMapping::playback_cc_updates_filter_property_mapping()
{
    Project p;
    p.initializeDefault();

    auto& cell = p.bankSets[0].banks[0].cells[0];
    cell.filterChain.clear();
    pvj::core::CellFilterNode node;
    node.id = QUuid::createUuid();
    node.typeId = QStringLiteral("blur");
    node.params = pvj::core::defaultParamsFor(node.typeId);
    cell.filterChain.append(node);

    cell.propertyMappings.clear();
    pvj::core::PropertyMapping pm;
    pm.property = QStringLiteral("filter.%1.amount").arg(node.id.toString(QUuid::WithoutBraces));
    pm.input = InputType::MidiCC;
    pm.channel = 0;
    pm.number = 18;
    pm.minValue = 0.0;
    pm.maxValue = 1.0;
    cell.propertyMappings.append(pm);

    InputRouter r;
    r.setProject(&p);

    bool got = false;
    QObject::connect(&r, &InputRouter::propertyValueChanged, this,
                     [&](int, int, int, const QString& prop, double value) {
                         got = true;
                         QCOMPARE(prop.toLower(), pm.property.toLower());
                         QVERIFY(value >= 0.0 && value <= 1.0);
                     });
    const unsigned char cc[] = {0xB0, 18, 96};
    r.handleMidiBytes(cc, sizeof(cc));
    QVERIFY(got);
}

QTEST_MAIN(TestInputMapping)
#include "test_input_mapping.moc"

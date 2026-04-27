#pragma once

#include "InputTypes.h"

#include "core/Model.h"

#include <QHash>
#include <QObject>
#include <QString>

namespace pvj::core {
class Project;
}

namespace pvj::input {

/// Matches keyboard / MIDI against `Project::triggerMappings` and per-cell
/// `PropertyMapping` lists, and handles learn-mode capture for persistence.
class InputRouter final : public QObject
{
    Q_OBJECT
public:
    explicit InputRouter(QObject* parent = nullptr);

    void setProject(pvj::core::Project* project);

    LearnKind learnKind() const { return m_learnKind; }
    QString   learnPropertyName() const { return m_learnProperty; }

    void cancelLearn();
    void beginLearnCellTrigger(int bankSetIndex, int bankIndex, int cellIndex);
    void beginLearnPropertyCc(int bankSetIndex, int bankIndex, int cellIndex,
                              const QString& propertyName);
    void beginLearnPropertyNote(int bankSetIndex, int bankIndex, int cellIndex,
                                const QString& propertyName, core::PropertyButtonMode mode,
                                double buttonValue = 1.0);
    /// Learn a MIDI note for bank navigation (buttons only; keyboard ignored).
    void beginLearnBankNav(core::TriggerTarget target, int bankSetIndex, int bankIndex = 0);

    /// Remove all MIDI mappings for one property on the given cell.
    void clearPropertyMappingForCell(int bankSetIndex, int bankIndex, int cellIndex,
                                     const QString& propertyName);

    void handleMidiBytes(const unsigned char* data, size_t len);
    /// Returns true if the key event should be swallowed (learn capture).
    bool handleKeyEvent(int qtKey, int keyboardModifiers, bool press);

signals:
    /// Fixed mapping: MIDI channel 16 (index 15), notes 60–65 → mix slots 0–5.
    void mixLayerDirect(int slotIndex);

    void triggerCell(int bankSetIndex, int bankIndex, int cellIndex);
    void bankNext(int bankSetIndex);
    void bankPrev(int bankSetIndex);
    void bankSelect(int bankSetIndex, int bankIndex);
    void propertyValueChanged(int bankSetIndex, int bankIndex, int cellIndex,
                              const QString& propertyName, double value);
    void propertyToggleRequested(int bankSetIndex, int bankIndex, int cellIndex,
                                 const QString& propertyName);

    void learnFinished(const QString& message);
    void learnCancelled();

private:
    static QString keyTextFromQt(int qtKey, int keyboardModifiers);
    void dispatchLearn(const InputEvent& ev);
    void applyLearnCellTrigger(const InputEvent& ev);
    void applyLearnPropertyCc(const InputEvent& ev);
    void applyLearnPropertyNote(const InputEvent& ev);
    void applyLearnBankNav(const InputEvent& ev);
    void removeConflictingTriggers(const core::TriggerMapping& except);
    void removeConflictingPropertyMapping(int bankSetIndex, int bankIndex, int cellIndex,
                                          const QString& property, const core::PropertyMapping& except);
    void removeAllPropertyMappingsForProperty(int bankSetIndex, int bankIndex, int cellIndex,
                                              const QString& propertyName);

    void dispatchPlayback(const InputEvent& ev);
    bool matchTrigger(const core::TriggerMapping& t, const InputEvent& ev) const;
    double scaleCcToProperty(double normalized01, double minV, double maxV) const;

    pvj::core::Project* m_project = nullptr;
    LearnKind           m_learnKind = LearnKind::None;
    int                 m_learnBankSet = 0;
    int                 m_learnBank    = 0;
    int                 m_learnCell    = 0;
    QString             m_learnProperty;
    core::PropertyButtonMode m_learnButtonMode = core::PropertyButtonMode::Continuous;
    double              m_learnButtonValue = 1.0;

    core::TriggerTarget m_learnBankNavTarget = core::TriggerTarget::BankNext;
    int                 m_learnBankNavSet    = 0;
    int                 m_learnBankNavBankIdx  = 0;

    /// Last CC values for edge detection when TRIGGERMAPPINGS use MidiCC → cell.
    QHash<quint32, int> m_lastCcValue;
};

} // namespace pvj::input

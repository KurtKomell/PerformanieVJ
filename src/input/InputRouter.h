#pragma once

#include "InputTypes.h"

#include "core/Model.h"

#include <QElapsedTimer>
#include <QHash>
#include <QKeyCombination>
#include <QKeySequence>
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
    bool handleKeyEvent(QKeyCombination combo, bool press);

    void assignKeyboardTriggerForCell(int bankSetIndex, int bankIndex, int cellIndex,
                                      const QString& keyText, int qtKey = 0);

    /// Portable `QKeySequence` text for persistence / matching (empty when invalid).
    static QString portableTextFromCombination(const QKeyCombination& combo);
    static QString portableTextFromSequence(const QKeySequence& seq);
    static bool    isUsableCellKeyboardKeyText(const QString& keyText);
    /// True for standalone modifier keys (Ctrl/Shift/Alt/Meta alone).
    static bool    isModifierOnlyKey(const QKeyCombination& combo);

signals:
    /// Fixed mapping: MIDI channel 16 (index 15), notes 60–65 → mix slots 0–5.
    void mixLayerDirect(int slotIndex);

    void triggerCell(int bankSetIndex, int bankIndex, int cellIndex, bool fromMidiNote);
    /// MIDI note-off for a mapped cell slot (momentary pads).
    void releaseCell(int bankSetIndex, int bankIndex, int cellIndex);
    void bankNext(int bankSetIndex);
    void bankPrev(int bankSetIndex);
    void bankSelect(int bankSetIndex, int bankIndex);
    void propertyValueChanged(int bankSetIndex, int bankIndex, int cellIndex,
                              const QString& propertyName, double value);
    void propertyToggleRequested(int bankSetIndex, int bankIndex, int cellIndex,
                                 const QString& propertyName);

    void learnFinished(const QString& message);
    /// Non-terminal hint during learn (wrong message type, etc.).
    void learnHint(const QString& message);
    void learnCancelled();
    void triggerMappingsChanged();
    void propertyMappingsChanged();

private:
    static QString keyTextFromQt(int qtKey, int keyboardModifiers);
    void dispatchLearn(const InputEvent& ev);
    void applyLearnCellTrigger(const InputEvent& ev);
    void applyLearnPropertyCc(const InputEvent& ev);
    void applyLearnPropertyNote(const InputEvent& ev);
    void applyLearnPropertyAftertouch(int channel, int note, int pressure);
    void applyLearnBankNav(const InputEvent& ev);
    void tryBeginLearnNotePending(const InputEvent& ev);
    void onLearnNoteReleased(int channel, int note);
    void clearLearnPendingNote();
    void emitLearnTypeMismatch(const InputEvent& ev, const QString& expected);
    void removeConflictingTriggers(const core::TriggerMapping& except);
    void removeConflictingPropertyMapping(int bankSetIndex, int bankIndex, int cellIndex,
                                          const QString& property, const core::PropertyMapping& except);
    void removeAllPropertyMappingsForProperty(int bankSetIndex, int bankIndex, int cellIndex,
                                              const QString& propertyName);

    bool dispatchPlayback(const InputEvent& ev);
    void dispatchCellNoteReleased(int channel, int note);
    static void midiNoteReleasedThunk(void* ctx, int channel, int note);
    void clearMidiNoteDown(int channel, int note);
    bool matchTrigger(const core::TriggerMapping& t, const InputEvent& ev) const;
    static bool keyboardTriggersMatch(const QString& stored, const QString& incoming);
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
    /// Note-on latch per ch+note (cleared on note-off) so cell triggers fire once per press.
    QHash<quint32, bool> m_midiNoteDown;

    unsigned char m_runningStatus = 0;

    /// During property CC/note learn: wait for hold or aftertouch before committing.
    bool          m_learnPendingNoteActive = false;
    int           m_learnPendingChannel    = 0;
    int           m_learnPendingNumber     = 0;
    QElapsedTimer m_learnPendingTimer;
};

} // namespace pvj::input

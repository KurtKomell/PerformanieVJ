#include "InputRouter.h"

#include "core/Model.h"
#include "core/Project.h"
#include "core/PropertyRegistry.h"

#include <QKeyCombination>
#include <QKeySequence>
#include <QtGlobal>
#include <cmath>

namespace pvj::input {

namespace {

/// Global mix-slot shortcuts (handled before project trigger mappings).
constexpr int kMixLayerDirectMidiChannel = 15; // MIDI channel 16 (zero-based)
constexpr int kMixLayerDirectMidiNoteBase = 60; // C4 → slot 0 … 72 → slot 12
constexpr int kMixLayerDirectCount        = 13;

/// Hold a note this long during property learn → map poly aftertouch instead of a short note tap.
constexpr int kAftertouchLearnHoldMs = 500;

constexpr quint32 ccKey(int channel, int ccNumber)
{
    return (quint32(channel & 0x1F) << 8) | quint32(ccNumber & 0x7F);
}

bool sameTriggerBinding(const pvj::core::TriggerMapping& a, const pvj::core::TriggerMapping& b)
{
    if (a.input != b.input) {
        return false;
    }
    if (a.input == pvj::core::InputType::Key) {
        return a.keyText == b.keyText;
    }
    return a.channel == b.channel && a.number == b.number;
}

using NoteReleasedFn = void (*)(void* ctx, int channel, int note);

/// Parse one MIDI message from `data`. On success sets `*consumed` and fills `out`.
/// System-realtime bytes (0xF8–0xFF) advance `consumed` by 1 and return false.
/// Invokes `onNoteReleased` for note-off and note-on-with-velocity-zero (running status).
bool tryParseOneMidiMessage(const unsigned char* data, size_t len, size_t* consumed, InputEvent* out,
                            unsigned char* runningStatus, void* noteReleaseCtx,
                            NoteReleasedFn onNoteReleased)
{
    if (!data || len < 1 || !consumed || !out || !runningStatus) {
        return false;
    }
    *consumed = 0;

    unsigned char status = data[0];
    int           dataOffset = 1;

    if (status < 0x80) {
        if (*runningStatus == 0) {
            *consumed = 1;
            return false;
        }
        status     = *runningStatus;
        dataOffset = 0;
    } else if (status >= 0xF8) {
        *consumed = 1;
        return false;
    } else {
        const unsigned char high = status & 0xF0;
        if (high == 0x80 || high == 0x90 || high == 0xA0 || high == 0xB0 || high == 0xE0) {
            *runningStatus = status;
        }
    }

  // Data bytes after status (or after implicit status in running-status mode):
  // program change / channel pressure = 1; note/CC/pitch = 2.
    const unsigned char statusHigh = status & 0xF0;
    const int           dataByteCount =
        (statusHigh == 0xC0 || statusHigh == 0xD0) ? 1 : 2;
    if (int(len) - dataOffset < dataByteCount) {
        return false;
    }

    *consumed = size_t(dataOffset + dataByteCount);
    const int channel = status & 0x0F;

    if ((status & 0xF0) == 0xB0) {
        const int cc    = int(data[dataOffset]) & 0x7F;
        const int value = int(data[dataOffset + 1]) & 0x7F;
        out->type       = pvj::core::InputType::MidiCC;
        out->channel    = channel;
        out->number     = cc;
        out->value      = value;
        return true;
    }
    if ((status & 0xF0) == 0xA0) {
        const int note     = int(data[dataOffset]) & 0x7F;
        const int pressure = int(data[dataOffset + 1]) & 0x7F;
        out->type          = pvj::core::InputType::MidiAftertouch;
        out->channel       = channel;
        out->number        = note;
        out->value         = pressure;
        return true;
    }
    if ((status & 0xF0) == 0x90) {
        const int note = int(data[dataOffset]) & 0x7F;
        const int vel  = int(data[dataOffset + 1]) & 0x7F;
        if (vel == 0) {
            if (onNoteReleased) {
                onNoteReleased(noteReleaseCtx, channel, note);
            }
            return false;
        }
        out->type    = pvj::core::InputType::MidiNote;
        out->channel = channel;
        out->number  = note;
        out->value   = vel;
        return true;
    }
    // Note-off (0x80): consume but do not route — release clears latch for next press.
    if ((status & 0xF0) == 0x80) {
        const int note = int(data[dataOffset]) & 0x7F;
        if (onNoteReleased) {
            onNoteReleased(noteReleaseCtx, channel, note);
        }
        return false;
    }

    return false;
}

} // namespace

void InputRouter::midiNoteReleasedThunk(void* ctx, int channel, int note)
{
    auto* router = static_cast<InputRouter*>(ctx);
    if (!router) {
        return;
    }
    router->clearMidiNoteDown(channel, note);
    if (router->m_learnKind == LearnKind::PropertyCc
        || router->m_learnKind == LearnKind::PropertyNote) {
        router->onLearnNoteReleased(channel, note);
    }
    router->dispatchCellNoteReleased(channel, note);
}

void InputRouter::dispatchCellNoteReleased(int channel, int note)
{
    if (!m_project || m_learnKind != LearnKind::None) {
        return;
    }
    for (const auto& t : m_project->triggerMappings) {
        if (t.target != pvj::core::TriggerTarget::Cell) {
            continue;
        }
        if (t.input != pvj::core::InputType::MidiNote) {
            continue;
        }
        if (t.channel != channel || t.number != note) {
            continue;
        }
        emit releaseCell(t.bankSetIndex, t.bankIndex, t.cellIndex);
        return;
    }
}

void InputRouter::clearMidiNoteDown(int channel, int note)
{
    m_midiNoteDown.remove(ccKey(channel, note));
}

InputRouter::InputRouter(QObject* parent)
    : QObject(parent)
{
}

void InputRouter::setProject(pvj::core::Project* project)
{
    m_project = project;
}

void InputRouter::clearLearnPendingNote()
{
    m_learnPendingNoteActive = false;
    m_learnPendingChannel    = 0;
    m_learnPendingNumber     = 0;
}

void InputRouter::cancelLearn()
{
    if (m_learnKind == LearnKind::None) {
        return;
    }
    clearLearnPendingNote();
    m_learnKind = LearnKind::None;
    m_learnProperty.clear();
    m_learnButtonMode  = pvj::core::PropertyButtonMode::Continuous;
    m_learnButtonValue = 1.0;
    m_learnBankNavTarget = pvj::core::TriggerTarget::BankNext;
    m_learnBankNavSet    = 0;
    m_learnBankNavBankIdx = 0;
    emit learnCancelled();
}

void InputRouter::beginLearnCellTrigger(int bankSetIndex, int bankIndex, int cellIndex)
{
    clearLearnPendingNote();
    m_learnKind     = LearnKind::CellTrigger;
    m_learnBankSet  = bankSetIndex;
    m_learnBank     = bankIndex;
    m_learnCell     = cellIndex;
    m_learnProperty.clear();
}

void InputRouter::beginLearnPropertyCc(int bankSetIndex, int bankIndex, int cellIndex,
                                       const QString& propertyName)
{
    clearLearnPendingNote();
    m_learnKind     = LearnKind::PropertyCc;
    m_learnBankSet  = bankSetIndex;
    m_learnBank     = bankIndex;
    m_learnCell     = cellIndex;
    m_learnProperty = propertyName;
    m_learnButtonMode  = pvj::core::PropertyButtonMode::Continuous;
    m_learnButtonValue = 1.0;
}

void InputRouter::beginLearnPropertyNote(int bankSetIndex, int bankIndex, int cellIndex,
                                         const QString& propertyName, core::PropertyButtonMode mode,
                                         double buttonValue)
{
    clearLearnPendingNote();
    m_learnKind        = LearnKind::PropertyNote;
    m_learnBankSet     = bankSetIndex;
    m_learnBank        = bankIndex;
    m_learnCell        = cellIndex;
    m_learnProperty    = propertyName;
    m_learnButtonMode  = mode;
    m_learnButtonValue = buttonValue;
}

void InputRouter::beginLearnBankNav(pvj::core::TriggerTarget target, int bankSetIndex, int bankIndex)
{
    if (target == pvj::core::TriggerTarget::BankSetSwitch) {
        return;
    }
    clearLearnPendingNote();
    m_learnKind     = LearnKind::BankNav;
    m_learnBankNavTarget = target;
    m_learnBankNavSet    = bankSetIndex;
    m_learnBankNavBankIdx = bankIndex;
}

QString InputRouter::portableTextFromCombination(const QKeyCombination& combo)
{
    if (combo.key() == Qt::Key_unknown) {
        return {};
    }
    const QString text = QKeySequence(combo).toString(QKeySequence::PortableText);
    if (!text.isEmpty()) {
        return text;
    }
    return {};
}

QString InputRouter::portableTextFromSequence(const QKeySequence& seq)
{
    if (seq.isEmpty()) {
        return {};
    }
    const QString whole = seq.toString(QKeySequence::PortableText);
    if (!whole.isEmpty()) {
        return whole;
    }
    for (int i = 0; i < int(seq.count()); ++i) {
        const QString part = portableTextFromCombination(seq[i]);
        if (!part.isEmpty()) {
            return part;
        }
    }
    return {};
}

bool InputRouter::isUsableCellKeyboardKeyText(const QString& keyText)
{
    return !keyText.trimmed().isEmpty();
}

bool InputRouter::isModifierOnlyKey(const QKeyCombination& combo)
{
    switch (combo.key()) {
    case Qt::Key_Control:
    case Qt::Key_Shift:
    case Qt::Key_Alt:
    case Qt::Key_Meta:
    case Qt::Key_AltGr:
        return true;
    default:
        return false;
    }
}

QString InputRouter::keyTextFromQt(int qtKey, int keyboardModifiers)
{
    if (qtKey == 0 || qtKey == int(Qt::Key_unknown)) {
        return {};
    }
    return portableTextFromCombination(QKeyCombination::fromCombined(qtKey | keyboardModifiers));
}

bool InputRouter::handleKeyEvent(QKeyCombination combo, bool press)
{
    if (!press) {
        return false;
    }
    if (combo.key() == Qt::Key_unknown) {
        return false;
    }

    InputEvent ev;
    ev.type              = pvj::core::InputType::Key;
    ev.qtKey             = int(combo.key());
    ev.keyboardModifiers = int(combo.keyboardModifiers().toInt());
    ev.keyText           = portableTextFromCombination(combo);

    if (m_learnKind != LearnKind::None) {
        if (m_learnKind == LearnKind::PropertyCc || m_learnKind == LearnKind::PropertyNote
            || m_learnKind == LearnKind::BankNav) {
            return false;
        }
        dispatchLearn(ev);
        return true;
    }
    return dispatchPlayback(ev);
}

bool InputRouter::handleKeyEvent(int qtKey, int keyboardModifiers, bool press)
{
    if (!press || qtKey == 0) {
        return false;
    }
    return handleKeyEvent(QKeyCombination::fromCombined(qtKey | keyboardModifiers), press);
}

void InputRouter::emitLearnTypeMismatch(const InputEvent& ev, const QString& expected)
{
    QString got = tr("unknown MIDI");
    if (ev.type == pvj::core::InputType::MidiCC) {
        got = tr("CC ch%1 #%2").arg(ev.channel + 1).arg(ev.number);
    } else if (ev.type == pvj::core::InputType::MidiNote) {
        got = tr("note ch%1 #%2").arg(ev.channel + 1).arg(ev.number);
    } else if (ev.type == pvj::core::InputType::MidiAftertouch) {
        got = tr("aftertouch ch%1 note%2").arg(ev.channel + 1).arg(ev.number);
    }
    emit learnHint(tr("Received %1 — expected %2.").arg(got, expected));
}

void InputRouter::handleMidiBytes(const unsigned char* data, size_t len)
{
    if (!data || len < 1) {
        return;
    }

    size_t offset = 0;
    while (offset < len) {
        InputEvent ev;
        size_t     consumed = 0;
        if (!tryParseOneMidiMessage(data + offset, len - offset, &consumed, &ev, &m_runningStatus,
                                    this, &InputRouter::midiNoteReleasedThunk)) {
            if (consumed == 0) {
                break;
            }
            offset += consumed;
            continue;
        }
        offset += consumed;

        if (m_learnKind != LearnKind::None) {
            if (!m_project) {
                return;
            }
            dispatchLearn(ev);
            continue;
        }

        if (!m_project) {
            return;
        }
        dispatchPlayback(ev);
    }
}

void InputRouter::tryBeginLearnNotePending(const InputEvent& ev)
{
    if (ev.type != pvj::core::InputType::MidiNote) {
        return;
    }
    m_learnPendingNoteActive = true;
    m_learnPendingChannel    = ev.channel;
    m_learnPendingNumber     = ev.number;
    m_learnPendingTimer.restart();
    if (m_learnKind == LearnKind::PropertyNote) {
        emit learnHint(tr("Release quickly for a note button, or hold / press harder for aftertouch."));
    } else {
        emit learnHint(tr("Hold the note and press harder for aftertouch, or move a knob for CC."));
    }
}

void InputRouter::onLearnNoteReleased(int channel, int note)
{
    if (!m_learnPendingNoteActive || channel != m_learnPendingChannel
        || note != m_learnPendingNumber) {
        return;
    }
    const bool heldLong =
        m_learnPendingTimer.isValid() && m_learnPendingTimer.elapsed() >= kAftertouchLearnHoldMs;
    clearLearnPendingNote();
    if (heldLong) {
        applyLearnPropertyAftertouch(channel, note, 0);
        return;
    }
    if (m_learnKind == LearnKind::PropertyNote) {
        InputEvent ev;
        ev.type    = pvj::core::InputType::MidiNote;
        ev.channel = channel;
        ev.number  = note;
        ev.value   = 127;
        applyLearnPropertyNote(ev);
        return;
    }
    emit learnHint(tr("Hold the note longer for aftertouch, or move a MIDI CC knob."));
}

void InputRouter::dispatchLearn(const InputEvent& ev)
{
    if (!m_project) {
        cancelLearn();
        return;
    }

    if (m_learnKind == LearnKind::PropertyCc || m_learnKind == LearnKind::PropertyNote) {
        if (ev.type == pvj::core::InputType::MidiAftertouch) {
            applyLearnPropertyAftertouch(ev.channel, ev.number, ev.value);
            clearLearnPendingNote();
            return;
        }
        if (ev.type == pvj::core::InputType::MidiNote) {
            tryBeginLearnNotePending(ev);
            return;
        }
        if (m_learnKind == LearnKind::PropertyCc) {
            if (ev.type == pvj::core::InputType::MidiCC) {
                clearLearnPendingNote();
                applyLearnPropertyCc(ev);
                return;
            }
            return;
        }
        if (ev.type == pvj::core::InputType::MidiCC) {
            emitLearnTypeMismatch(ev, tr("a MIDI note (press or hold a pad)"));
        }
        return;
    }

    switch (m_learnKind) {
    case LearnKind::CellTrigger:
        applyLearnCellTrigger(ev);
        break;
    case LearnKind::PropertyCc:
    case LearnKind::PropertyNote:
        break;
    case LearnKind::BankNav:
        applyLearnBankNav(ev);
        break;
    case LearnKind::None:
        break;
    }
}

void InputRouter::applyLearnCellTrigger(const InputEvent& ev)
{
    if (ev.type != pvj::core::InputType::MidiNote && ev.type != pvj::core::InputType::Key) {
        if (ev.type == pvj::core::InputType::MidiCC) {
            emitLearnTypeMismatch(ev, tr("a MIDI note or keyboard key"));
        }
        return;
    }

    if (!m_project) {
        return;
    }

    if (ev.type == pvj::core::InputType::Key) {
        const QKeyCombination combo = QKeyCombination::fromCombined(ev.qtKey | ev.keyboardModifiers);
        if (isModifierOnlyKey(combo) || !isUsableCellKeyboardKeyText(ev.keyText)) {
            // Keep learn mode active; do not emit learnFinished (that commits/clears undo baseline).
            emit learnHint(tr("Key is empty — press a letter, number, or function key "
                              "(modifier keys alone are not supported)."));
            return;
        }
        m_project->setKeyboardTriggerForCell(m_learnBankSet, m_learnBank, m_learnCell, ev.keyText,
                                             ev.qtKey);
        m_learnKind = LearnKind::None;
        emit triggerMappingsChanged();
        emit learnFinished(tr("Mapped %1 to cell %2 (all banks)")
                               .arg(ev.keyText)
                               .arg(m_learnCell + 1));
        return;
    }

    pvj::core::TriggerMapping t;
    t.input        = ev.type;
    t.channel      = ev.channel;
    t.number       = ev.number;
    t.keyText.clear();
    t.target       = pvj::core::TriggerTarget::Cell;
    t.bankSetIndex = m_learnBankSet;
    t.bankIndex    = pvj::core::kBankIndexAllBanks;
    t.cellIndex    = m_learnCell;

    removeConflictingTriggers(t);
    m_project->setMidiCellTriggerForCell(m_learnBankSet, m_learnCell, ev.channel, ev.number);

    m_learnKind = LearnKind::None;
    emit triggerMappingsChanged();
    emit learnFinished(tr("Mapped %1 to cell %2 (all banks)")
                       .arg(tr("MIDI note ch%1 note%2").arg(ev.channel + 1).arg(ev.number))
                       .arg(m_learnCell + 1));
}

void InputRouter::assignKeyboardTriggerForCell(int bankSetIndex, int bankIndex, int cellIndex,
                                               const QString& keyText, int qtKey)
{
    if (!m_project) {
        return;
    }
    if (!keyText.isEmpty() && !isUsableCellKeyboardKeyText(keyText)) {
        return;
    }
    m_project->setKeyboardTriggerForCell(bankSetIndex, bankIndex, cellIndex, keyText, qtKey);
    emit triggerMappingsChanged();
}

void InputRouter::applyLearnPropertyCc(const InputEvent& ev)
{
    if (ev.type != pvj::core::InputType::MidiCC) {
        if (ev.type == pvj::core::InputType::MidiNote) {
            emitLearnTypeMismatch(ev, tr("a MIDI CC (move a knob or fader)"));
        }
        return;
    }
    if (m_learnBankSet < 0 || m_learnBankSet >= m_project->bankSets.size()) {
        cancelLearn();
        return;
    }
    auto& set = m_project->bankSets[m_learnBankSet];
    if (m_learnBank < 0 || m_learnBank >= set.banks.size()) {
        cancelLearn();
        return;
    }
    auto& bank = set.banks[m_learnBank];
    if (m_learnCell < 0 || m_learnCell >= bank.cells.size()) {
        cancelLearn();
        return;
    }

    pvj::core::PropertyMapping m;
    m.property = m_learnProperty;
    m.input    = pvj::core::InputType::MidiCC;
    m.channel  = ev.channel;
    m.number   = ev.number;
    pvj::core::PropertyRegistry::learnMinMax(m_learnProperty, &m.minValue, &m.maxValue);
    m.buttonMode  = pvj::core::PropertyButtonMode::Continuous;
    m.buttonValue = 1.0;

    removeAllPropertyMappingsForProperty(m_learnBankSet, m_learnBank, m_learnCell, m_learnProperty);

    bank.cells[m_learnCell].propertyMappings.append(m);

    const int savedBankSet = m_learnBankSet;
    const int savedBank    = m_learnBank;
    const int savedCell    = m_learnCell;
    const QString savedProp = m.property;

    m_learnKind = LearnKind::None;
    m_learnProperty.clear();

    emit propertyMappingsChanged();
    emit learnFinished(tr("Mapped MIDI CC ch%1 cc%2 → %3")
                       .arg(ev.channel + 1)
                       .arg(ev.number)
                       .arg(savedProp));

    const double n = ev.value / 127.0;
    const double scaled = scaleCcToProperty(n, m.minValue, m.maxValue);
    const QString prop = pvj::core::PropertyRegistry::resolvePropertyId(savedProp);
    if (pvj::core::PropertyRegistry::kindOf(prop) == pvj::core::PropertyRegistry::Kind::Enum) {
        const double span = m.maxValue - m.minValue;
        const double n01  = span > 1e-9 ? (scaled - m.minValue) / span : 0.0;
        emit propertyValueChanged(savedBankSet, savedBank, savedCell, prop, n01);
    } else {
        emit propertyValueChanged(savedBankSet, savedBank, savedCell, prop, scaled);
    }
}

void InputRouter::applyLearnPropertyNote(const InputEvent& ev)
{
    if (ev.type != pvj::core::InputType::MidiNote) {
        if (ev.type == pvj::core::InputType::MidiCC) {
            emitLearnTypeMismatch(ev, tr("a MIDI note (press a pad or key)"));
        }
        return;
    }
    if (m_learnBankSet < 0 || m_learnBankSet >= m_project->bankSets.size()) {
        cancelLearn();
        return;
    }
    auto& set = m_project->bankSets[m_learnBankSet];
    if (m_learnBank < 0 || m_learnBank >= set.banks.size()) {
        cancelLearn();
        return;
    }
    auto& bank = set.banks[m_learnBank];
    if (m_learnCell < 0 || m_learnCell >= bank.cells.size()) {
        cancelLearn();
        return;
    }

    pvj::core::PropertyMapping m;
    m.property    = m_learnProperty;
    m.input       = pvj::core::InputType::MidiNote;
    m.channel     = ev.channel;
    m.number      = ev.number;
    m.minValue    = 0.0;
    m.maxValue    = 1.0;
    m.buttonMode  = m_learnButtonMode;
    m.buttonValue = m_learnButtonValue;

    removeAllPropertyMappingsForProperty(m_learnBankSet, m_learnBank, m_learnCell, m_learnProperty);

    bank.cells[m_learnCell].propertyMappings.append(m);

    m_learnKind = LearnKind::None;
    m_learnProperty.clear();
    m_learnButtonMode  = pvj::core::PropertyButtonMode::Continuous;
    m_learnButtonValue = 1.0;

    emit propertyMappingsChanged();
    emit learnFinished(tr("Mapped MIDI note ch%1 note%2 → %3")
                       .arg(ev.channel + 1)
                       .arg(ev.number)
                       .arg(m.property));
}

void InputRouter::applyLearnPropertyAftertouch(int channel, int note, int pressure)
{
    if (m_learnBankSet < 0 || m_learnBankSet >= m_project->bankSets.size()) {
        cancelLearn();
        return;
    }
    auto& set = m_project->bankSets[m_learnBankSet];
    if (m_learnBank < 0 || m_learnBank >= set.banks.size()) {
        cancelLearn();
        return;
    }
    auto& bank = set.banks[m_learnBank];
    if (m_learnCell < 0 || m_learnCell >= bank.cells.size()) {
        cancelLearn();
        return;
    }

    pvj::core::PropertyMapping m;
    m.property = m_learnProperty;
    m.input    = pvj::core::InputType::MidiAftertouch;
    m.channel  = channel;
    m.number   = note;
    pvj::core::PropertyRegistry::learnMinMax(m_learnProperty, &m.minValue, &m.maxValue);
    m.buttonMode  = pvj::core::PropertyButtonMode::Continuous;
    m.buttonValue = 1.0;

    removeAllPropertyMappingsForProperty(m_learnBankSet, m_learnBank, m_learnCell, m_learnProperty);

    bank.cells[m_learnCell].propertyMappings.append(m);

    const int savedBankSet = m_learnBankSet;
    const int savedBank    = m_learnBank;
    const int savedCell    = m_learnCell;
    const QString savedProp = m.property;

    m_learnKind = LearnKind::None;
    m_learnProperty.clear();
    m_learnButtonMode  = pvj::core::PropertyButtonMode::Continuous;
    m_learnButtonValue = 1.0;

    emit propertyMappingsChanged();
    emit learnFinished(tr("Mapped MIDI aftertouch ch%1 note%2 → %3")
                       .arg(channel + 1)
                       .arg(note)
                       .arg(savedProp));

    const double n = pressure / 127.0;
    const double scaled = scaleCcToProperty(n, m.minValue, m.maxValue);
    const QString prop = pvj::core::PropertyRegistry::resolvePropertyId(savedProp);
    if (pvj::core::PropertyRegistry::kindOf(prop) == pvj::core::PropertyRegistry::Kind::Enum) {
        const double span = m.maxValue - m.minValue;
        const double n01  = span > 1e-9 ? (scaled - m.minValue) / span : 0.0;
        emit propertyValueChanged(savedBankSet, savedBank, savedCell, prop, n01);
    } else if (pressure > 0) {
        emit propertyValueChanged(savedBankSet, savedBank, savedCell, prop, scaled);
    }
}

void InputRouter::applyLearnBankNav(const InputEvent& ev)
{
    if (ev.type != pvj::core::InputType::MidiNote) {
        if (ev.type == pvj::core::InputType::MidiCC) {
            emitLearnTypeMismatch(ev, tr("a MIDI note"));
        }
        return;
    }
    if (!m_project) {
        cancelLearn();
        return;
    }

    pvj::core::TriggerMapping t;
    t.input        = pvj::core::InputType::MidiNote;
    t.channel      = ev.channel;
    t.number       = ev.number;
    t.keyText.clear();
    t.target       = m_learnBankNavTarget;
    t.bankSetIndex = m_learnBankNavSet;
    t.bankIndex    = (m_learnBankNavTarget == pvj::core::TriggerTarget::BankSelect) ? m_learnBankNavBankIdx
                                                                                     : 0;
    t.cellIndex    = 0;

    removeConflictingTriggers(t);
    m_project->triggerMappings.append(t);

    m_learnKind = LearnKind::None;

    QString what;
    switch (m_learnBankNavTarget) {
    case pvj::core::TriggerTarget::BankNext:
        what = tr("bank next");
        break;
    case pvj::core::TriggerTarget::BankPrev:
        what = tr("bank previous");
        break;
    case pvj::core::TriggerTarget::BankSelect:
        what = tr("bank select %1").arg(m_learnBankNavBankIdx + 1);
        break;
    default:
        what = tr("bank action");
        break;
    }

    emit learnFinished(tr("Mapped MIDI note ch%1 note%2 → %3")
                       .arg(ev.channel + 1)
                       .arg(ev.number)
                       .arg(what));
}

void InputRouter::removeAllPropertyMappingsForProperty(int bankSetIndex, int bankIndex, int cellIndex,
                                                       const QString& propertyName)
{
    if (!m_project) {
        return;
    }
    if (bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    auto& bank = m_project->bankSets[bankSetIndex].banks[bankIndex];
    if (cellIndex < 0 || cellIndex >= bank.cells.size()) {
        return;
    }
    auto& list = bank.cells[cellIndex].propertyMappings;
    QList<pvj::core::PropertyMapping> kept;
    for (const auto& m : list) {
        if (m.property == propertyName) {
            continue;
        }
        kept.append(m);
    }
    list = std::move(kept);
}

void InputRouter::clearPropertyMappingForCell(int bankSetIndex, int bankIndex, int cellIndex,
                                              const QString& propertyName)
{
    removeAllPropertyMappingsForProperty(bankSetIndex, bankIndex, cellIndex, propertyName);
    emit propertyMappingsChanged();
}

void InputRouter::removeConflictingTriggers(const pvj::core::TriggerMapping& except)
{
    if (!m_project) {
        return;
    }
    QList<pvj::core::TriggerMapping> kept;
    kept.reserve(m_project->triggerMappings.size());
    for (const auto& t : m_project->triggerMappings) {
        if (sameTriggerBinding(t, except)) {
            continue;
        }
        kept.append(t);
    }
    m_project->triggerMappings = std::move(kept);
}

void InputRouter::removeConflictingPropertyMapping(int bankSetIndex, int bankIndex, int cellIndex,
                                                   const QString& property,
                                                   const pvj::core::PropertyMapping& except)
{
    if (!m_project) {
        return;
    }
    if (bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    auto& bank = m_project->bankSets[bankSetIndex].banks[bankIndex];
    if (cellIndex < 0 || cellIndex >= bank.cells.size()) {
        return;
    }
    auto& list = bank.cells[cellIndex].propertyMappings;
    QList<pvj::core::PropertyMapping> kept;
    for (const auto& m : list) {
        if (m.property == property
            && m.input == except.input
            && m.channel == except.channel
            && m.number == except.number) {
            continue;
        }
        kept.append(m);
    }
    list = std::move(kept);
}

double InputRouter::scaleCcToProperty(double normalized01, double minV, double maxV) const
{
    return minV + (maxV - minV) * normalized01;
}

std::optional<int> InputRouter::lastContinuousValue(pvj::core::InputType type, int channel,
                                                    int number) const
{
    const quint32 k = ccKey(channel, number);
    if (type == pvj::core::InputType::MidiCC) {
        const auto it = m_lastCcValue.constFind(k);
        if (it == m_lastCcValue.cend()) {
            return std::nullopt;
        }
        return int(*it);
    }
    if (type == pvj::core::InputType::MidiAftertouch) {
        const auto it = m_lastAftertouchValue.constFind(k);
        if (it == m_lastAftertouchValue.cend()) {
            return std::nullopt;
        }
        return int(*it);
    }
    return std::nullopt;
}

bool InputRouter::scaleContinuousMapping(const pvj::core::PropertyMapping& pm, int raw0to127,
                                         QString* outProperty, double* outValue) const
{
    if (!outProperty || !outValue) {
        return false;
    }
    if (pm.input != pvj::core::InputType::MidiCC
        && pm.input != pvj::core::InputType::MidiAftertouch) {
        return false;
    }
    const QString prop = pvj::core::PropertyRegistry::resolvePropertyId(pm.property);
    if (prop.isEmpty()) {
        return false;
    }
    double minV = pm.minValue;
    double maxV = pm.maxValue;
    const QString pl = prop.toLower();
    if (pl == QLatin1String("feedbackinhueshift") || pl == QLatin1String("feedbackhueshift")) {
        minV = 0.0;
        maxV = 1.0;
    }
    const double n = qBound(0, raw0to127, 127) / 127.0;
    double scaled = scaleCcToProperty(n, minV, maxV);
    if (pvj::core::PropertyRegistry::kindOf(prop) == pvj::core::PropertyRegistry::Kind::Enum) {
        const double span = maxV - minV;
        scaled = span > 1e-9 ? (scaled - minV) / span : 0.0;
    }
    *outProperty = prop;
    *outValue = scaled;
    return true;
}

bool InputRouter::keyboardTriggersMatch(const QString& stored, const QString& incoming)
{
    if (stored == incoming) {
        return true;
    }
    if (stored.isEmpty() || incoming.isEmpty()) {
        return false;
    }
    const QKeySequence storedSeq(stored, QKeySequence::PortableText);
    const QKeySequence incomingSeq(incoming, QKeySequence::PortableText);
    if (!storedSeq.isEmpty() && storedSeq == incomingSeq) {
        return true;
    }
    const QString storedPortable = storedSeq.toString(QKeySequence::PortableText);
    const QString incomingPortable = incomingSeq.toString(QKeySequence::PortableText);
    return !storedPortable.isEmpty() && storedPortable == incomingPortable;
}

bool InputRouter::matchTrigger(const pvj::core::TriggerMapping& t, const InputEvent& ev) const
{
    if (t.input != ev.type) {
        return false;
    }
    if (t.input == pvj::core::InputType::Key) {
        return keyboardTriggersMatch(t.keyText, ev.keyText);
    }
    if (t.input == pvj::core::InputType::MidiNote) {
        return t.channel == ev.channel && t.number == ev.number;
    }
    if (t.input == pvj::core::InputType::MidiCC) {
        return t.channel == ev.channel && t.number == ev.number;
    }
    if (t.input == pvj::core::InputType::MidiAftertouch) {
        return t.channel == ev.channel && t.number == ev.number;
    }
    return false;
}

bool InputRouter::dispatchPlayback(const InputEvent& ev)
{
    if (ev.type == pvj::core::InputType::MidiNote
        && ev.channel == kMixLayerDirectMidiChannel
        && ev.number >= kMixLayerDirectMidiNoteBase
        && ev.number < kMixLayerDirectMidiNoteBase + kMixLayerDirectCount) {
        emit mixLayerDirect(ev.number - kMixLayerDirectMidiNoteBase);
        return true;
    }

    if (!m_project) {
        return false;
    }

    bool cellTriggered = false;

    int prevCc = -1;
    if (ev.type == pvj::core::InputType::MidiCC) {
        const quint32 k = ccKey(ev.channel, ev.number);
        prevCc = m_lastCcValue.value(k, 0);
    }

    // 1) Property mappings — CC faders / knobs / poly aftertouch (per cell)
    bool ccUsedAsPropertyFader = false;
    if (ev.type == pvj::core::InputType::MidiCC
        || ev.type == pvj::core::InputType::MidiAftertouch) {
        for (int bi = 0; bi < m_project->bankSets.size(); ++bi) {
            const auto& set = m_project->bankSets[bi];
            for (int bj = 0; bj < set.banks.size(); ++bj) {
                const auto& bank = set.banks[bj];
                for (int ci = 0; ci < bank.cells.size(); ++ci) {
                    for (const auto& pm : bank.cells[ci].propertyMappings) {
                        if (pm.input != ev.type) {
                            continue;
                        }
                        if (pm.channel != ev.channel || pm.number != ev.number) {
                            continue;
                        }
                        ccUsedAsPropertyFader = true;
                        QString prop;
                        double scaled = 0.0;
                        if (!scaleContinuousMapping(pm, ev.value, &prop, &scaled)) {
                            continue;
                        }
                        emit propertyValueChanged(bi, bj, ci, prop, scaled);
                    }
                }
            }
        }
    }

    // 1b) Property mappings — MIDI notes (toggle / set-on-press)
    if (ev.type == pvj::core::InputType::MidiNote) {
        for (int bi = 0; bi < m_project->bankSets.size(); ++bi) {
            const auto& set = m_project->bankSets[bi];
            for (int bj = 0; bj < set.banks.size(); ++bj) {
                const auto& bank = set.banks[bj];
                for (int ci = 0; ci < bank.cells.size(); ++ci) {
                    for (const auto& pm : bank.cells[ci].propertyMappings) {
                        if (pm.input != pvj::core::InputType::MidiNote) {
                            continue;
                        }
                        if (pm.channel != ev.channel || pm.number != ev.number) {
                            continue;
                        }
                        if (pm.buttonMode == pvj::core::PropertyButtonMode::Continuous) {
                            continue;
                        }
                        const QString prop =
                            pvj::core::PropertyRegistry::resolvePropertyId(pm.property);
                        if (pm.buttonMode == pvj::core::PropertyButtonMode::Toggle) {
                            emit propertyToggleRequested(bi, bj, ci, prop);
                        } else {
                            emit propertyValueChanged(bi, bj, ci, prop, pm.buttonValue);
                        }
                    }
                }
            }
        }
    }

    // 2) Trigger mappings (TRIGGERMAPPINGS)
    bool cellTriggerEmitted = false;
    for (const auto& t : m_project->triggerMappings) {
        if (!matchTrigger(t, ev)) {
            continue;
        }

        if (t.input == pvj::core::InputType::MidiCC) {
            // A CC used as a continuous fader (e.g. Input Hue) must not also launch a
            // cell when it crosses 64 — GrandVJ clip-launch CCs often collide with knobs.
            if (ccUsedAsPropertyFader && t.target == pvj::core::TriggerTarget::Cell) {
                continue;
            }
            if (!(prevCc < 64 && ev.value >= 64)) {
                continue;
            }
        }
        if (t.input == pvj::core::InputType::MidiNote && ev.value <= 0) {
            continue;
        }

        switch (t.target) {
        case pvj::core::TriggerTarget::Cell:
            if (cellTriggerEmitted) {
                break;
            }
            if (t.input == pvj::core::InputType::MidiNote) {
                const quint32 noteKey = ccKey(ev.channel, ev.number);
                if (m_midiNoteDown.contains(noteKey)) {
                    break;
                }
                m_midiNoteDown.insert(noteKey, true);
            }
            emit triggerCell(t.bankSetIndex, t.bankIndex, t.cellIndex,
                             t.input == pvj::core::InputType::MidiNote);
            cellTriggerEmitted = true;
            cellTriggered = true;
            break;
        case pvj::core::TriggerTarget::BankNext:
            emit bankNext(t.bankSetIndex);
            break;
        case pvj::core::TriggerTarget::BankPrev:
            emit bankPrev(t.bankSetIndex);
            break;
        case pvj::core::TriggerTarget::BankSelect:
            emit bankSelect(t.bankSetIndex, t.bankIndex);
            break;
        case pvj::core::TriggerTarget::BankSetSwitch:
            break;
        case pvj::core::TriggerTarget::Property:
            if (!t.propertyName.isEmpty()) {
                const QString prop =
                    pvj::core::PropertyRegistry::resolvePropertyId(t.propertyName);
                double minV = 0.0;
                double maxV = 1.0;
                pvj::core::PropertyRegistry::learnMinMax(prop, &minV, &maxV);
                double v = 1.0;
                if (t.input == pvj::core::InputType::MidiCC) {
                    v = scaleCcToProperty(ev.value / 127.0, minV, maxV);
                } else if (t.input == pvj::core::InputType::MidiNote) {
                    const double span = maxV - minV;
                    v = minV + span * (ev.value / 127.0);
                }
                if (pvj::core::PropertyRegistry::kindOf(prop)
                    == pvj::core::PropertyRegistry::Kind::Enum) {
                    const double span = maxV - minV;
                    v = span > 1e-9 ? (v - minV) / span : 0.0;
                }
                emit propertyValueChanged(t.bankSetIndex, t.bankIndex, t.cellIndex, prop, v);
            }
            break;
        }
    }

    if (ev.type == pvj::core::InputType::MidiCC) {
        const quint32 k = ccKey(ev.channel, ev.number);
        m_lastCcValue.insert(k, ev.value);
    } else if (ev.type == pvj::core::InputType::MidiAftertouch) {
        const quint32 k = ccKey(ev.channel, ev.number);
        m_lastAftertouchValue.insert(k, ev.value);
    }

    return cellTriggered;
}

} // namespace pvj::input

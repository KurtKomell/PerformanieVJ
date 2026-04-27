#include "InputRouter.h"

#include "core/Model.h"
#include "core/Project.h"
#include "core/PropertyRegistry.h"

#include <QKeySequence>
#include <cmath>

namespace pvj::input {

namespace {

/// Global mix-slot shortcuts (handled before project trigger mappings).
constexpr int kMixLayerDirectMidiChannel = 15; // MIDI channel 16 (zero-based)
constexpr int kMixLayerDirectMidiNoteBase = 60; // C4 → slot 0 … 71 → slot 11
constexpr int kMixLayerDirectCount        = 12;

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

} // namespace

InputRouter::InputRouter(QObject* parent)
    : QObject(parent)
{
}

void InputRouter::setProject(pvj::core::Project* project)
{
    m_project = project;
}

void InputRouter::cancelLearn()
{
    if (m_learnKind == LearnKind::None) {
        return;
    }
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
    m_learnKind     = LearnKind::CellTrigger;
    m_learnBankSet  = bankSetIndex;
    m_learnBank     = bankIndex;
    m_learnCell     = cellIndex;
    m_learnProperty.clear();
}

void InputRouter::beginLearnPropertyCc(int bankSetIndex, int bankIndex, int cellIndex,
                                       const QString& propertyName)
{
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
    m_learnKind     = LearnKind::BankNav;
    m_learnBankNavTarget = target;
    m_learnBankNavSet    = bankSetIndex;
    m_learnBankNavBankIdx = bankIndex;
}

QString InputRouter::keyTextFromQt(int qtKey, int keyboardModifiers)
{
    const QKeySequence seq(qtKey | keyboardModifiers);
    return seq.toString(QKeySequence::PortableText);
}

bool InputRouter::handleKeyEvent(int qtKey, int keyboardModifiers, bool press)
{
    if (!press) {
        return false;
    }
    if (qtKey == 0) {
        return false;
    }

    InputEvent ev;
    ev.type               = pvj::core::InputType::Key;
    ev.qtKey              = qtKey;
    ev.keyboardModifiers  = keyboardModifiers;
    ev.keyText            = keyTextFromQt(qtKey, keyboardModifiers);

    if (m_learnKind != LearnKind::None) {
        if (m_learnKind == LearnKind::PropertyCc || m_learnKind == LearnKind::PropertyNote
            || m_learnKind == LearnKind::BankNav) {
            return false;
        }
        dispatchLearn(ev);
        return true;
    }
    dispatchPlayback(ev);
    return false;
}

void InputRouter::handleMidiBytes(const unsigned char* data, size_t len)
{
    if (!data || len < 1 || !m_project) {
        return;
    }

    const unsigned char status = data[0];
    if (status == 0xF8 || status == 0xFE) {
        return; // clock / active sense
    }

    InputEvent ev;

    if (len >= 3 && (status & 0xF0) == 0xB0) {
        // Control change
        const int channel = status & 0x0F;
        const int cc      = int(data[1]) & 0x7F;
        const int value   = int(data[2]) & 0x7F;
        ev.type    = pvj::core::InputType::MidiCC;
        ev.channel = channel;
        ev.number  = cc;
        ev.value   = value;

        if (m_learnKind != LearnKind::None) {
            dispatchLearn(ev);
            return;
        }
        dispatchPlayback(ev);
        return;
    }

    if (len >= 3 && (status & 0xF0) == 0x90) {
        // Note on
        const int channel = status & 0x0F;
        const int note    = int(data[1]) & 0x7F;
        const int vel     = int(data[2]) & 0x7F;
        ev.type    = pvj::core::InputType::MidiNote;
        ev.channel = channel;
        ev.number  = note;
        ev.value   = vel;

        if (vel == 0) {
            // Note-on with velocity 0 = note off for routing
            return;
        }

        if (m_learnKind != LearnKind::None) {
            dispatchLearn(ev);
            return;
        }
        dispatchPlayback(ev);
        return;
    }
}

void InputRouter::dispatchLearn(const InputEvent& ev)
{
    if (!m_project) {
        cancelLearn();
        return;
    }

    switch (m_learnKind) {
    case LearnKind::CellTrigger:
        applyLearnCellTrigger(ev);
        break;
    case LearnKind::PropertyCc:
        applyLearnPropertyCc(ev);
        break;
    case LearnKind::PropertyNote:
        applyLearnPropertyNote(ev);
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
        return;
    }

    pvj::core::TriggerMapping t;
    t.input         = ev.type;
    t.channel       = (ev.type == pvj::core::InputType::Key) ? 0 : ev.channel;
    t.number        = (ev.type == pvj::core::InputType::Key) ? ev.qtKey : ev.number;
    t.keyText       = (ev.type == pvj::core::InputType::Key) ? ev.keyText : QString();
    t.target        = pvj::core::TriggerTarget::Cell;
    t.bankSetIndex  = m_learnBankSet;
    t.bankIndex     = m_learnBank;
    t.cellIndex     = m_learnCell;

    removeConflictingTriggers(t);
    m_project->triggerMappings.append(t);

    m_learnKind = LearnKind::None;
    emit learnFinished(tr("Mapped %1 to cell %2 (bank set %3, bank %4)")
                       .arg(ev.type == pvj::core::InputType::Key ? ev.keyText
                                                                  : tr("MIDI note ch%1 note%2")
                                                                    .arg(ev.channel + 1)
                                                                    .arg(ev.number))
                       .arg(m_learnCell + 1)
                       .arg(m_learnBankSet + 1)
                       .arg(m_learnBank + 1));
}

void InputRouter::applyLearnPropertyCc(const InputEvent& ev)
{
    if (ev.type != pvj::core::InputType::MidiCC) {
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

    m_learnKind = LearnKind::None;
    m_learnProperty.clear();

    emit learnFinished(tr("Mapped MIDI CC ch%1 cc%2 → %3")
                       .arg(ev.channel + 1)
                       .arg(ev.number)
                       .arg(m.property));
}

void InputRouter::applyLearnPropertyNote(const InputEvent& ev)
{
    if (ev.type != pvj::core::InputType::MidiNote) {
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

    emit learnFinished(tr("Mapped MIDI note ch%1 note%2 → %3")
                       .arg(ev.channel + 1)
                       .arg(ev.number)
                       .arg(m.property));
}

void InputRouter::applyLearnBankNav(const InputEvent& ev)
{
    if (ev.type != pvj::core::InputType::MidiNote) {
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

bool InputRouter::matchTrigger(const pvj::core::TriggerMapping& t, const InputEvent& ev) const
{
    if (t.input != ev.type) {
        return false;
    }
    if (t.input == pvj::core::InputType::Key) {
        return t.keyText == ev.keyText;
    }
    if (t.input == pvj::core::InputType::MidiNote) {
        return t.channel == ev.channel && t.number == ev.number;
    }
    if (t.input == pvj::core::InputType::MidiCC) {
        return t.channel == ev.channel && t.number == ev.number;
    }
    return false;
}

void InputRouter::dispatchPlayback(const InputEvent& ev)
{
    if (ev.type == pvj::core::InputType::MidiNote
        && ev.channel == kMixLayerDirectMidiChannel
        && ev.number >= kMixLayerDirectMidiNoteBase
        && ev.number < kMixLayerDirectMidiNoteBase + kMixLayerDirectCount) {
        emit mixLayerDirect(ev.number - kMixLayerDirectMidiNoteBase);
        return;
    }

    if (!m_project) {
        return;
    }

    int prevCc = -1;
    if (ev.type == pvj::core::InputType::MidiCC) {
        const quint32 k = ccKey(ev.channel, ev.number);
        prevCc = m_lastCcValue.value(k, 0);
    }

    // 1) Property mappings — CC faders / knobs (per cell)
    if (ev.type == pvj::core::InputType::MidiCC) {
        const double n = ev.value / 127.0;
        for (int bi = 0; bi < m_project->bankSets.size(); ++bi) {
            const auto& set = m_project->bankSets[bi];
            for (int bj = 0; bj < set.banks.size(); ++bj) {
                const auto& bank = set.banks[bj];
                for (int ci = 0; ci < bank.cells.size(); ++ci) {
                    for (const auto& pm : bank.cells[ci].propertyMappings) {
                        if (pm.input != pvj::core::InputType::MidiCC) {
                            continue;
                        }
                        if (pm.channel != ev.channel || pm.number != ev.number) {
                            continue;
                        }
                        const double scaled = scaleCcToProperty(n, pm.minValue, pm.maxValue);
                        if (pvj::core::PropertyRegistry::kindOf(pm.property)
                            == pvj::core::PropertyRegistry::Kind::Enum) {
                            const double span = pm.maxValue - pm.minValue;
                            const double n01  = span > 1e-9 ? (scaled - pm.minValue) / span : 0.0;
                            emit propertyValueChanged(bi, bj, ci, pm.property, n01);
                        } else {
                            emit propertyValueChanged(bi, bj, ci, pm.property, scaled);
                        }
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
                        if (pm.buttonMode == pvj::core::PropertyButtonMode::Toggle) {
                            emit propertyToggleRequested(bi, bj, ci, pm.property);
                        } else {
                            emit propertyValueChanged(bi, bj, ci, pm.property, pm.buttonValue);
                        }
                    }
                }
            }
        }
    }

    // 2) Trigger mappings (TRIGGERMAPPINGS)
    for (const auto& t : m_project->triggerMappings) {
        if (!matchTrigger(t, ev)) {
            continue;
        }

        if (t.input == pvj::core::InputType::MidiCC) {
            if (!(prevCc < 64 && ev.value >= 64)) {
                continue;
            }
        }

        switch (t.target) {
        case pvj::core::TriggerTarget::Cell:
            emit triggerCell(t.bankSetIndex, t.bankIndex, t.cellIndex);
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
                double v = 1.0;
                if (t.input == pvj::core::InputType::MidiCC) {
                    v = scaleCcToProperty(ev.value / 127.0, 0.0, 1.0);
                } else if (t.input == pvj::core::InputType::MidiNote) {
                    v = ev.value / 127.0;
                }
                emit propertyValueChanged(t.bankSetIndex, t.bankIndex, t.cellIndex,
                                          t.propertyName, v);
            }
            break;
        }
    }

    if (ev.type == pvj::core::InputType::MidiCC) {
        const quint32 k = ccKey(ev.channel, ev.number);
        m_lastCcValue.insert(k, ev.value);
    }
}

} // namespace pvj::input

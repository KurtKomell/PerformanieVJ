#include "Project.h"

#include "FilterEffectIds.h"
#include "InputMappingLabels.h"

#include <QHash>

#include <QtGlobal>

namespace pvj::core {

namespace {

constexpr int kDefaultBanksPerSet = 64;
constexpr int kDefaultGridRows = 4;
constexpr int kDefaultGridCols = 12;
constexpr int kMinGridDim = 1;
constexpr int kMaxGridDim = 16;

int sanitizeGridDim(int value, int fallback)
{
    if (value <= 0) {
        value = fallback;
    }
    return qBound(kMinGridDim, value, kMaxGridDim);
}

int gridCellCount(int rows, int cols)
{
    return sanitizeGridDim(rows, kDefaultGridRows) * sanitizeGridDim(cols, kDefaultGridCols);
}

void resizeBankCells(Bank& bank, int cellCount)
{
    bank.cells.resize(cellCount);
    for (int i = 0; i < bank.cells.size(); ++i) {
        bank.cells[i].index = i;
    }
}

BankSet makeDefaultBankSet(BankSetType type, int cellsPerBank)
{
    BankSet set;
    set.type = type;
    set.banks.reserve(kDefaultBanksPerSet);
    for (int b = 0; b < kDefaultBanksPerSet; ++b) {
        Bank bank;
        bank.index = b;
        bank.name = QStringLiteral("Bank %1").arg(b + 1);
        resizeBankCells(bank, cellsPerBank);
        set.banks.append(bank);
    }
    return set;
}

} // namespace

Project::Project() = default;

void Project::initializeDefault()
{
    mediaLibrary.clear();
    bankSets.clear();
    triggerMappings.clear();
    settings = Settings{};
    settings.matrix.gridRows = sanitizeGridDim(settings.matrix.gridRows, kDefaultGridRows);
    settings.matrix.gridCols = sanitizeGridDim(settings.matrix.gridCols, kDefaultGridCols);

    const int cellsPerBank = gridCellCount(settings.matrix.gridRows, settings.matrix.gridCols);
    bankSets.append(makeDefaultBankSet(BankSetType::TypeA, cellsPerBank));
}

void Project::ensureSingleBankSet()
{
    while (bankSets.size() > 1) {
        bankSets.removeLast();
    }
    if (!bankSets.isEmpty()) {
        bankSets[0].type = BankSetType::TypeA;
    }
    for (int i = triggerMappings.size() - 1; i >= 0; --i) {
        const TriggerMapping& t = triggerMappings.at(i);
        if (t.target == TriggerTarget::BankSetSwitch) {
            triggerMappings.removeAt(i);
            continue;
        }
        if (t.bankSetIndex > 0) {
            triggerMappings.removeAt(i);
            continue;
        }
    }
    for (TriggerMapping& t : triggerMappings) {
        t.bankSetIndex = 0;
    }
}

void Project::resizeBanksForGrid(int rows, int cols)
{
    settings.matrix.gridRows = sanitizeGridDim(rows, kDefaultGridRows);
    settings.matrix.gridCols = sanitizeGridDim(cols, kDefaultGridCols);
    const int cellsPerBank = gridCellCount(settings.matrix.gridRows, settings.matrix.gridCols);
    for (BankSet& set : bankSets) {
        for (Bank& bank : set.banks) {
            resizeBankCells(bank, cellsPerBank);
        }
    }
}

const MediaItem* Project::findMedia(const QUuid& id) const
{
    for (const auto& m : mediaLibrary) {
        if (m.id == id) {
            return &m;
        }
    }
    return nullptr;
}

MediaItem* Project::findMedia(const QUuid& id)
{
    for (auto& m : mediaLibrary) {
        if (m.id == id) {
            return &m;
        }
    }
    return nullptr;
}

void Project::stripMaxineFiltersFromCells()
{
    for (BankSet& set : bankSets) {
        for (Bank& bank : set.banks) {
            for (Cell& cell : bank.cells) {
                QList<CellFilterNode> kept;
                kept.reserve(cell.filterChain.size());
                for (const CellFilterNode& node : cell.filterChain) {
                    if (filterUsesMaxineBackend(node.typeId)) {
                        continue;
                    }
                    kept.append(node);
                }
                cell.filterChain = std::move(kept);
            }
        }
    }
}

void Project::sanitizeOutputFilters()
{
    sanitizeOutputFilterChain(settings.output.filterChain);
}

int Project::bankSetIndex(BankSetType type) const
{
    for (int i = 0; i < bankSets.size(); ++i) {
        if (bankSets[i].type == type) {
            return i;
        }
    }
    return -1;
}

namespace {

bool isCellTrigger(const TriggerMapping& t)
{
    return t.target == TriggerTarget::Cell;
}

bool isCellKeyboardTrigger(const TriggerMapping& t)
{
    return isCellTrigger(t) && t.input == InputType::Key;
}

bool isCellMidiNoteTrigger(const TriggerMapping& t)
{
    return isCellTrigger(t) && t.input == InputType::MidiNote;
}

bool isCellSlotTrigger(const TriggerMapping& t, int bankSetIndex, int cellIndex)
{
    return isCellTrigger(t) && t.bankSetIndex == bankSetIndex && t.cellIndex == cellIndex;
}

bool cellKeyboardTriggerMatchesLookup(const TriggerMapping& t, int bankSetIndex, int bankIndex,
                                      int cellIndex)
{
    if (!isCellKeyboardTrigger(t) || t.bankSetIndex != bankSetIndex || t.cellIndex != cellIndex) {
        return false;
    }
    if (t.bankIndex == kBankIndexAllBanks) {
        return true;
    }
    return t.bankIndex == bankIndex;
}

QString cellSlotKey(int bankSetIndex, int cellIndex)
{
    return QStringLiteral("%1:%2").arg(bankSetIndex).arg(cellIndex);
}

} // namespace

QString Project::keyboardTriggerForCell(int bankSetIndex, int bankIndex, int cellIndex) const
{
    QString allBanksKey;
    QString legacyKey;
    for (const auto& t : triggerMappings) {
        if (!cellKeyboardTriggerMatchesLookup(t, bankSetIndex, bankIndex, cellIndex)) {
            continue;
        }
        if (t.bankIndex == kBankIndexAllBanks) {
            allBanksKey = t.keyText;
        } else {
            legacyKey = t.keyText;
        }
    }
    return !allBanksKey.isEmpty() ? allBanksKey : legacyKey;
}

void Project::setKeyboardTriggerForCell(int bankSetIndex, int /*bankIndex*/, int cellIndex,
                                       const QString& keyText, int qtKey)
{
    QList<TriggerMapping> kept;
    kept.reserve(triggerMappings.size());
    for (const auto& t : triggerMappings) {
        if (isCellKeyboardTrigger(t) && isCellSlotTrigger(t, bankSetIndex, cellIndex)) {
            continue;
        }
        if (!keyText.isEmpty() && isCellKeyboardTrigger(t) && t.keyText == keyText) {
            continue;
        }
        kept.append(t);
    }
    triggerMappings = std::move(kept);

    if (keyText.isEmpty()) {
        return;
    }

    TriggerMapping t;
    t.input        = InputType::Key;
    t.channel      = 0;
    t.number       = qtKey;
    t.keyText      = keyText;
    t.target       = TriggerTarget::Cell;
    t.bankSetIndex = bankSetIndex;
    t.bankIndex    = kBankIndexAllBanks;
    t.cellIndex    = cellIndex;
    triggerMappings.append(t);
}

void Project::clearKeyboardTriggerForCell(int bankSetIndex, int bankIndex, int cellIndex)
{
    setKeyboardTriggerForCell(bankSetIndex, bankIndex, cellIndex, QString());
}

void Project::midiCellTriggerForCell(int bankSetIndex, int bankIndex, int cellIndex, int* channel,
                                     int* number) const
{
    if (channel) {
        *channel = -1;
    }
    if (number) {
        *number = -1;
    }
    int legacyChannel = -1;
    int legacyNumber  = -1;
    for (const auto& t : triggerMappings) {
        if (!isCellMidiNoteTrigger(t) || t.bankSetIndex != bankSetIndex
            || t.cellIndex != cellIndex) {
            continue;
        }
        if (t.bankIndex == kBankIndexAllBanks) {
            if (channel) {
                *channel = t.channel;
            }
            if (number) {
                *number = t.number;
            }
            return;
        }
        if (t.bankIndex == bankIndex) {
            legacyChannel = t.channel;
            legacyNumber  = t.number;
        }
    }
    if (legacyChannel >= 0) {
        if (channel) {
            *channel = legacyChannel;
        }
        if (number) {
            *number = legacyNumber;
        }
    }
}

void Project::setMidiCellTriggerForCell(int bankSetIndex, int cellIndex, int channel, int number)
{
    QList<TriggerMapping> kept;
    kept.reserve(triggerMappings.size());
    for (const auto& t : triggerMappings) {
        if (isCellMidiNoteTrigger(t) && isCellSlotTrigger(t, bankSetIndex, cellIndex)) {
            continue;
        }
        if (isCellMidiNoteTrigger(t) && t.channel == channel && t.number == number) {
            continue;
        }
        kept.append(t);
    }
    triggerMappings = std::move(kept);

    TriggerMapping t;
    t.input        = InputType::MidiNote;
    t.channel      = channel;
    t.number       = number;
    t.target       = TriggerTarget::Cell;
    t.bankSetIndex = bankSetIndex;
    t.bankIndex    = kBankIndexAllBanks;
    t.cellIndex    = cellIndex;
    triggerMappings.append(t);
}

void Project::clearMidiCellTriggerForCell(int bankSetIndex, int cellIndex)
{
    QList<TriggerMapping> kept;
    kept.reserve(triggerMappings.size());
    for (const auto& t : triggerMappings) {
        if (isCellMidiNoteTrigger(t) && isCellSlotTrigger(t, bankSetIndex, cellIndex)) {
            continue;
        }
        kept.append(t);
    }
    triggerMappings = std::move(kept);
}

QString Project::propertyMappingLabel(int bankSetIndex, int bankIndex, int cellIndex,
                                      const QString& propertyId) const
{
    if (propertyId.isEmpty() || bankSetIndex < 0 || bankSetIndex >= bankSets.size()) {
        return {};
    }
    const auto& set = bankSets[bankSetIndex];
    if (bankIndex < 0 || bankIndex >= set.banks.size()) {
        return {};
    }
    const auto& bank = set.banks[bankIndex];
    if (cellIndex < 0 || cellIndex >= bank.cells.size()) {
        return {};
    }
    for (const PropertyMapping& pm : bank.cells[cellIndex].propertyMappings) {
        if (pm.property == propertyId) {
            return formatPropertyMappingLabel(pm);
        }
    }
    return {};
}

void Project::normalizeCellSlotTriggers()
{
    QHash<QString, TriggerMapping> bestKey;
    QHash<QString, TriggerMapping> bestMidi;
    QList<TriggerMapping> normalized;
    normalized.reserve(triggerMappings.size());

    for (const auto& t : triggerMappings) {
        if (!isCellKeyboardTrigger(t) && !isCellMidiNoteTrigger(t)) {
            normalized.append(t);
            continue;
        }
        const QString slot = cellSlotKey(t.bankSetIndex, t.cellIndex);
        if (isCellKeyboardTrigger(t)) {
            TriggerMapping copy = t;
            copy.bankIndex = kBankIndexAllBanks;
            auto it = bestKey.find(slot);
            if (it == bestKey.end() || t.bankIndex == kBankIndexAllBanks) {
                bestKey.insert(slot, copy);
            }
            continue;
        }
        TriggerMapping copy = t;
        copy.bankIndex = kBankIndexAllBanks;
        auto it = bestMidi.find(slot);
        if (it == bestMidi.end() || t.bankIndex == kBankIndexAllBanks) {
            bestMidi.insert(slot, copy);
        }
    }

    for (const auto& t : bestKey) {
        normalized.append(t);
    }
    for (const auto& t : bestMidi) {
        normalized.append(t);
    }
    triggerMappings = std::move(normalized);
}

} // namespace pvj::core

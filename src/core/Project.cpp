#include "Project.h"

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

int Project::bankSetIndex(BankSetType type) const
{
    for (int i = 0; i < bankSets.size(); ++i) {
        if (bankSets[i].type == type) {
            return i;
        }
    }
    return -1;
}

} // namespace pvj::core

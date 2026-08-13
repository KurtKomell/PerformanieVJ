#include "undo/ProjectUndoCommands.h"

namespace pvj::app {

namespace {

pvj::core::Cell* mutableCellAt(pvj::core::Project* project, const CellRef& ref)
{
    if (!project || ref.bankSet < 0 || ref.bankSet >= project->bankSets.size()) {
        return nullptr;
    }
    auto& set = project->bankSets[ref.bankSet];
    if (ref.bank < 0 || ref.bank >= set.banks.size()) {
        return nullptr;
    }
    auto& bank = set.banks[ref.bank];
    if (ref.cell < 0 || ref.cell >= bank.cells.size()) {
        return nullptr;
    }
    return &bank.cells[ref.cell];
}

} // namespace

CellsReplaceCommand::CellsReplaceCommand(pvj::core::Project* project,
                                         QVector<CellSnapshot> before,
                                         QVector<CellSnapshot> after,
                                         const QString& text,
                                         QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_project(project)
    , m_before(std::move(before))
    , m_after(std::move(after))
{
}

void CellsReplaceCommand::undo()
{
    apply(m_before);
}

void CellsReplaceCommand::redo()
{
    apply(m_after);
}

void CellsReplaceCommand::apply(const QVector<CellSnapshot>& snaps) const
{
    if (!m_project) {
        return;
    }
    for (const CellSnapshot& snap : snaps) {
        if (pvj::core::Cell* cell = mutableCellAt(m_project, snap.ref)) {
            const int index = cell->index;
            *cell = snap.cell;
            cell->index = index;
        }
    }
}

BankReplaceCommand::BankReplaceCommand(pvj::core::Project* project,
                                       int bankSetIndex,
                                       int bankIndex,
                                       pvj::core::Bank before,
                                       pvj::core::Bank after,
                                       const QString& text,
                                       QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_project(project)
    , m_bankSet(bankSetIndex)
    , m_bank(bankIndex)
    , m_before(std::move(before))
    , m_after(std::move(after))
{
}

void BankReplaceCommand::undo()
{
    apply(m_before);
}

void BankReplaceCommand::redo()
{
    apply(m_after);
}

void BankReplaceCommand::apply(const pvj::core::Bank& bank) const
{
    if (!m_project || m_bankSet < 0 || m_bankSet >= m_project->bankSets.size()) {
        return;
    }
    auto& set = m_project->bankSets[m_bankSet];
    if (m_bank < 0 || m_bank >= set.banks.size()) {
        return;
    }
    const int index = set.banks[m_bank].index;
    set.banks[m_bank] = bank;
    set.banks[m_bank].index = index;
}

ProjectReplaceCommand::ProjectReplaceCommand(pvj::core::Project* project,
                                             pvj::core::Project before,
                                             pvj::core::Project after,
                                             const QString& text,
                                             QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_project(project)
    , m_before(std::move(before))
    , m_after(std::move(after))
{
}

void ProjectReplaceCommand::undo()
{
    apply(m_before);
}

void ProjectReplaceCommand::redo()
{
    apply(m_after);
}

void ProjectReplaceCommand::apply(const pvj::core::Project& state) const
{
    if (!m_project) {
        return;
    }
    // Preserve file path / format identity of the live document.
    const QString filePath = m_project->filePath;
    const QString formatVersion = m_project->formatVersion;
    const QString sourceFormat = m_project->sourceFormat;
    *m_project = state;
    m_project->filePath = filePath;
    m_project->formatVersion = formatVersion;
    m_project->sourceFormat = sourceFormat;
}

TriggerMappingsReplaceCommand::TriggerMappingsReplaceCommand(
    pvj::core::Project* project,
    QList<pvj::core::TriggerMapping> before,
    QList<pvj::core::TriggerMapping> after,
    const QString& text,
    QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_project(project)
    , m_before(std::move(before))
    , m_after(std::move(after))
{
}

void TriggerMappingsReplaceCommand::undo()
{
    if (m_project) {
        m_project->triggerMappings = m_before;
    }
}

void TriggerMappingsReplaceCommand::redo()
{
    if (m_project) {
        m_project->triggerMappings = m_after;
    }
}

} // namespace pvj::app

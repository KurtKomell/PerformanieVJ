#pragma once

#include "core/Model.h"
#include "core/Project.h"

#include <QUndoCommand>
#include <QVector>

namespace pvj::app {

struct CellRef {
    int bankSet = 0;
    int bank    = 0;
    int cell    = 0;
};

struct CellSnapshot {
    CellRef           ref;
    pvj::core::Cell   cell;
};

/// Restores multiple cells (before on undo, after on redo).
class CellsReplaceCommand final : public QUndoCommand
{
public:
    CellsReplaceCommand(pvj::core::Project* project,
                        QVector<CellSnapshot> before,
                        QVector<CellSnapshot> after,
                        const QString& text,
                        QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    void apply(const QVector<CellSnapshot>& snaps) const;

    pvj::core::Project*   m_project = nullptr;
    QVector<CellSnapshot> m_before;
    QVector<CellSnapshot> m_after;
};

/// Replaces one bank (content + name).
class BankReplaceCommand final : public QUndoCommand
{
public:
    BankReplaceCommand(pvj::core::Project* project,
                       int bankSetIndex,
                       int bankIndex,
                       pvj::core::Bank before,
                       pvj::core::Bank after,
                       const QString& text,
                       QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    void apply(const pvj::core::Bank& bank) const;

    pvj::core::Project* m_project = nullptr;
    int                 m_bankSet = 0;
    int                 m_bank    = 0;
    pvj::core::Bank     m_before;
    pvj::core::Bank     m_after;
};

/// Full project document restore (media, banks, triggers, settings).
class ProjectReplaceCommand final : public QUndoCommand
{
public:
    ProjectReplaceCommand(pvj::core::Project* project,
                          pvj::core::Project before,
                          pvj::core::Project after,
                          const QString& text,
                          QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    void apply(const pvj::core::Project& state) const;

    pvj::core::Project* m_project = nullptr;
    pvj::core::Project  m_before;
    pvj::core::Project  m_after;
};

/// Trigger-mapping list replace (learn / clear).
class TriggerMappingsReplaceCommand final : public QUndoCommand
{
public:
    TriggerMappingsReplaceCommand(pvj::core::Project* project,
                                  QList<pvj::core::TriggerMapping> before,
                                  QList<pvj::core::TriggerMapping> after,
                                  const QString& text,
                                  QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    pvj::core::Project*                 m_project = nullptr;
    QList<pvj::core::TriggerMapping>    m_before;
    QList<pvj::core::TriggerMapping>    m_after;
};

} // namespace pvj::app

#pragma once

#include "core/Model.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QScrollArea;

namespace pvj::app {
class NodeGraphWidget;
}

namespace pvj::core {
struct Cell;
class Project;
}

namespace pvj::app {

/// Modeless editor: video source + horizontal filter chain graph.
class FilterNodeEditorWindow : public QDialog
{
    Q_OBJECT
public:
    explicit FilterNodeEditorWindow(QWidget* parent = nullptr);

    void openForCell(int bankSetIndex, int bankIndex, int cellIndex,
                     pvj::core::Cell* cell, pvj::core::Project* project);

signals:
    void chainEdited(int bankSetIndex, int bankIndex, int cellIndex);
    void midiLearnCcRequested(int bankSetIndex, int bankIndex, int cellIndex, const QString& propertyId);
    void midiLearnNoteRequested(int bankSetIndex, int bankIndex, int cellIndex,
                                const QString& propertyId, bool toggle, double buttonValue);
    void midiClearMappingRequested(int bankSetIndex, int bankIndex, int cellIndex, const QString& propertyId);

private:
    void emitChainEdited();
    void refreshSourceHint();
    void syncSourceComboFromCell();
    void applySourceToCell();

    int m_bankSetIndex = -1;
    int m_bankIndex    = -1;
    int m_cellIndex    = -1;

    pvj::core::Cell*    m_cell    = nullptr;
    pvj::core::Project* m_project = nullptr;

    QComboBox*       m_sourceCombo = nullptr;
    QLabel*          m_sourceHint  = nullptr;
    QScrollArea*     m_scrollArea  = nullptr;
    NodeGraphWidget* m_graph       = nullptr;

    /// Remember media id when switching source to generator and back.
    QUuid m_stashedMediaId;
};

} // namespace pvj::app

#pragma once

#include <QWidget>

#include <functional>

#include <QHash>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QUuid>
#include <QVector>

class QBoxLayout;
class QComboBox;
class QContextMenuEvent;
class QEvent;
class QFormLayout;
class QLabel;
class QMenu;
class QScrollArea;
class QWidget;

namespace pvj::core {
struct Cell;
struct CellFilterNode;
enum class FilterParamKind;
struct FilterParamSpec;
class Project;
}

namespace pvj::app {

/// Horizontal node graph: Source -> Filter* -> Output. Edits `Cell::filterChain` in place.
class NodeGraphWidget : public QWidget
{
    Q_OBJECT
public:
    explicit NodeGraphWidget(QWidget* parent = nullptr);

    void setCell(pvj::core::Cell* cell, pvj::core::Project* project);
    void refresh();

signals:
    void chainChanged();
    void filterChainChanged();
    void filterParamsChanged();
    void midiLearnCcRequested(const QString& propertyId);
    void midiLearnNoteRequested(const QString& propertyId, bool toggle, double buttonValue);
    void midiClearMappingRequested(const QString& propertyId);
    /// Same roles as `FilterNodeEditorWindow` source combo user data (0=media, 2=test…).
    void sourceChangeRequested(int role);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    enum class NodeKind { Source, Filter, Output };

    struct GraphNode {
        QRectF   rect;
        NodeKind kind = NodeKind::Source;
        int      filterIndex = -1; ///< index in filterChain when kind == Filter
        QUuid    filterId;
    };

    void rebuildLayout();
    int  hitTest(const QPointF& pos) const;
    int  finalIndexFromGhostX(double gx) const;
    void buildCatalogMenu(QMenu* root, const std::function<void(const QString& typeId)>& onPick);
    QString sourceTitleLine() const;
    QString filterLabelFor(const QString& typeId) const;

    void addFilter(const QString& typeId);
    void insertFilter(int index, const QString& typeId);
    void removeFilterAt(int filterIndex);
    void moveFilter(int fromIndex, int toIndex);
    void duplicateFilter(int filterIndex);
    void setFilterTypeId(int filterIndex, const QString& typeId);
    void rebuildParamEditors();
    void clearParamEditors();
    void ensureNodeParams(pvj::core::CellFilterNode& node) const;
    const core::FilterParamSpec* paramSpecFor(const QString& typeId, const QString& paramName) const;
    void emitChainEdited();
    void emitParamsEdited();
    bool eventFilter(QObject* watched, QEvent* event) override;

    pvj::core::Cell*    m_cell    = nullptr;
    pvj::core::Project* m_project = nullptr;

    QVector<GraphNode> m_nodes;
    int                  m_selectedGraphIndex = -1;

    bool    m_dragging     = false;
    int     m_dragFromFilterIndex = -1;
    QPointF m_dragGrabOffset;
    double  m_dragGhostX = 0.0;

    QWidget* m_paramHost = nullptr;
    QBoxLayout* m_paramHostLayout = nullptr;
    struct ParamWidgetBinding {
        QWidget* widget = nullptr;
        QUuid nodeId;
        QString paramName;
        QString typeId;
        pvj::core::FilterParamKind kind;
        double minV = 0.0;
        double maxV = 1.0;
    };
    QList<ParamWidgetBinding> m_paramWidgets;

    static constexpr double kPad     = 20.0;
    static constexpr double kBoxW    = 158.0;
    static constexpr double kBoxH    = 54.0;
    static constexpr double kGap     = 48.0;
};

} // namespace pvj::app

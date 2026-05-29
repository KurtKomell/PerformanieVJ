#pragma once

#include <QDialog>

class QListWidget;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace pvj::core {
struct CellFilterNode;
class Project;
}

namespace pvj::app {

/// Post-mixer NVIDIA output filter chain editor (formerly Output inspector tab).
class OutputProcessingDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OutputProcessingDialog(QWidget* parent = nullptr);

    void setProject(pvj::core::Project* project);

signals:
    void filterChainChanged();

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onOutputFilterSelectionChanged();
    void onOutputFilterAddTriggered();
    void onOutputFilterMoveUp();
    void onOutputFilterMoveDown();
    void onOutputFilterRemove();

private:
    void refreshOutputFilterUi();
    void rebuildOutputParamEditors();
    void clearOutputParamEditors();
    void emitFilterChainChanged();
    QString outputFilterLabelFor(const QString& typeId) const;
    void ensureOutputNodeParams(pvj::core::CellFilterNode& node) const;

    pvj::core::Project* m_project = nullptr;
    bool m_loading = false;

    QListWidget* m_outputFilterList = nullptr;
    QPushButton* m_outputFilterUpBtn = nullptr;
    QPushButton* m_outputFilterDownBtn = nullptr;
    QPushButton* m_outputFilterRemoveBtn = nullptr;
    QPushButton* m_outputFilterAddBtn = nullptr;
    QWidget*     m_outputParamHost = nullptr;
    QVBoxLayout* m_outputParamLayout = nullptr;
    int          m_outputFilterSelectedIndex = -1;
};

} // namespace pvj::app

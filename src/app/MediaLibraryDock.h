#pragma once

#include <QDockWidget>

class QFileSystemModel;
class QListView;
class QStandardItemModel;
class QTreeView;
class QToolButton;

namespace pvj::core {
class Project;
}

namespace pvj::app {

class PreviewWidget;

// Right-side collapsible dock:
//   [Toolbar]
//   [Folder tree  (QFileSystemModel)]
//   [Project media list]
//   [File preview thumbnail]
// The dock emits mediaActivated() when the user double-clicks a clip the app
// should load (into a cell or the preview).
class MediaLibraryDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit MediaLibraryDock(QWidget* parent = nullptr);

    void setProject(pvj::core::Project* project);

    // Refresh the list of project media entries (call after import/open).
    void refreshProjectMedia();

signals:
    void mediaActivated(const QString& absolutePath);

private slots:
    void onChooseRoot();
    void onTreeActivated(const QModelIndex& index);
    void onProjectListActivated(const QModelIndex& index);

private:
    void restoreOrInitMediaRoot();
    void setRootPath(const QString& path);
    void showFilePreview(const QString& absolutePath);

    pvj::core::Project* m_project = nullptr;

    QToolButton*        m_chooseRootBtn  = nullptr;
    QFileSystemModel*   m_fsModel        = nullptr;
    QTreeView*          m_tree           = nullptr;
    QStandardItemModel* m_projectModel   = nullptr;
    QListView*          m_projectList    = nullptr;
    PreviewWidget*      m_preview        = nullptr;
};

} // namespace pvj::app

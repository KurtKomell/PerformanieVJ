#include "MediaLibraryDock.h"

#include "SplitterHelpers.h"
#include "PreviewWidget.h"
#include "core/Project.h"
#include "video/MediaProbe.h"
#include "video/ThumbnailExtractor.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDrag>
#include <QFileDialog>
#include <QMimeData>
#include <QUrl>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QDir>
#include <QList>
#include <QSettings>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListView>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

namespace pvj::app {

/// Drags the stored absolute path as a local file URL (for drop onto the bank grid).
class ProjectMediaListView final : public QListView
{
public:
    explicit ProjectMediaListView(QWidget* parent = nullptr)
        : QListView(parent)
    {
        setDragDropMode(QAbstractItemView::DragOnly);
        setDragEnabled(true);
        setSelectionMode(QAbstractItemView::SingleSelection);
    }

protected:
    void startDrag(Qt::DropActions supportedActions) override
    {
        Q_UNUSED(supportedActions);
        const QModelIndexList idxs = selectedIndexes();
        if (idxs.isEmpty()) {
            return;
        }
        const QString path = idxs.first().data(Qt::UserRole).toString();
        if (path.isEmpty() || !QFileInfo(path).isFile()) {
            return;
        }
        auto* mime = new QMimeData;
        mime->setUrls({QUrl::fromLocalFile(path)});
        QDrag drag(this);
        drag.setMimeData(mime);
        (void)drag.exec(Qt::CopyAction);
    }
};

namespace {

// Native QFileIconProvider icons can trip Qt 6 debug builds on Windows
// (qpixmap_win.cpp asserts on non-mono bitmaps from the shell). Use theme
// icons only — sufficient for navigation + project list.
class SafeFileIconProvider final : public QFileIconProvider
{
public:
    QIcon icon(IconType type) const override
    {
        Q_UNUSED(type);
        return QIcon();
    }

    QIcon icon(const QFileInfo& info) const override
    {
        Q_UNUSED(info);
        return QIcon();
    }
};

const QStringList kMediaExtensions = {
    QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("mkv"),
    QStringLiteral("avi"), QStringLiteral("webm"),QStringLiteral("m4v"),
    QStringLiteral("dxv"), QStringLiteral("mp3"), QStringLiteral("wav"),
    QStringLiteral("flac"),QStringLiteral("ogg"), QStringLiteral("aac"),
    QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
    QStringLiteral("gif"), QStringLiteral("bmp"),
};

QStringList nameFilters()
{
    QStringList out;
    out.reserve(kMediaExtensions.size());
    for (const auto& ext : kMediaExtensions) {
        out.append(QStringLiteral("*.") + ext);
    }
    return out;
}

QString defaultMediaFolder()
{
    QString p = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (!p.isEmpty()) {
        return p;
    }
    p = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!p.isEmpty()) {
        return p;
    }
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

QString formatProbeTooltip(const pvj::video::MediaProbeResult& p)
{
    if (!p.ok) {
        return p.errorMessage;
    }
    QStringList parts;
    parts << QFileInfo(p.filePath).fileName();
    if (p.hasVideo) {
        parts << QStringLiteral("%1x%2")
                     .arg(p.videoSize.width())
                     .arg(p.videoSize.height());
        if (p.videoFps > 0.05) {
            parts << QStringLiteral("%1 fps").arg(p.videoFps, 0, 'f', 2);
        }
        parts << p.videoCodec;
    }
    if (p.durationMs > 0) {
        parts << QStringLiteral("%1 s").arg(p.durationMs / 1000.0, 0, 'f', 1);
    }
    if (p.hasAudio) {
        parts << QStringLiteral("%1 ch @ %2 Hz")
                     .arg(p.audioChannels)
                     .arg(p.audioSampleRate);
        parts << p.audioCodec;
    }
    if (!p.formatName.isEmpty()) {
        parts << QStringLiteral("container: %1").arg(p.formatName);
    }
    return parts.join(QStringLiteral(" | "));
}
} // namespace

MediaLibraryDock::MediaLibraryDock(QWidget* parent)
    : QDockWidget(tr("Media Library"), parent)
{
    setFeatures(QDockWidget::DockWidgetMovable
                | QDockWidget::DockWidgetFloatable
                | QDockWidget::DockWidgetClosable);
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* host = new QWidget(this);
    auto* layout = new QVBoxLayout(host);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(0, 0, 0, 0);
    m_chooseRootBtn = new QToolButton(host);
    m_chooseRootBtn->setText(tr("Media folder…"));
    m_chooseRootBtn->setToolTip(
        tr("Choose the top folder for the file tree (not the whole disk). "
           "The tree only shows this folder and its subfolders."));
    connect(m_chooseRootBtn, &QToolButton::clicked, this, &MediaLibraryDock::onChooseRoot);
    toolbar->addWidget(m_chooseRootBtn);
    toolbar->addStretch(1);
    layout->addLayout(toolbar);

    auto* splitter = new PvjSplitter(Qt::Vertical, host);

    // File system tree (top)
    m_fsModel = new QFileSystemModel(this);
    m_fsModel->setIconProvider(new SafeFileIconProvider());
    m_fsModel->setNameFilters(nameFilters());
    m_fsModel->setNameFilterDisables(false);
    m_fsModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

    m_tree = new QTreeView(splitter);
    m_tree->setModel(m_fsModel);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(false);
    for (int col = 1; col < m_fsModel->columnCount(); ++col) {
        m_tree->hideColumn(col);
    }
    m_tree->setUniformRowHeights(true);
    m_tree->setAnimated(false);
    m_tree->setDragEnabled(true);
    m_tree->setDragDropMode(QAbstractItemView::DragOnly);
    m_tree->setDefaultDropAction(Qt::CopyAction);
    connect(m_tree, &QTreeView::doubleClicked,
            this, &MediaLibraryDock::onTreeActivated);

    splitter->addWidget(m_tree);

    // Project media list (middle)
    auto* projHost = new QWidget(splitter);
    auto* projLayout = new QVBoxLayout(projHost);
    projLayout->setContentsMargins(0, 0, 0, 0);
    projLayout->addWidget(new QLabel(tr("Project media"), projHost));

    m_projectModel = new QStandardItemModel(this);
    m_projectList = new ProjectMediaListView(projHost);
    m_projectList->setModel(m_projectModel);
    m_projectList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_projectList, &QListView::doubleClicked,
            this, &MediaLibraryDock::onProjectListActivated);
    projLayout->addWidget(m_projectList, 1);

    splitter->addWidget(projHost);

    // Preview (bottom)
    m_preview = new PreviewWidget(splitter);
    m_preview->setLabel(tr("Preview"));
    splitter->addWidget(m_preview);

    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 3);
    layout->addWidget(splitter, 1);

    setWidget(host);

    restoreOrInitMediaRoot();
}

void MediaLibraryDock::setRootPath(const QString& path)
{
    if (path.isEmpty() || !QFileInfo(path).isDir()) {
        return;
    }
    const QModelIndex rootIdx = m_fsModel->setRootPath(path);
    m_tree->setRootIndex(rootIdx);
}

void MediaLibraryDock::restoreOrInitMediaRoot()
{
    QSettings settings;
    const QString key = QStringLiteral("mediaLibrary/rootPath");
    QString path = settings.value(key).toString();
    if (path.isEmpty() || !QFileInfo(path).isDir()) {
        path = defaultMediaFolder();
    }
    setRootPath(path);
}

void MediaLibraryDock::onChooseRoot()
{
    const QString current = m_fsModel->rootPath();
    const QString picked = QFileDialog::getExistingDirectory(this,
        tr("Choose media folder (tree starts here)"), current);
    if (!picked.isEmpty()) {
        setRootPath(picked);
        QSettings settings;
        settings.setValue(QStringLiteral("mediaLibrary/rootPath"), picked);
    }
}

void MediaLibraryDock::setProject(pvj::core::Project* project)
{
    m_project = project;
    refreshProjectMedia();
}

void MediaLibraryDock::refreshProjectMedia()
{
    m_projectModel->clear();
    m_projectModel->setHorizontalHeaderLabels({tr("Name")});
    if (!m_project) {
        return;
    }
    for (const auto& m : m_project->mediaLibrary) {
        const QString display = m.displayName.isEmpty()
            ? QFileInfo(m.path).fileName() : m.displayName;
        auto* item = new QStandardItem(display);
        item->setToolTip(m.path);
        item->setData(m.path, Qt::UserRole);
        item->setEditable(false);
        m_projectModel->appendRow(item);
    }
}

void MediaLibraryDock::showFilePreview(const QString& absolutePath)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto probe = pvj::video::MediaProbe::probe(absolutePath);
    const QList<QImage> film = pvj::video::ThumbnailExtractor::extractPreviewKeyframes(
        absolutePath, 5, m_preview->size());
    QApplication::restoreOverrideCursor();

    m_preview->setLabel(QFileInfo(absolutePath).fileName());
    QString tip = formatProbeTooltip(probe);

    if (film.size() >= 2) {
        m_preview->setFilmstripFrames(film);
    } else if (film.size() == 1) {
        m_preview->setFrame(film.first(), 0);
    } else {
        const QString suf = QFileInfo(absolutePath).suffix().toLower();
        static const QStringList kRaster = {
            QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
            QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("webp"),
        };
        QImage raster;
        if (kRaster.contains(suf) && raster.load(absolutePath)) {
            m_preview->setFrame(
                raster.scaled(m_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation),
                0);
        } else {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            const auto thumb = pvj::video::ThumbnailExtractor::extract(absolutePath, m_preview->size());
            QApplication::restoreOverrideCursor();
            if (thumb.ok && !thumb.image.isNull()) {
                m_preview->setFrame(thumb.image, thumb.timestampMs);
            } else {
                m_preview->clearFrame();
                if (!thumb.errorMessage.isEmpty()) {
                    tip = tip.isEmpty() ? thumb.errorMessage
                                        : thumb.errorMessage + QLatin1Char('\n') + tip;
                }
            }
        }
    }
    m_preview->setToolTip(tip);
}

void MediaLibraryDock::onTreeActivated(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }
    const QString path = m_fsModel->filePath(index);
    if (!QFileInfo(path).isFile()) {
        return;
    }
    showFilePreview(path);
    emit mediaActivated(path);
}

void MediaLibraryDock::onProjectListActivated(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }
    const QString path = index.data(Qt::UserRole).toString();
    if (path.isEmpty()) {
        return;
    }
    showFilePreview(path);
    emit mediaActivated(path);
}

} // namespace pvj::app

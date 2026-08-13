#include "BankGridWidget.h"

#include "core/FilterCatalog.h"
#include "core/FilterEffectIds.h"
#include "core/Project.h"
#include "video/ThumbnailExtractor.h"

#include <QApplication>
#include <QKeySequence>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEnterEvent>
#include <QFileInfo>
#include <QList>
#include <QMimeData>
#include <QUrl>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabBar>
#include <QThread>
#include <QTimer>
#include <QtConcurrent>
#include <QVBoxLayout>

#include <QtGlobal>

#include <QPointer>
#include <QVector>

#include <algorithm>

namespace pvj::app {

namespace {

void drawCellDropShadow(QPainter& p, const QRectF& r, qreal radius)
{
    p.save();
    p.translate(1, 2);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 72));
    p.drawRoundedRect(r, radius, radius);
    p.restore();
}

/// Tight outer glow + crisp stroke; keep expansion small so 84×64 cells do not clip badly.
void drawGlowFrame(QPainter& p, const QRectF& r, qreal radius, const QColor& core, int passes = 3)
{
    for (int i = passes; i >= 1; --i) {
        const qreal spread = qreal(i) * 1.25;
        const qreal width  = 1.0 + qreal(i) * 1.5;
        QColor c = core;
        c.setAlpha(qBound(12, 18 + i * 16, 95));
        p.setPen(QPen(c, width));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(-spread, -spread, spread, spread),
                          radius + spread * 0.4,
                          radius + spread * 0.4);
    }
    QColor rim = core.lighter(108);
    rim.setAlpha(240);
    p.setPen(QPen(rim, 2));
    p.drawRoundedRect(r, radius, radius);
}

bool mimeDataHasLocalFile(const QMimeData* md)
{
    if (!md) {
        return false;
    }
    for (const QUrl& u : md->urls()) {
        if (!u.isLocalFile()) {
            continue;
        }
        if (QFileInfo(u.toLocalFile()).isFile()) {
            return true;
        }
    }
    return false;
}

QString firstLocalFilePath(const QMimeData* md)
{
    if (!md) {
        return {};
    }
    for (const QUrl& u : md->urls()) {
        if (!u.isLocalFile()) {
            continue;
        }
        const QString p = u.toLocalFile();
        if (QFileInfo(p).isFile()) {
            return p;
        }
    }
    return {};
}

} // namespace

using pvj::core::Project;

// Small internal helper widget for a single cell. Shows the cell index and
// a colored status indicator (filled = has content, empty = default cell).
class CellButton : public QPushButton
{
    Q_OBJECT
public:
    CellButton(int index, QWidget* parent = nullptr)
        : QPushButton(parent)
        , m_index(index)
    {
        setFixedSize(84, 64);
        setCheckable(true);
        setAutoExclusive(false);
        setFocusPolicy(Qt::NoFocus);
        setContextMenuPolicy(Qt::CustomContextMenu);
        setToolTip(tr("Left click: trigger clip on a mix layer (blue outline when playing)\n"
                      "Right click: menu (peek preview, copy, paste)\n"
                      "Hover: short filmstrip preview\n"
                      "Double-click: filters / nodes\n"
                      "Mapping mode (Ctrl+M): click cell, then key or MIDI note"));

        m_triggerTimer.setSingleShot(true);
        connect(&m_triggerTimer, &QTimer::timeout, this, [this] {
            emit cellTriggeredDelayed(m_index);
        });
        connect(this, &QPushButton::clicked, this, &CellButton::onClicked);

        m_filmTimer.setInterval(220);
        connect(&m_filmTimer, &QTimer::timeout, this, [this] {
            if (!m_cellHovered || m_filmFrames.size() < 2) {
                return;
            }
            m_filmIndex = (m_filmIndex + 1) % m_filmFrames.size();
            update();
        });
    }

    void setCellState(bool hasContent, const QString& label)
    {
        if (m_hasContent != hasContent || m_label != label) {
            m_hasContent = hasContent;
            m_label = label;
            update();
        }
    }

    void setBigLabel(const QString& text)
    {
        if (m_bigLabel != text) {
            m_bigLabel = text;
            update();
        }
    }

    /// Mix slot index 1..13 (GPU layer) when this cell is routed to that mixer slot; -1 = none.
    void setMixSlot(int slotIndex)
    {
        if (m_mixSlot != slotIndex) {
            m_mixSlot = slotIndex;
            update();
        }
    }

    void setPeekHighlight(bool on)
    {
        if (m_peekHighlight != on) {
            m_peekHighlight = on;
            update();
        }
    }

    void setMixerFilterActive(bool on)
    {
        if (m_mixerFilterActive != on) {
            m_mixerFilterActive = on;
            update();
        }
    }

    void setFilmstrip(const QVector<QImage>& frames)
    {
        m_filmTimer.stop();
        m_filmFrames.clear();
        const int n = qMin(5, frames.size());
        m_filmFrames.reserve(n);
        for (int i = 0; i < n; ++i) {
            if (!frames[i].isNull()) {
                m_filmFrames.append(frames[i]);
            }
        }
        m_filmIndex = 0;
        if (m_cellHovered && m_filmFrames.size() > 1) {
            m_filmTimer.start();
        }
        update();
    }

    void clearThumbnail()
    {
        m_filmTimer.stop();
        m_filmFrames.clear();
        m_filmIndex = 0;
        update();
    }

    int cellIndex() const { return m_index; }

    void setMidiMapMode(bool on)
    {
        if (m_midiMapMode == on) {
            return;
        }
        m_midiMapMode = on;
        update();
    }

    void setKeyboardShortcutLabel(const QString& label)
    {
        if (m_keyboardShortcutLabel == label) {
            return;
        }
        m_keyboardShortcutLabel = label;
        update();
    }

    void setMidiTriggerLabel(const QString& label)
    {
        if (m_midiTriggerLabel == label) {
            return;
        }
        m_midiTriggerLabel = label;
        update();
    }

signals:
    void cellPressedForSelection(int index);
    void cellTriggeredDelayed(int index);
    void cellEditRequested(int index);
    /// MIDI mapping edit mode: left-click should learn trigger for this cell index.
    void midiLearnCellRequested(int index);

protected:
    void enterEvent(QEnterEvent* e) override
    {
        QPushButton::enterEvent(e);
        m_cellHovered = true;
        if (m_filmFrames.size() > 1) {
            m_filmTimer.start();
        }
        update();
    }

    void leaveEvent(QEvent* e) override
    {
        QPushButton::leaveEvent(e);
        m_cellHovered = false;
        m_filmTimer.stop();
        m_filmIndex = 0;
        update();
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton) {
            QPushButton::mousePressEvent(e);
            emit cellPressedForSelection(m_index);
            e->accept();
            return;
        }
        QPushButton::mousePressEvent(e);
    }

    void mouseDoubleClickEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) {
            e->accept();
            return;
        }
        m_triggerTimer.stop();
        m_skipNextClick = true;
        emit cellEditRequested(m_index);
        QPushButton::mouseDoubleClickEvent(e);
    }

    void paintEvent(QPaintEvent* /*e*/) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        constexpr qreal kCellRadius = 6.0;

        const QRectF r = rect().adjusted(1, 1, -1, -1);
        QPainterPath clipPath;
        clipPath.addRoundedRect(r, kCellRadius, kCellRadius);

        const QImage* thumb = nullptr;
        if (!m_filmFrames.isEmpty()) {
            if (m_cellHovered && m_filmFrames.size() > 1) {
                thumb = &m_filmFrames[m_filmIndex % m_filmFrames.size()];
            } else {
                thumb = &m_filmFrames.front();
            }
        }
        if (thumb && !thumb->isNull()) {
            p.setClipPath(clipPath);
            const QSizeF thumbSize(thumb->width(), thumb->height());
            const QSizeF scaled = thumbSize.scaled(r.size(), Qt::KeepAspectRatio);
            QRectF drawRect(QPointF(r.center() - QPointF(scaled.width() / 2, scaled.height() / 2)),
                            scaled);
            p.drawImage(drawRect, *thumb);
            p.setClipping(false);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 100));
            p.drawPath(clipPath);
        } else {
            QLinearGradient fill(r.topLeft(), r.bottomRight());
            const bool onMix = (m_mixSlot >= 0);
            const bool sel   = isChecked();
            if (sel && onMix) {
                fill.setColorAt(0, QColor(72, 58, 42));
                fill.setColorAt(0.5, QColor(44, 52, 68));
                fill.setColorAt(1, QColor(32, 30, 28));
            } else if (sel) {
                fill.setColorAt(0, QColor(78, 52, 36));
                fill.setColorAt(1, QColor(38, 28, 22));
            } else if (onMix) {
                fill.setColorAt(0, QColor(40, 52, 72));
                fill.setColorAt(1, QColor(24, 30, 44));
            } else if (m_hasContent) {
                fill.setColorAt(0, QColor(44, 58, 68));
                fill.setColorAt(1, QColor(30, 40, 48));
            } else {
                fill.setColorAt(0, QColor(46, 50, 60));
                fill.setColorAt(1, QColor(24, 26, 32));
            }
            p.setPen(QPen(QColor(58, 64, 78), 1));
            p.setBrush(fill);
            p.drawRoundedRect(r, kCellRadius, kCellRadius);
        }

        // Blue = routed to a mix layer (left-click trigger). Orange = clip preview peek (right-click).
        const QColor kOrangePeek(255, 145, 64);
        const QColor kBlueMix(58, 156, 255);
        const QColor kBorderIdle(52, 58, 72);

        const bool onMix  = (m_mixSlot >= 0);
        const bool peek   = m_peekHighlight;
        const bool sel    = isChecked();
        const bool highlightOrange = sel || peek;
        const QColor kPurpleMixerFx(190, 110, 255);

        const bool showShadow = onMix || highlightOrange || m_mixerFilterActive;
        if (showShadow) {
            drawCellDropShadow(p, r, kCellRadius);
        }

        p.setBrush(Qt::NoBrush);
        if (onMix) {
            drawGlowFrame(p, r, kCellRadius, kBlueMix, 3);
        }
        if (m_mixerFilterActive) {
            const QRectF purpleRect = onMix ? r.adjusted(5, 5, -5, -5) : r;
            const qreal purpleRadius = onMix ? 4.0 : kCellRadius;
            drawGlowFrame(p, purpleRect, purpleRadius, kPurpleMixerFx, onMix ? 2 : 3);
        }
        if (highlightOrange) {
            const QRectF orangeRect = (onMix || m_mixerFilterActive) ? r.adjusted(5, 5, -5, -5) : r;
            const qreal orangeRadius = (onMix || m_mixerFilterActive) ? 4.0 : kCellRadius;
            const int orangePasses = (onMix || m_mixerFilterActive) ? 2 : 3;
            drawGlowFrame(p, orangeRect, orangeRadius, kOrangePeek, orangePasses);
        } else if (!onMix && !m_mixerFilterActive) {
            p.setPen(QPen(kBorderIdle, 1));
            p.drawRoundedRect(r, kCellRadius, kCellRadius);
        }

        if (!m_keyboardShortcutLabel.isEmpty()) {
            p.setPen(QColor(200, 220, 255));
            QFont keyFont = p.font();
            keyFont.setPointSizeF(std::max(6.0, keyFont.pointSizeF() * 0.62));
            keyFont.setBold(true);
            p.setFont(keyFont);
            const QRectF keyRect = r.adjusted(3, 2, -3, -3);
            p.drawText(keyRect, Qt::AlignTop | Qt::AlignRight, m_keyboardShortcutLabel);
        }

        p.setPen(QColor(255, 255, 255));
        QFont small = p.font();
        small.setPointSizeF(small.pointSizeF() * 0.82);
        small.setBold(true);
        p.setFont(small);
        p.drawText(r.adjusted(5, 3, -5, -5), Qt::AlignTop | Qt::AlignLeft,
                   QString::number(m_index + 1));

        if (!m_bigLabel.isEmpty() && m_filmFrames.isEmpty()) {
            QFont big = p.font();
            big.setBold(true);
            big.setPointSizeF(big.pointSizeF() * 1.6);
            p.setFont(big);
            p.setPen(QColor(235, 240, 255));
            p.drawText(r, Qt::AlignCenter, m_bigLabel);
        } else if (!m_label.isEmpty() && m_filmFrames.isEmpty()) {
            QFont f = p.font();
            f.setBold(false);
            p.setFont(f);
            p.drawText(r.adjusted(5, 3, -5, -5), Qt::AlignBottom | Qt::AlignLeft,
                       m_label);
        }

        if (m_midiMapMode) {
            p.setPen(QPen(QColor(120, 255, 160), 2));
            p.setBrush(QColor(80, 220, 120, 85));
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), kCellRadius, kCellRadius);

            if (!m_midiTriggerLabel.isEmpty()) {
                QFont midiFont = p.font();
                midiFont.setBold(true);
                midiFont.setPointSizeF(std::max(6.0, midiFont.pointSizeF() * 0.62));
                p.setFont(midiFont);
                const QFontMetrics fm(midiFont);
                const int padH = 3;
                const int padV = 1;
                const int tw = fm.horizontalAdvance(m_midiTriggerLabel);
                const int th = fm.height();
                QRect badgeRect(width() - tw - padH * 2 - 4,
                                height() - th - padV * 2 - 4,
                                tw + padH * 2,
                                th + padV * 2);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(20, 32, 24, 210));
                p.drawRoundedRect(badgeRect, 3, 3);
                p.setPen(QColor(180, 255, 200));
                p.drawText(badgeRect, Qt::AlignCenter, m_midiTriggerLabel);
            }
        }
    }

private slots:
    void onClicked()
    {
        if (m_skipNextClick) {
            m_skipNextClick = false;
            return;
        }
        if (m_midiMapMode) {
            emit midiLearnCellRequested(m_index);
            return;
        }
        m_triggerTimer.start(QApplication::doubleClickInterval());
    }

private:
    int     m_index = 0;
    bool    m_hasContent = false;
    QString m_label;
    QString m_bigLabel;
    bool    m_peekHighlight = false;
    QTimer           m_triggerTimer;
    QTimer           m_filmTimer;
    QVector<QImage> m_filmFrames;
    int              m_filmIndex = 0;
    bool             m_cellHovered = false;
    bool             m_skipNextClick = false;
    int              m_mixSlot = -1;
    bool             m_mixerFilterActive = false;
    bool             m_midiMapMode = false;
    QString          m_keyboardShortcutLabel;
    QString          m_midiTriggerLabel;
};

BankGridWidget::BankGridWidget(QWidget* parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    const int cores = QThread::idealThreadCount();
    m_preloadPool.setMaxThreadCount(qBound(4, cores > 0 ? cores : 4, 12));
    buildUi();
}

void BankGridWidget::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);
    outer->setSpacing(4);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(6);

    m_bankTabs = new QTabBar(this);
    m_bankTabs->setExpanding(false);
    m_bankTabs->setDrawBase(false);
    m_bankTabs->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_bankTabs, &QTabBar::currentChanged,
            this, &BankGridWidget::onBankTabChanged);
    connect(m_bankTabs, &QTabBar::customContextMenuRequested,
            this, &BankGridWidget::onBankTabContextMenu);

    header->addWidget(m_bankTabs, 1);
    outer->addLayout(header);

    m_grid = new QGridLayout;
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(4);
    m_grid->setVerticalSpacing(4);
    rebuildGridButtons();
    outer->addLayout(m_grid, 1);
}

void BankGridWidget::rebuildGridButtons()
{
    while (QLayoutItem* item = m_grid->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            delete w;
        }
        delete item;
    }
    m_cells.clear();
    for (int r = 0; r < m_gridRows; ++r) {
        for (int c = 0; c < m_gridCols; ++c) {
            const int idx = r * m_gridCols + c;
            auto* btn = new CellButton(idx, this);
            connect(btn, &CellButton::cellPressedForSelection, this, [this, idx](int) {
                m_selectedCell = idx;
                for (int j = 0; j < m_cells.size(); ++j) {
                    m_cells[j]->setChecked(j == idx);
                }
                emit cellSelected(m_bankSetIndex, m_bankIndex, idx);
            });
            connect(btn, &CellButton::cellTriggeredDelayed, this, [this, idx](int) {
                emit cellTriggered(m_bankSetIndex, m_bankIndex, idx);
            });
            connect(btn, &CellButton::cellEditRequested, this, [this, idx](int) {
                emit cellEditRequested(m_bankSetIndex, m_bankIndex, idx);
            });
            connect(btn, &QWidget::customContextMenuRequested, this, [this, idx, btn](const QPoint& pos) {
                selectCell(idx);
                emit cellContextMenuRequested(m_bankSetIndex, m_bankIndex, idx,
                                              btn->mapToGlobal(pos));
            });
            connect(btn, &CellButton::midiLearnCellRequested, this, [this, idx](int) {
                emit midiLearnCellTriggerRequested(m_bankSetIndex, m_bankIndex, idx);
            });
            m_grid->addWidget(btn, r, c);
            m_cells.append(btn);
        }
    }
}

void BankGridWidget::setProject(pvj::core::Project* project)
{
    m_project = project;
    m_bankSetIndex = 0;
    m_bankIndex    = 0;
    m_selectedCell = -1;
    ++m_thumbGeneration;
    m_filmCache.clear();
    m_thumbPending.clear();
    m_preloadBatchIds.clear();
    m_preloadQueue.clear();
    m_preloadRunningIds.clear();
    m_enrichQueue.clear();
    m_enrichRunningIds.clear();
    m_preloadBatchTotal     = 0;
    m_preloadCompletedCount = 0;
    if (m_project) {
        const int rows = qBound(1, m_project->settings.matrix.gridRows, 16);
        const int cols = qBound(1, m_project->settings.matrix.gridCols, 16);
        setGridDimensions(rows, cols);
    }
    refresh();
}

void BankGridWidget::preloadProjectMediaThumbnails()
{
    if (!m_project) {
        return;
    }

    using pvj::core::VisualType;

    m_preloadBatchIds.clear();
    m_preloadQueue.clear();
    m_preloadRunningIds.clear();
    m_enrichQueue.clear();
    m_enrichRunningIds.clear();
    m_preloadBatchTotal     = 0;
    m_preloadCompletedCount = 0;

    QSet<QUuid> seen;
    auto queueIfExists = [this, &seen](const QUuid& id, const QString& path) {
        if (id.isNull() || path.isEmpty() || !QFileInfo::exists(path) || seen.contains(id)) {
            return;
        }
        seen.insert(id);
        ++m_preloadBatchTotal;
        if (m_filmCache.contains(id)) {
            ++m_preloadCompletedCount;
            return;
        }
        m_preloadBatchIds.insert(id);
        // Defer the actual extraction: jobs are launched in bounded batches by
        // startQueuedPreloadJobs() so progress updates arrive continuously.
        if (!m_thumbPending.contains(id)) {
            m_preloadQueue.append(qMakePair(id, path));
        }
    };

    for (const pvj::core::MediaItem& item : m_project->mediaLibrary) {
        queueIfExists(item.id, item.path);
    }

    for (const auto& set : m_project->bankSets) {
        for (const auto& bank : set.banks) {
            for (const auto& cell : bank.cells) {
                if (cell.visual.type != VisualType::Media || cell.visual.mediaId.isNull()) {
                    continue;
                }
                const pvj::core::MediaItem* m = m_project->findMedia(cell.visual.mediaId);
                if (m) {
                    queueIfExists(cell.visual.mediaId, m->path);
                }
            }
        }
    }

    if (m_preloadBatchTotal == 0) {
        emit mediaPreloadFinished();
    } else {
        emit mediaPreloadProgress(m_preloadCompletedCount, m_preloadBatchTotal);
        startQueuedPreloadJobs();
        if (m_preloadBatchIds.isEmpty()) {
            emit mediaPreloadFinished();
        }
    }
}

void BankGridWidget::startQueuedPreloadJobs()
{
    const int maxConcurrent = m_preloadPool.maxThreadCount();
    while (m_preloadRunningIds.size() < maxConcurrent && !m_preloadQueue.isEmpty()) {
        const QPair<QUuid, QString> job = m_preloadQueue.takeFirst();
        if (job.first.isNull() || m_filmCache.contains(job.first)) {
            continue;
        }
        m_preloadRunningIds.insert(job.first);
        ensureThumbnail(job.first, job.second, /*forPreload=*/true);
    }
}

void BankGridWidget::markPreloadItemFinished(const QUuid& mediaId)
{
    m_preloadRunningIds.remove(mediaId);
    if (!m_preloadBatchIds.remove(mediaId)) {
        // Not part of the active batch (e.g. a one-off request) — still keep the
        // preload queue flowing in case a slot just freed up.
        startQueuedPreloadJobs();
        return;
    }
    ++m_preloadCompletedCount;
    if (m_preloadBatchTotal > 0) {
        emit mediaPreloadProgress(m_preloadCompletedCount, m_preloadBatchTotal);
    }
    // Launch the next queued job before deciding whether the batch is done, so
    // m_preloadBatchIds stays non-empty while work remains.
    startQueuedPreloadJobs();
    if (m_preloadBatchIds.isEmpty()) {
        m_preloadBatchTotal     = 0;
        m_preloadCompletedCount = 0;
        m_preloadQueue.clear();
        m_preloadRunningIds.clear();
        emit mediaPreloadFinished();
        queueFilmstripEnrichAfterPreload();
    }
}

void BankGridWidget::queueFilmstripEnrichAfterPreload()
{
    if (!m_project) {
        return;
    }
    m_enrichQueue.clear();
    for (auto it = m_filmCache.constBegin(); it != m_filmCache.constEnd(); ++it) {
        if (it.value().size() >= 2) {
            continue;
        }
        const pvj::core::MediaItem* item = m_project->findMedia(it.key());
        if (!item || item->path.isEmpty() || !QFileInfo::exists(item->path)) {
            continue;
        }
        m_enrichQueue.append(qMakePair(it.key(), item->path));
    }
    startQueuedEnrichJobs();
}

void BankGridWidget::startQueuedEnrichJobs()
{
    constexpr int kMaxConcurrentEnrich = 2;
    while (m_enrichRunningIds.size() < kMaxConcurrentEnrich && !m_enrichQueue.isEmpty()) {
        const QPair<QUuid, QString> job = m_enrichQueue.takeFirst();
        if (job.first.isNull()) {
            continue;
        }
        m_enrichRunningIds.insert(job.first);
        enrichFilmstripAsync(job.first, job.second);
    }
}

void BankGridWidget::requestThumbnailForMedia(const QUuid& mediaId)
{
    if (!m_project || mediaId.isNull()) {
        return;
    }
    const pvj::core::MediaItem* item = m_project->findMedia(mediaId);
    if (!item || item->path.isEmpty() || !QFileInfo::exists(item->path)) {
        return;
    }
    ensureThumbnail(mediaId, item->path);
}

void BankGridWidget::setGridDimensions(int rows, int cols)
{
    const int nextRows = qBound(1, rows, 16);
    const int nextCols = qBound(1, cols, 16);
    if (nextRows == m_gridRows && nextCols == m_gridCols) {
        return;
    }
    m_gridRows = nextRows;
    m_gridCols = nextCols;
    rebuildGridButtons();
    if (m_selectedCell >= m_cells.size()) {
        m_selectedCell = -1;
    }
    if (m_project) {
        refresh();
    }
}

void BankGridWidget::selectCell(int cellIndex)
{
    if (cellIndex < 0 || cellIndex >= m_cells.size()) {
        return;
    }
    m_selectedCell = cellIndex;
    for (int j = 0; j < m_cells.size(); ++j) {
        m_cells[j]->setChecked(j == m_selectedCell);
    }
    emit cellSelected(m_bankSetIndex, m_bankIndex, m_selectedCell);
}

void BankGridWidget::setMixSlotPlayback(const std::array<MixSlotCellRef, kMixSlotCount>& mixSlotRefs)
{
    m_mixSlots = mixSlotRefs;
    updateCellVisuals(false);
}

void BankGridWidget::setActiveMixerFilterCells(const QList<MixSlotCellRef>& refs)
{
    m_activeMixerFilterCells = refs;
    updateCellVisuals(false);
}

void BankGridWidget::setPeekCellHighlight(const MixSlotCellRef& ref)
{
    m_peekRef = ref;
    updateCellVisuals(false);
}

void BankGridWidget::revealAndSelectCell(int bankSetIndex, int bankIndex, int cellIndex)
{
    if (!m_project || m_project->bankSets.isEmpty()) {
        return;
    }
    m_bankSetIndex = qBound(0, bankSetIndex, m_project->bankSets.size() - 1);
    const auto& set = m_project->bankSets[m_bankSetIndex];
    if (set.banks.isEmpty()) {
        return;
    }
    m_bankIndex = qBound(0, bankIndex, set.banks.size() - 1);
    const auto& bank = set.banks[m_bankIndex];
    const int nCells = qMin(int(m_cells.size()), bank.cells.size());
    if (nCells <= 0) {
        m_selectedCell = -1;
        rebuildBankTabs();
        updateCellVisuals();
        emit bankSelected(m_bankSetIndex, m_bankIndex);
        return;
    }
    const int maxCell = nCells - 1;
    m_selectedCell = qBound(0, cellIndex, maxCell);

    rebuildBankTabs();
    {
        QSignalBlocker block(m_bankTabs);
        m_bankTabs->setCurrentIndex(m_bankIndex);
    }

    updateCellVisuals();
    for (int j = 0; j < m_cells.size(); ++j) {
        m_cells[j]->setChecked(j == m_selectedCell);
    }
    emit bankSelected(m_bankSetIndex, m_bankIndex);
    emit cellSelected(m_bankSetIndex, m_bankIndex, m_selectedCell);
}

void BankGridWidget::refresh()
{
    rebuildBankTabs();
    updateCellVisuals();
}

void BankGridWidget::refreshCellLabels()
{
    updateCellVisuals(false);
}

void BankGridWidget::rebuildBankTabs()
{
    QSignalBlocker block(m_bankTabs);
    while (m_bankTabs->count() > 0) {
        m_bankTabs->removeTab(0);
    }

    if (!m_project || m_bankSetIndex < 0 || m_bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    const auto& set = m_project->bankSets[m_bankSetIndex];
    for (int i = 0; i < set.banks.size(); ++i) {
        const QString name = set.banks[i].name.isEmpty()
            ? tr("Bank %1").arg(i + 1)
            : set.banks[i].name;
        m_bankTabs->addTab(name);
    }
    m_bankTabs->setCurrentIndex(qBound(0, m_bankIndex, m_bankTabs->count() - 1));
}

void BankGridWidget::updateCellVisuals(bool reloadThumbnails)
{
    using pvj::core::VisualType;

    if (!m_project || m_bankSetIndex < 0 || m_bankSetIndex >= m_project->bankSets.size()) {
        for (auto* b : m_cells) {
            b->setCellState(false, {});
            b->setBigLabel({});
            b->setChecked(false);
            b->setEnabled(false);
            b->setMixSlot(-1);
            b->setPeekHighlight(false);
            b->setMidiMapMode(m_midiMappingEditMode);
            b->setKeyboardShortcutLabel({});
        }
        return;
    }

    const auto& set = m_project->bankSets[m_bankSetIndex];
    if (m_bankIndex < 0 || m_bankIndex >= set.banks.size()) {
        return;
    }
    const auto& bank = set.banks[m_bankIndex];

    for (int i = 0; i < m_cells.size(); ++i) {
        auto* btn = m_cells[i];
        btn->setEnabled(true);
        btn->setChecked(i == m_selectedCell);
        if (i >= bank.cells.size()) {
            btn->setCellState(false, {});
            btn->setBigLabel({});
            btn->setMixSlot(-1);
            btn->setPeekHighlight(false);
            btn->setMixerFilterActive(false);
            continue;
        }
        int mixSlot = -1;
        for (int j = 0; j < kMixSlotCount; ++j) {
            if (j == 0) {
                continue;
            }
            const MixSlotCellRef& ref = m_mixSlots[static_cast<size_t>(j)];
            if (ref.bankSet == m_bankSetIndex && ref.bank == m_bankIndex && ref.cell == i) {
                mixSlot = j;
                break;
            }
        }
        btn->setMixSlot(mixSlot);
        const bool peekHere = (m_peekRef.bankSet == m_bankSetIndex
                               && m_peekRef.bank == m_bankIndex
                               && m_peekRef.cell == i);
        btn->setPeekHighlight(peekHere);
        bool mixerFxActive = false;
        for (const MixSlotCellRef& ref : m_activeMixerFilterCells) {
            if (ref.bankSet == m_bankSetIndex && ref.bank == m_bankIndex && ref.cell == i) {
                mixerFxActive = true;
                break;
            }
        }
        btn->setMixerFilterActive(mixerFxActive);
        const auto& cell = bank.cells[i];
        bool has = false;
        QString label;
        QString bigLabel;
        if (reloadThumbnails) {
            btn->clearThumbnail();
        }
        if (cell.visual.type == VisualType::Media) {
            has = !cell.visual.mediaId.isNull();
            if (has) {
                const auto* m = m_project->findMedia(cell.visual.mediaId);
                if (m) {
                    label = m->displayName.isEmpty()
                        ? QFileInfo(m->path).fileName() : m->displayName;
                    if (reloadThumbnails) {
                        if (m_filmCache.contains(cell.visual.mediaId)) {
                            btn->setFilmstrip(m_filmCache.value(cell.visual.mediaId));
                        } else {
                            ensureThumbnail(cell.visual.mediaId, m->path);
                        }
                    }
                }
            }
        } else if (cell.visual.type == VisualType::Generator) {
            has = true;
            if (cell.visual.generator == pvj::core::GeneratorKind::InternalFeedback) {
                bigLabel = cell.name.isEmpty() ? tr("FB") : cell.name;
            } else {
                label = tr("GEN");
            }
        } else if (cell.visual.type == VisualType::MixerFilter) {
            has = true;
            bigLabel = cell.name.isEmpty() ? tr("FX") : cell.name;
        } else {
            const bool filterOnly = std::any_of(
                cell.filterChain.cbegin(), cell.filterChain.cend(),
                [](const pvj::core::CellFilterNode& n) {
                    return !pvj::core::isFeedbackMarkerNode(n.typeId);
                });
            if (filterOnly) {
                has = true;
                bigLabel = cell.name.isEmpty() ? tr("FX") : cell.name;
            }
        }
        btn->setCellState(has, label);
        btn->setBigLabel(bigLabel);
        btn->setMidiMapMode(m_midiMappingEditMode);
        QString keyBadge;
        const QString keyText =
            m_project->keyboardTriggerForCell(m_bankSetIndex, m_bankIndex, i);
        if (!keyText.isEmpty()) {
            const QKeySequence seq(keyText);
            keyBadge = seq.toString(QKeySequence::NativeText);
            if (keyBadge.size() > 6) {
                keyBadge = keyBadge.left(5) + QChar(0x2026);
            }
        }
        btn->setKeyboardShortcutLabel(keyBadge);

        QString midiBadge = QStringLiteral("—");
        if (m_project) {
            int midiCh = -1;
            int midiNote = -1;
            m_project->midiCellTriggerForCell(m_bankSetIndex, m_bankIndex, i, &midiCh, &midiNote);
            if (midiCh >= 0 && midiNote >= 0) {
                midiBadge = QStringLiteral("CH%1 N%2").arg(midiCh + 1).arg(midiNote);
            }
        }
        btn->setMidiTriggerLabel(midiBadge);
    }
}

void BankGridWidget::setMidiMappingEditMode(bool on)
{
    if (m_midiMappingEditMode == on) {
        return;
    }
    m_midiMappingEditMode = on;
    for (auto* b : m_cells) {
        b->setMidiMapMode(on);
    }
}

bool BankGridWidget::midiMappingEditMode() const
{
    return m_midiMappingEditMode;
}

namespace {

QVector<QImage> extractThumbnailFrames(const QString& absolutePath, bool fastSingleFrame)
{
    QVector<QImage> images;
    const QSize thumbSize(160, 96);

    if (fastSingleFrame) {
        const pvj::video::ThumbnailResult r =
            pvj::video::ThumbnailExtractor::extract(absolutePath, thumbSize, -1);
        if (r.ok && !r.image.isNull()) {
            images.append(r.image);
        }
        return images;
    }

    const QList<QImage> keyframes =
        pvj::video::ThumbnailExtractor::extractPreviewKeyframes(absolutePath, 5, thumbSize);
    images.reserve(keyframes.size());
    for (const QImage& im : keyframes) {
        if (!im.isNull()) {
            images.append(im);
        }
    }

    if (images.size() < 2) {
        images.clear();
        const QString suf = QFileInfo(absolutePath).suffix().toLower();
        static const QStringList kRaster = {
            QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
            QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("webp"),
        };
        if (kRaster.contains(suf)) {
            QImage im;
            if (im.load(absolutePath)) {
                images.append(im.scaled(thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
        if (images.isEmpty()) {
            const pvj::video::ThumbnailResult r =
                pvj::video::ThumbnailExtractor::extract(absolutePath, thumbSize, -1);
            if (r.ok && !r.image.isNull()) {
                images.append(r.image);
            }
        }
    }
    return images;
}

void deliverThumbnailFrames(BankGridWidget* widget,
                            const QUuid& mediaId,
                            const QVector<QImage>& framesCopy,
                            quint64 generation,
                            bool afterEnrich)
{
    if (!widget) {
        return;
    }
    QMetaObject::invokeMethod(
        widget,
        [safe = QPointer<BankGridWidget>(widget),
         mediaId,
         framesCopy,
         generation,
         afterEnrich]() {
            if (safe) {
                safe->deliverThumbnailFromWorker(mediaId, framesCopy, generation, afterEnrich);
            }
        },
        Qt::QueuedConnection);
}

} // namespace

void BankGridWidget::ensureThumbnail(const QUuid& mediaId, const QString& absolutePath, bool forPreload)
{
    if (mediaId.isNull() || absolutePath.isEmpty()) {
        return;
    }
    if (m_thumbPending.contains(mediaId)) {
        return;
    }
    m_thumbPending.insert(mediaId);

    QPointer<BankGridWidget> safe(this);
    const quint64 generation = m_thumbGeneration;
    auto worker = [safe, mediaId, absolutePath, generation, forPreload]() {
        const QVector<QImage> images =
            extractThumbnailFrames(absolutePath, /*fastSingleFrame=*/forPreload);
        if (!safe) {
            return;
        }
        deliverThumbnailFrames(safe.data(), mediaId, images, generation, /*afterEnrich=*/false);
    };

    if (forPreload) {
        (void)QtConcurrent::run(&m_preloadPool, worker);
    } else {
        (void)QtConcurrent::run(worker);
    }
}

void BankGridWidget::enrichFilmstripAsync(const QUuid& mediaId, const QString& absolutePath)
{
    if (mediaId.isNull() || absolutePath.isEmpty()) {
        m_enrichRunningIds.remove(mediaId);
        startQueuedEnrichJobs();
        return;
    }
    if (m_thumbPending.contains(mediaId)) {
        m_enrichRunningIds.remove(mediaId);
        startQueuedEnrichJobs();
        return;
    }
    m_thumbPending.insert(mediaId);

    QPointer<BankGridWidget> safe(this);
    const quint64 generation = m_thumbGeneration;
    (void)QtConcurrent::run([safe, mediaId, absolutePath, generation]() {
        const QVector<QImage> images =
            extractThumbnailFrames(absolutePath, /*fastSingleFrame=*/false);
        if (!safe) {
            return;
        }
        deliverThumbnailFrames(safe.data(), mediaId, images, generation, /*afterEnrich=*/true);
    });
}

QImage BankGridWidget::cachedCellPreview(int bankSetIndex, int bankIndex, int cellIndex) const
{
    if (!m_project || bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()
        || bankIndex < 0 || cellIndex < 0) {
        return {};
    }
    const auto& set = m_project->bankSets[bankSetIndex];
    if (bankIndex >= set.banks.size()) {
        return {};
    }
    const auto& bank = set.banks[bankIndex];
    if (cellIndex >= bank.cells.size()) {
        return {};
    }
    const auto& cell = bank.cells[cellIndex];
    if (cell.visual.type != pvj::core::VisualType::Media || cell.visual.mediaId.isNull()) {
        return {};
    }
    const auto it = m_filmCache.constFind(cell.visual.mediaId);
    if (it == m_filmCache.constEnd() || it->isEmpty() || it->first().isNull()) {
        return {};
    }
    return it->first();
}

void BankGridWidget::deliverThumbnailFromWorker(const QUuid& mediaId,
                                              const QVector<QImage>& framesCopy,
                                              quint64 generation, bool afterEnrich)
{
    if (m_thumbGeneration != generation) {
        return;
    }
    QVector<QImage> framesOut;
    framesOut.reserve(framesCopy.size());
    for (const QImage& im : framesCopy) {
        if (!im.isNull()) {
            QImage safeImage = im;
            if (safeImage.format() != QImage::Format_RGB32) {
                safeImage = safeImage.convertToFormat(QImage::Format_RGB32);
            }
            framesOut.push_back(safeImage);
        }
    }
    applyThumbnailStrip(mediaId, framesOut);
    if (afterEnrich) {
        m_enrichRunningIds.remove(mediaId);
        startQueuedEnrichJobs();
    }
}

void BankGridWidget::applyThumbnailStrip(const QUuid& mediaId, const QVector<QImage>& frames)
{
    m_thumbPending.remove(mediaId);
    markPreloadItemFinished(mediaId);
    if (!frames.isEmpty()) {
        m_filmCache.insert(mediaId, frames);
    } else {
        m_filmCache.remove(mediaId);
    }
    emit cellPreviewCacheUpdated(mediaId);
    if (!m_project) {
        return;
    }
    if (m_bankSetIndex < 0 || m_bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    const auto& set = m_project->bankSets[m_bankSetIndex];
    if (m_bankIndex < 0 || m_bankIndex >= set.banks.size()) {
        return;
    }
    const auto& bk = set.banks[m_bankIndex];
    for (int i = 0; i < m_cells.size() && i < bk.cells.size(); ++i) {
        if (bk.cells[i].visual.mediaId == mediaId) {
            if (frames.isEmpty()) {
                m_cells[i]->clearThumbnail();
            } else {
                m_cells[i]->setFilmstrip(frames);
            }
        }
    }
}

void BankGridWidget::onBankTabContextMenu(const QPoint& pos)
{
    if (!m_project || m_bankSetIndex < 0 || m_bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    const int tab = m_bankTabs->tabAt(pos);
    if (tab < 0) {
        return;
    }
    const auto& set = m_project->bankSets[m_bankSetIndex];
    if (tab >= set.banks.size()) {
        return;
    }
    selectBank(tab);
    const QPoint global = m_bankTabs->mapToGlobal(pos);
    emit bankContextMenuRequested(m_bankSetIndex, tab, global);
}

void BankGridWidget::onBankTabChanged(int idx)
{
    if (idx < 0) {
        return;
    }
    m_bankIndex = idx;
    m_selectedCell = -1;
    updateCellVisuals();
    emit bankSelected(m_bankSetIndex, m_bankIndex);
}

void BankGridWidget::setBankSetIndex(int bankSetIndex)
{
    if (!m_project || m_project->bankSets.isEmpty()) {
        return;
    }
    m_bankSetIndex = qBound(0, bankSetIndex, m_project->bankSets.size() - 1);
    m_bankIndex    = 0;
    m_selectedCell = -1;
    refresh();
    emit bankSelected(m_bankSetIndex, m_bankIndex);
}

void BankGridWidget::stepBank(int delta)
{
    if (!m_project) {
        return;
    }
    if (m_bankSetIndex < 0 || m_bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    const auto& set = m_project->bankSets[m_bankSetIndex];
    if (set.banks.isEmpty()) {
        return;
    }
    m_bankIndex = qBound(0, m_bankIndex + delta, set.banks.size() - 1);
    QSignalBlocker block(m_bankTabs);
    m_bankTabs->setCurrentIndex(m_bankIndex);
    updateCellVisuals();
    emit bankSelected(m_bankSetIndex, m_bankIndex);
}

void BankGridWidget::selectBank(int bankIndex)
{
    if (!m_project) {
        return;
    }
    if (m_bankSetIndex < 0 || m_bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    const auto& set = m_project->bankSets[m_bankSetIndex];
    if (set.banks.isEmpty()) {
        return;
    }
    m_bankIndex = qBound(0, bankIndex, set.banks.size() - 1);
    QSignalBlocker block(m_bankTabs);
    m_bankTabs->setCurrentIndex(m_bankIndex);
    updateCellVisuals();
    emit bankSelected(m_bankSetIndex, m_bankIndex);
}

int BankGridWidget::cellIndexAtPosition(const QPoint& posInThisWidget) const
{
    for (int i = 0; i < m_cells.size(); ++i) {
        if (m_cells[i]->geometry().contains(posInThisWidget)) {
            return i;
        }
    }
    return -1;
}

void BankGridWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (mimeDataHasLocalFile(event->mimeData())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void BankGridWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (!mimeDataHasLocalFile(event->mimeData())) {
        event->ignore();
        return;
    }
    if (cellIndexAtPosition(event->position().toPoint()) >= 0) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void BankGridWidget::dropEvent(QDropEvent* event)
{
    const QString path = firstLocalFilePath(event->mimeData());
    if (path.isEmpty()) {
        event->ignore();
        return;
    }
    const int cell = cellIndexAtPosition(event->position().toPoint());
    if (cell < 0) {
        event->ignore();
        return;
    }
    emit mediaDroppedOnCell(m_bankSetIndex, m_bankIndex, cell, path);
    event->acceptProposedAction();
}

} // namespace pvj::app

#include "BankGridWidget.moc"

#pragma once

#include "core/Model.h"

#include <array>
#include <QHash>
#include <QImage>
#include <QPair>
#include <QSet>
#include <QThreadPool>
#include <QVector>
#include <QUuid>
#include <QWidget>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QGridLayout;
class QTabBar;

namespace pvj::core {
class Project;
}

namespace pvj::app {

static constexpr int kMixSlotCount = 14;

class CellButton;

/// One occupied mix slot (which bank cell is feeding the mixer).
struct MixSlotCellRef {
    int bankSet = -1;
    int bank    = -1;
    int cell    = -1;
};

// Configurable grid of cell triggers plus a tab bar for switching banks inside the bank set.
class BankGridWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BankGridWidget(QWidget* parent = nullptr);

    void setProject(pvj::core::Project* project);
    /// Queue thumbnail extraction for every media item in the project (async, deduplicated).
    void preloadProjectMediaThumbnails();
    /// Queue thumbnail for one library item (e.g. after Locate).
    void requestThumbnailForMedia(const QUuid& mediaId);
    int mediaPreloadCompleted() const { return m_preloadCompletedCount; }
    int mediaPreloadTotal() const { return m_preloadBatchTotal; }
    void setGridDimensions(int rows, int cols);
    void refresh();
    /// Updates cell labels/highlights without reloading filmstrip thumbnails.
    void refreshCellLabels();

    int activeBankSetIndex() const { return m_bankSetIndex; }
    int activeBankIndex()    const { return m_bankIndex; }
    int selectedCellIndex()  const { return m_selectedCell; }

    /// Updates inspector selection (subtle outline) and emits cellSelected (no trigger).
    void selectCell(int cellIndex);

    /// Highlights cells that map to mix slots (blue glow in CellButton). Pass 14 entries (GPU layer 0..13).
    void setMixSlotPlayback(const std::array<MixSlotCellRef, kMixSlotCount>& mixSlotRefs);

    /// Purple glow: mixer-wide filter presets currently triggered (no mix layer).
    void setActiveMixerFilterCells(const QList<MixSlotCellRef>& refs);

    /// Orange outline: cell shown in the large clip preview (right-click peek). Invalid ref = none.
    void setPeekCellHighlight(const MixSlotCellRef& ref);

    /// Switch bank set / bank tab and select a cell (used for mix-slot shortcuts).
    void revealAndSelectCell(int bankSetIndex, int bankIndex, int cellIndex);

    /// First cached filmstrip frame for a cell (empty if none yet).
    QImage cachedCellPreview(int bankSetIndex, int bankIndex, int cellIndex) const;

    void setBankSetIndex(int bankSetIndex);
    void stepBank(int delta);
    void selectBank(int bankIndex);

    /// GrandVJ-style edit mode: green cells, click = learn MIDI/keyboard trigger for that cell.
    void setMidiMappingEditMode(bool on);
    bool midiMappingEditMode() const;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

signals:
    /// User dropped a media file from the library (or OS) onto a bank cell.
    void mediaDroppedOnCell(int bankSetIndex, int bankIndex, int cellIndex, const QString& absolutePath);
    void cellTriggered(int bankSetIndex, int bankIndex, int cellIndex);
    void cellSelected (int bankSetIndex, int bankIndex, int cellIndex);
    void bankSelected (int bankSetIndex, int bankIndex);
    /// Double-click on a cell: open filter / node editor (single click triggers playback).
    void cellEditRequested(int bankSetIndex, int bankIndex, int cellIndex);
    /// Right-click on a cell: show that clip in the main clip preview (if it is playing on a mix slot).
    void cellPeekPreviewRequested(int bankSetIndex, int bankIndex, int cellIndex);
    /// Right-click context menu on a cell (global position).
    void cellContextMenuRequested(int bankSetIndex, int bankIndex, int cellIndex, QPoint globalPos);
    /// Right-click context menu on a bank tab (global position).
    void bankContextMenuRequested(int bankSetIndex, int bankIndex, QPoint globalPos);
    /// MIDI mapping mode: user clicked a cell to assign a note/key trigger.
    void midiLearnCellTriggerRequested(int bankSetIndex, int bankIndex, int cellIndex);
    /// Grid filmstrip cache updated (inspector preview can refresh).
    void cellPreviewCacheUpdated(const QUuid& mediaId);
    /// Thumbnail preload progress for the current batch (`completed` of `total`).
    void mediaPreloadProgress(int completed, int total);
    /// All thumbnails from the current preload batch are ready (or batch was empty).
    void mediaPreloadFinished();

public slots:
    /// Called from the thumbnail worker thread via queued functor (must stay callable).
    void applyThumbnailStrip(const QUuid& mediaId, const QVector<QImage>& frames);
    void deliverThumbnailFromWorker(const QUuid& mediaId, const QVector<QImage>& frames,
                                    quint64 generation, bool afterEnrich);

private slots:
    void onBankTabChanged(int idx);
    void onBankTabContextMenu(const QPoint& pos);

private:
    void buildUi();
    void rebuildGridButtons();
    void rebuildBankTabs();
    /// @param reloadThumbnails When false, only updates selection / mix highlights and labels
    ///        without clearing filmstrip thumbnails (avoids UI stalls during inspector drags).
    void updateCellVisuals(bool reloadThumbnails = true);
    void ensureThumbnail(const QUuid& mediaId, const QString& absolutePath, bool forPreload = false);
    void enrichFilmstripAsync(const QUuid& mediaId, const QString& absolutePath);
    void markPreloadItemFinished(const QUuid& mediaId);
    void startQueuedPreloadJobs();
    void queueFilmstripEnrichAfterPreload();
    void startQueuedEnrichJobs();
    int  cellIndexAtPosition(const QPoint& posInThisWidget) const;

    pvj::core::Project* m_project = nullptr;
    int m_bankSetIndex = 0;
    int m_bankIndex    = 0;
    int m_selectedCell = -1;
    int m_gridRows     = 4;
    int m_gridCols     = 12;

    QTabBar*           m_bankTabs     = nullptr;
    QGridLayout*       m_grid         = nullptr;
    QList<CellButton*> m_cells;

    QHash<QUuid, QVector<QImage>> m_filmCache;
    QSet<QUuid>          m_thumbPending;
    quint64                m_thumbGeneration = 0;
    QSet<QUuid>            m_preloadBatchIds;
    int                    m_preloadBatchTotal     = 0;
    int                    m_preloadCompletedCount = 0;
    // Pending preload jobs not yet started, and the ids currently extracting.
    // Bounding concurrency keeps thumbnail completions spread out over time so
    // the load progress bar advances smoothly instead of jumping at the end.
    QVector<QPair<QUuid, QString>> m_preloadQueue;
    QSet<QUuid>                    m_preloadRunningIds;
    QThreadPool                    m_preloadPool;
    QVector<QPair<QUuid, QString>> m_enrichQueue;
    QSet<QUuid>                    m_enrichRunningIds;

    std::array<MixSlotCellRef, kMixSlotCount> m_mixSlots{};
    QList<MixSlotCellRef> m_activeMixerFilterCells;
    MixSlotCellRef m_peekRef{};

    bool m_midiMappingEditMode = false;
};

} // namespace pvj::app

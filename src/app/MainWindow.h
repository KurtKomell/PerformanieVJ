#pragma once

#include <QImage>
#include <QList>
#include <QMainWindow>
#include <QPoint>
#include <QSize>

#include <array>
#include <memory>
#include <optional>

class QAction;
class QActionGroup;
class QCloseEvent;
class QEvent;
class QFrame;
class QLabel;
class QMenu;
class QProgressDialog;
class QTimer;
class QUndoStack;

#include "core/CellOps.h"
#include "core/Model.h"
#include "core/Project.h"
#include "undo/ProjectUndoCommands.h"

namespace pvj::video {
class VideoDecoder;
}

namespace pvj::audio {
class AudioEngine;
class FfmpegAudioDecoder;
}

namespace pvj::render {
class FullscreenOutputWindow;
class RhiMixerWidget;
class RhiPreviewWidget;
}

namespace pvj::input {
class InputRouter;
class MidiInput;
}

namespace pvj::app {

class BankGridWidget;
class FilterNodeEditorWindow;
class OutputProcessingDialog;
class MediaLibraryDock;
class ParameterInspector;
class PvjSplitter;

/// Per-mixer-layer keying snapshot (independent even when the same bank cell plays on multiple layers).
struct LayerKeyingState {
    bool valid = false;
    bool keyingEnabled = false;
    pvj::core::KeyingMode keyingMode = pvj::core::KeyingMode::Luma;
    double keyThreshold = 0.25;
    double keySoftness = 0.12;
    double keyLumaCenter = 0.5;
    bool keyLumaInvert = false;
    double keyChromaHue = 0.33;
    bool keyChromaInvert = false;
    double keyChannelR = 1.0;
    double keyChannelG = 1.0;
    double keyChannelB = 1.0;
    pvj::core::MaskType maskType = pvj::core::MaskType::None;
    double maskFeather = 0.1;
    double maskRectWidth = 1.0;
    double maskRectHeight = 1.0;
    double maskRadius = 0.5;
    double maskEllipseX = 0.6;
    double maskEllipseY = 0.45;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onFileNew();
    void onFileOpen();
    void onFileSave();
    void onFileSaveAs();
    void onRecentFileTriggered();
    void onImportVj2();
    void onImportVj2IntoBank();
    void onImportAvc();
    void onConfigureBankGrid();
    void onEditPreferences();

    void onCellSelected(int bankSetIndex, int bankIndex, int cellIndex);
    void onCellTriggered(int bankSetIndex, int bankIndex, int cellIndex, bool toggleIfPlaying = true);
    void onCellEditRequested(int bankSetIndex, int bankIndex, int cellIndex);
    void onCellPeekPreviewRequested(int bankSetIndex, int bankIndex, int cellIndex);
    void onCellContextMenu(int bankSetIndex, int bankIndex, int cellIndex, QPoint globalPos);
    void onBankContextMenu(int bankSetIndex, int bankIndex, QPoint globalPos);
    void copySelectedCell();
    void pasteIntoSelectedCell();
    void pasteIntoSelectedCellWithMidi();
    void pasteIntoSelectedCellWithoutMidi();
    void pasteIntoSelectedCellParamsWithMidi();
    void copyBank(int bankSetIndex, int bankIndex);
    void pasteBankIntoActive();
    void renameBank(int bankSetIndex, int bankIndex);
    void renameCell(int bankSetIndex, int bankIndex, int cellIndex);
    void onMediaActivated(const QString& absolutePath);
    void onMediaDroppedOnCell(int bankSetIndex, int bankIndex, int cellIndex, const QString& absolutePath);
    void onCellEdited(int bankSetIndex, int bankIndex, int cellIndex);
    void onCellPlaybackChanged(int bankSetIndex, int bankIndex, int cellIndex);
    void onInspectorVisualSeekStep(int seconds);
    void onInspectorScratchApply();

    void onLearnCellTrigger();
    void onLearnPropertyCc();
    void onLearnMidiFadeTransparency();
    void onCancelLearn();
    void onMidiMappingEditToggled(bool on);
    void onApplyMappingToAllBanksToggled(bool on);
    void onCopyKeyboardMappingToAllBanks();
    void onCopyLayerMappingToAllBanks();
    void onCopyMidiMappingToAllBanks();
    void onMidiLearnCellTriggerFromGrid(int bankSetIndex, int bankIndex, int cellIndex);

    void onInputTriggerCell(int bankSetIndex, int bankIndex, int cellIndex, bool fromMidiNote);
    void onInputReleaseCell(int bankSetIndex, int bankIndex, int cellIndex);
    void onInputBankNext(int bankSetIndex);
    void onInputBankPrev(int bankSetIndex);
    void onInputBankSelect(int bankSetIndex, int bankIndex);
    void onInputPropertyMapped(int bankSetIndex, int bankIndex, int cellIndex,
                               const QString& propertyName, double value);
    void onInputPropertyToggle(int bankSetIndex, int bankIndex, int cellIndex,
                               const QString& propertyName);

    void onInspectorMidiLearnCc(const QString& propertyName);
    void onInspectorMidiLearnNote(const QString& propertyName, bool toggle, double buttonValue);
    void onInspectorMidiClearMapping(const QString& propertyName);

    void onLearnBankNext();
    void onLearnBankPrev();
    void onLearnBankSelect();

    void promptOpenLastProjectIfNeeded();

    void onMixLayerDirect(int slotIndex);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
private:
    void setupMenus();
    void setupCentralLayout();
    void restoreViewSettings();
    void saveViewSettings();
    void setParameterInspectorVisible(bool visible);
    void updateWindowTitle();
    bool saveToPath(const QString& path);
    bool saveProject();
    bool saveProjectAs();
    /// Returns false if the user cancelled (keep current project / abort close).
    bool maybeSave();
    void markProjectDirty();
    void clearProjectDirty();
    bool isProjectDirty() const;
    void openProjectFromPath(const QString& path);
    void rememberRecentProject(const QString& path);
    void rememberLastProjectPath();
    void updateRecentFilesMenu();
    void rebindUiToProject(bool preloadMedia = true);
    void showProjectLoadProgress(const QString& label, int percent);
    void showMediaPreloadHint();
    void closeMediaPreloadHint();
    void onMediaPreloadProgress(int completed, int total);
    void onMediaPreloadFinished();

    void stopAllPlaybackAndClear();
    void playMediaOnLayer(int layer, const QString& path);
    void stopMixLayer(int layer);
    bool startCellOnMixLayer(int layer, int bankSetIndex, int bankIndex, int cellIndex);
    /// Apply last-seen MIDI CC/aftertouch positions to a newly started playing cell.
    void reapplyCurrentMidiControllersToPlayingCell(int bankSetIndex, int bankIndex, int cellIndex);
    void reapplyPlayingCell(int bankSetIndex, int bankIndex, int cellIndex);
    static bool cellIsPlayable(const pvj::core::Cell& cell);
    static bool cellIsMixerFilterType(const pvj::core::Cell& cell);

    void toggleMixerFilterCell(int bankSetIndex, int bankIndex, int cellIndex);
    void pruneActiveMixerFilterCells();
    bool isMixerFilterCellActive(int bankSetIndex, int bankIndex, int cellIndex) const;
    QList<pvj::core::CellFilterNode> buildMergedOutputFilterChain() const;

    void assignMediaToCell(int bankSetIndex, int bankIndex, int cellIndex, const QString& absolutePath);
    void resolveMissingProjectMedia();
    void scheduleResolveMissingProjectMedia();
    void processNextMissingMedia();
    void finishMissingMediaPass();
    void clearCellsUsingMedia(const QUuid& mediaId);
    bool ensureMediaFileAvailable(const QUuid& mediaId, const QString& path);
    bool tryLocateMissingMedia(const QUuid& mediaId, const QString& oldPath);
    enum class MissingMediaAction { Locate, Skip, SkipAll, Delete, DeleteAll };
    MissingMediaAction promptMissingMediaFile(const QString& path);
    void applyBankGridDimensions(int rows, int cols);
    void applyStagePixelSize(QSize px);
    struct DeckSlot {
        int bankSet = -1;
        int bank    = -1;
        int cell    = -1;
    };

    const pvj::core::Cell* cellAtDeck(const DeckSlot& slot) const;
    void snapshotLayerKeyingFromCell(int layer, const pvj::core::Cell* cell);
    void applyLayerKeyingStateToCell(pvj::core::Cell& cell, const LayerKeyingState& state);
    void syncInspectorEditLayerForCell(int bankSetIndex, int bankIndex, int cellIndex);
    void syncInspectorLayerKeyingOverride();
    void scheduleMixerUpdateFromCells();
    /// Apply layer opacity to the mixer immediately (bypasses throttled full sync).
    /// Needed for feedback cells: they have no decoder `frameReady` to drive repaints.
    void syncLiveMixerOpacityForCell(int bankSetIndex, int bankIndex, int cellIndex);
    void syncFilterParamsToMixer(int bankSetIndex, int bankIndex, int cellIndex);
    /// Lightweight mixer sync for a playing internal-feedback cell (preserves ring buffer).
    void syncPlayingFeedbackCellToMixer(int bankSetIndex, int bankIndex, int cellIndex);
    void schedulePlayingFeedbackCellMixerSync(int bankSetIndex, int bankIndex, int cellIndex);
    void syncPlayingFeedbackCellAfterPropertyChange(int bankSetIndex, int bankIndex, int cellIndex,
                                                    const QString& propertyName);

    void updateMixerFromPlayingCells();
    /// Sync one mix layer from deck state to the preview mixer (no full-layer scan).
    void syncMixLayerFromCell(int layer, bool requestRepaint = true);
    void updateDeckAPreviewRotation();
    void syncMixerToFullscreen();
    void syncOutputFilterChainToMixers();
    void rebuildOutputScreenMenu();
    void openFullscreenOutputOnScreen(int screenIndex);
    void closeFullscreenOutput();
    void onOutputProcessingFilters();
    int activeFullscreenScreenIndex() const;
    void refreshPreviewForSelectedCell();
    void applyClipPreviewPane();
    void setClipPreviewPeekChrome(bool active);
    void clearClipPeek();
    /// True when the given bank cell is the one currently shown in right-click peek preview.
    bool cellMatchesPeekSelection(int bankSetIndex, int bankIndex, int cellIndex) const;

    int findLayerPlayingCell(int bankSet, int bank, int cell) const;
    /// Copies layer/mapping settings from `src` to the same cell index on every other bank.
    /// Returns the number of banks updated (excluding `sourceBankIndex`).
    int applyCellLayerSettingsToAllBanks(int bankSetIndex, int sourceBankIndex, int cellIndex,
                                         const pvj::core::Cell& src);
    int applyPreferredLayerToAllBanks(int bankSetIndex, int sourceBankIndex, int cellIndex,
                                      int preferredLayer);
    int applyMidiMappingsToAllBanks(int bankSetIndex, int sourceBankIndex, int cellIndex,
                                    const pvj::core::Cell& src);
    /// Promotes the selected cell's keyboard + MIDI-note triggers to all-banks form.
    bool applyKeyboardTriggerToAllBanks(int bankSetIndex, int bankIndex, int cellIndex);
    void refreshAfterMappingApplyToAllBanks(int bankSetIndex, int bankIndex, int cellIndex,
                                            int playingLayer);
    /// Active bank for slot triggers (`kBankIndexAllBanks`), else mapping bank index.
    int resolveCellTriggerBankIndex(int mappingBankIndex) const;
    int pickMixSlotForTrigger(int bankSet, int bank, int cell);
    static int gpuLayerFromPreferred(int preferredLayer);
    bool selectionMatchesSlot(const DeckSlot& s) const;
    void setupInputMapping();
    void populateMidiPortMenu();
    void showMidiInputStatus();
    /// Opens or disables MIDI inputs; persists choice. Returns true if at least one port opened.
    bool applyMidiInputPorts(const QStringList& portNames);
    void syncMidiPreferencesDialog(class PreferencesDialog& dlg);
    void tickLayerFade();
    void startLayerFadeIn(int layer, float targetTransparency, float fadeParam);

    void syncMixSlotHighlightsToBankGrid();
    void syncMixerFilterHighlightsToBankGrid();
    void syncPeekHighlightToBankGrid();

    void pasteIntoSelectedCell(pvj::core::CellPasteMode mode);
    void clearUndoStack();
    void shutdownUndoStack();
    void refreshUiAfterProjectEdit();
    void pushCellsReplaceCommand(QVector<CellSnapshot> before, QVector<CellSnapshot> after,
                                 const QString& text);
    QVector<CellSnapshot> snapshotCellAcrossBanks(int bankSetIndex, int bankIndex,
                                                  int cellIndex) const;
    QVector<CellSnapshot> snapshotAllCellsInBankSet(int bankSetIndex) const;
    std::optional<CellSnapshot> snapshotSingleCell(int bankSetIndex, int bankIndex,
                                                   int cellIndex) const;
    void beginInspectorContinuousEdit();
    void endInspectorContinuousEdit();
    void onInspectorDiscreteCellChanged(int bankSetIndex, int bankIndex, int cellIndex);
    void syncInspectorEditBaselines();
    void captureLearnBaseline();
    void commitLearnUndoIfNeeded(const QString& text);
    void commitFilterCellUndo(int bankSetIndex, int bankIndex, int cellIndex, const QString& text);
    void scheduleFilterParamsUndo(int bankSetIndex, int bankIndex, int cellIndex);

    std::unique_ptr<pvj::core::Project> m_project;
    QUndoStack* m_undoStack = nullptr;
    bool m_projectDirty = false;
    bool m_closing = false;
    bool m_refreshingUiAfterEdit = false;
    bool m_suppressInspectorUndo = false;
    bool m_cellClipboardValid = false;
    pvj::core::Cell m_cellClipboard;
    bool m_bankClipboardValid = false;
    pvj::core::Bank m_bankClipboard;

    bool m_inspectorContinuousEdit = false;
    std::optional<CellSnapshot> m_inspectorEditBefore;
    QList<pvj::core::TriggerMapping> m_learnTriggersBefore;
    std::optional<CellSnapshot> m_learnCellBefore;
    bool m_learnBaselineValid = false;

    std::optional<CellSnapshot> m_filterEditBefore;
    QTimer* m_filterParamsUndoTimer = nullptr;
    int m_filterParamsUndoBankSet = -1;
    int m_filterParamsUndoBank = -1;
    int m_filterParamsUndoCell = -1;

    static constexpr int kMixLayers = 14;
    static constexpr int kBackgroundLayer = 0;
    static constexpr int kUserLayerMin = 1;
    static constexpr int kUserLayerMax = 13;
    std::unique_ptr<pvj::audio::AudioEngine> m_audioEngine;
    std::array<std::unique_ptr<pvj::audio::FfmpegAudioDecoder>, kMixLayers> m_audioDecoders;
    std::array<std::unique_ptr<pvj::video::VideoDecoder>, kMixLayers> m_decoders;

    MediaLibraryDock*              m_mediaDock = nullptr;
    QFrame*                        m_clipPreviewFrame = nullptr;
    pvj::render::RhiPreviewWidget* m_previewA = nullptr;
    pvj::render::RhiMixerWidget*   m_previewB = nullptr;
    ParameterInspector*          m_inspector = nullptr;
    BankGridWidget*                m_bankGrid  = nullptr;
    PvjSplitter*                   m_mainSplit = nullptr;
    PvjSplitter*                   m_upperSplit = nullptr;
    QAction*                       m_actToggleInspector = nullptr;
    std::unique_ptr<FilterNodeEditorWindow> m_filterEditor;

    std::unique_ptr<pvj::input::InputRouter> m_inputRouter;
    std::unique_ptr<pvj::input::MidiInput>   m_midiInput;
    QMenu* m_midiPortMenu = nullptr;
    QMenu* m_outputMenu = nullptr;
    QActionGroup* m_outputScreenGroup = nullptr;
    QAction* m_actCloseFullscreenOutput = nullptr;
    int m_outputScreenIndex = 0;

    std::unique_ptr<OutputProcessingDialog> m_outputProcessingDialog;
    std::unique_ptr<pvj::render::FullscreenOutputWindow> m_fullscreenOut;

    std::array<DeckSlot, kMixLayers> m_layerSlots{};
    /// Mixer-wide filter presets triggered from bank cells (no mix layer); merge order = list order.
    QList<DeckSlot> m_activeMixerFilterCells;
    std::array<LayerKeyingState, kMixLayers> m_layerKeying{};
    /// Mix layer whose keying snapshot receives inspector edits (-1 = none).
    int m_inspectorEditLayer = -1;
    /// Last `updateMixerFromPlayingCells` saw a media clip on this mix slot (for avoiding
    /// redundant `VideoDecoder::stop` / `seek(0)` on slots without media).
    std::array<bool, kMixLayers> m_mixerSlotHadMedia{};

    /// Mix-layer index (0..5) forced into the clip preview pane after right-click; -1 = off.
    int m_clipPeekLayer = -1;

    /// Last frame per layer for fullscreen mirror when output is toggled on.
    std::array<QImage, kMixLayers> m_lastFrames{};

    QTimer* m_layerFadeTimer = nullptr;
    /// Coalesce inspector `refreshFromModel` after MIDI CC (avoid full UI rebuild per message).
    QTimer* m_midiInspectorDebounceTimer = nullptr;
    QTimer* m_mixerUpdateDebounceTimer = nullptr;
    QTimer* m_feedbackMixerSyncTimer = nullptr;
    int m_pendingFeedbackSyncBankSet = -1;
    int m_pendingFeedbackSyncBank = -1;
    int m_pendingFeedbackSyncCell = -1;
    /// Leading-edge throttle state for `scheduleMixerUpdateFromCells`.
    /// True when another update was requested while the cooldown timer was still active;
    /// the timer's `timeout` slot will then apply the latest model state and restart cooldown.
    bool m_mixerUpdatePending = false;
    std::array<bool, kMixLayers>  m_layerFadeAnimating{};
    std::array<float, kMixLayers> m_layerFadeTarget{};
    std::array<float, kMixLayers> m_layerFadeTargetAudio{};
    std::array<int, kMixLayers>   m_layerFadeElapsedMs{};
    std::array<int, kMixLayers>   m_layerFadeDurationMs{};

    QAction* m_actMidiMappingEdit        = nullptr;
    QAction* m_actApplyMappingAllBanks = nullptr;
    QMenu*   m_recentFilesMenu           = nullptr;

    bool m_skipAllMissingMedia = false;
    bool m_deleteAllMissingMedia = false;
    bool m_missingMediaPassActive = false;
    bool m_missingMediaDialogOpen = false;
    bool m_missingMediaResolveScheduled = false;
    bool m_missingMediaCellsCleared = false;
    bool m_missingMediaUpdated = false;
    QList<QUuid> m_missingMediaQueue;
    bool m_startupProjectPromptDone = false;

    QProgressDialog* m_mediaPreloadDialog = nullptr;

    QSize m_stagePixelSize;
};

} // namespace pvj::app

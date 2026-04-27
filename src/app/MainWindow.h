#pragma once

#include <QImage>
#include <QMainWindow>
#include <QSize>

#include <array>
#include <memory>

class QAction;
class QCloseEvent;
class QEvent;
class QFrame;
class QLabel;
class QMenu;
class QTimer;

namespace pvj::core {
struct Cell;
class Project;
}

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
class MediaLibraryDock;
class ParameterInspector;
class PvjSplitter;

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
    void onImportAvc();
    void onConfigureBankGrid();
    void onEditPreferences();

    void onCellSelected(int bankSetIndex, int bankIndex, int cellIndex);
    void onCellTriggered(int bankSetIndex, int bankIndex, int cellIndex);
    void onCellEditRequested(int bankSetIndex, int bankIndex, int cellIndex);
    void onCellPeekPreviewRequested(int bankSetIndex, int bankIndex, int cellIndex);
    void onMediaActivated(const QString& absolutePath);
    void onMediaDroppedOnCell(int bankSetIndex, int bankIndex, int cellIndex, const QString& absolutePath);
    void onCellEdited(int bankSetIndex, int bankIndex, int cellIndex);
    void onFullscreenOutputToggled();
    void onInspectorVisualSeekStep(int seconds);
    void onInspectorScratchApply();

    void onLearnCellTrigger();
    void onLearnPropertyCc();
    void onLearnMidiFadeTransparency();
    void onCancelLearn();
    void onMidiMappingEditToggled(bool on);
    void onMidiLearnCellTriggerFromGrid(int bankSetIndex, int bankIndex, int cellIndex);

    void onInputTriggerCell(int bankSetIndex, int bankIndex, int cellIndex);
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

    void onMixLayerDirect(int slotIndex);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void setupMenus();
    void setupCentralLayout();
    void updateWindowTitle();
    bool saveToPath(const QString& path);
    void openProjectFromPath(const QString& path);
    void rememberRecentProject(const QString& path);
    void updateRecentFilesMenu();
    void rebindUiToProject();

    void stopAllPlaybackAndClear();
    void playMediaOnLayer(int layer, const QString& path);

    void assignMediaToCell(int bankSetIndex, int bankIndex, int cellIndex, const QString& absolutePath);
    void applyBankGridDimensions(int rows, int cols);
    void applyStagePixelSize(QSize px);
    struct DeckSlot {
        int bankSet = -1;
        int bank    = -1;
        int cell    = -1;
    };

    const pvj::core::Cell* cellAtDeck(const DeckSlot& slot);

    void updateMixerFromPlayingCells();
    void updateDeckAPreviewRotation();
    void syncMixerToFullscreen();
    void refreshPreviewForSelectedCell();
    void applyClipPreviewPane();
    void setClipPreviewPeekChrome(bool active);
    void clearClipPeek();
    /// True when the given bank cell is the one currently shown in right-click peek preview.
    bool cellMatchesPeekSelection(int bankSetIndex, int bankIndex, int cellIndex) const;

    int findLayerPlayingCell(int bankSet, int bank, int cell) const;
    int pickMixSlotForTrigger(int bankSet, int bank, int cell);
    bool selectionMatchesSlot(const DeckSlot& s) const;
    void setupInputMapping();
    void tickLayerFade();
    void startLayerFadeIn(int layer, float targetTransparency, float fadeParam);

    void syncMixSlotHighlightsToBankGrid();
    void syncPeekHighlightToBankGrid();

    std::unique_ptr<pvj::core::Project> m_project;

    static constexpr int kMixLayers = 12;
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
    std::unique_ptr<FilterNodeEditorWindow> m_filterEditor;

    std::unique_ptr<pvj::input::InputRouter> m_inputRouter;
    std::unique_ptr<pvj::input::MidiInput>   m_midiInput;

    std::unique_ptr<pvj::render::FullscreenOutputWindow> m_fullscreenOut;

    std::array<DeckSlot, kMixLayers> m_layerSlots{};
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
    std::array<bool, kMixLayers>  m_layerFadeAnimating{};
    std::array<float, kMixLayers> m_layerFadeTarget{};
    std::array<float, kMixLayers> m_layerFadeTargetAudio{};
    std::array<int, kMixLayers>   m_layerFadeElapsedMs{};
    std::array<int, kMixLayers>   m_layerFadeDurationMs{};

    QAction* m_actMidiMappingEdit = nullptr;
    QMenu*   m_recentFilesMenu    = nullptr;

    QSize m_stagePixelSize;
};

} // namespace pvj::app

#pragma once

#include "core/Model.h"

#include <QImage>
#include <QList>
#include <QTabWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDial;
class QDoubleSpinBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QStackedWidget;
class QToolButton;
class QEvent;

namespace pvj::core {
class Project;
}

namespace pvj::app {

struct LayerKeyingState;

class VisualThumbnailLabel;
class KeyRangeWidget;

// Tabbed inspector for bank cell properties: Visual, Transition, Mixing,
// Position, Output.
class ParameterInspector : public QTabWidget
{
    Q_OBJECT
public:
    explicit ParameterInspector(QWidget* parent = nullptr);

    void setProject(pvj::core::Project* project);
    void setSelection(int bankSetIndex, int bankIndex, int cellIndex);
    void setMidiMappingEditMode(bool on);

    /// Reloads spin boxes from the model (e.g. after MIDI mapped a property).
    void refreshFromModel();

    /// When set, keying sliders read/write through this per-layer snapshot (see MainWindow).
    void setLayerKeyingOverride(const LayerKeyingState* state);

    int outputScreenIndex() const;

    /// Current bank grid / inspector selection (for preview sync).
    int selectedBankSetIndex() const { return m_bankSetIndex; }
    int selectedBankIndex() const { return m_bankIndex; }
    int selectedCellIndex() const { return m_cellIndex; }

    /// Live frame for the Visual-tab thumbnail (same layer as clip preview when playing).
    void setVisualThumbnail(const QImage& frame);
    void clearVisualThumbnail();

signals:
    void cellChanged(int bankSetIndex, int bankIndex, int cellIndex);
    /// Layer, visual source, or media assignment changed — re-apply live playback if active.
    void cellPlaybackChanged(int bankSetIndex, int bankIndex, int cellIndex);
    void fullscreenOutputToggled();
    /// Step playback by whole seconds (±1 from arrow buttons).
    void visualSeekStepRequested(int seconds);
    /// Apply scratch head position to the playing decoder (see `scratchHeadU`).
    void scratchApplyRequested();

    void midiLearnCcRequested(const QString& propertyId);
    void midiLearnNoteRequested(const QString& propertyId, bool toggle, double buttonValue);
    void midiClearMappingRequested(const QString& propertyId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onTransparencySliderChanged(int v);
    void onMovieSpeedSliderChanged(int v);
    void onFadeSliderChanged(int v);
    void onRotationChanged(double v);
    void onCopyModeChanged(int idx);
    void onKeyingModeChanged(int idx);
    void onKeyingEnabledToggled(bool checked);
    void onKeyLumaTargetChanged(int id);
    void onKeyLumaRangeChanged(double minV, double maxV);
    void onKeyChromaTargetChanged(int id);
    void onKeyChromaRangeChanged(double minV, double maxV);
    void onMaskTypeGroupIdClicked(int id);
    void onMatteRoleChanged(int idx);
    void onKeyHardnessSliderChanged(int v);
    void onKeyFeatherSliderChanged(int v);
    void onMaskRectWidthChanged(int v);
    void onMaskRectHeightChanged(int v);
    void onMaskRadiusChanged(int v);
    void onMaskEllipseXChanged(int v);
    void onMaskEllipseYChanged(int v);
    void onMaskFeatherChanged(int v);
    void onKeyChannelRChanged(int v);
    void onKeyChannelGChanged(int v);
    void onKeyChannelBChanged(int v);
    void onMixingPresetChanged(int idx);
    void onPreferredLayerChanged(int idx);

    void onAudioDialChanged(int v);
    void onPlayModeGroupClicked(int id);
    void onPriorityGroupClicked(int id);
    void onClipPauseToggled(bool checked);
    void onSegmentInChanged(int v);
    void onSegmentOutChanged(int v);
    void onScratchSliderChanged(int v);
    void onScratchSliderReleased();
    void onOverlayTextEdited(const QString& t);
    void onTcStartEdited(const QString& t);

    void onVisualSourceChanged(int idx);
    void onFeedbackLoopRetentionChanged(int v);
    void onFeedbackLiveInjectChanged(int v);
    void onFeedbackSaturationChanged(int v);
    void onFeedbackBrightnessChanged(int v);
    void onFeedbackContrastChanged(int v);
    void onFeedbackHueShiftChanged(int v);
    void onFeedbackGammaChanged(int v);
    void onFeedbackRotationChanged(int v);
    void onFeedbackZoomChanged(int v);
    void onFeedbackFrameDelayChanged(int v);
    void onFeedbackInputModeChanged(int idx);
    void onFeedbackWrapModeChanged(int idx);
    void onPictureWrapModeChanged(int idx);

private:
    void rebuildScreenList();

    QWidget* buildVisualTab();
    QWidget* buildTransitionTab();
    QWidget* buildMixingTab();
    QWidget* buildFeedbackTab();
    QWidget* buildPositionTab();
    QWidget* buildOutputTab();

    pvj::core::Cell* currentCell();
    void refreshFromCell();
    void refreshKeyingUi();
    void emitChanged();
    void emitPlaybackChanged();

    void markMixingCustom();
    void applyMixingPreset(int comboIndex);
    void syncMaskTypeButtons();
    void syncPlayModeButtons();
    void syncPriorityButtons();
    void syncPreferredLayerButtons(int layerIndex, bool hasCell);
    void syncKeyingModeUi();
    void syncMaskControlVisibility();
    void syncFeedbackForVisualSource(bool hasCell, int visualSourceKind);
    void buildKeyingPanel();
    void syncKeyingPanelPlacement(bool hasCell, int visualSourceKind);
    void markSliderResetDefault(QSlider* slider, int defaultSliderValue);

    void registerMidiWidgets();
    void tagMidiWidget(QWidget* w, const QString& propertyId, const QVariant& noteValue = QVariant());
    void showMidiContextMenu(QWidget* w, const QPoint& globalPos);

    pvj::core::Project* m_project = nullptr;
    int m_bankSetIndex = -1;
    int m_bankIndex    = -1;
    int m_cellIndex    = -1;
    const LayerKeyingState* m_layerKeyingOverride = nullptr;
    int m_feedbackTabIndex = -1;
    bool m_loading = false;
    bool m_midiMappingEditMode = false;

    QLabel*               m_visualLabel    = nullptr;
    QComboBox*            m_visualSourceCombo = nullptr;
    VisualThumbnailLabel* m_visualThumb    = nullptr;
    QToolButton*    m_visualPrevBtn  = nullptr;
    QToolButton*    m_visualNextBtn  = nullptr;
    QDial*          m_audioDial      = nullptr;
    QLabel*         m_audioDialValue  = nullptr;
    QButtonGroup*   m_playModeGroup   = nullptr;
    QSlider*        m_speedSlider     = nullptr;
    QLabel*         m_speedValueLabel = nullptr;
    QToolButton*    m_pauseClipBtn    = nullptr;
    QSlider*        m_segmentInSlider  = nullptr;
    QSlider*        m_segmentOutSlider = nullptr;
    QSlider*        m_scratchSlider    = nullptr;
    QLineEdit*      m_overlayLineEdit  = nullptr;
    QLineEdit*      m_tcStartEdit      = nullptr;
    QButtonGroup*   m_priorityGroup    = nullptr;
    QButtonGroup*   m_preferredLayerGroup = nullptr;

    QSlider*        m_transparencySlider = nullptr;
    QLabel*         m_transparencyValue  = nullptr;
    QSlider*        m_fadeSlider     = nullptr;
    QLabel*         m_fadeValue      = nullptr;
    QDoubleSpinBox* m_rotation       = nullptr;

    QComboBox*      m_mixingPreset   = nullptr;
    QComboBox*      m_copyMode       = nullptr;
    QCheckBox*      m_keyingEnabled  = nullptr;
    QComboBox*      m_keyingMode     = nullptr;
    QButtonGroup*   m_keyLumaTargetGroup = nullptr;
    QButtonGroup*   m_keyChromaTargetGroup = nullptr;
    QButtonGroup*   m_maskTypeGroup  = nullptr;
    QComboBox*      m_matteRoleCombo = nullptr;
    QWidget*        m_maskRectWidthRow = nullptr;
    QWidget*        m_maskRectHeightRow = nullptr;
    QWidget*        m_maskRadiusRow = nullptr;
    QWidget*        m_maskEllipseXRow = nullptr;
    QWidget*        m_maskEllipseYRow = nullptr;
    QWidget*        m_maskFeatherRow = nullptr;
    QSlider*        m_maskRectWidthSlider = nullptr;
    QSlider*        m_maskRectHeightSlider = nullptr;
    QSlider*        m_maskRadiusSlider = nullptr;
    QSlider*        m_maskEllipseXSlider = nullptr;
    QSlider*        m_maskEllipseYSlider = nullptr;
    QSlider*        m_maskFeatherSlider = nullptr;
    QLabel*         m_maskRectWidthValue = nullptr;
    QLabel*         m_maskRectHeightValue = nullptr;
    QLabel*         m_maskRadiusValue = nullptr;
    QLabel*         m_maskEllipseXValue = nullptr;
    QLabel*         m_maskEllipseYValue = nullptr;
    QLabel*         m_maskFeatherValue = nullptr;
    QLabel*         m_keyingHint     = nullptr;
    QLabel*         m_keyRgbHint     = nullptr;
    QWidget*        m_keyLumaTargetRow = nullptr;
    QWidget*        m_keyRangeRow = nullptr;
    KeyRangeWidget* m_keyRangeSlider = nullptr;
    QLabel*         m_keyRangeValue = nullptr;
    QWidget*        m_keyChromaTargetRow = nullptr;
    QWidget*        m_keyChromaRangeRow = nullptr;
    KeyRangeWidget* m_keyChromaRangeSlider = nullptr;
    QLabel*         m_keyChromaRangeValue = nullptr;
    QSlider*        m_keyHardnessSlider = nullptr;
    QLabel*         m_keyHardnessValue  = nullptr;
    QSlider*        m_keyFeatherSlider  = nullptr;
    QLabel*         m_keyFeatherValue   = nullptr;
    QSlider*        m_keyRSlider = nullptr;
    QSlider*        m_keyGSlider = nullptr;
    QSlider*        m_keyBSlider = nullptr;
    QLabel*         m_keyRValue = nullptr;
    QLabel*         m_keyGValue = nullptr;
    QLabel*         m_keyBValue = nullptr;
    QWidget*        m_keyRRow = nullptr;
    QWidget*        m_keyGRow = nullptr;
    QWidget*        m_keyBRow = nullptr;
    QWidget*        m_keyingPanel = nullptr;
    QWidget*        m_mixingKeyingSlot = nullptr;
    QWidget*        m_feedbackKeyingSlot = nullptr;
    QWidget*        m_feedbackKeyingSection = nullptr;

    QSlider*        m_feedbackLoopRetentionSlider = nullptr;
    QLabel*         m_feedbackLoopRetentionValue = nullptr;
    QSlider*        m_feedbackLiveInjectSlider = nullptr;
    QLabel*         m_feedbackLiveInjectValue = nullptr;
    QSlider*        m_feedbackSaturationSlider = nullptr;
    QLabel*         m_feedbackSaturationValue = nullptr;
    QSlider*        m_feedbackBrightnessSlider = nullptr;
    QLabel*         m_feedbackBrightnessValue = nullptr;
    QSlider*        m_feedbackContrastSlider = nullptr;
    QLabel*         m_feedbackContrastValue = nullptr;
    QSlider*        m_feedbackHueShiftSlider = nullptr;
    QLabel*         m_feedbackHueShiftValue = nullptr;
    QSlider*        m_feedbackGammaSlider = nullptr;
    QLabel*         m_feedbackGammaValue = nullptr;
    QSlider*        m_feedbackRotationSlider = nullptr;
    QLabel*         m_feedbackRotationValue = nullptr;
    QSlider*        m_feedbackZoomSlider = nullptr;
    QLabel*         m_feedbackZoomValue = nullptr;
    QSlider*        m_feedbackFrameDelaySlider = nullptr;
    QLabel*         m_feedbackFrameDelayValue = nullptr;
    QComboBox*      m_feedbackInputModeCombo = nullptr;
    QLabel*         m_feedbackInputModeHint = nullptr;
    QComboBox*      m_feedbackWrapCombo = nullptr;

    QComboBox*      m_pictureWrapCombo = nullptr;

    QComboBox*      m_outputScreen   = nullptr;
    QPushButton*    m_fullscreenBtn  = nullptr;
    QGridLayout*    m_visualGrid     = nullptr;
    /// Top of Visual tab (thumbnail row + clip name); clip-only block is `m_visualClipSection`.
    QWidget*        m_visualStandardSection = nullptr;
    QWidget*        m_visualClipSection = nullptr;
    QLabel*         m_visualNote     = nullptr;

    QList<QWidget*> m_midiTaggedWidgets;
};

} // namespace pvj::app

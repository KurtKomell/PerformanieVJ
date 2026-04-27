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

class VisualThumbnailLabel;

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
    void onMaskTypeGroupIdClicked(int id);
    void onKeyHardnessSliderChanged(int v);
    void onKeyFeatherSliderChanged(int v);
    void onKeyChannelRChanged(int v);
    void onKeyChannelGChanged(int v);
    void onKeyChannelBChanged(int v);
    void onMixingPresetChanged(int idx);
    void onLayerBandGroupClicked(int id);

    void onAudioDialChanged(int v);
    void onPlayModeGroupClicked(int id);
    void onPriorityGroupClicked(int id);
    void onClipPauseToggled(bool checked);
    void onSegmentInChanged(int v);
    void onSegmentOutChanged(int v);
    void onScratchSliderChanged(int v);
    void onScratchSliderReleased();
    void onFeedbackPresetApplyClicked();
    void onFeedbackEnabledToggled(bool checked);
    void onFeedbackStrengthChanged(int v);
    void onFeedbackZoomChanged(int v);
    void onFeedbackRotationChanged(int v);
    void onFeedbackRotationAnimatedToggled(bool checked);
    void onFeedbackDecayChanged(int v);
    void onFeedbackBrightnessChanged(int v);
    void onFeedbackSaturationChanged(int v);
    void onFeedbackGammaChanged(int v);
    void onFeedbackContrastChanged(int v);
    void onFeedbackLayerBrightnessChanged(int v);
    void onFeedbackLayerSaturationChanged(int v);
    void onFeedbackLayerGammaChanged(int v);
    void onFeedbackLayerContrastChanged(int v);
    void onFeedbackWrapModeChanged(int idx);
    void onOverlayTextEdited(const QString& t);
    void onTcStartEdited(const QString& t);

private:
    void rebuildScreenList();

    QWidget* buildVisualTab();
    QWidget* buildTransitionTab();
    QWidget* buildMixingTab();
    QWidget* buildPositionTab();
    QWidget* buildOutputTab();

    pvj::core::Cell* currentCell();
    void refreshFromCell();
    void emitChanged();

    void markMixingCustom();
    void applyMixingPreset(int comboIndex);
    void syncMaskTypeButtons();
    void syncPlayModeButtons();
    void syncPriorityButtons();
    void syncVisualRowVisibility(const pvj::core::Cell* cell);

    void registerMidiWidgets();
    void tagMidiWidget(QWidget* w, const QString& propertyId, const QVariant& noteValue = QVariant());
    void showMidiContextMenu(QWidget* w, const QPoint& globalPos);

    pvj::core::Project* m_project = nullptr;
    int m_bankSetIndex = -1;
    int m_bankIndex    = -1;
    int m_cellIndex    = -1;
    bool m_loading = false;
    bool m_midiMappingEditMode = false;

    QLabel*               m_visualLabel    = nullptr;
    VisualThumbnailLabel* m_visualThumb    = nullptr;
    QToolButton*    m_visualPrevBtn  = nullptr;
    QToolButton*    m_visualNextBtn  = nullptr;
    QToolButton*    m_feedbackPresetApplyBtn = nullptr;
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
    QButtonGroup*   m_layerBandGroup   = nullptr;

    QSlider*        m_transparencySlider = nullptr;
    QLabel*         m_transparencyValue  = nullptr;
    QSlider*        m_fadeSlider     = nullptr;
    QLabel*         m_fadeValue      = nullptr;
    QDoubleSpinBox* m_rotation       = nullptr;

    QComboBox*      m_mixingPreset   = nullptr;
    QComboBox*      m_copyMode       = nullptr;
    QButtonGroup*   m_maskTypeGroup  = nullptr;
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

    QComboBox*      m_outputScreen   = nullptr;
    QPushButton*    m_fullscreenBtn  = nullptr;
    QGridLayout*    m_visualGrid     = nullptr;
    /// Top of Visual tab (thumbnail row + clip name); clip-only block is `m_visualClipSection`.
    QWidget*        m_visualStandardSection = nullptr;
    /// Clip / playback rows hidden when Feedback source is selected.
    QWidget*        m_visualClipSection = nullptr;
    QGroupBox*      m_feedbackGroup = nullptr;
    QCheckBox*      m_feedbackEnabled = nullptr;
    QSlider*        m_feedbackStrengthSlider = nullptr;
    QLabel*         m_feedbackStrengthValue = nullptr;
    QSlider*        m_feedbackZoomSlider = nullptr;
    QLabel*         m_feedbackZoomValue = nullptr;
    QSlider*        m_feedbackRotationSlider = nullptr;
    QLabel*         m_feedbackRotationValue = nullptr;
    QCheckBox*      m_feedbackRotationAnimated = nullptr;
    QSlider*        m_feedbackDecaySlider = nullptr;
    QLabel*         m_feedbackDecayValue = nullptr;
    QSlider*        m_feedbackBrightnessSlider = nullptr;
    QLabel*         m_feedbackBrightnessValue = nullptr;
    QSlider*        m_feedbackSaturationSlider = nullptr;
    QLabel*         m_feedbackSaturationValue = nullptr;
    QSlider*        m_feedbackGammaSlider = nullptr;
    QLabel*         m_feedbackGammaValue = nullptr;
    QSlider*        m_feedbackContrastSlider = nullptr;
    QLabel*         m_feedbackContrastValue = nullptr;
    QSlider*        m_feedbackLayerBrightnessSlider = nullptr;
    QLabel*         m_feedbackLayerBrightnessValue = nullptr;
    QSlider*        m_feedbackLayerSaturationSlider = nullptr;
    QLabel*         m_feedbackLayerSaturationValue = nullptr;
    QSlider*        m_feedbackLayerGammaSlider = nullptr;
    QLabel*         m_feedbackLayerGammaValue = nullptr;
    QSlider*        m_feedbackLayerContrastSlider = nullptr;
    QLabel*         m_feedbackLayerContrastValue = nullptr;
    QComboBox*      m_feedbackWrapMode = nullptr;
    QLabel*         m_visualNote     = nullptr;

    QList<QWidget*> m_midiTaggedWidgets;
};

} // namespace pvj::app

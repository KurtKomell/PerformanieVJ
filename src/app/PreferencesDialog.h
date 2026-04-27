#pragma once

#include <QDialog>
#include <QSize>

class QComboBox;
class QDialogButtonBox;
class QSpinBox;

namespace pvj::app {

/// Global application preferences. Currently only the fixed output ("stage")
/// resolution in pixels. The stage size is decoupled from any preview/widget
/// size so mixer composition renders at a stable aspect.
class PreferencesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);

    void setStagePixelSize(QSize size);
    QSize stagePixelSize() const;

signals:
    /// Emitted with the new stage pixel size after the user accepts the dialog.
    void stagePixelSizeChanged(QSize size);

private slots:
    void onPresetActivated(int index);

private:
    QSpinBox*        m_widthSpin  = nullptr;
    QSpinBox*        m_heightSpin = nullptr;
    QComboBox*       m_preset     = nullptr;
    QDialogButtonBox* m_buttons   = nullptr;
};

} // namespace pvj::app

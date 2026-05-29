#pragma once

#include <QByteArray>
#include <QDialog>
#include <QSize>
#include <QStringList>

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace pvj::app {

/// Global application preferences: stage resolution and MIDI input devices.
class PreferencesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);

    void setStagePixelSize(QSize size);
    QSize stagePixelSize() const;

    /// @param selectedNames Ports to show checked (saved preference).
    /// @param connectedNames Ports currently open in the app.
    void setMidiInputPorts(const QStringList& ports, const QStringList& selectedNames,
                           const QStringList& connectedNames);

    QStringList selectedMidiPortNames() const;

    /// Reset monitor (e.g. after port list / connection change).
    void resetMidiSignalMonitor(bool inputEnabled);
    /// Call for each incoming MIDI message while the dialog is open.
    void reportMidiInputActivity(const QByteArray& bytes);

signals:
    void stagePixelSizeChanged(QSize size);
    void midiInputPortsChanged(const QStringList& portNames);
    void midiDevicesRefreshRequested();

private slots:
    void onPresetActivated(int index);
    void emitMidiPortsIfNeeded();

private:
    void rebuildMidiList(const QStringList& ports, const QStringList& selectedNames);

    QSpinBox*         m_widthSpin        = nullptr;
    QSpinBox*         m_heightSpin       = nullptr;
    QComboBox*        m_preset           = nullptr;
    QListWidget*      m_midiDeviceList   = nullptr;
    QPushButton*      m_midiRefreshBtn   = nullptr;
    QLabel*           m_midiStatusLabel  = nullptr;
    QLineEdit*        m_midiSignalField  = nullptr;
    QDialogButtonBox* m_buttons          = nullptr;
    int               m_midiMessageCount = 0;
};

} // namespace pvj::app

#pragma once

#include <QWidget>

#include <QString>

namespace pvj::app {

/// Green border + mapping badge drawn on top of a MIDI-learnable control (map mode only).
class MidiMapOverlay : public QWidget
{
    Q_OBJECT
public:
    static constexpr const char* kOverlayObjectName = "pvjMidiOverlay";

    explicit MidiMapOverlay(QWidget* target);

    void setMappingLabel(const QString& label);

    static MidiMapOverlay* overlayFor(QWidget* target);
    static void setActiveOn(QWidget* target, bool active, const QString& label);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void syncToTarget();

    QString m_mappingLabel;
};

} // namespace pvj::app

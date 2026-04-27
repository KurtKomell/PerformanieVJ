#pragma once

#include <QWidget>

class QScreen;
class QShowEvent;

namespace pvj::render {
class RhiMixerWidget;
}

namespace pvj::render {

// Borderless fullscreen surface on a chosen QScreen, mirroring the mixer output.
class FullscreenOutputWindow : public QWidget
{
    Q_OBJECT
public:
    explicit FullscreenOutputWindow(QWidget* parent = nullptr);

    RhiMixerWidget* mixerWidget() const { return m_mixer; }

    void setTargetScreen(QScreen* screen);
    QScreen* targetScreen() const { return m_screen; }

public slots:
    void enterFullscreen();
    void leaveFullscreen();

protected:
    void showEvent(QShowEvent* event) override;

private:
    RhiMixerWidget* m_mixer = nullptr;
    QScreen*          m_screen = nullptr;
};

} // namespace pvj::render

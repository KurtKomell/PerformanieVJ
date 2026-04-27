#include "FullscreenOutputWindow.h"

#include "RhiMixerWidget.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QScreen>
#include <QShowEvent>
#include <QWindow>

namespace pvj::render {

FullscreenOutputWindow::FullscreenOutputWindow(QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAutoFillBackground(true);

    m_mixer = new RhiMixerWidget(this);
    m_mixer->setLabel(QString());

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_mixer, 1);

    if (QScreen* s = QGuiApplication::primaryScreen()) {
        m_screen = s;
    }
}

void FullscreenOutputWindow::setTargetScreen(QScreen* screen)
{
    if (screen) {
        m_screen = screen;
    }
}

void FullscreenOutputWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_screen) {
        if (QWindow* w = windowHandle()) {
            w->setScreen(m_screen);
        }
        setGeometry(m_screen->geometry());
    }
}

void FullscreenOutputWindow::enterFullscreen()
{
    if (m_screen) {
        setGeometry(m_screen->geometry());
    }
    show();
    if (QWindow* w = windowHandle()) {
        if (m_screen) {
            w->setScreen(m_screen);
            setGeometry(m_screen->geometry());
        }
    }
    showFullScreen();
}

void FullscreenOutputWindow::leaveFullscreen()
{
    hide();
}

} // namespace pvj::render

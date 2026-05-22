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
    if (!screen) {
        return;
    }
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.contains(screen)) {
        m_screen = screen;
    }
}

void FullscreenOutputWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    QScreen* target = resolveTargetScreen();
    if (!target) {
        return;
    }
    if (QWindow* w = windowHandle()) {
        w->setScreen(target);
    }
    setGeometry(target->geometry());
}

void FullscreenOutputWindow::enterFullscreen()
{
    QScreen* target = resolveTargetScreen();
    if (!target) {
        return;
    }
    m_screen = target;

    if (QWindow* w = windowHandle()) {
        w->setScreen(target);
    }
    setGeometry(target->geometry());
    show();
    if (QWindow* w = windowHandle()) {
        w->setScreen(target);
    }
    setGeometry(target->geometry());
    showFullScreen();
}

void FullscreenOutputWindow::leaveFullscreen()
{
    hide();
}

QScreen* FullscreenOutputWindow::resolveTargetScreen() const
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        return nullptr;
    }
    if (m_screen && screens.contains(m_screen)) {
        return m_screen;
    }
    if (QScreen* primary = QGuiApplication::primaryScreen()) {
        return primary;
    }
    return screens.first();
}

} // namespace pvj::render

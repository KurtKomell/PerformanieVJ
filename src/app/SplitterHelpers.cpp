#include "SplitterHelpers.h"

#include <QResizeEvent>
#include <QShowEvent>

namespace pvj::app {

namespace {

constexpr int kMinSplitterHandleWidth = 5;

} // namespace

void PvjSplitterHandle::applySafeCursor()
{
#ifndef QT_NO_CURSOR
    unsetCursor();
#endif
}

void PvjSplitterHandle::resizeEvent(QResizeEvent* event)
{
    QSplitterHandle::resizeEvent(event);
    // QSplitterHandle::resizeEvent may call setMask() when handleWidth < 5 (tiny grab area).
    // On Qt 6.10 Windows debug builds that can interact badly with mono bitmap conversion.
    clearMask();
    setAttribute(Qt::WA_MouseNoMask, false);
    setContentsMargins(0, 0, 0, 0);
    applySafeCursor();
}

void PvjSplitterHandle::showEvent(QShowEvent* event)
{
    QSplitterHandle::showEvent(event);
    applySafeCursor();
}

void PvjSplitterHandle::changeEvent(QEvent* event)
{
    QSplitterHandle::changeEvent(event);
    if (event->type() == QEvent::CursorChange) {
        applySafeCursor();
    }
}

void PvjSplitterHandle::enterEvent(QEnterEvent* event)
{
    QSplitterHandle::enterEvent(event);
    applySafeCursor();
}

void PvjSplitterHandle::mouseMoveEvent(QMouseEvent* event)
{
    QSplitterHandle::mouseMoveEvent(event);
    applySafeCursor();
}

void PvjSplitter::ensureSafeSplitterHandles()
{
    if (handleWidth() < kMinSplitterHandleWidth) {
        setHandleWidth(kMinSplitterHandleWidth);
    }
    applySafeCursorToAllHandles();
}

PvjSplitter::PvjSplitter(QWidget* parent)
    : QSplitter(parent)
{
    ensureSafeSplitterHandles();
}

PvjSplitter::PvjSplitter(Qt::Orientation orientation, QWidget* parent)
    : QSplitter(orientation, parent)
{
    ensureSafeSplitterHandles();
}

void PvjSplitter::setOrientation(Qt::Orientation o)
{
    QSplitter::setOrientation(o);
    applySafeCursorToAllHandles();
}

bool PvjSplitter::restoreState(const QByteArray& state)
{
    const bool ok = QSplitter::restoreState(state);
    if (ok) {
        ensureSafeSplitterHandles();
    }
    return ok;
}

void PvjSplitter::childEvent(QChildEvent* e)
{
    QSplitter::childEvent(e);
    if (e->added() && qobject_cast<QSplitterHandle*>(e->child()) != nullptr) {
        ensureSafeSplitterHandles();
    }
}

void PvjSplitter::applySafeCursorToAllHandles()
{
#ifndef QT_NO_CURSOR
    for (int i = 0; i < count(); ++i) {
        if (QSplitterHandle* h = handle(i)) {
            h->unsetCursor();
            h->clearMask();
        }
    }
#endif
}

} // namespace pvj::app

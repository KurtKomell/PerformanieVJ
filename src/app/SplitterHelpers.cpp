#include "SplitterHelpers.h"

#include <QResizeEvent>
#include <QShowEvent>

namespace pvj::app {

namespace {

constexpr int kMinSplitterHandleWidth = 5;

Qt::CursorShape safeResizeCursorFor(Qt::Orientation orientation)
{
    // Stock Win32 cursors (IDC_SIZEWE / IDC_SIZENS) — safe on Qt 6.10 Windows debug.
    // Do NOT use Qt::SplitHCursor / Qt::SplitVCursor (PNG → qt_createIconMask assert).
    return orientation == Qt::Horizontal ? Qt::SizeHorCursor : Qt::SizeVerCursor;
}

} // namespace

void PvjSplitterHandle::applySafeCursor()
{
#ifndef QT_NO_CURSOR
    const Qt::CursorShape want = safeResizeCursorFor(orientation());
    if (testAttribute(Qt::WA_SetCursor) && cursor().shape() == want) {
        return;
    }
    setCursor(want);
#endif
}

void PvjSplitterHandle::resizeEvent(QResizeEvent* event)
{
    // Skip QSplitterHandle::resizeEvent: it may call setMask() when handleWidth < 5.
    QWidget::resizeEvent(event);
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
    applySafeCursor();
    QSplitterHandle::enterEvent(event);
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
    const Qt::CursorShape want = safeResizeCursorFor(orientation());
    for (int i = 0; i < count(); ++i) {
        if (QSplitterHandle* h = handle(i)) {
            h->setCursor(want);
            h->clearMask();
        }
    }
#endif
}

} // namespace pvj::app

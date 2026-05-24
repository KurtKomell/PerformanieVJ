#pragma once

#include <QChildEvent>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QSplitter>
#include <QSplitterHandle>

namespace pvj::app {

/// Splitter handle that avoids pixmap-based resize cursors on Windows.
/// Qt::SplitHCursor / Qt::SplitVCursor / Qt::SizeHorCursor / Qt::SizeVerCursor can
/// trigger Q_ASSERT(bm.format() == QImage::Format_Mono) in qpixmap_win.cpp when Qt
/// builds the Win32 cursor mask (Qt 6.10+, some DPI / theme combinations).
class PvjSplitterHandle final : public QSplitterHandle
{
public:
    explicit PvjSplitterHandle(Qt::Orientation orientation, QSplitter* parent)
        : QSplitterHandle(orientation, parent)
    {
        applySafeCursor();
    }

protected:
    void enterEvent(QEnterEvent* event) override
    {
        applySafeCursor();
        QSplitterHandle::enterEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        applySafeCursor();
        QSplitterHandle::mouseMoveEvent(event);
    }

private:
    void applySafeCursor()
    {
#ifndef QT_NO_CURSOR
        // Do not use Split* or Size* shapes — they go through qt_createIconMask on Windows.
        unsetCursor();
#endif
    }
};

/// QSplitter that keeps handles free of pixmap cursors (including hidden handle 0).
/// QSplitter::setOrientation is not virtual and restoreState() calls it internally, so this
/// class also extends childEvent and restoreState to re-apply safe cursors whenever handles change.
class PvjSplitter final : public QSplitter
{
public:
    explicit PvjSplitter(QWidget* parent = nullptr)
        : QSplitter(parent)
    {
    }
    explicit PvjSplitter(Qt::Orientation orientation, QWidget* parent = nullptr)
        : QSplitter(orientation, parent)
    {
    }

    void setOrientation(Qt::Orientation o)
    {
        QSplitter::setOrientation(o);
        applySafeCursorToAllHandles();
    }

    bool restoreState(const QByteArray& state)
    {
        const bool ok = QSplitter::restoreState(state);
        if (ok) {
            applySafeCursorToAllHandles();
        }
        return ok;
    }

protected:
    QSplitterHandle* createHandle() override { return new PvjSplitterHandle(orientation(), this); }

    void childEvent(QChildEvent* e) override
    {
        QSplitter::childEvent(e);
        if (e->added() && qobject_cast<QSplitterHandle*>(e->child()) != nullptr) {
            applySafeCursorToAllHandles();
        }
    }

private:
    void applySafeCursorToAllHandles()
    {
#ifndef QT_NO_CURSOR
        for (int i = 0; i < count(); ++i) {
            if (QSplitterHandle* h = handle(i)) {
                h->unsetCursor();
            }
        }
#endif
    }
};

} // namespace pvj::app

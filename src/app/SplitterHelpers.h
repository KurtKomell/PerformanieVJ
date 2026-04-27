#pragma once

#include <QChildEvent>
#include <QSplitter>
#include <QSplitterHandle>

namespace pvj::app {

/// Splitter handle that avoids Qt::SplitHCursor / Qt::SplitVCursor on Windows.
/// Those shapes use pixmap cursors that can hit Q_ASSERT(bm.format() == QImage::Format_Mono)
/// in qpixmap_win.cpp (qt_createIconMask) on some Qt / DPI / PNG combinations.
class PvjSplitterHandle final : public QSplitterHandle
{
public:
    explicit PvjSplitterHandle(Qt::Orientation orientation, QSplitter* parent)
        : QSplitterHandle(orientation, parent)
    {
        applyResizeCursor();
    }

private:
    void applyResizeCursor()
    {
#ifndef QT_NO_CURSOR
        setCursor(orientation() == Qt::Horizontal ? Qt::SizeHorCursor : Qt::SizeVerCursor);
#endif
    }
};

/// QSplitter that keeps system resize cursors on all handles (including hidden handle 0).
/// QSplitter::setOrientation is not virtual and restoreState() calls it internally, so this
/// class also extends childEvent and restoreState to re-apply cursors whenever handles change.
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
        applyResizeCursorToAllHandles();
    }

    bool restoreState(const QByteArray& state)
    {
        const bool ok = QSplitter::restoreState(state);
        if (ok) {
            applyResizeCursorToAllHandles();
        }
        return ok;
    }

protected:
    QSplitterHandle* createHandle() override { return new PvjSplitterHandle(orientation(), this); }

    void childEvent(QChildEvent* e) override
    {
        QSplitter::childEvent(e);
        if (e->added() && qobject_cast<QSplitterHandle*>(e->child()) != nullptr) {
            applyResizeCursorToAllHandles();
        }
    }

private:
    void applyResizeCursorToAllHandles()
    {
#ifndef QT_NO_CURSOR
        const Qt::CursorShape shape = orientation() == Qt::Horizontal ? Qt::SizeHorCursor
                                                                      : Qt::SizeVerCursor;
        for (int i = 0; i < count(); ++i) {
            if (QSplitterHandle* h = handle(i)) {
                h->setCursor(shape);
            }
        }
#endif
    }
};

} // namespace pvj::app

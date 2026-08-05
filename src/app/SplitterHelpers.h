#pragma once

#include <QChildEvent>
#include <QEnterEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSplitter>
#include <QSplitterHandle>

namespace pvj::app {

/// Splitter handle that avoids pixmap-based Split* cursors on Windows.
/// Qt::SplitHCursor / Qt::SplitVCursor (and Drag*/Hand cursors) are PNG-based on
/// Windows and can trigger Q_ASSERT(bm.format() == QImage::Format_Mono) in
/// qpixmap_win.cpp (Qt 6.10+). We use stock SizeHor/SizeVer cursors instead.
class PvjSplitterHandle final : public QSplitterHandle
{
public:
    explicit PvjSplitterHandle(Qt::Orientation orientation, QSplitter* parent)
        : QSplitterHandle(orientation, parent)
    {
        applySafeCursor();
    }

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void applySafeCursor();
};

/// QSplitter that keeps handles free of pixmap cursors (including hidden handle 0).
/// QSplitter::setOrientation is not virtual and restoreState() calls it internally, so this
/// class also extends childEvent and restoreState to re-apply safe cursors whenever handles change.
class PvjSplitter final : public QSplitter
{
public:
    explicit PvjSplitter(QWidget* parent = nullptr);
    explicit PvjSplitter(Qt::Orientation orientation, QWidget* parent = nullptr);

    void setOrientation(Qt::Orientation o);
    bool restoreState(const QByteArray& state);

protected:
    QSplitterHandle* createHandle() override { return new PvjSplitterHandle(orientation(), this); }

    void childEvent(QChildEvent* e) override;

private:
    void ensureSafeSplitterHandles();
    void applySafeCursorToAllHandles();
};

} // namespace pvj::app

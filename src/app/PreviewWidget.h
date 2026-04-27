#pragma once

#include <QFrame>
#include <QImage>
#include <QList>

class QTimer;

namespace pvj::app {

// Placeholder preview surface. Shows a checker pattern with a label when no
// frame is available. When setFrame() is called the widget draws the image
// letterboxed while preserving its aspect ratio. In M5 this is replaced by a
// QRhiWidget that uploads textures to the GPU directly.
class PreviewWidget : public QFrame
{
    Q_OBJECT
public:
    explicit PreviewWidget(QWidget* parent = nullptr);

    void setLabel(const QString& text);
    QString label() const { return m_label; }

public slots:
    void setFrame(QImage frame, qint64 pts = 0);
    void clearFrame();
    /// Cycles through images (e.g. five video keyframes). Clears single-frame mode.
    void setFilmstripFrames(const QList<QImage>& frames);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override { return {320, 180}; }

private:
    QString m_label;
    QImage  m_frame;
    qint64  m_pts = 0;

    QTimer*        m_filmTimer = nullptr;
    QList<QImage> m_filmFrames;
    int             m_filmIndex = 0;
};

} // namespace pvj::app

#include "PreviewWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QTimer>

namespace pvj::app {

PreviewWidget::PreviewWidget(QWidget* parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(true);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(22, 24, 30));
    setPalette(pal);

    m_filmTimer = new QTimer(this);
    m_filmTimer->setInterval(220);
    connect(m_filmTimer, &QTimer::timeout, this, [this] {
        if (m_filmFrames.size() < 2) {
            return;
        }
        m_filmIndex = (m_filmIndex + 1) % m_filmFrames.size();
        update();
    });
}

void PreviewWidget::setLabel(const QString& text)
{
    if (m_label != text) {
        m_label = text;
        update();
    }
}

void PreviewWidget::setFrame(QImage frame, qint64 pts)
{
    m_filmTimer->stop();
    m_filmFrames.clear();
    m_filmIndex = 0;
    m_frame = std::move(frame);
    m_pts = pts;
    update();
}

void PreviewWidget::clearFrame()
{
    m_filmTimer->stop();
    m_filmFrames.clear();
    m_filmIndex = 0;
    m_frame = {};
    m_pts = 0;
    update();
}

void PreviewWidget::setFilmstripFrames(const QList<QImage>& frames)
{
    m_filmFrames = frames;
    m_filmIndex = 0;
    m_frame = {};
    m_pts = 0;
    if (m_filmFrames.size() > 1) {
        m_filmTimer->start();
    } else {
        m_filmTimer->stop();
    }
    update();
}

void PreviewWidget::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect r = contentsRect();
    p.fillRect(r, QColor(14, 16, 20));

    const QImage* drawImg = nullptr;
    if (!m_filmFrames.isEmpty()) {
        drawImg = &m_filmFrames[m_filmIndex % m_filmFrames.size()];
    } else if (!m_frame.isNull()) {
        drawImg = &m_frame;
    }

    if (drawImg && !drawImg->isNull()) {
        const QSize img = drawImg->size();
        const double srcA = double(img.width()) / double(img.height());
        const double dstA = double(r.width())   / double(r.height());
        QRect target = r;
        if (srcA > dstA) {
            const int h = int(r.width() / srcA);
            target = QRect(r.left(), r.top() + (r.height() - h) / 2, r.width(), h);
        } else {
            const int w = int(r.height() * srcA);
            target = QRect(r.left() + (r.width() - w) / 2, r.top(), w, r.height());
        }
        p.drawImage(target, *drawImg);

        if (!m_label.isEmpty()) {
            p.setPen(QColor(230, 230, 230, 220));
            QFont f = p.font();
            f.setBold(true);
            p.setFont(f);
            p.drawText(r.adjusted(8, 6, -8, -6), Qt::AlignTop | Qt::AlignLeft, m_label);
        }
        return;
    }

    // No frame: checker pattern with centered label
    const int cell = 16;
    const QColor a(26, 29, 36);
    const QColor b(36, 40, 50);
    for (int y = r.top(); y < r.bottom(); y += cell) {
        for (int x = r.left(); x < r.right(); x += cell) {
            const bool even = ((x / cell) + (y / cell)) % 2 == 0;
            p.fillRect(QRect(x, y, cell, cell), even ? a : b);
        }
    }
    if (!m_label.isEmpty()) {
        p.setPen(QColor(220, 220, 220));
        QFont f = p.font();
        f.setPointSizeF(f.pointSizeF() * 1.4);
        f.setBold(true);
        p.setFont(f);
        p.drawText(r, Qt::AlignCenter, m_label);
    }
}

} // namespace pvj::app

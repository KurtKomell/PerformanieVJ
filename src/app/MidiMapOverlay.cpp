#include "MidiMapOverlay.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>

namespace pvj::app {

namespace {

constexpr int kBorderPx = 2;
constexpr QColor kGreenBorder(80, 220, 120);
constexpr QColor kBadgeBg(20, 32, 24, 210);
constexpr QColor kBadgeText(180, 255, 200);

} // namespace

MidiMapOverlay::MidiMapOverlay(QWidget* target)
    : QWidget(target)
{
    setObjectName(QLatin1String(kOverlayObjectName));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    if (target) {
        target->installEventFilter(this);
        syncToTarget();
        raise();
    }
}

void MidiMapOverlay::setMappingLabel(const QString& label)
{
    if (m_mappingLabel == label) {
        return;
    }
    m_mappingLabel = label;
    update();
}

MidiMapOverlay* MidiMapOverlay::overlayFor(QWidget* target)
{
    if (!target) {
        return nullptr;
    }
    return target->findChild<MidiMapOverlay*>(QLatin1String(kOverlayObjectName));
}

void MidiMapOverlay::setActiveOn(QWidget* target, bool active, const QString& label)
{
    if (!target) {
        return;
    }
    MidiMapOverlay* overlay = overlayFor(target);
    if (!active) {
        if (overlay) {
            overlay->hide();
        }
        return;
    }
    if (!overlay) {
        overlay = new MidiMapOverlay(target);
    }
    overlay->setMappingLabel(label.isEmpty() ? QStringLiteral("—") : label);
    overlay->syncToTarget();
    overlay->show();
    overlay->raise();
}

void MidiMapOverlay::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(1, 1, -2, -2);
    p.setPen(QPen(kGreenBorder, kBorderPx));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 4, 4);

    if (m_mappingLabel.isEmpty()) {
        return;
    }

    QFont badgeFont = font();
    badgeFont.setBold(true);
    badgeFont.setPointSizeF(std::max(7.0, badgeFont.pointSizeF() * 0.72));
    p.setFont(badgeFont);
    const QFontMetrics fm(badgeFont);
    const int padH = 4;
    const int padV = 2;
    const int tw = fm.horizontalAdvance(m_mappingLabel);
    const int th = fm.height();
    QRect badgeRect(width() - tw - padH * 2 - 3,
                    height() - th - padV * 2 - 3,
                    tw + padH * 2,
                    th + padV * 2);
    QPainterPath badgePath;
    badgePath.addRoundedRect(badgeRect, 3, 3);
    p.fillPath(badgePath, kBadgeBg);
    p.setPen(Qt::NoPen);
    p.setPen(kBadgeText);
    p.drawText(badgeRect, Qt::AlignCenter, m_mappingLabel);
}

bool MidiMapOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget()) {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::LayoutRequest:
            syncToTarget();
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MidiMapOverlay::syncToTarget()
{
    QWidget* target = parentWidget();
    if (!target) {
        return;
    }
    setGeometry(target->rect());
}

} // namespace pvj::app

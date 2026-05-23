#include "BankGridWidget.h"

#include "core/Project.h"
#include "video/ThumbnailExtractor.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEnterEvent>
#include <QFileInfo>
#include <QList>
#include <QMimeData>
#include <QUrl>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTimer>
#include <QtConcurrent>
#include <QVBoxLayout>

#include <QtGlobal>

#include <QPointer>
#include <QVector>

#include <algorithm>

namespace pvj::app {

namespace {

void drawCellDropShadow(QPainter& p, const QRectF& r, qreal radius)
{
    p.save();
    p.translate(1, 2);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 72));
    p.drawRoundedRect(r, radius, radius);
    p.restore();
}

/// Tight outer glow + crisp stroke; keep expansion small so 84×64 cells do not clip badly.
void drawGlowFrame(QPainter& p, const QRectF& r, qreal radius, const QColor& core, int passes = 3)
{
    for (int i = passes; i >= 1; --i) {
        const qreal spread = qreal(i) * 1.25;
        const qreal width  = 1.0 + qreal(i) * 1.5;
        QColor c = core;
        c.setAlpha(qBound(12, 18 + i * 16, 95));
        p.setPen(QPen(c, width));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(-spread, -spread, spread, spread),
                          radius + spread * 0.4,
                          radius + spread * 0.4);
    }
    QColor rim = core.lighter(108);
    rim.setAlpha(240);
    p.setPen(QPen(rim, 2));
    p.drawRoundedRect(r, radius, radius);
}

bool mimeDataHasLocalFile(const QMimeData* md)
{
    if (!md) {
        return false;
    }
    for (const QUrl& u : md->urls()) {
        if (!u.isLocalFile()) {
            continue;
        }
        if (QFileInfo(u.toLocalFile()).isFile()) {
            return true;
        }
    }
    return false;
}

QString firstLocalFilePath(const QMimeData* md)
{
    if (!md) {
        return {};
    }
    for (const QUrl& u : md->urls()) {
        if (!u.isLocalFile()) {
            continue;
        }
        const QString p = u.toLocalFile();
        if (QFileInfo(p).isFile()) {
            return p;
        }
    }
    return {};
}

} // namespace

using pvj::core::Project;

// Small internal helper widget for a single cell. Shows the cell index and
// a colored status indicator (filled = has content, empty = default cell).
class CellButton : public QPushButton
{
    Q_OBJECT
public:
    CellButton(int index, QWidget* parent = nullptr)
        : QPushButton(parent)
        , m_index(index)
    {
        setFixedSize(84, 64);
        setCheckable(true);
        setAutoExclusive(false);
        setFocusPolicy(Qt::NoFocus);
        setToolTip(tr("Left click: trigger clip on a mix layer (blue outline when playing)\n"
                      "Right click: show that layer in the large clip preview (orange outline)\n"
                      "Hover: short filmstrip preview\n"
                      "Double-click: filters / nodes\n"
                      "MIDI map mode (Ctrl+M): assign trigger"));

        m_triggerTimer.setSingleShot(true);
        connect(&m_triggerTimer, &QTimer::timeout, this, [this] {
            emit cellTriggeredDelayed(m_index);
        });
        connect(this, &QPushButton::clicked, this, &CellButton::onClicked);

        m_filmTimer.setInterval(220);
        connect(&m_filmTimer, &QTimer::timeout, this, [this] {
            if (!m_cellHovered || m_filmFrames.size() < 2) {
                return;
            }
            m_filmIndex = (m_filmIndex + 1) % m_filmFrames.size();
            update();
        });
    }

    void setCellState(bool hasContent, const QString& label)
    {
        if (m_hasContent != hasContent || m_label != label) {
            m_hasContent = hasContent;
            m_label = label;
            update();
        }
    }

    /// Mix slot index 0..11 when this cell is routed to that mixer slot; -1 = none.
    void setMixSlot(int slotIndex)
    {
        if (m_mixSlot != slotIndex) {
            m_mixSlot = slotIndex;
            update();
        }
    }

    void setPeekHighlight(bool on)
    {
        if (m_peekHighlight != on) {
            m_peekHighlight = on;
            update();
        }
    }

    void setFilmstrip(const QVector<QImage>& frames)
    {
        m_filmTimer.stop();
        m_filmFrames.clear();
        const int n = qMin(5, frames.size());
        m_filmFrames.reserve(n);
        for (int i = 0; i < n; ++i) {
            if (!frames[i].isNull()) {
                m_filmFrames.append(frames[i]);
            }
        }
        m_filmIndex = 0;
        if (m_cellHovered && m_filmFrames.size() > 1) {
            m_filmTimer.start();
        }
        update();
    }

    void clearThumbnail()
    {
        m_filmTimer.stop();
        m_filmFrames.clear();
        m_filmIndex = 0;
        update();
    }

    int cellIndex() const { return m_index; }

    void setMidiMapMode(bool on)
    {
        if (m_midiMapMode == on) {
            return;
        }
        m_midiMapMode = on;
        update();
    }

signals:
    void cellPressedForSelection(int index);
    void cellTriggeredDelayed(int index);
    void cellEditRequested(int index);
    void cellRightClicked();
    /// MIDI mapping edit mode: left-click should learn trigger for this cell index.
    void midiLearnCellRequested(int index);

protected:
    void enterEvent(QEnterEvent* e) override
    {
        QPushButton::enterEvent(e);
        m_cellHovered = true;
        if (m_filmFrames.size() > 1) {
            m_filmTimer.start();
        }
        update();
    }

    void leaveEvent(QEvent* e) override
    {
        QPushButton::leaveEvent(e);
        m_cellHovered = false;
        m_filmTimer.stop();
        m_filmIndex = 0;
        update();
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::RightButton) {
            emit cellRightClicked();
            e->accept();
            return;
        }
        if (e->button() == Qt::LeftButton) {
            QPushButton::mousePressEvent(e);
            emit cellPressedForSelection(m_index);
            e->accept();
            return;
        }
        QPushButton::mousePressEvent(e);
    }

    void mouseDoubleClickEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) {
            e->accept();
            return;
        }
        m_triggerTimer.stop();
        m_skipNextClick = true;
        emit cellEditRequested(m_index);
        QPushButton::mouseDoubleClickEvent(e);
    }

    void paintEvent(QPaintEvent* /*e*/) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        constexpr qreal kCellRadius = 6.0;

        const QRectF r = rect().adjusted(1, 1, -1, -1);
        QPainterPath clipPath;
        clipPath.addRoundedRect(r, kCellRadius, kCellRadius);

        const QImage* thumb = nullptr;
        if (!m_filmFrames.isEmpty()) {
            if (m_cellHovered && m_filmFrames.size() > 1) {
                thumb = &m_filmFrames[m_filmIndex % m_filmFrames.size()];
            } else {
                thumb = &m_filmFrames.front();
            }
        }
        if (thumb && !thumb->isNull()) {
            p.setClipPath(clipPath);
            const QSizeF thumbSize(thumb->width(), thumb->height());
            const QSizeF scaled = thumbSize.scaled(r.size(), Qt::KeepAspectRatio);
            QRectF drawRect(QPointF(r.center() - QPointF(scaled.width() / 2, scaled.height() / 2)),
                            scaled);
            p.drawImage(drawRect, *thumb);
            p.setClipping(false);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 100));
            p.drawPath(clipPath);
        } else {
            QLinearGradient fill(r.topLeft(), r.bottomRight());
            const bool onMix = (m_mixSlot >= 0);
            const bool sel   = isChecked();
            if (sel && onMix) {
                fill.setColorAt(0, QColor(72, 58, 42));
                fill.setColorAt(0.5, QColor(44, 52, 68));
                fill.setColorAt(1, QColor(32, 30, 28));
            } else if (sel) {
                fill.setColorAt(0, QColor(78, 52, 36));
                fill.setColorAt(1, QColor(38, 28, 22));
            } else if (onMix) {
                fill.setColorAt(0, QColor(40, 52, 72));
                fill.setColorAt(1, QColor(24, 30, 44));
            } else if (m_hasContent) {
                fill.setColorAt(0, QColor(44, 58, 68));
                fill.setColorAt(1, QColor(30, 40, 48));
            } else {
                fill.setColorAt(0, QColor(46, 50, 60));
                fill.setColorAt(1, QColor(24, 26, 32));
            }
            p.setPen(QPen(QColor(58, 64, 78), 1));
            p.setBrush(fill);
            p.drawRoundedRect(r, kCellRadius, kCellRadius);
        }

        // Blue = routed to a mix layer (left-click trigger). Orange = clip preview peek (right-click).
        const QColor kOrangePeek(255, 145, 64);
        const QColor kBlueMix(58, 156, 255);
        const QColor kBorderIdle(52, 58, 72);
        const QColor kSelectionMuted(120, 128, 145);

        const bool onMix  = (m_mixSlot >= 0);
        const bool peek   = m_peekHighlight;
        const bool sel    = isChecked();

        const bool showShadow = onMix || peek || sel;
        if (showShadow) {
            drawCellDropShadow(p, r, kCellRadius);
        }

        p.setBrush(Qt::NoBrush);
        if (onMix && peek) {
            drawGlowFrame(p, r, kCellRadius, kBlueMix, 3);
            const QRectF inner = r.adjusted(5, 5, -5, -5);
            drawGlowFrame(p, inner, 4.0, kOrangePeek, 2);
        } else if (onMix) {
            drawGlowFrame(p, r, kCellRadius, kBlueMix, 3);
        } else if (peek) {
            drawGlowFrame(p, r, kCellRadius, kOrangePeek, 3);
        } else if (sel) {
            p.setPen(QPen(kSelectionMuted, 1.5));
            p.drawRoundedRect(r, kCellRadius, kCellRadius);
        } else {
            p.setPen(QPen(kBorderIdle, 1));
            p.drawRoundedRect(r, kCellRadius, kCellRadius);
        }

        if (onMix) {
            p.setPen(QColor(220, 236, 255));
            QFont tag = p.font();
            tag.setPointSizeF(std::max(6.0, tag.pointSizeF() * 0.65));
            tag.setBold(true);
            p.setFont(tag);
            p.drawText(r.adjusted(3, 2, -3, -3), Qt::AlignTop | Qt::AlignRight,
                       QStringLiteral("M%1").arg(m_mixSlot + 1));
        }

        p.setPen(QColor(255, 255, 255));
        QFont small = p.font();
        small.setPointSizeF(small.pointSizeF() * 0.82);
        small.setBold(true);
        p.setFont(small);
        p.drawText(r.adjusted(5, 3, -5, -5), Qt::AlignTop | Qt::AlignLeft,
                   QString::number(m_index + 1));

        if (!m_label.isEmpty() && m_filmFrames.isEmpty()) {
            QFont f = p.font();
            f.setBold(false);
            p.setFont(f);
            p.drawText(r.adjusted(5, 3, -5, -5), Qt::AlignBottom | Qt::AlignLeft,
                       m_label);
        }

        if (m_midiMapMode) {
            p.setPen(QPen(QColor(120, 255, 160), 2));
            p.setBrush(QColor(80, 220, 120, 85));
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), kCellRadius, kCellRadius);
        }
    }

private slots:
    void onClicked()
    {
        if (m_skipNextClick) {
            m_skipNextClick = false;
            return;
        }
        if (m_midiMapMode) {
            emit midiLearnCellRequested(m_index);
            return;
        }
        m_triggerTimer.start(QApplication::doubleClickInterval());
    }

private:
    int     m_index = 0;
    bool    m_hasContent = false;
    QString m_label;
    bool    m_peekHighlight = false;
    QTimer           m_triggerTimer;
    QTimer           m_filmTimer;
    QVector<QImage> m_filmFrames;
    int              m_filmIndex = 0;
    bool             m_cellHovered = false;
    bool             m_skipNextClick = false;
    int              m_mixSlot = -1;
    bool             m_midiMapMode = false;
};

BankGridWidget::BankGridWidget(QWidget* parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    buildUi();
}

void BankGridWidget::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);
    outer->setSpacing(4);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(6);

    m_bankTabs = new QTabBar(this);
    m_bankTabs->setExpanding(false);
    m_bankTabs->setDrawBase(false);
    connect(m_bankTabs, &QTabBar::currentChanged,
            this, &BankGridWidget::onBankTabChanged);

    header->addWidget(m_bankTabs, 1);
    outer->addLayout(header);

    m_grid = new QGridLayout;
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(4);
    m_grid->setVerticalSpacing(4);
    rebuildGridButtons();
    outer->addLayout(m_grid, 1);
}

void BankGridWidget::rebuildGridButtons()
{
    while (QLayoutItem* item = m_grid->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            delete w;
        }
        delete item;
    }
    m_cells.clear();
    for (int r = 0; r < m_gridRows; ++r) {
        for (int c = 0; c < m_gridCols; ++c) {
            const int idx = r * m_gridCols + c;
            auto* btn = new CellButton(idx, this);
            connect(btn, &CellButton::cellPressedForSelection, this, [this, idx](int) {
                m_selectedCell = idx;
                for (int j = 0; j < m_cells.size(); ++j) {
                    m_cells[j]->setChecked(j == idx);
                }
                emit cellSelected(m_bankSetIndex, m_bankIndex, idx);
            });
            connect(btn, &CellButton::cellTriggeredDelayed, this, [this, idx](int) {
                emit cellTriggered(m_bankSetIndex, m_bankIndex, idx);
            });
            connect(btn, &CellButton::cellEditRequested, this, [this, idx](int) {
                emit cellEditRequested(m_bankSetIndex, m_bankIndex, idx);
            });
            connect(btn, &CellButton::cellRightClicked, this, [this, idx]() {
                emit cellPeekPreviewRequested(m_bankSetIndex, m_bankIndex, idx);
            });
            connect(btn, &CellButton::midiLearnCellRequested, this, [this, idx](int) {
                emit midiLearnCellTriggerRequested(m_bankSetIndex, m_bankIndex, idx);
            });
            m_grid->addWidget(btn, r, c);
            m_cells.append(btn);
        }
    }
}

void BankGridWidget::setProject(pvj::core::Project* project)
{
    m_project = project;
    m_bankSetIndex = 0;
    m_bankIndex    = 0;
    m_selectedCell = -1;
    m_filmCache.clear();
    m_thumbPending.clear();
    if (m_project) {
        const int rows = qBound(1, m_project->settings.matrix.gridRows, 16);
        const int cols = qBound(1, m_project->settings.matrix.gridCols, 16);
        setGridDimensions(rows, cols);
    }
    refresh();
}

void BankGridWidget::setGridDimensions(int rows, int cols)
{
    const int nextRows = qBound(1, rows, 16);
    const int nextCols = qBound(1, cols, 16);
    if (nextRows == m_gridRows && nextCols == m_gridCols) {
        return;
    }
    m_gridRows = nextRows;
    m_gridCols = nextCols;
    rebuildGridButtons();
    if (m_selectedCell >= m_cells.size()) {
        m_selectedCell = -1;
    }
    if (m_project) {
        refresh();
    }
}

void BankGridWidget::selectCell(int cellIndex)
{
    if (cellIndex < 0 || cellIndex >= m_cells.size()) {
        return;
    }
    m_selectedCell = cellIndex;
    for (int j = 0; j < m_cells.size(); ++j) {
        m_cells[j]->setChecked(j == m_selectedCell);
    }
    emit cellSelected(m_bankSetIndex, m_bankIndex, m_selectedCell);
}

void BankGridWidget::setMixSlotPlayback(const std::array<MixSlotCellRef, 12>& mixSlotRefs)
{
    m_mixSlots = mixSlotRefs;
    updateCellVisuals(false);
}

void BankGridWidget::setPeekCellHighlight(const MixSlotCellRef& ref)
{
    m_peekRef = ref;
    updateCellVisuals(false);
}

void BankGridWidget::revealAndSelectCell(int bankSetIndex, int bankIndex, int cellIndex)
{
    if (!m_project || m_project->bankSets.isEmpty()) {
        return;
    }
    m_bankSetIndex = qBound(0, bankSetIndex, m_project->bankSets.size() - 1);
    const auto& set = m_project->bankSets[m_bankSetIndex];
    if (set.banks.isEmpty()) {
        return;
    }
    m_bankIndex = qBound(0, bankIndex, set.banks.size() - 1);
    const auto& bank = set.banks[m_bankIndex];
    const int nCells = qMin(int(m_cells.size()), bank.cells.size());
    if (nCells <= 0) {
        m_selectedCell = -1;
        rebuildBankTabs();
        updateCellVisuals();
        emit bankSelected(m_bankSetIndex, m_bankIndex);
        return;
    }
    const int maxCell = nCells - 1;
    m_selectedCell = qBound(0, cellIndex, maxCell);

    rebuildBankTabs();
    {
        QSignalBlocker block(m_bankTabs);
        m_bankTabs->setCurrentIndex(m_bankIndex);
    }

    updateCellVisuals();
    for (int j = 0; j < m_cells.size(); ++j) {
        m_cells[j]->setChecked(j == m_selectedCell);
    }
    emit bankSelected(m_bankSetIndex, m_bankIndex);
    emit cellSelected(m_bankSetIndex, m_bankIndex, m_selectedCell);
}

void BankGridWidget::refresh()
{
    rebuildBankTabs();
    updateCellVisuals();
}

void BankGridWidget::rebuildBankTabs()
{
    QSignalBlocker block(m_bankTabs);
    while (m_bankTabs->count() > 0) {
        m_bankTabs->removeTab(0);
    }

    if (!m_project || m_bankSetIndex < 0 || m_bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    const auto& set = m_project->bankSets[m_bankSetIndex];
    for (int i = 0; i < set.banks.size(); ++i) {
        const QString name = set.banks[i].name.isEmpty()
            ? tr("Bank %1").arg(i + 1)
            : set.banks[i].name;
        m_bankTabs->addTab(name);
    }
    m_bankTabs->setCurrentIndex(qBound(0, m_bankIndex, m_bankTabs->count() - 1));
}

void BankGridWidget::updateCellVisuals(bool reloadThumbnails)
{
    using pvj::core::VisualType;

    if (!m_project || m_bankSetIndex < 0 || m_bankSetIndex >= m_project->bankSets.size()) {
        for (auto* b : m_cells) {
            b->setCellState(false, {});
            b->setChecked(false);
            b->setEnabled(false);
            b->setMixSlot(-1);
            b->setPeekHighlight(false);
            b->setMidiMapMode(m_midiMappingEditMode);
        }
        return;
    }

    const auto& set = m_project->bankSets[m_bankSetIndex];
    if (m_bankIndex < 0 || m_bankIndex >= set.banks.size()) {
        return;
    }
    const auto& bank = set.banks[m_bankIndex];

    for (int i = 0; i < m_cells.size(); ++i) {
        auto* btn = m_cells[i];
        btn->setEnabled(true);
        btn->setChecked(i == m_selectedCell);
        if (i >= bank.cells.size()) {
            btn->setCellState(false, {});
            btn->setMixSlot(-1);
            btn->setPeekHighlight(false);
            continue;
        }
        int mixSlot = -1;
        for (int j = 0; j < 12; ++j) {
            const MixSlotCellRef& ref = m_mixSlots[static_cast<size_t>(j)];
            if (ref.bankSet == m_bankSetIndex && ref.bank == m_bankIndex && ref.cell == i) {
                mixSlot = j;
                break;
            }
        }
        btn->setMixSlot(mixSlot);
        const bool peekHere = (m_peekRef.bankSet == m_bankSetIndex
                               && m_peekRef.bank == m_bankIndex
                               && m_peekRef.cell == i);
        btn->setPeekHighlight(peekHere);
        const auto& cell = bank.cells[i];
        bool has = false;
        QString label;
        if (reloadThumbnails) {
            btn->clearThumbnail();
        }
        if (cell.visual.type == VisualType::Media) {
            has = !cell.visual.mediaId.isNull();
            if (has) {
                const auto* m = m_project->findMedia(cell.visual.mediaId);
                if (m) {
                    label = m->displayName.isEmpty()
                        ? QFileInfo(m->path).fileName() : m->displayName;
                    if (reloadThumbnails) {
                        if (m_filmCache.contains(cell.visual.mediaId)) {
                            btn->setFilmstrip(m_filmCache.value(cell.visual.mediaId));
                        } else {
                            ensureThumbnail(cell.visual.mediaId, m->path);
                        }
                    }
                }
            }
        } else if (cell.visual.type == VisualType::Generator) {
            has = true;
            if (cell.visual.generator == pvj::core::GeneratorKind::InternalFeedback) {
                label = tr("FB");
            } else {
                label = tr("GEN");
            }
        }
        btn->setCellState(has, label);
        btn->setMidiMapMode(m_midiMappingEditMode);
    }
}

void BankGridWidget::setMidiMappingEditMode(bool on)
{
    if (m_midiMappingEditMode == on) {
        return;
    }
    m_midiMappingEditMode = on;
    for (auto* b : m_cells) {
        b->setMidiMapMode(on);
    }
}

bool BankGridWidget::midiMappingEditMode() const
{
    return m_midiMappingEditMode;
}

void BankGridWidget::ensureThumbnail(const QUuid& mediaId, const QString& absolutePath)
{
    if (mediaId.isNull() || absolutePath.isEmpty()) {
        return;
    }
    if (m_thumbPending.contains(mediaId)) {
        return;
    }
    m_thumbPending.insert(mediaId);

    QPointer<BankGridWidget> safe(this);
    (void)QtConcurrent::run([safe, mediaId, absolutePath]() {
        QList<QImage> images = pvj::video::ThumbnailExtractor::extractPreviewKeyframes(
            absolutePath, 5, QSize(160, 96));

        if (images.size() < 2) {
            images.clear();
            const QString suf = QFileInfo(absolutePath).suffix().toLower();
            static const QStringList kRaster = {
                QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("webp"),
            };
            if (kRaster.contains(suf)) {
                QImage im;
                if (im.load(absolutePath)) {
                    images.append(im.scaled(QSize(160, 96), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
            }
            if (images.isEmpty()) {
                const pvj::video::ThumbnailResult r = pvj::video::ThumbnailExtractor::extract(
                    absolutePath, QSize(160, 96), -1);
                if (r.ok && !r.image.isNull()) {
                    images.append(r.image);
                }
            }
        }

        if (!safe) {
            return;
        }
        QMetaObject::invokeMethod(
            safe.data(),
            [safe, mediaId, images]() {
                if (!safe) {
                    return;
                }
                QVector<QImage> framesOut;
                framesOut.reserve(images.size());
                for (const QImage& im : images) {
                    if (!im.isNull()) {
                        QImage safeImage = im;
                        if (safeImage.format() != QImage::Format_RGB32) {
                            safeImage = safeImage.convertToFormat(QImage::Format_RGB32);
                        }
                        framesOut.push_back(safeImage);
                    }
                }
                safe->applyThumbnailStrip(mediaId, framesOut);
            },
            Qt::QueuedConnection);
    });
}

void BankGridWidget::applyThumbnailStrip(const QUuid& mediaId, const QVector<QImage>& frames)
{
    m_thumbPending.remove(mediaId);
    if (!frames.isEmpty()) {
        m_filmCache.insert(mediaId, frames);
    } else {
        m_filmCache.remove(mediaId);
    }
    if (!m_project) {
        return;
    }
    if (m_bankSetIndex < 0 || m_bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    const auto& set = m_project->bankSets[m_bankSetIndex];
    if (m_bankIndex < 0 || m_bankIndex >= set.banks.size()) {
        return;
    }
    const auto& bk = set.banks[m_bankIndex];
    for (int i = 0; i < m_cells.size() && i < bk.cells.size(); ++i) {
        if (bk.cells[i].visual.mediaId == mediaId) {
            if (frames.isEmpty()) {
                m_cells[i]->clearThumbnail();
            } else {
                m_cells[i]->setFilmstrip(frames);
            }
        }
    }
}

void BankGridWidget::onBankTabChanged(int idx)
{
    if (idx < 0) {
        return;
    }
    m_bankIndex = idx;
    m_selectedCell = -1;
    updateCellVisuals();
    emit bankSelected(m_bankSetIndex, m_bankIndex);
}

void BankGridWidget::setBankSetIndex(int bankSetIndex)
{
    if (!m_project || m_project->bankSets.isEmpty()) {
        return;
    }
    m_bankSetIndex = qBound(0, bankSetIndex, m_project->bankSets.size() - 1);
    m_bankIndex    = 0;
    m_selectedCell = -1;
    refresh();
    emit bankSelected(m_bankSetIndex, m_bankIndex);
}

void BankGridWidget::stepBank(int delta)
{
    if (!m_project) {
        return;
    }
    if (m_bankSetIndex < 0 || m_bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    const auto& set = m_project->bankSets[m_bankSetIndex];
    if (set.banks.isEmpty()) {
        return;
    }
    m_bankIndex = qBound(0, m_bankIndex + delta, set.banks.size() - 1);
    QSignalBlocker block(m_bankTabs);
    m_bankTabs->setCurrentIndex(m_bankIndex);
    updateCellVisuals();
    emit bankSelected(m_bankSetIndex, m_bankIndex);
}

void BankGridWidget::selectBank(int bankIndex)
{
    if (!m_project) {
        return;
    }
    if (m_bankSetIndex < 0 || m_bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    const auto& set = m_project->bankSets[m_bankSetIndex];
    if (set.banks.isEmpty()) {
        return;
    }
    m_bankIndex = qBound(0, bankIndex, set.banks.size() - 1);
    QSignalBlocker block(m_bankTabs);
    m_bankTabs->setCurrentIndex(m_bankIndex);
    updateCellVisuals();
    emit bankSelected(m_bankSetIndex, m_bankIndex);
}

int BankGridWidget::cellIndexAtPosition(const QPoint& posInThisWidget) const
{
    for (int i = 0; i < m_cells.size(); ++i) {
        if (m_cells[i]->geometry().contains(posInThisWidget)) {
            return i;
        }
    }
    return -1;
}

void BankGridWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (mimeDataHasLocalFile(event->mimeData())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void BankGridWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (!mimeDataHasLocalFile(event->mimeData())) {
        event->ignore();
        return;
    }
    if (cellIndexAtPosition(event->position().toPoint()) >= 0) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void BankGridWidget::dropEvent(QDropEvent* event)
{
    const QString path = firstLocalFilePath(event->mimeData());
    if (path.isEmpty()) {
        event->ignore();
        return;
    }
    const int cell = cellIndexAtPosition(event->position().toPoint());
    if (cell < 0) {
        event->ignore();
        return;
    }
    emit mediaDroppedOnCell(m_bankSetIndex, m_bankIndex, cell, path);
    event->acceptProposedAction();
}

} // namespace pvj::app

#include "BankGridWidget.moc"

#include "NodeGraphWidget.h"

#include "FilterPickerDialog.h"
#include "MidiLearnMenu.h"
#include "MidiMapOverlay.h"
#include "core/FilterCatalog.h"
#include "core/FilterEffectIds.h"
#include "core/FilterParamSchema.h"
#include "render/maxine/MaxineFilterBackend.h"
#include "core/Model.h"
#include "core/Project.h"

#include <QColorDialog>
#include <QCoreApplication>
#include <QContextMenuEvent>
#include <QFormLayout>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QVBoxLayout>
#include <QtMath>

namespace pvj::app {

using pvj::core::Cell;
using pvj::core::CellFilterNode;
using pvj::core::Project;

namespace {

constexpr int kUnitSliderMax = 1000;

QString elideMiddle(const QFontMetricsF& fm, const QString& s, double maxW)
{
    if (fm.horizontalAdvance(s) <= maxW) {
        return s;
    }
    return fm.elidedText(s, Qt::ElideMiddle, int(maxW));
}

int rangeToSlider(double value, double minValue, double maxValue)
{
    if (qFuzzyCompare(minValue, maxValue)) {
        return 0;
    }
    const double t = (qBound(minValue, value, maxValue) - minValue) / (maxValue - minValue);
    return int(qRound(qBound(0.0, t, 1.0) * double(kUnitSliderMax)));
}

double sliderToRange(int sliderValue, double minValue, double maxValue)
{
    const double t = qBound(0.0, double(sliderValue) / double(kUnitSliderMax), 1.0);
    return minValue + (maxValue - minValue) * t;
}

QString formatFilterParamValue(const pvj::core::FilterParamSpec& spec, double value)
{
    using pvj::core::FilterParamKind;
    switch (spec.kind) {
    case FilterParamKind::Percent:
        return QString::number(int(qRound(qBound(0.0, value, 1.0) * 100.0))) + QLatin1Char('%');
    case FilterParamKind::Angle:
        return QString::number(value, 'f', 1) + QChar(0xB0);
    case FilterParamKind::Bool:
        return value >= 0.5 ? QObject::tr("On") : QObject::tr("Off");
    case FilterParamKind::EnumIndex: {
        const int idx = qBound(0, int(qRound(value)), qMax(0, spec.enumLabels.size() - 1));
        return spec.enumLabels.value(idx);
    }
    case FilterParamKind::Color: {
        const int rgb = int(qBound(0.0, value, 16777215.0));
        return QStringLiteral("#%1%2%3")
            .arg((rgb >> 16) & 0xFF, 2, 16, QChar('0'))
            .arg((rgb >> 8) & 0xFF, 2, 16, QChar('0'))
            .arg(rgb & 0xFF, 2, 16, QChar('0'));
    }
    default:
        return QString::number(value, 'f', 3);
    }
}

int sliderValueForParam(const pvj::core::FilterParamSpec& spec, double value)
{
    using pvj::core::FilterParamKind;
    switch (spec.kind) {
    case FilterParamKind::EnumIndex:
        return qBound(0, int(qRound(value)), qMax(0, spec.enumLabels.size() - 1));
    case FilterParamKind::Bool:
        return value >= 0.5 ? kUnitSliderMax : 0;
    default:
        return rangeToSlider(value, spec.minV, spec.maxV);
    }
}

double paramValueFromSlider(const pvj::core::FilterParamSpec& spec, int sliderValue)
{
    using pvj::core::FilterParamKind;
    switch (spec.kind) {
    case FilterParamKind::EnumIndex:
        return double(qBound(0, sliderValue, qMax(0, spec.enumLabels.size() - 1)));
    case FilterParamKind::Bool:
        return sliderValue >= kUnitSliderMax / 2 ? 1.0 : 0.0;
    default:
        return sliderToRange(sliderValue, spec.minV, spec.maxV);
    }
}

} // namespace

NodeGraphWidget::NodeGraphWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    m_paramHost = new QWidget(this);
    m_paramHostLayout = new QHBoxLayout(m_paramHost);
    m_paramHostLayout->setContentsMargins(0, 0, 0, 0);
    m_paramHostLayout->setSpacing(10);
    root->addSpacing(int(kPad * 2 + kBoxH + 10));
    root->addWidget(m_paramHost);
}

void NodeGraphWidget::setCell(Cell* cell, Project* project)
{
    m_cell    = cell;
    m_project = project;
    m_selectedGraphIndex = -1;
    m_dragging = false;
    rebuildLayout();
    rebuildParamEditors();
    updateGeometry();
    update();
}

void NodeGraphWidget::setDeckContext(int bankSetIndex, int bankIndex, int cellIndex)
{
    m_bankSetIndex = bankSetIndex;
    m_bankIndex    = bankIndex;
    m_cellIndex    = cellIndex;
    if (m_midiMappingEditMode) {
        syncMidiMapOverlays();
    }
}

void NodeGraphWidget::setMidiMappingEditMode(bool on)
{
    if (m_midiMappingEditMode == on) {
        return;
    }
    m_midiMappingEditMode = on;
    syncMidiMapOverlays();
}

QString NodeGraphWidget::midiLabelForWidget(QWidget* w) const
{
    if (!w || !m_project || m_cellIndex < 0) {
        return QStringLiteral("—");
    }
    const QString prop = w->property("pvjProperty").toString();
    if (prop.isEmpty()) {
        return QStringLiteral("—");
    }
    const QString label =
        m_project->propertyMappingLabel(m_bankSetIndex, m_bankIndex, m_cellIndex, prop);
    return label.isEmpty() ? QStringLiteral("—") : label;
}

void NodeGraphWidget::syncMidiMapOverlays()
{
    for (const ParamWidgetBinding& binding : m_paramWidgets) {
        if (!binding.widget) {
            continue;
        }
        MidiMapOverlay::setActiveOn(binding.widget, m_midiMappingEditMode,
                                    midiLabelForWidget(binding.widget));
    }
}

void NodeGraphWidget::refresh()
{
    rebuildLayout();
    rebuildParamEditors();
    updateGeometry();
    update();
}

void NodeGraphWidget::rebuildLayout()
{
    m_nodes.clear();
    if (!m_cell) {
        return;
    }

    double x = kPad;
    const double y = kPad;

    GraphNode src;
    src.rect     = QRectF(x, y, kBoxW, kBoxH);
    src.kind     = NodeKind::Source;
    src.filterIndex = -1;
    m_nodes.append(src);
    x += kBoxW + kGap;

    const int nf = m_cell->filterChain.size();
    for (int i = 0; i < nf; ++i) {
        GraphNode gn;
        gn.rect        = QRectF(x, y, kBoxW, kBoxH);
        gn.kind        = NodeKind::Filter;
        gn.filterIndex = i;
        gn.filterId    = m_cell->filterChain[i].id;
        m_nodes.append(gn);
        x += kBoxW + kGap;
    }

    GraphNode out;
    out.rect        = QRectF(x, y, kBoxW, kBoxH);
    out.kind        = NodeKind::Output;
    out.filterIndex = -1;
    m_nodes.append(out);
}

QString NodeGraphWidget::sourceTitleLine() const
{
    if (!m_cell) {
        return QString();
    }
    if (m_cell->visual.type == pvj::core::VisualType::Media) {
        if (!m_cell->visual.mediaId.isNull() && m_project) {
            if (const auto* m = m_project->findMedia(m_cell->visual.mediaId)) {
                return QFileInfo(m->path).fileName();
            }
            return tr("(media missing)");
        }
        return tr("Media (no clip)");
    }
    if (m_cell->visual.type == pvj::core::VisualType::Generator) {
        switch (m_cell->visual.generator) {
        case pvj::core::GeneratorKind::TestPattern:  return tr("Test pattern");
        case pvj::core::GeneratorKind::SolidColor:   return tr("Solid color");
        case pvj::core::GeneratorKind::InputSpout:   return tr("Spout");
        case pvj::core::GeneratorKind::InputNdi:     return tr("NDI");
        case pvj::core::GeneratorKind::InternalFeedback: return tr("Feedback loop");
        default:                                     return tr("Generator");
        }
    }
    if (m_cell->visual.type == pvj::core::VisualType::MixerFilter) {
        if (!m_cell->filterChain.isEmpty()) {
            return filterLabelFor(m_cell->filterChain.front().typeId);
        }
        return tr("Mixer filter");
    }
    return tr("Empty");
}

QString NodeGraphWidget::filterLabelFor(const QString& typeId) const
{
    if (pvj::core::isFeedbackMarkerNode(typeId)) {
        return tr("Feedback / Render Target");
    }
    const QString en = pvj::core::filterCatalogEnglishName(typeId);
    if (!en.isEmpty()) {
        return QCoreApplication::translate("FilterCatalog", en.toUtf8().constData());
    }
    return typeId;
}

void NodeGraphWidget::pickFilterType(const std::function<void(const QString& typeId)>& onPick)
{
    FilterPickerOptions opts;
    opts.title = tr("Select filter");
    if (const auto typeId = FilterPickerDialog::pick(this, opts)) {
        onPick(*typeId);
    }
}

void NodeGraphWidget::addFilter(const QString& typeId)
{
    if (!m_cell || typeId.isEmpty()) {
        return;
    }
    CellFilterNode n;
    n.typeId = typeId;
    n.params = pvj::core::defaultParamsFor(typeId);
    m_cell->filterChain.append(n);
    emitChainEdited();
    refresh();
}

void NodeGraphWidget::insertFilter(int index, const QString& typeId)
{
    if (!m_cell || typeId.isEmpty()) {
        return;
    }
    CellFilterNode n;
    n.typeId = typeId;
    n.params = pvj::core::defaultParamsFor(typeId);
    index = qBound(0, index, m_cell->filterChain.size());
    m_cell->filterChain.insert(index, n);
    emitChainEdited();
    refresh();
}

void NodeGraphWidget::insertFeedbackMarker(int index)
{
    if (!m_cell) {
        return;
    }
    auto& chain = m_cell->filterChain;
    int existingIndex = -1;
    for (int i = 0; i < chain.size(); ++i) {
        if (pvj::core::isFeedbackMarkerNode(chain[i].typeId)) {
            existingIndex = i;
            break;
        }
    }
    if (existingIndex >= 0) {
        if (existingIndex < index) {
            --index;
        }
        chain.removeAt(existingIndex);
    }
    CellFilterNode marker;
    marker.typeId = pvj::core::feedbackMarkerTypeId();
    marker.params.clear();
    index = qBound(0, index, chain.size());
    chain.insert(index, marker);
    emitChainEdited();
    refresh();
}

void NodeGraphWidget::removeFilterAt(int filterIndex)
{
    if (!m_cell || filterIndex < 0 || filterIndex >= m_cell->filterChain.size()) {
        return;
    }
    m_cell->filterChain.removeAt(filterIndex);
    emitChainEdited();
    refresh();
}

void NodeGraphWidget::moveFilter(int fromIndex, int toIndex)
{
    if (!m_cell || fromIndex == toIndex) {
        return;
    }
    auto& chain = m_cell->filterChain;
    if (fromIndex < 0 || fromIndex >= chain.size()) {
        return;
    }
    if (toIndex < 0 || toIndex >= chain.size()) {
        return;
    }
    CellFilterNode n = chain.takeAt(fromIndex);
    chain.insert(toIndex, n);
    emitChainEdited();
    refresh();
}

void NodeGraphWidget::duplicateFilter(int filterIndex)
{
    if (!m_cell || filterIndex < 0 || filterIndex >= m_cell->filterChain.size()) {
        return;
    }
    CellFilterNode copy = m_cell->filterChain[filterIndex];
    copy.id = QUuid::createUuid();
    m_cell->filterChain.insert(filterIndex + 1, copy);
    emitChainEdited();
    refresh();
}

void NodeGraphWidget::setFilterTypeId(int filterIndex, const QString& typeId)
{
    if (!m_cell || filterIndex < 0 || filterIndex >= m_cell->filterChain.size() || typeId.isEmpty()) {
        return;
    }
    m_cell->filterChain[filterIndex].typeId = typeId;
    m_cell->filterChain[filterIndex].params = pvj::core::defaultParamsFor(typeId);
    emitChainEdited();
    emitParamsEdited();
    refresh();
}

bool NodeGraphWidget::isFeedbackCell() const
{
    return m_cell
        && m_cell->visual.type == pvj::core::VisualType::Generator
        && m_cell->visual.generator == pvj::core::GeneratorKind::InternalFeedback;
}

int NodeGraphWidget::hitTest(const QPointF& pos) const
{
    for (int i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes[i].rect.contains(pos)) {
            return i;
        }
    }
    return -1;
}

int NodeGraphWidget::finalIndexFromGhostX(double gx) const
{
    if (!m_cell || m_cell->filterChain.isEmpty() || m_nodes.size() < 2) {
        return 0;
    }
    const int n = m_cell->filterChain.size();
    QVector<double> cx;
    cx.reserve(n);
    for (int i = 0; i < n; ++i) {
        cx.append(m_nodes[i + 1].rect.center().x());
    }
    for (int i = 0; i < n - 1; ++i) {
        const double mid = (cx[i] + cx[i + 1]) / 2.0;
        if (gx < mid) {
            return i;
        }
    }
    return n - 1;
}

QSize NodeGraphWidget::sizeHint() const
{
    if (!m_cell) {
        return QSize(600, int(kPad * 2 + kBoxH + 8));
    }
    const int numNodes = 2 + m_cell->filterChain.size();
    const double w = kPad * 2 + numNodes * kBoxW + qMax(0, numNodes - 1) * kGap;
    const int paramsHeight = m_paramHost ? m_paramHost->sizeHint().height() : 0;
    return QSize(int(qCeil(w)), int(kPad * 2 + kBoxH + 8) + 8 + paramsHeight);
}

QSize NodeGraphWidget::minimumSizeHint() const
{
    return sizeHint();
}

void NodeGraphWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().color(QPalette::Base));

    if (m_nodes.isEmpty()) {
        return;
    }

    const QColor wireColor = palette().color(QPalette::Mid);
    const QColor srcFill   = QColor(QStringLiteral("#3a4a5a"));
    const QColor filtFill  = QColor(QStringLiteral("#2d333d"));
    const QColor outFill   = QColor(QStringLiteral("#2a3d2a"));
    const QColor selStroke = QColor(QStringLiteral("#6a9eee"));

    // Connections
    p.setPen(QPen(wireColor, 2));
    for (int i = 0; i < m_nodes.size() - 1; ++i) {
        QRectF a = m_nodes[i].rect;
        QRectF b = m_nodes[i + 1].rect;
        if (m_dragging && m_nodes[i + 1].kind == NodeKind::Filter
            && m_nodes[i + 1].filterIndex == m_dragFromFilterIndex) {
            continue;
        }
        if (m_dragging && m_nodes[i].kind == NodeKind::Filter
            && m_nodes[i].filterIndex == m_dragFromFilterIndex) {
            continue;
        }

        QPointF p0(a.right(), a.center().y());
        QPointF p1(b.left(), b.center().y());
        const double dx = qMax(40.0, (p1.x() - p0.x()) * 0.45);
        QPainterPath path;
        path.moveTo(p0);
        path.cubicTo(p0 + QPointF(dx, 0), p1 - QPointF(dx, 0), p1);
        p.drawPath(path);
    }

    // Drag ghost connection stubs (optional thin lines) — skip for clarity

    QFont f = p.font();
    f.setPointSizeF(qMax(8.0, f.pointSizeF()));
    p.setFont(f);
    const QFontMetricsF fm(f);

    auto drawNode = [&](int idx, const QRectF& r, const QColor& fill, const QString& line1,
                        const QString& line2 = {}) {
        const bool sel = (idx == m_selectedGraphIndex);
        p.setPen(sel ? QPen(selStroke, 2) : QPen(wireColor, 1));
        p.setBrush(fill);
        p.drawRoundedRect(r, 6, 6);
        p.setPen(palette().color(QPalette::Text));
        const double pad = 8;
        const double maxW = r.width() - pad * 2;
        p.drawText(QRectF(r.left() + pad, r.top() + 6, maxW, r.height() - 12),
                   Qt::AlignLeft | Qt::AlignTop,
                   elideMiddle(fm, line1, maxW));
        if (!line2.isEmpty()) {
            p.setPen(palette().color(QPalette::PlaceholderText));
            p.drawText(QRectF(r.left() + pad, r.top() + 6 + fm.height(), maxW, fm.height() + 2),
                       Qt::AlignLeft | Qt::AlignTop,
                       elideMiddle(fm, line2, maxW));
        }
    };

    for (int i = 0; i < m_nodes.size(); ++i) {
        const GraphNode& gn = m_nodes[i];
        if (m_dragging && gn.kind == NodeKind::Filter && gn.filterIndex == m_dragFromFilterIndex) {
            continue;
        }
        switch (gn.kind) {
        case NodeKind::Source:
            drawNode(i, gn.rect, srcFill, tr("Source"), sourceTitleLine());
            break;
        case NodeKind::Filter: {
            const QString& tid = m_cell->filterChain[gn.filterIndex].typeId;
            drawNode(i, gn.rect, filtFill, tr("Filter"), filterLabelFor(tid));
            break;
        }
        case NodeKind::Output:
            drawNode(i, gn.rect, outFill, tr("Output"), tr("Mixer"));
            break;
        }
    }

    if (m_dragging && m_dragFromFilterIndex >= 0 && m_dragFromFilterIndex < m_cell->filterChain.size()) {
        const QString& tid = m_cell->filterChain[m_dragFromFilterIndex].typeId;
        const double ghostY = (m_dragFromFilterIndex + 1 < m_nodes.size())
            ? m_nodes[m_dragFromFilterIndex + 1].rect.top()
            : kPad;
        QRectF gr(m_dragGhostX - kBoxW / 2, ghostY, kBoxW, kBoxH);
        p.setOpacity(0.85);
        drawNode(-1, gr, filtFill, tr("Filter"), filterLabelFor(tid));
        p.setOpacity(1.0);
    }
}

void NodeGraphWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QPointF pos = event->position();
    const int hit = hitTest(pos);
    m_selectedGraphIndex = hit;
    if (hit >= 0 && m_nodes[hit].kind == NodeKind::Filter) {
        m_dragging           = true;
        m_dragFromFilterIndex = m_nodes[hit].filterIndex;
        m_dragGhostX          = m_nodes[hit].rect.center().x();
        m_dragGrabOffset      = pos - QPointF(m_nodes[hit].rect.center());
    } else {
        m_dragging = false;
    }
    update();
}

void NodeGraphWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        m_dragGhostX = event->position().x() - m_dragGrabOffset.x();
        update();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void NodeGraphWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        if (m_cell && m_dragFromFilterIndex >= 0) {
            const int toFinal = finalIndexFromGhostX(m_dragGhostX);
            if (toFinal != m_dragFromFilterIndex) {
                moveFilter(m_dragFromFilterIndex, toFinal);
            } else {
                update();
            }
        }
        m_dragging = false;
        m_dragFromFilterIndex = -1;
        update();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void NodeGraphWidget::emitChainEdited()
{
    emit chainChanged();
    emit filterChainChanged();
}

void NodeGraphWidget::emitParamsEdited()
{
    emit filterParamsChanged();
}

void NodeGraphWidget::ensureNodeParams(CellFilterNode& node) const
{
    const auto schema = pvj::core::filterParamSchemas().value(node.typeId);
    if (schema.params.isEmpty()) {
        node.params.clear();
        return;
    }
    QList<pvj::core::EffectParam> updated = node.params;
    for (const auto& spec : schema.params) {
        bool found = false;
        for (auto& p : updated) {
            if (p.name == spec.name) {
                p.value = qBound(spec.minV, p.value, spec.maxV);
                found = true;
                break;
            }
        }
        if (!found) {
            updated.append({ spec.name, spec.defaultV });
        }
    }
    while (updated.size() > schema.params.size()) {
        updated.removeLast();
    }
    node.params = updated;
}

const pvj::core::FilterParamSpec* NodeGraphWidget::paramSpecFor(const QString& typeId, const QString& paramName) const
{
    const auto it = pvj::core::filterParamSchemas().find(typeId);
    if (it == pvj::core::filterParamSchemas().end()) {
        return nullptr;
    }
    const auto& spec = it.value();
    for (const auto& param : spec.params) {
        if (param.name == paramName) {
            return &param;
        }
    }
    return nullptr;
}

void NodeGraphWidget::clearParamEditors()
{
    m_paramWidgets.clear();
    if (!m_paramHostLayout) {
        return;
    }
    while (QLayoutItem* it = m_paramHostLayout->takeAt(0)) {
        if (it->widget()) {
            it->widget()->deleteLater();
        }
        delete it;
    }
}

void NodeGraphWidget::rebuildParamEditors()
{
    clearParamEditors();
    if (!m_cell || !m_paramHostLayout) {
        return;
    }
    for (int i = 0; i < m_cell->filterChain.size(); ++i) {
        auto& node = m_cell->filterChain[i];
        ensureNodeParams(node);
        const auto schemaIt = pvj::core::filterParamSchemas().find(node.typeId);
        if (schemaIt == pvj::core::filterParamSchemas().end()) {
            continue;
        }
        if (schemaIt.value().params.isEmpty()) {
            continue;
        }
        auto* group = new QWidget(m_paramHost);
        auto* form = new QFormLayout(group);
        form->setContentsMargins(8, 6, 8, 6);
        form->setSpacing(4);
        auto* title = new QLabel(filterLabelFor(node.typeId), group);
        title->setStyleSheet(QStringLiteral("font-weight: 600;"));
        form->addRow(title);

        QString currentSection;
        for (const auto& spec : schemaIt.value().params) {
            if (!spec.section.isEmpty() && spec.section != currentSection) {
                currentSection = spec.section;
                auto* sectionLabel = new QLabel(currentSection, group);
                sectionLabel->setStyleSheet(QStringLiteral("font-weight: 600; color: #aaa;"));
                form->addRow(sectionLabel);
            }
            int paramIndex = -1;
            for (int p = 0; p < node.params.size(); ++p) {
                if (node.params[p].name == spec.name) {
                    paramIndex = p;
                    break;
                }
            }
            if (paramIndex < 0) {
                node.params.append({ spec.name, spec.defaultV });
                paramIndex = node.params.size() - 1;
            }

            const double currentValue = node.params[paramIndex].value;

            QWidget* editor = nullptr;
            const QString property = QStringLiteral("filter.%1.%2")
                                         .arg(node.id.toString(QUuid::WithoutBraces), spec.name);

            if (spec.kind == pvj::core::FilterParamKind::EnumIndex) {
                auto* combo = new QComboBox(group);
                for (const QString& label : spec.enumLabels) {
                    combo->addItem(label);
                }
                const int maxIdx = qMax(0, spec.enumLabels.size() - 1);
                combo->setCurrentIndex(qBound(0, int(qRound(currentValue)), maxIdx));
                connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                        [this, nodeId = node.id, name = spec.name, spec](int idx) {
                            if (!m_cell) {
                                return;
                            }
                            const double paramValue = double(qBound(0, idx, qMax(0, spec.enumLabels.size() - 1)));
                            for (auto& n : m_cell->filterChain) {
                                if (n.id != nodeId) {
                                    continue;
                                }
                                for (auto& p : n.params) {
                                    if (p.name == name) {
                                        p.value = qBound(spec.minV, paramValue, spec.maxV);
                                        emitParamsEdited();
                                        return;
                                    }
                                }
                            }
                        });
                editor = combo;
                const QVariant noteValue =
                    spec.maxV > 0.0 ? QVariant(currentValue / spec.maxV) : QVariant();
                MidiLearnMenu::tagMidiWidget(editor, property, noteValue);
                editor->installEventFilter(this);
                m_paramWidgets.append({ editor, node.id, spec.name, node.typeId, spec.kind, spec.minV, spec.maxV });
                form->addRow(spec.label, combo);
                continue;
            }

            if (spec.kind == pvj::core::FilterParamKind::Bool) {
                auto* check = new QCheckBox(group);
                check->setChecked(currentValue >= 0.5);
                connect(check, &QCheckBox::toggled, this,
                        [this, nodeId = node.id, name = spec.name, spec](bool on) {
                            if (!m_cell) {
                                return;
                            }
                            const double paramValue = on ? 1.0 : 0.0;
                            for (auto& n : m_cell->filterChain) {
                                if (n.id != nodeId) {
                                    continue;
                                }
                                for (auto& p : n.params) {
                                    if (p.name == name) {
                                        p.value = qBound(spec.minV, paramValue, spec.maxV);
                                        emitParamsEdited();
                                        return;
                                    }
                                }
                            }
                        });
                editor = check;
                MidiLearnMenu::tagMidiWidget(editor, property, QVariant());
                editor->installEventFilter(this);
                m_paramWidgets.append({ editor, node.id, spec.name, node.typeId, spec.kind, spec.minV, spec.maxV });
                form->addRow(spec.label, check);
                continue;
            }

            if (spec.kind == pvj::core::FilterParamKind::Color) {
                const int rgb = int(qBound(0.0, currentValue, 16777215.0));
                auto* colorBtn = new QPushButton(group);
                colorBtn->setText(formatFilterParamValue(spec, currentValue));
                colorBtn->setStyleSheet(QStringLiteral(
                    "QPushButton { background-color: #%1%2%3; color: #222; }")
                                            .arg((rgb >> 16) & 0xFF, 2, 16, QChar('0'))
                                            .arg((rgb >> 8) & 0xFF, 2, 16, QChar('0'))
                                            .arg(rgb & 0xFF, 2, 16, QChar('0')));
                connect(colorBtn, &QPushButton::clicked, this,
                        [this, nodeId = node.id, name = spec.name, spec, colorBtn]() {
                            if (!m_cell) {
                                return;
                            }
                            double cur = spec.defaultV;
                            for (const auto& n : m_cell->filterChain) {
                                if (n.id != nodeId) {
                                    continue;
                                }
                                for (const auto& p : n.params) {
                                    if (p.name == name) {
                                        cur = p.value;
                                        break;
                                    }
                                }
                                break;
                            }
                            const int packed = int(qBound(0.0, cur, 16777215.0));
                            const QColor initial((packed >> 16) & 0xFF, (packed >> 8) & 0xFF,
                                                 packed & 0xFF);
                            const QColor picked =
                                QColorDialog::getColor(initial, this, spec.label);
                            if (!picked.isValid()) {
                                return;
                            }
                            const double paramValue =
                                double((picked.red() << 16) | (picked.green() << 8) | picked.blue());
                            for (auto& n : m_cell->filterChain) {
                                if (n.id != nodeId) {
                                    continue;
                                }
                                for (auto& p : n.params) {
                                    if (p.name == name) {
                                        p.value = qBound(spec.minV, paramValue, spec.maxV);
                                        colorBtn->setText(formatFilterParamValue(spec, p.value));
                                        colorBtn->setStyleSheet(QStringLiteral(
                                            "QPushButton { background-color: #%1%2%3; }")
                                                                .arg(picked.red(), 2, 16, QChar('0'))
                                                                .arg(picked.green(), 2, 16, QChar('0'))
                                                                .arg(picked.blue(), 2, 16, QChar('0')));
                                        emitParamsEdited();
                                        return;
                                    }
                                }
                            }
                        });
                editor = colorBtn;
                MidiLearnMenu::tagMidiWidget(editor, property, QVariant());
                editor->installEventFilter(this);
                m_paramWidgets.append({ editor, node.id, spec.name, node.typeId, spec.kind, spec.minV,
                                        spec.maxV });
                form->addRow(spec.label, colorBtn);
                continue;
            }

            auto* row = new QWidget(group);
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(6);

            auto* slider = new QSlider(Qt::Horizontal, row);
            auto* valueLabel = new QLabel(formatFilterParamValue(spec, currentValue), row);
            valueLabel->setMinimumWidth(52);
            valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

            slider->setRange(0, kUnitSliderMax);
            slider->setValue(sliderValueForParam(spec, currentValue));

            connect(slider, &QSlider::valueChanged, this,
                    [this, nodeId = node.id, name = spec.name, typeId = node.typeId, spec, valueLabel,
                     slider](int v) {
                        if (!m_cell) {
                            return;
                        }
                        const double paramValue = paramValueFromSlider(spec, v);
                        valueLabel->setText(formatFilterParamValue(spec, paramValue));
                        for (auto& n : m_cell->filterChain) {
                            if (n.id != nodeId) {
                                continue;
                            }
                            for (auto& p : n.params) {
                                if (p.name == name) {
                                    p.value = qBound(spec.minV, paramValue, spec.maxV);
                                    break;
                                }
                            }
                            if (typeId == QStringLiteral("box_blur")
                                && (name == QStringLiteral("horizontal_strength")
                                    || name == QStringLiteral("vertical_strength"))) {
                                bool ganged = true;
                                for (const auto& bp : n.params) {
                                    if (bp.name == QStringLiteral("same_horizontal_vertical")) {
                                        ganged = bp.value >= 0.5;
                                        break;
                                    }
                                }
                                if (ganged) {
                                    const QString otherName =
                                        name == QStringLiteral("horizontal_strength")
                                            ? QStringLiteral("vertical_strength")
                                            : QStringLiteral("horizontal_strength");
                                    for (auto& p : n.params) {
                                        if (p.name == otherName) {
                                            p.value = qBound(spec.minV, paramValue, spec.maxV);
                                            break;
                                        }
                                    }
                                    for (const auto& binding : m_paramWidgets) {
                                        if (binding.nodeId != nodeId || binding.paramName != otherName) {
                                            continue;
                                        }
                                        if (auto* otherSlider = qobject_cast<QSlider*>(binding.widget)) {
                                            if (otherSlider == slider) {
                                                continue;
                                            }
                                            const pvj::core::FilterParamSpec* otherSpec =
                                                paramSpecFor(binding.typeId, binding.paramName);
                                            if (!otherSpec) {
                                                continue;
                                            }
                                            QSignalBlocker blocker(otherSlider);
                                            otherSlider->setValue(
                                                sliderValueForParam(*otherSpec, paramValue));
                                            if (auto* row = qobject_cast<QWidget*>(otherSlider->parent())) {
                                                for (QLabel* lbl : row->findChildren<QLabel*>()) {
                                                    lbl->setText(
                                                        formatFilterParamValue(*otherSpec, paramValue));
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            emitParamsEdited();
                            return;
                        }
                    });

            rowLayout->addWidget(slider, 1);
            rowLayout->addWidget(valueLabel);

            editor = slider;
            MidiLearnMenu::tagMidiWidget(editor, property, QVariant());
            editor->installEventFilter(this);
            m_paramWidgets.append({ editor, node.id, spec.name, node.typeId, spec.kind, spec.minV, spec.maxV });

            form->addRow(spec.label, row);
        }
        m_paramHostLayout->addWidget(group);
    }
    m_paramHostLayout->addStretch(1);
    if (m_midiMappingEditMode) {
        syncMidiMapOverlays();
    }
}

bool NodeGraphWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::ContextMenu) {
        auto* w = qobject_cast<QWidget*>(watched);
        if (w && !w->property("pvjProperty").toString().isEmpty()) {
            auto* ce = static_cast<QContextMenuEvent*>(event);
            MidiLearnMenu::showForWidget(
                this,
                w,
                ce->globalPos(),
                [this](const QString& property) { emit midiLearnCcRequested(property); },
                [this](const QString& property, bool toggle, double value) {
                    emit midiLearnNoteRequested(property, toggle, value);
                },
                [this](const QString& property) { emit midiClearMappingRequested(property); });
            return true;
        }
    }
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            auto* w = qobject_cast<QWidget*>(watched);
            if (w && !w->property("pvjProperty").toString().isEmpty()) {
                if (m_midiMappingEditMode) {
                    MidiLearnMenu::showForWidget(
                        this,
                        w,
                        me->globalPosition().toPoint(),
                        [this](const QString& property) { emit midiLearnCcRequested(property); },
                        [this](const QString& property, bool toggle, double value) {
                            emit midiLearnNoteRequested(property, toggle, value);
                        },
                        [this](const QString& property) { emit midiClearMappingRequested(property); });
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void NodeGraphWidget::contextMenuEvent(QContextMenuEvent* event)
{
    const QPointF pos = event->pos();
    const int hit     = hitTest(pos);
    if (hit < 0 || !m_cell) {
        QWidget::contextMenuEvent(event);
        return;
    }

    QMenu menu(this);
    const bool feedbackCell = isFeedbackCell();

    if (m_nodes[hit].kind == NodeKind::Source) {
        if (m_cell->visual.type == pvj::core::VisualType::MixerFilter) {
            connect(menu.addAction(tr("Choose source filter…")), &QAction::triggered, this, [this]() {
                pickFilterType([this](const QString& typeId) {
                    if (!m_cell || typeId.isEmpty()) {
                        return;
                    }
                    m_cell->visual.type = pvj::core::VisualType::MixerFilter;
                    pvj::core::CellFilterNode node;
                    node.typeId = typeId;
                    node.params = pvj::core::defaultParamsFor(typeId);
                    if (m_cell->filterChain.isEmpty()) {
                        m_cell->filterChain.append(node);
                    } else {
                        m_cell->filterChain[0] = node;
                    }
                    emitChainEdited();
                    refresh();
                });
            });
            menu.addSeparator();
            connect(menu.addAction(tr("Add filter at end…")), &QAction::triggered, this, [this]() {
                pickFilterType([this](const QString& typeId) { addFilter(typeId); });
            });
            menu.exec(event->globalPos());
            return;
        }
        auto addSrc = [&](const QString& text, int role) {
            connect(menu.addAction(text), &QAction::triggered, this, [this, role]() {
                emit sourceChangeRequested(role);
            });
        };
        addSrc(tr("Media clip (project)"), 0);
        addSrc(tr("Test pattern"), 2);
        addSrc(tr("Solid color"), 3);
        addSrc(tr("Spout (Windows)"), 4);
        addSrc(tr("NDI"), 5);
        if (feedbackCell) {
            menu.addSeparator();
            connect(menu.addAction(tr("Insert feedback marker")), &QAction::triggered, this, [this]() {
                insertFeedbackMarker(0);
            });
        }
        menu.exec(event->globalPos());
        return;
    }

    if (m_nodes[hit].kind == NodeKind::Output) {
        auto* addEnd = menu.addAction(tr("Add filter at end…"));
        connect(addEnd, &QAction::triggered, this, [this]() {
            pickFilterType([this](const QString& typeId) { addFilter(typeId); });
        });
        if (feedbackCell) {
            menu.addSeparator();
            connect(menu.addAction(tr("Insert feedback marker")), &QAction::triggered, this, [this]() {
                insertFeedbackMarker(m_cell ? m_cell->filterChain.size() : 0);
            });
        }
        menu.exec(event->globalPos());
        return;
    }

    if (m_nodes[hit].kind == NodeKind::Filter) {
        const int fi = m_nodes[hit].filterIndex;
        const bool markerNode = pvj::core::isFeedbackMarkerNode(m_cell->filterChain[fi].typeId);

        auto* addBefore = menu.addAction(tr("Insert filter before…"));
        connect(addBefore, &QAction::triggered, this, [this, fi]() {
            pickFilterType([this, fi](const QString& typeId) { insertFilter(fi, typeId); });
        });
        auto* addAfter = menu.addAction(tr("Insert filter after…"));
        connect(addAfter, &QAction::triggered, this, [this, fi]() {
            pickFilterType([this, fi](const QString& typeId) { insertFilter(fi + 1, typeId); });
        });
        if (feedbackCell) {
            connect(menu.addAction(tr("Insert feedback marker before")), &QAction::triggered, this, [this, fi]() {
                insertFeedbackMarker(fi);
            });
            connect(menu.addAction(tr("Insert feedback marker after")), &QAction::triggered, this, [this, fi]() {
                insertFeedbackMarker(fi + 1);
            });
        }
        menu.addSeparator();

        if (!markerNode) {
            auto* dup = menu.addAction(tr("Duplicate"));
            connect(dup, &QAction::triggered, this, [this, fi]() { duplicateFilter(fi); });

            auto* ch = menu.addAction(tr("Change type…"));
            connect(ch, &QAction::triggered, this, [this, fi]() {
                pickFilterType([this, fi](const QString& typeId) { setFilterTypeId(fi, typeId); });
            });
            menu.addSeparator();
        }

        if (fi > 0) {
            connect(menu.addAction(tr("Move left")), &QAction::triggered, this, [this, fi]() {
                moveFilter(fi, fi - 1);
            });
        }
        if (fi + 1 < m_cell->filterChain.size()) {
            connect(menu.addAction(tr("Move right")), &QAction::triggered, this, [this, fi]() {
                moveFilter(fi, fi + 1);
            });
        }
        if (!markerNode) {
            connect(menu.addAction(tr("Remove")), &QAction::triggered, this, [this, fi]() {
                removeFilterAt(fi);
            });
        }
    }

    menu.exec(event->globalPos());
}

} // namespace pvj::app

#include "OutputProcessingDialog.h"

#include "core/FilterCatalog.h"
#include "core/FilterEffectIds.h"
#include "core/FilterParamSchema.h"
#include "core/Project.h"
#include "render/maxine/MaxineFilterBackend.h"

#include <QColorDialog>
#include <QCoreApplication>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QVBoxLayout>

namespace pvj::app {

namespace {

constexpr int kOutputFilterSliderMax = 1000;

double outputSliderToRange(int sliderValue, double minValue, double maxValue)
{
    const double t = qBound(0.0, double(sliderValue) / double(kOutputFilterSliderMax), 1.0);
    return minValue + (maxValue - minValue) * t;
}

int outputRangeToSlider(double value, double minValue, double maxValue)
{
    if (qFuzzyCompare(minValue, maxValue)) {
        return 0;
    }
    const double t = (qBound(minValue, value, maxValue) - minValue) / (maxValue - minValue);
    return int(qRound(qBound(0.0, t, 1.0) * double(kOutputFilterSliderMax)));
}

QString formatOutputFilterParamValue(const pvj::core::FilterParamSpec& spec, double value)
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

int outputSliderValueForParam(const pvj::core::FilterParamSpec& spec, double value)
{
    using pvj::core::FilterParamKind;
    switch (spec.kind) {
    case FilterParamKind::EnumIndex:
        return qBound(0, int(qRound(value)), qMax(0, spec.enumLabels.size() - 1));
    case FilterParamKind::Bool:
        return value >= 0.5 ? kOutputFilterSliderMax : 0;
    default:
        return outputRangeToSlider(value, spec.minV, spec.maxV);
    }
}

double outputParamValueFromSlider(const pvj::core::FilterParamSpec& spec, int sliderValue)
{
    using pvj::core::FilterParamKind;
    switch (spec.kind) {
    case FilterParamKind::EnumIndex:
        return double(qBound(0, sliderValue, qMax(0, spec.enumLabels.size() - 1)));
    case FilterParamKind::Bool:
        return sliderValue >= kOutputFilterSliderMax / 2 ? 1.0 : 0.0;
    default:
        return outputSliderToRange(sliderValue, spec.minV, spec.maxV);
    }
}

} // namespace

OutputProcessingDialog::OutputProcessingDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Post-processing filters"));
    setMinimumSize(420, 360);

    auto* root = new QVBoxLayout(this);

    auto* nvidiaGroup = new QGroupBox(tr("NVIDIA Output Processing"), this);
    auto* nvidiaLayout = new QVBoxLayout(nvidiaGroup);

    auto* chainRow = new QWidget(nvidiaGroup);
    auto* chainRowLayout = new QHBoxLayout(chainRow);
    chainRowLayout->setContentsMargins(0, 0, 0, 0);

    m_outputFilterList = new QListWidget(chainRow);
    m_outputFilterList->setToolTip(tr("Post-mixer filter chain applied to the composed output"));
    m_outputFilterList->setMinimumHeight(120);
    connect(m_outputFilterList, &QListWidget::currentRowChanged, this,
            &OutputProcessingDialog::onOutputFilterSelectionChanged);
    chainRowLayout->addWidget(m_outputFilterList, 1);

    auto* btnCol = new QVBoxLayout();
    btnCol->setSpacing(4);
    m_outputFilterUpBtn = new QPushButton(tr("Up"), chainRow);
    m_outputFilterDownBtn = new QPushButton(tr("Down"), chainRow);
    m_outputFilterRemoveBtn = new QPushButton(tr("Remove"), chainRow);
    m_outputFilterAddBtn = new QPushButton(tr("Add filter…"), chainRow);
    m_outputFilterUpBtn->setToolTip(tr("Move selected filter earlier in the chain"));
    m_outputFilterDownBtn->setToolTip(tr("Move selected filter later in the chain"));
    m_outputFilterRemoveBtn->setToolTip(tr("Remove selected filter from the output chain"));
    m_outputFilterAddBtn->setToolTip(
        tr("Add an NVIDIA Maxine filter to the post-mixer output chain"));
    connect(m_outputFilterUpBtn, &QPushButton::clicked, this,
            &OutputProcessingDialog::onOutputFilterMoveUp);
    connect(m_outputFilterDownBtn, &QPushButton::clicked, this,
            &OutputProcessingDialog::onOutputFilterMoveDown);
    connect(m_outputFilterRemoveBtn, &QPushButton::clicked, this,
            &OutputProcessingDialog::onOutputFilterRemove);
    connect(m_outputFilterAddBtn, &QPushButton::clicked, this,
            &OutputProcessingDialog::onOutputFilterAddTriggered);
    btnCol->addWidget(m_outputFilterUpBtn);
    btnCol->addWidget(m_outputFilterDownBtn);
    btnCol->addWidget(m_outputFilterRemoveBtn);
    btnCol->addWidget(m_outputFilterAddBtn);
    btnCol->addStretch(1);
    chainRowLayout->addLayout(btnCol);

    nvidiaLayout->addWidget(chainRow);

    m_outputParamHost = new QWidget(nvidiaGroup);
    m_outputParamLayout = new QVBoxLayout(m_outputParamHost);
    m_outputParamLayout->setContentsMargins(0, 4, 0, 0);
    m_outputParamLayout->setSpacing(6);
    nvidiaLayout->addWidget(m_outputParamHost);

    root->addWidget(nvidiaGroup);
}

void OutputProcessingDialog::setProject(pvj::core::Project* project)
{
    m_project = project;
    refreshOutputFilterUi();
}

void OutputProcessingDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    refreshOutputFilterUi();
}

QString OutputProcessingDialog::outputFilterLabelFor(const QString& typeId) const
{
    const QString en = pvj::core::filterCatalogEnglishName(typeId);
    if (!en.isEmpty()) {
        return QCoreApplication::translate("FilterCatalog", en.toUtf8().constData());
    }
    return typeId;
}

void OutputProcessingDialog::ensureOutputNodeParams(pvj::core::CellFilterNode& node) const
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

void OutputProcessingDialog::clearOutputParamEditors()
{
    if (!m_outputParamLayout) {
        return;
    }
    while (QLayoutItem* it = m_outputParamLayout->takeAt(0)) {
        if (QWidget* w = it->widget()) {
            w->deleteLater();
        }
        delete it;
    }
}

void OutputProcessingDialog::rebuildOutputParamEditors()
{
    clearOutputParamEditors();
    if (!m_project || !m_outputParamLayout || m_outputFilterSelectedIndex < 0) {
        return;
    }
    auto& chain = m_project->settings.output.filterChain;
    if (m_outputFilterSelectedIndex >= chain.size()) {
        return;
    }
    auto& node = chain[m_outputFilterSelectedIndex];
    ensureOutputNodeParams(node);
    const auto schemaIt = pvj::core::filterParamSchemas().find(node.typeId);
    if (schemaIt == pvj::core::filterParamSchemas().end()) {
        return;
    }

    auto* group = new QWidget(m_outputParamHost);
    auto* form = new QFormLayout(group);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(4);
    auto* title = new QLabel(outputFilterLabelFor(node.typeId), group);
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

        if (spec.kind == pvj::core::FilterParamKind::EnumIndex) {
            auto* combo = new QComboBox(group);
            for (const QString& label : spec.enumLabels) {
                combo->addItem(label);
            }
            const int maxIdx = qMax(0, spec.enumLabels.size() - 1);
            combo->setCurrentIndex(qBound(0, int(qRound(currentValue)), maxIdx));
            connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                    [this, nodeId = node.id, name = spec.name, spec](int idx) {
                        if (!m_project || m_outputFilterSelectedIndex < 0) {
                            return;
                        }
                        auto& chainRef = m_project->settings.output.filterChain;
                        if (m_outputFilterSelectedIndex >= chainRef.size()) {
                            return;
                        }
                        const double paramValue =
                            double(qBound(0, idx, qMax(0, spec.enumLabels.size() - 1)));
                        auto& selected = chainRef[m_outputFilterSelectedIndex];
                        if (selected.id != nodeId) {
                            return;
                        }
                        for (auto& p : selected.params) {
                            if (p.name == name) {
                                p.value = qBound(spec.minV, paramValue, spec.maxV);
                                emitFilterChainChanged();
                                return;
                            }
                        }
                    });
            form->addRow(spec.label, combo);
            continue;
        }

        if (spec.kind == pvj::core::FilterParamKind::Bool) {
            auto* check = new QCheckBox(group);
            check->setChecked(currentValue >= 0.5);
            connect(check, &QCheckBox::toggled, this,
                    [this, nodeId = node.id, name = spec.name, spec](bool on) {
                        if (!m_project || m_outputFilterSelectedIndex < 0) {
                            return;
                        }
                        auto& chainRef = m_project->settings.output.filterChain;
                        if (m_outputFilterSelectedIndex >= chainRef.size()) {
                            return;
                        }
                        auto& selected = chainRef[m_outputFilterSelectedIndex];
                        if (selected.id != nodeId) {
                            return;
                        }
                        const double paramValue = on ? 1.0 : 0.0;
                        for (auto& p : selected.params) {
                            if (p.name == name) {
                                p.value = qBound(spec.minV, paramValue, spec.maxV);
                                emitFilterChainChanged();
                                return;
                            }
                        }
                    });
            form->addRow(spec.label, check);
            continue;
        }

        if (spec.kind == pvj::core::FilterParamKind::Color) {
            const int rgb = int(qBound(0.0, currentValue, 16777215.0));
            auto* colorBtn = new QPushButton(group);
            colorBtn->setText(formatOutputFilterParamValue(spec, currentValue));
            colorBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background-color: #%1%2%3; }")
                                        .arg((rgb >> 16) & 0xFF, 2, 16, QChar('0'))
                                        .arg((rgb >> 8) & 0xFF, 2, 16, QChar('0'))
                                        .arg(rgb & 0xFF, 2, 16, QChar('0')));
            connect(colorBtn, &QPushButton::clicked, this,
                    [this, nodeId = node.id, name = spec.name, spec, colorBtn]() {
                        if (!m_project || m_outputFilterSelectedIndex < 0) {
                            return;
                        }
                        auto& chainRef = m_project->settings.output.filterChain;
                        if (m_outputFilterSelectedIndex >= chainRef.size()) {
                            return;
                        }
                        auto& selected = chainRef[m_outputFilterSelectedIndex];
                        if (selected.id != nodeId) {
                            return;
                        }
                        double cur = spec.defaultV;
                        for (const auto& p : selected.params) {
                            if (p.name == name) {
                                cur = p.value;
                                break;
                            }
                        }
                        const int packed = int(qBound(0.0, cur, 16777215.0));
                        const QColor initial((packed >> 16) & 0xFF, (packed >> 8) & 0xFF,
                                             packed & 0xFF);
                        const QColor picked = QColorDialog::getColor(initial, this, spec.label);
                        if (!picked.isValid()) {
                            return;
                        }
                        const double paramValue =
                            double((picked.red() << 16) | (picked.green() << 8) | picked.blue());
                        for (auto& p : selected.params) {
                            if (p.name == name) {
                                p.value = qBound(spec.minV, paramValue, spec.maxV);
                                colorBtn->setText(formatOutputFilterParamValue(spec, p.value));
                                colorBtn->setStyleSheet(QStringLiteral(
                                    "QPushButton { background-color: #%1%2%3; }")
                                                            .arg(picked.red(), 2, 16, QChar('0'))
                                                            .arg(picked.green(), 2, 16, QChar('0'))
                                                            .arg(picked.blue(), 2, 16, QChar('0')));
                                emitFilterChainChanged();
                                return;
                            }
                        }
                    });
            form->addRow(spec.label, colorBtn);
            continue;
        }

        auto* row = new QWidget(group);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);

        auto* slider = new QSlider(Qt::Horizontal, row);
        auto* valueLabel = new QLabel(formatOutputFilterParamValue(spec, currentValue), row);
        valueLabel->setMinimumWidth(52);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        slider->setRange(0, kOutputFilterSliderMax);
        slider->setValue(outputSliderValueForParam(spec, currentValue));

        connect(slider, &QSlider::valueChanged, this,
                [this, nodeId = node.id, name = spec.name, typeId = node.typeId, spec, valueLabel,
                 slider](int v) {
                    if (!m_project || m_outputFilterSelectedIndex < 0) {
                        return;
                    }
                    auto& chainRef = m_project->settings.output.filterChain;
                    if (m_outputFilterSelectedIndex >= chainRef.size()) {
                        return;
                    }
                    const double paramValue = outputParamValueFromSlider(spec, v);
                    valueLabel->setText(formatOutputFilterParamValue(spec, paramValue));
                    auto& selected = chainRef[m_outputFilterSelectedIndex];
                    if (selected.id != nodeId) {
                        return;
                    }
                    for (auto& p : selected.params) {
                        if (p.name == name) {
                            p.value = qBound(spec.minV, paramValue, spec.maxV);
                            break;
                        }
                    }
                    if (typeId == QStringLiteral("box_blur")
                        && (name == QStringLiteral("horizontal_strength")
                            || name == QStringLiteral("vertical_strength"))) {
                        bool ganged = true;
                        for (const auto& bp : selected.params) {
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
                            for (auto& p : selected.params) {
                                if (p.name == otherName) {
                                    p.value = qBound(spec.minV, paramValue, spec.maxV);
                                    break;
                                }
                            }
                            if (auto* row = qobject_cast<QWidget*>(slider->parent())) {
                                if (auto* parentGroup = qobject_cast<QWidget*>(row->parent())) {
                                    for (QSlider* otherSlider : parentGroup->findChildren<QSlider*>()) {
                                        if (otherSlider == slider) {
                                            continue;
                                        }
                                        const pvj::core::FilterNodeSpec schema =
                                            pvj::core::filterParamSchemas().value(typeId);
                                        for (const auto& otherSpec : schema.params) {
                                            if (otherSpec.name != otherName) {
                                                continue;
                                            }
                                            QSignalBlocker blocker(otherSlider);
                                            otherSlider->setValue(
                                                outputSliderValueForParam(otherSpec, paramValue));
                                            if (auto* otherRow =
                                                    qobject_cast<QWidget*>(otherSlider->parent())) {
                                                for (QLabel* lbl :
                                                     otherRow->findChildren<QLabel*>()) {
                                                    lbl->setText(formatOutputFilterParamValue(
                                                        otherSpec, paramValue));
                                                }
                                            }
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    emitFilterChainChanged();
                });

        rowLayout->addWidget(slider, 1);
        rowLayout->addWidget(valueLabel);
        form->addRow(spec.label, row);
    }
    m_outputParamLayout->addWidget(group);
}

void OutputProcessingDialog::refreshOutputFilterUi()
{
    if (!m_outputFilterList) {
        return;
    }
    const bool wasLoading = m_loading;
    m_loading = true;
    QSignalBlocker listBlocker(m_outputFilterList);

    const QList<pvj::core::CellFilterNode> chain =
        m_project ? m_project->settings.output.filterChain
                  : QList<pvj::core::CellFilterNode>{};

    m_outputFilterList->clear();
    for (const auto& node : chain) {
        m_outputFilterList->addItem(outputFilterLabelFor(node.typeId));
    }

    int row = m_outputFilterSelectedIndex;
    if (row < 0 || row >= chain.size()) {
        row = chain.isEmpty() ? -1 : 0;
    }
    m_outputFilterSelectedIndex = row;
    if (row >= 0) {
        m_outputFilterList->setCurrentRow(row);
    } else {
        m_outputFilterList->clearSelection();
    }

    const bool hasSelection = row >= 0 && row < chain.size();
    if (m_outputFilterUpBtn) {
        m_outputFilterUpBtn->setEnabled(hasSelection && row > 0);
    }
    if (m_outputFilterDownBtn) {
        m_outputFilterDownBtn->setEnabled(hasSelection && row + 1 < chain.size());
    }
    if (m_outputFilterRemoveBtn) {
        m_outputFilterRemoveBtn->setEnabled(hasSelection);
    }
    if (m_outputFilterAddBtn) {
        m_outputFilterAddBtn->setEnabled(m_project != nullptr);
    }

    rebuildOutputParamEditors();
    m_loading = wasLoading;
}

void OutputProcessingDialog::emitFilterChainChanged()
{
    if (m_loading || !m_project) {
        return;
    }
    emit filterChainChanged();
}

void OutputProcessingDialog::onOutputFilterSelectionChanged()
{
    if (m_loading || !m_outputFilterList) {
        return;
    }
    m_outputFilterSelectedIndex = m_outputFilterList->currentRow();
    const int chainSize = m_project ? m_project->settings.output.filterChain.size() : 0;
    const bool hasSelection =
        m_outputFilterSelectedIndex >= 0 && m_outputFilterSelectedIndex < chainSize;
    if (m_outputFilterUpBtn) {
        m_outputFilterUpBtn->setEnabled(hasSelection && m_outputFilterSelectedIndex > 0);
    }
    if (m_outputFilterDownBtn) {
        m_outputFilterDownBtn->setEnabled(hasSelection && m_outputFilterSelectedIndex + 1 < chainSize);
    }
    if (m_outputFilterRemoveBtn) {
        m_outputFilterRemoveBtn->setEnabled(hasSelection);
    }
    rebuildOutputParamEditors();
}

void OutputProcessingDialog::onOutputFilterAddTriggered()
{
    if (!m_project || !m_outputFilterAddBtn) {
        return;
    }
    QMenu menu(this);
    for (const auto& e : pvj::core::filterCatalogEntries()) {
        if (e.category != pvj::core::maxineFilterCategoryKey()) {
            continue;
        }
        if (!pvj::core::isOutputAllowedFilter(e.typeId)) {
            continue;
        }
        const QString name =
            QCoreApplication::translate("FilterCatalog", e.englishName.toUtf8().constData());
        auto* act = menu.addAction(name);
        if (!pvj::render::maxineFiltersAvailable()) {
            act->setToolTip(tr("NVIDIA Maxine runtime not available — install NVIDIA Video Effects "
                               "(NVVideoEffects.dll + models) or rebuild with MAXINE_SDK_ROOT."));
        }
        connect(act, &QAction::triggered, this, [this, typeId = e.typeId]() {
            if (!m_project) {
                return;
            }
            pvj::core::CellFilterNode n;
            n.typeId = typeId;
            n.params = pvj::core::defaultParamsFor(typeId);
            m_project->settings.output.filterChain.append(n);
            m_outputFilterSelectedIndex = m_project->settings.output.filterChain.size() - 1;
            refreshOutputFilterUi();
            emitFilterChainChanged();
        });
    }
    if (menu.isEmpty()) {
        return;
    }
    menu.exec(m_outputFilterAddBtn->mapToGlobal(QPoint(0, m_outputFilterAddBtn->height())));
}

void OutputProcessingDialog::onOutputFilterMoveUp()
{
    if (!m_project || m_outputFilterSelectedIndex <= 0) {
        return;
    }
    auto& chain = m_project->settings.output.filterChain;
    chain.swapItemsAt(m_outputFilterSelectedIndex, m_outputFilterSelectedIndex - 1);
    --m_outputFilterSelectedIndex;
    refreshOutputFilterUi();
    emitFilterChainChanged();
}

void OutputProcessingDialog::onOutputFilterMoveDown()
{
    if (!m_project) {
        return;
    }
    auto& chain = m_project->settings.output.filterChain;
    if (m_outputFilterSelectedIndex < 0
        || m_outputFilterSelectedIndex + 1 >= chain.size()) {
        return;
    }
    chain.swapItemsAt(m_outputFilterSelectedIndex, m_outputFilterSelectedIndex + 1);
    ++m_outputFilterSelectedIndex;
    refreshOutputFilterUi();
    emitFilterChainChanged();
}

void OutputProcessingDialog::onOutputFilterRemove()
{
    if (!m_project || m_outputFilterSelectedIndex < 0) {
        return;
    }
    auto& chain = m_project->settings.output.filterChain;
    if (m_outputFilterSelectedIndex >= chain.size()) {
        return;
    }
    chain.removeAt(m_outputFilterSelectedIndex);
    if (m_outputFilterSelectedIndex >= chain.size()) {
        m_outputFilterSelectedIndex = chain.size() - 1;
    }
    refreshOutputFilterUi();
    emitFilterChainChanged();
}

} // namespace pvj::app

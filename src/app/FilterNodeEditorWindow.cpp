#include "FilterNodeEditorWindow.h"

#include "FilterPickerDialog.h"
#include "NodeGraphWidget.h"

#include "core/FilterCatalog.h"
#include "core/FilterEffectIds.h"
#include "core/FilterParamSchema.h"
#include "core/Project.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace pvj::app {

using pvj::core::Cell;
using pvj::core::GeneratorKind;
using pvj::core::Project;
using pvj::core::VisualType;

namespace {

// UserRole for source combo (must match graph context menu).
constexpr int kSourceMedia           = 0;
constexpr int kSourceTestPattern     = 2;
constexpr int kSourceSolid           = 3;
constexpr int kSourceSpout           = 4;
constexpr int kSourceNdi             = 5;
constexpr int kSourceInternalFeedback = 6;

void ensureFeedbackMarker(Cell* cell)
{
    if (!cell || cell->visual.type != VisualType::Generator
        || cell->visual.generator != GeneratorKind::InternalFeedback) {
        return;
    }
    for (const auto& node : cell->filterChain) {
        if (pvj::core::isFeedbackMarkerNode(node.typeId)) {
            return;
        }
    }
    pvj::core::CellFilterNode marker;
    marker.typeId = pvj::core::feedbackMarkerTypeId();
    cell->filterChain.prepend(marker);
}

} // namespace

FilterNodeEditorWindow::FilterNodeEditorWindow(QWidget* parent)
    : QDialog(parent)
{
    setModal(false);
    setWindowFlags(windowFlags() | Qt::Window);
    resize(920, 420);

    auto* root = new QVBoxLayout(this);

    m_srcBox = new QGroupBox(tr("Video source"), this);
    auto* srcLay = new QVBoxLayout(m_srcBox);
    m_sourceHint = new QLabel(this);
    m_sourceHint->setWordWrap(true);
    m_sourceHint->setStyleSheet(QStringLiteral("color: #9aa3b8;"));
    m_sourceCombo = new QComboBox(this);
    m_sourceCombo->addItem(tr("Media clip (project)"), kSourceMedia);
    m_sourceCombo->addItem(tr("Test pattern"), kSourceTestPattern);
    m_sourceCombo->addItem(tr("Solid color"), kSourceSolid);
    m_sourceCombo->addItem(tr("Spout (Windows)"), kSourceSpout);
    m_sourceCombo->addItem(tr("NDI"), kSourceNdi);
    m_sourceCombo->addItem(tr("Feedback loop (internal)"), kSourceInternalFeedback);
    m_pickSourceFilterBtn = new QPushButton(tr("Choose source filter…"), this);
    m_pickSourceFilterBtn->hide();
    connect(m_pickSourceFilterBtn, &QPushButton::clicked, this,
            &FilterNodeEditorWindow::pickMixerSourceFilter);
    srcLay->addWidget(m_sourceHint);
    srcLay->addWidget(m_sourceCombo);
    srcLay->addWidget(m_pickSourceFilterBtn);
    root->addWidget(m_srcBox);

    auto* graphBox = new QGroupBox(tr("Filter chain"), this);
    auto* graphLay = new QVBoxLayout(graphBox);
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_graph = new NodeGraphWidget(this);
    m_scrollArea->setWidget(m_graph);
    graphLay->addWidget(m_scrollArea, 1);
    root->addWidget(graphBox, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    // Close uses RejectRole → rejected(); avoids MSVC if-init quirks with button().
    connect(buttons, &QDialogButtonBox::rejected, this, &QWidget::close);
    root->addWidget(buttons);

    connect(m_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (!m_cell) {
            return;
        }
        applySourceToCell();
        emitChainEdited();
        refreshSourceHint();
        if (m_graph) {
            m_graph->refresh();
        }
    });

    connect(m_graph, &NodeGraphWidget::chainChanged, this, [this]() {
        emitChainEdited();
    });
    connect(m_graph, &NodeGraphWidget::filterChainChanged, this, [this]() {
        emitChainEdited();
    });
    connect(m_graph, &NodeGraphWidget::filterParamsChanged, this, [this]() {
        if (m_bankSetIndex >= 0 && m_bankIndex >= 0 && m_cellIndex >= 0) {
            emit filterParamsEdited(m_bankSetIndex, m_bankIndex, m_cellIndex);
        }
    });
    connect(m_graph, &NodeGraphWidget::midiLearnCcRequested, this, [this](const QString& propertyId) {
        emit midiLearnCcRequested(m_bankSetIndex, m_bankIndex, m_cellIndex, propertyId);
    });
    connect(m_graph, &NodeGraphWidget::midiLearnNoteRequested, this,
            [this](const QString& propertyId, bool toggle, double buttonValue) {
                emit midiLearnNoteRequested(m_bankSetIndex, m_bankIndex, m_cellIndex,
                                            propertyId, toggle, buttonValue);
            });
    connect(m_graph, &NodeGraphWidget::midiClearMappingRequested, this, [this](const QString& propertyId) {
        emit midiClearMappingRequested(m_bankSetIndex, m_bankIndex, m_cellIndex, propertyId);
    });

    connect(m_graph, &NodeGraphWidget::sourceChangeRequested, this, [this](int role) {
        const int idx = m_sourceCombo->findData(role);
        if (idx >= 0) {
            QSignalBlocker b(m_sourceCombo);
            m_sourceCombo->setCurrentIndex(idx);
        }
        applySourceToCell();
        emitChainEdited();
        refreshSourceHint();
        if (m_graph) {
            m_graph->refresh();
        }
    });
}

void FilterNodeEditorWindow::emitChainEdited()
{
    if (m_bankSetIndex >= 0 && m_bankIndex >= 0 && m_cellIndex >= 0) {
        emit chainEdited(m_bankSetIndex, m_bankIndex, m_cellIndex);
    }
}

void FilterNodeEditorWindow::openForCell(int bankSetIndex, int bankIndex, int cellIndex,
                                         Cell* cell, Project* project)
{
    m_bankSetIndex = bankSetIndex;
    m_bankIndex    = bankIndex;
    m_cellIndex    = cellIndex;
    m_cell         = cell;
    m_project      = project;

    if (!m_cell || !m_project) {
        return;
    }

    m_stashedMediaId = m_cell->visual.mediaId;
    for (auto& node : m_cell->filterChain) {
        if (node.params.isEmpty()) {
            node.params = pvj::core::defaultParamsFor(node.typeId);
        }
    }
    if (m_cell->visual.type != VisualType::MixerFilter) {
        ensureFeedbackMarker(m_cell);
    }

    setWindowTitle(tr("Filters & nodes — bank %1, cell %2")
                       .arg(m_bankIndex + 1)
                       .arg(m_cellIndex + 1));

    syncSourceUiForCell();
    syncSourceComboFromCell();
    m_graph->setCell(m_cell, m_project);
    m_graph->setDeckContext(m_bankSetIndex, m_bankIndex, m_cellIndex);
    m_graph->refresh();
    refreshSourceHint();

    show();
    raise();
    activateWindow();
}

void FilterNodeEditorWindow::syncSourceUiForCell()
{
    const bool mixerFx = m_cell && m_cell->visual.type == VisualType::MixerFilter;
    if (m_srcBox) {
        m_srcBox->setTitle(mixerFx ? tr("Mixer filter (source)")
                                   : tr("Video source"));
    }
    if (m_sourceCombo) {
        m_sourceCombo->setVisible(!mixerFx);
    }
    if (m_pickSourceFilterBtn) {
        m_pickSourceFilterBtn->setVisible(mixerFx);
    }
}

void FilterNodeEditorWindow::pickMixerSourceFilter()
{
    if (!m_cell) {
        return;
    }
    FilterPickerOptions opts;
    opts.title = tr("Mixer filter (source)");
    opts.includeEntry = [](const pvj::core::FilterCatalogEntry& e) {
        return !pvj::core::filterUsesMaxineBackend(e.typeId)
            && !pvj::core::isFeedbackMarkerNode(e.typeId);
    };
    const auto typeId = FilterPickerDialog::pick(this, opts);
    if (!typeId) {
        return;
    }
    m_cell->visual.type = VisualType::MixerFilter;
    m_cell->visual.generator = GeneratorKind::None;
    m_cell->visual.mediaId = {};
    pvj::core::CellFilterNode node;
    node.typeId = *typeId;
    node.params = pvj::core::defaultParamsFor(*typeId);
    if (m_cell->filterChain.isEmpty()) {
        m_cell->filterChain.append(node);
    } else {
        m_cell->filterChain[0] = node;
    }
    syncSourceUiForCell();
    emitChainEdited();
    refreshSourceHint();
    if (m_graph) {
        m_graph->refresh();
    }
}

void FilterNodeEditorWindow::refreshSourceHint()
{
    if (!m_cell) {
        return;
    }
    if (m_cell->visual.type == VisualType::MixerFilter) {
        QString hint = tr("Applied to the full mixer when this cell is triggered (no mix layer). "
                          "NVIDIA / Maxine filters in the chain always run last.");
        if (!m_cell->filterChain.isEmpty()) {
            const QString& tid = m_cell->filterChain.front().typeId;
            const QString en = pvj::core::filterCatalogEnglishName(tid);
            hint += QLatin1Char('\n')
                + tr("Source filter: %1").arg(en.isEmpty() ? tid : en);
        }
        m_sourceHint->setText(hint);
        return;
    }
    if (m_cell->visual.type == VisualType::Media && !m_cell->visual.mediaId.isNull()) {
        if (const auto* m = m_project->findMedia(m_cell->visual.mediaId)) {
            m_sourceHint->setText(tr("Media clip: %1").arg(QFileInfo(m->path).fileName()));
        } else {
            m_sourceHint->setText(tr("Media clip: (reference missing)"));
        }
    } else if (m_cell->visual.type == VisualType::Generator
               && m_cell->visual.generator == GeneratorKind::InternalFeedback) {
        m_sourceHint->setText(
            tr("Feedback layer — insert filters before or after the "
               "\"Feedback / Render Target\" marker to split pre-feedback and output-only processing. "
               "Keying is configured in the cell inspector and stays after the marker."));
    } else if (m_cell->visual.type == VisualType::Generator) {
        m_sourceHint->setText(tr("Generator source (no file clip)."));
    } else {
        m_sourceHint->setText(tr("No media clip assigned — pick a source or assign media in the library."));
    }
}

void FilterNodeEditorWindow::syncSourceComboFromCell()
{
    if (!m_cell) {
        return;
    }
    if (m_cell->visual.type == VisualType::MixerFilter) {
        return;
    }

    int role = kSourceMedia;
    if (m_cell->visual.type == VisualType::Media) {
        role = kSourceMedia;
    } else if (m_cell->visual.type == VisualType::Generator) {
        switch (m_cell->visual.generator) {
        case GeneratorKind::TestPattern:  role = kSourceTestPattern; break;
        case GeneratorKind::SolidColor:   role = kSourceSolid;       break;
        case GeneratorKind::InputSpout:   role = kSourceSpout;       break;
        case GeneratorKind::InputNdi:     role = kSourceNdi;         break;
        case GeneratorKind::InternalFeedback:
            role = kSourceInternalFeedback;
            break;
        default:
            role = kSourceTestPattern;
            break;
        }
    }

    const bool feedbackCell = m_cell->visual.type == VisualType::Generator
        && m_cell->visual.generator == GeneratorKind::InternalFeedback;
    m_sourceCombo->setEnabled(!feedbackCell);

    const int idx = m_sourceCombo->findData(role);
    if (idx >= 0) {
        QSignalBlocker b(m_sourceCombo);
        m_sourceCombo->setCurrentIndex(idx);
    }
}

void FilterNodeEditorWindow::applySourceToCell()
{
    if (!m_cell || m_cell->visual.type == VisualType::MixerFilter) {
        return;
    }

    const int role = m_sourceCombo->currentData().toInt();

    switch (role) {
    case kSourceMedia:
        m_cell->visual.type = VisualType::Media;
        m_cell->visual.generator = GeneratorKind::None;
        m_cell->visual.mediaId = m_stashedMediaId;
        break;
    case kSourceTestPattern:
        m_cell->visual.type = VisualType::Generator;
        m_cell->visual.generator = GeneratorKind::TestPattern;
        m_cell->visual.mediaId = {};
        break;
    case kSourceSolid:
        m_cell->visual.type = VisualType::Generator;
        m_cell->visual.generator = GeneratorKind::SolidColor;
        m_cell->visual.mediaId = {};
        break;
    case kSourceSpout:
        m_cell->visual.type = VisualType::Generator;
        m_cell->visual.generator = GeneratorKind::InputSpout;
        m_cell->visual.mediaId = {};
        break;
    case kSourceNdi:
        m_cell->visual.type = VisualType::Generator;
        m_cell->visual.generator = GeneratorKind::InputNdi;
        m_cell->visual.mediaId = {};
        break;
    case kSourceInternalFeedback:
        m_cell->visual.type = VisualType::Generator;
        m_cell->visual.generator = GeneratorKind::InternalFeedback;
        m_cell->visual.mediaId = {};
        ensureFeedbackMarker(m_cell);
        break;
    default:
        break;
    }
}

void FilterNodeEditorWindow::setMidiMappingEditMode(bool on)
{
    if (m_graph) {
        m_graph->setMidiMappingEditMode(on);
    }
}

void FilterNodeEditorWindow::refreshMidiMapOverlays()
{
    if (m_graph) {
        m_graph->syncMidiMapOverlays();
    }
}

} // namespace pvj::app

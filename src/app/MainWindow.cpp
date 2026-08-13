#include "MainWindow.h"

#include "BankGridWidget.h"
#include "FilterNodeEditorWindow.h"
#include "OutputProcessingDialog.h"
#include "MediaLibraryDock.h"
#include "ParameterInspector.h"
#include "PreferencesDialog.h"
#include "SplitterHelpers.h"

#include "core/AvcImporter.h"
#include "core/BankOps.h"
#include "core/CellOps.h"
#include "core/FilterEffectIds.h"
#include "core/MixerFilterOps.h"
#include "core/Model.h"
#include "core/Project.h"
#include "core/PropertyRegistry.h"
#include "core/PvjSerializer.h"
#include "core/Vj2Importer.h"

#include "audio/AudioEngine.h"
#include "audio/FfmpegAudioDecoder.h"
#include "render/FullscreenOutputWindow.h"
#include "render/RhiMixerWidget.h"
#include "render/RhiPreviewWidget.h"
#include "video/VideoDecoder.h"

#include "input/InputRouter.h"
#include "input/MidiInput.h"
#include "undo/ProjectUndoCommands.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QByteArray>
#include <QCloseEvent>
#include <QDebug>
#include <QLabel>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFrame>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QProgressBar>
#include <QProgressDialog>
#include <QMessageBox>
#include <QFont>
#include <QSignalBlocker>
#include <QPushButton>
#include <QSettings>
#include <QScreen>
#include <QSet>
#include <QStatusBar>
#include <QTimer>
#include <QUndoStack>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

#include <QtMath>

#include <algorithm>
#include <limits>
#include <optional>

namespace pvj::app {

using pvj::core::AvcImporter;
using pvj::core::MediaItem;
using pvj::core::Project;
using pvj::core::PvjSerializer;
using pvj::core::Vj2Importer;
using pvj::core::Bank;
using pvj::core::Cell;
using pvj::core::PlayMode;
using pvj::core::VisualType;
using pvj::core::GeneratorKind;
using pvj::core::cellIsMixerFilter;
using pvj::core::isFeedbackMarkerNode;

namespace {

bool movieSpeedIsRealtime(double speed)
{
    return qAbs(speed - 1.0) <= 1e-3;
}

float effectiveLayerAudioGain(const Cell* c)
{
    if (!c || !movieSpeedIsRealtime(c->props.movieSpeed)) {
        return 0.f;
    }
    return float(c->props.audioGain);
}

int mediaPreloadPercent(int completed, int total)
{
    if (total <= 0) {
        return 0;
    }
    const int clampedCompleted = qBound(0, completed, total);
    if (clampedCompleted == 0) {
        return 0;
    }
    if (clampedCompleted >= total) {
        return 100;
    }
    return qBound(1, static_cast<int>((qint64(clampedCompleted) * 100 + total - 1) / total), 99);
}

bool cellAllowsRename(const Cell& cell)
{
    if (cell.visual.type == VisualType::Generator
        && cell.visual.generator == GeneratorKind::InternalFeedback) {
        return true;
    }
    if (cell.visual.type == VisualType::MixerFilter) {
        return true;
    }
    if (cell.visual.type == VisualType::Empty) {
        return std::any_of(cell.filterChain.cbegin(), cell.filterChain.cend(),
                         [](const pvj::core::CellFilterNode& n) {
                             return !isFeedbackMarkerNode(n.typeId);
                         });
    }
    return false;
}


constexpr const char* kPvjFilter = "PerformanieVJ Project (*.pvj)";
constexpr const char* kVj2Filter = "GrandVJ Project (*.vj2)";
constexpr const char* kAvcFilter = "Resolume Composition (*.avc)";
constexpr const char* kUiMainSplitterState = "ui/mainSplitterState";
constexpr const char* kUiUpperSplitterState = "ui/upperSplitterState";
constexpr const char* kUiParameterInspectorVisible = "ui/parameterInspectorVisible";
constexpr const char* kUiMediaLibraryVisible = "ui/mediaLibraryVisible";
constexpr const char* kOutputStageWidth    = "output/stageWidth";
constexpr const char* kOutputStageHeight   = "output/stageHeight";
constexpr const char* kOutputScreenIndex   = "output/screenIndex";
constexpr int kDefaultStageWidth  = 1920;
constexpr int kDefaultStageHeight = 1080;
constexpr const char* kRecentProjectPaths = "recent/projectPaths";
constexpr const char* kLastProjectPath    = "session/lastProjectPath";
constexpr int         kRecentProjectMax   = 5;

QStringList loadRecentProjectPathsSetting()
{
    QSettings settings;
    const QVariant v = settings.value(QString::fromLatin1(kRecentProjectPaths));
    const QStringList raw = v.toStringList();
    QStringList out;
    out.reserve(qMin(raw.size(), kRecentProjectMax));
    for (const QString& p : raw) {
        if (p.isEmpty()) {
            continue;
        }
        const QString abs = QFileInfo(p).absoluteFilePath();
        if (!out.contains(abs)) {
            out.append(abs);
        }
        if (out.size() >= kRecentProjectMax) {
            break;
        }
    }
    return out;
}

void saveRecentProjectPathsSetting(const QStringList& paths)
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(kRecentProjectPaths), paths);
}

void pushRecentProjectPath(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }
    const QString abs = QFileInfo(path).absoluteFilePath();
    QStringList list = loadRecentProjectPathsSetting();
    list.removeAll(abs);
    list.prepend(abs);
    while (list.size() > kRecentProjectMax) {
        list.removeLast();
    }
    saveRecentProjectPathsSetting(list);
}

void saveLastProjectPathSetting(const QString& path)
{
    QSettings settings;
    if (path.isEmpty()) {
        settings.remove(QString::fromLatin1(kLastProjectPath));
        return;
    }
    settings.setValue(QString::fromLatin1(kLastProjectPath),
                      QFileInfo(path).absoluteFilePath());
}

QString loadLastProjectPathSetting()
{
    QSettings settings;
    return settings.value(QString::fromLatin1(kLastProjectPath)).toString();
}

QSize loadStagePixelSize()
{
    QSettings settings;
    const int w = settings.value(QLatin1String(kOutputStageWidth),  kDefaultStageWidth ).toInt();
    const int h = settings.value(QLatin1String(kOutputStageHeight), kDefaultStageHeight).toInt();
    return QSize(qMax(w, 16), qMax(h, 16));
}

void saveStagePixelSize(QSize px)
{
    QSettings settings;
    settings.setValue(QLatin1String(kOutputStageWidth),  px.width());
    settings.setValue(QLatin1String(kOutputStageHeight), px.height());
}

bool focusAllowsGlobalShortcuts()
{
    QWidget* w = QApplication::focusWidget();
    if (!w) {
        return true;
    }
    const QString cn = QString::fromLatin1(w->metaObject()->className());
    if (cn.endsWith(QLatin1String("LineEdit"))) {
        return false;
    }
    if (cn.contains(QLatin1String("TextEdit"))) {
        return false;
    }
    if (cn.contains(QLatin1String("SpinBox"))) {
        return false;
    }
    if (cn.contains(QLatin1String("Slider"))) {
        return false;
    }
    if (cn.contains(QLatin1String("Dial"))) {
        return false;
    }
    if (cn.contains(QLatin1String("KeySequenceEdit"))) {
        return false;
    }
    return true;
}

bool playModeUsesLoop(PlayMode m)
{
    switch (m) {
    case PlayMode::Once:
    case PlayMode::HoldLastFrame:
    case PlayMode::PlayBackwardOnce:
        return false;
    default:
        return true;
    }
}

bool applyMappedProperty(pvj::core::Cell& c, const QString& raw, double v)
{
    const QString p = pvj::core::PropertyRegistry::resolvePropertyId(raw);
    namespace PReg = pvj::core::PropertyRegistry;
    if (PReg::isFilterParamProperty(p)) {
        if (PReg::kindOf(p) == PReg::Kind::Enum) {
            return PReg::applyEnumFromNormalized(c, p, v);
        }
        return PReg::applyValue(c, p, v);
    }
    if (PReg::kindOf(p) == PReg::Kind::Enum) {
        return PReg::applyEnumFromNormalized(c, p, v);
    }
    return PReg::applyValue(c, p, v);
}

/// Mixer samples layers with `picture.rotationDeg`; inspector/MIDI Rotation Z is `rotationZ`
/// in −1…+1 (±180°). Combine both so MIDI/UI rotation actually turns the layer.
pvj::core::PictureParams pictureParamsForMixer(const Cell& c)
{
    pvj::core::PictureParams pic = c.props.picture;
    pic.rotationDeg += c.props.rotationZ * 180.0;
    return pic;
}

int preferredLayerFromCell(const Cell* c)
{
    if (!c) {
        return 4;
    }
    return qBound(0, c->props.preferredLayer, 12);
}

} // namespace

int MainWindow::gpuLayerFromPreferred(int preferredLayer)
{
    return qBound(kUserLayerMin, preferredLayer + 1, kUserLayerMax);
}

namespace {

struct LayerKeyingView {
    bool keyingEnabled = false;
    pvj::core::KeyingMode keyingMode = pvj::core::KeyingMode::Luma;
    double keyThreshold = 0.25;
    double keySoftness = 0.12;
    double keyLumaCenter = 0.5;
    bool keyLumaInvert = false;
    double keyChromaHue = 0.33;
    bool keyChromaInvert = false;
    pvj::core::MaskType maskType = pvj::core::MaskType::None;
    double maskFeather = 0.1;
    double maskRectWidth = 1.0;
    double maskRectHeight = 1.0;
    double maskRadius = 0.5;
    double maskEllipseX = 0.6;
    double maskEllipseY = 0.45;
};

LayerKeyingView layerKeyingView(const Cell* c, const LayerKeyingState* layerKeying)
{
    LayerKeyingView v;
    if (layerKeying && layerKeying->valid) {
        v.keyingEnabled = layerKeying->keyingEnabled;
        v.keyingMode = layerKeying->keyingMode;
        v.keyThreshold = layerKeying->keyThreshold;
        v.keySoftness = layerKeying->keySoftness;
        v.keyLumaCenter = layerKeying->keyLumaCenter;
        v.keyLumaInvert = layerKeying->keyLumaInvert;
        v.keyChromaHue = layerKeying->keyChromaHue;
        v.keyChromaInvert = layerKeying->keyChromaInvert;
        v.maskType = layerKeying->maskType;
        v.maskFeather = layerKeying->maskFeather;
        v.maskRectWidth = layerKeying->maskRectWidth;
        v.maskRectHeight = layerKeying->maskRectHeight;
        v.maskRadius = layerKeying->maskRadius;
        v.maskEllipseX = layerKeying->maskEllipseX;
        v.maskEllipseY = layerKeying->maskEllipseY;
        return v;
    }
    if (!c) {
        return v;
    }
    v.keyingEnabled = c->props.keyingEnabled;
    v.keyingMode = c->props.keyingMode;
    v.keyThreshold = c->props.keyThreshold;
    v.keySoftness = c->props.keySoftness;
    v.keyLumaCenter = c->props.keyLumaCenter;
    v.keyLumaInvert = c->props.keyLumaInvert;
    v.keyChromaHue = c->props.keyChromaHue;
    v.keyChromaInvert = c->props.keyChromaInvert;
    v.maskType = c->props.maskType;
    v.maskFeather = c->props.maskFeather;
    v.maskRectWidth = c->props.maskRectWidth;
    v.maskRectHeight = c->props.maskRectHeight;
    v.maskRadius = c->props.maskRadius;
    v.maskEllipseX = c->props.maskEllipseX;
    v.maskEllipseY = c->props.maskEllipseY;
    return v;
}

void setNodeParam(pvj::core::CellFilterNode& node, const QString& name, double value)
{
    for (auto& p : node.params) {
        if (p.name == name) {
            p.value = value;
            return;
        }
    }
    node.params.append({ name, value });
}

void syncKeyNodeFromView(pvj::core::CellFilterNode& node, const LayerKeyingView& v)
{
    const QString t = node.typeId.toLower();
    if (t == QLatin1String("chroma_key")) {
        setNodeParam(node, QStringLiteral("mode"), v.keyChromaInvert ? 1.0 : 0.0);
        setNodeParam(node, QStringLiteral("hue"), qBound(0.0, v.keyChromaHue, 1.0));
        setNodeParam(node, QStringLiteral("threshold"), qBound(0.0, v.keyThreshold, 1.0));
        setNodeParam(node, QStringLiteral("softness"), qBound(0.0, v.keySoftness, 1.0));
    } else if (t == QLatin1String("luma_key")) {
        setNodeParam(node, QStringLiteral("mode"), v.keyLumaInvert ? 1.0 : 0.0);
        setNodeParam(node, QStringLiteral("brightness"), qBound(0.0, v.keyLumaCenter, 1.0));
        setNodeParam(node, QStringLiteral("threshold"), qBound(0.0, v.keyThreshold, 1.0));
        setNodeParam(node, QStringLiteral("softness"), qBound(0.0, v.keySoftness, 1.0));
    }
}

QList<pvj::core::CellFilterNode> effectiveFilterChainForMixer(const Cell* c,
                                                              const LayerKeyingState* layerKeying)
{
    if (!c) {
        return {};
    }
    const LayerKeyingView kv = layerKeyingView(c, layerKeying);
    QList<pvj::core::CellFilterNode> chain = c->filterChain;
    bool hasExplicitKeyNode = false;
    bool hasExplicitMaskNode = false;
    for (const auto& n : chain) {
        const QString t = n.typeId.toLower();
        if (t == QLatin1String("chroma_key") || t == QLatin1String("luma_key")) {
            hasExplicitKeyNode = true;
        }
        if (t == QLatin1String("mask") || t == QLatin1String("linear_mask")
            || t == QLatin1String("crop") || t == QLatin1String("crop_rectangle")) {
            hasExplicitMaskNode = true;
        }
    }
    if (!hasExplicitMaskNode && kv.maskType != pvj::core::MaskType::None) {
        pvj::core::CellFilterNode maskNode;
        maskNode.typeId = QStringLiteral("mask");
        const double feather = qBound(0.0, kv.maskFeather, 1.0);
        switch (kv.maskType) {
        case pvj::core::MaskType::Rectangle:
        case pvj::core::MaskType::SoftEdge:
            maskNode.params = {
                { QStringLiteral("mode"), double(int(kv.maskType)) },
                { QStringLiteral("sizeX"), qBound(0.0, kv.maskRectWidth, 1.0) },
                { QStringLiteral("sizeY"), qBound(0.0, kv.maskRectHeight, 1.0) },
                { QStringLiteral("feather"), feather },
            };
            break;
        case pvj::core::MaskType::Circle:
        case pvj::core::MaskType::Custom:
            maskNode.params = {
                { QStringLiteral("mode"), double(int(kv.maskType)) },
                { QStringLiteral("sizeX"), qBound(0.0, kv.maskRadius, 1.0) },
                { QStringLiteral("sizeY"), qBound(0.0, kv.maskRadius, 1.0) },
                { QStringLiteral("feather"), feather },
            };
            break;
        case pvj::core::MaskType::Ellipse:
            maskNode.params = {
                { QStringLiteral("mode"), double(int(kv.maskType)) },
                { QStringLiteral("sizeX"), qBound(0.0, kv.maskEllipseX, 1.0) },
                { QStringLiteral("sizeY"), qBound(0.0, kv.maskEllipseY, 1.0) },
                { QStringLiteral("feather"), feather },
            };
            break;
        case pvj::core::MaskType::None:
            break;
        }
        if (!maskNode.params.isEmpty()) {
            chain.append(maskNode);
        }
    }
    if (hasExplicitKeyNode) {
        QList<pvj::core::CellFilterNode> keyNodes;
        QList<pvj::core::CellFilterNode> nonKeyNodes;
        keyNodes.reserve(chain.size());
        nonKeyNodes.reserve(chain.size());
        for (auto& n : chain) {
            const QString t = n.typeId.toLower();
            if (t == QLatin1String("chroma_key") || t == QLatin1String("luma_key")) {
                syncKeyNodeFromView(n, kv);
                keyNodes.append(n);
            } else {
                nonKeyNodes.append(n);
            }
        }
        nonKeyNodes.append(keyNodes);
        chain = std::move(nonKeyNodes);
        return chain;
    }
    if (!kv.keyingEnabled) {
        return chain;
    }
    if (kv.maskType != pvj::core::MaskType::None) {
        return chain;
    }

    pvj::core::CellFilterNode keyNode;
    if (kv.keyingMode == pvj::core::KeyingMode::Chroma) {
        keyNode.typeId = QStringLiteral("chroma_key");
        keyNode.params = {
            { QStringLiteral("mode"), kv.keyChromaInvert ? 1.0 : 0.0 },
            { QStringLiteral("hue"), qBound(0.0, kv.keyChromaHue, 1.0) },
            { QStringLiteral("threshold"), qBound(0.0, kv.keyThreshold, 1.0) },
            { QStringLiteral("softness"), qBound(0.0, kv.keySoftness, 1.0) },
        };
    } else {
        keyNode.typeId = QStringLiteral("luma_key");
        keyNode.params = {
            { QStringLiteral("mode"), kv.keyLumaInvert ? 1.0 : 0.0 },
            { QStringLiteral("brightness"), qBound(0.0, kv.keyLumaCenter, 1.0) },
            { QStringLiteral("threshold"), qBound(0.0, kv.keyThreshold, 1.0) },
            { QStringLiteral("softness"), qBound(0.0, kv.keySoftness, 1.0) },
        };
    }
    chain.append(keyNode);
    return chain;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_project(std::make_unique<Project>())
{
    m_undoStack = new QUndoStack(this);
    connect(m_undoStack, &QUndoStack::indexChanged, this, [this](int) {
        refreshUiAfterProjectEdit();
    });
    connect(m_undoStack, &QUndoStack::cleanChanged, this, [this](bool clean) {
        if (!clean) {
            markProjectDirty();
        }
    });

    m_filterParamsUndoTimer = new QTimer(this);
    m_filterParamsUndoTimer->setSingleShot(true);
    m_filterParamsUndoTimer->setInterval(350);
    connect(m_filterParamsUndoTimer, &QTimer::timeout, this, [this]() {
        if (m_filterParamsUndoBankSet < 0) {
            return;
        }
        commitFilterCellUndo(m_filterParamsUndoBankSet, m_filterParamsUndoBank,
                             m_filterParamsUndoCell, tr("Edit filter parameters"));
        m_filterParamsUndoBankSet = -1;
    });

    m_audioEngine = std::make_unique<pvj::audio::AudioEngine>(this);
    if (!m_audioEngine->initialize(pvj::audio::AudioEngine::kDefaultSampleRate)) {
        qWarning() << "Audio output could not be initialized; video-only playback.";
    }

    for (int i = 0; i < kMixLayers; ++i) {
        m_decoders[i] = std::make_unique<pvj::video::VideoDecoder>();
        m_audioDecoders[i] = std::make_unique<pvj::audio::FfmpegAudioDecoder>(
            &m_audioEngine->layerBuffer(i),
            pvj::audio::AudioEngine::kDefaultSampleRate,
            this);
        connect(m_audioDecoders[i].get(), &pvj::audio::FfmpegAudioDecoder::errorOccurred,
                this, [this, i](const QString& msg) {
            statusBar()->showMessage(tr("Audio error (layer %1): %2")
                                     .arg(i + 1).arg(msg), 5000);
        });
    }

    m_project->initializeDefault();

    resize(1600, 900);
    setupCentralLayout();

    m_filterEditor = std::make_unique<FilterNodeEditorWindow>(this);
    connect(m_filterEditor.get(), &FilterNodeEditorWindow::chainEdited, this,
            [this](int bankSetIndex, int bankIndex, int cellIndex) {
                if (m_filterParamsUndoTimer) {
                    m_filterParamsUndoTimer->stop();
                }
                commitFilterCellUndo(bankSetIndex, bankIndex, cellIndex, tr("Edit filter graph"));
                if (m_bankGrid) {
                    m_bankGrid->refresh();
                }
                if (m_inspector) {
                    m_inspector->refreshFromModel();
                }
                const int layer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
                const Cell* cell = layer >= 0 ? cellAtDeck(m_layerSlots[static_cast<size_t>(layer)]) : nullptr;
                const bool feedbackPlaying = cell && cell->visual.type == VisualType::Generator
                    && cell->visual.generator == GeneratorKind::InternalFeedback;
                if (feedbackPlaying) {
                    syncPlayingFeedbackCellToMixer(bankSetIndex, bankIndex, cellIndex);
                } else {
                    updateMixerFromPlayingCells();
                }
                if (isMixerFilterCellActive(bankSetIndex, bankIndex, cellIndex)) {
                    syncOutputFilterChainToMixers();
                }
                syncMixerFilterHighlightsToBankGrid();
            });
    connect(m_filterEditor.get(), &FilterNodeEditorWindow::filterParamsEdited, this,
            [this](int bankSetIndex, int bankIndex, int cellIndex) {
                scheduleFilterParamsUndo(bankSetIndex, bankIndex, cellIndex);
                const int layer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
                const Cell* cell = layer >= 0 ? cellAtDeck(m_layerSlots[static_cast<size_t>(layer)]) : nullptr;
                const bool feedbackPlaying = cell && cell->visual.type == VisualType::Generator
                    && cell->visual.generator == GeneratorKind::InternalFeedback;
                if (feedbackPlaying) {
                    syncPlayingFeedbackCellToMixer(bankSetIndex, bankIndex, cellIndex);
                } else {
                    syncFilterParamsToMixer(bankSetIndex, bankIndex, cellIndex);
                }
                if (isMixerFilterCellActive(bankSetIndex, bankIndex, cellIndex)) {
                    syncOutputFilterChainToMixers();
                }
            });
    connect(m_filterEditor.get(), &FilterNodeEditorWindow::midiLearnCcRequested, this,
            [this](int bankSetIndex, int bankIndex, int cellIndex, const QString& propertyId) {
                if (!m_inputRouter) {
                    return;
                }
                captureLearnBaseline();
                m_learnCellBefore = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
                m_inputRouter->beginLearnPropertyCc(bankSetIndex, bankIndex, cellIndex, propertyId);
                statusBar()->showMessage(tr("Learn: move a MIDI CC (0–127)…"), 15000);
            });
    connect(m_filterEditor.get(), &FilterNodeEditorWindow::midiLearnNoteRequested, this,
            [this](int bankSetIndex, int bankIndex, int cellIndex, const QString& propertyId,
                   bool toggle, double buttonValue) {
                if (!m_inputRouter) {
                    return;
                }
                captureLearnBaseline();
                m_learnCellBefore = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
                m_inputRouter->beginLearnPropertyNote(
                    bankSetIndex, bankIndex, cellIndex, propertyId,
                    toggle ? pvj::core::PropertyButtonMode::Toggle
                           : pvj::core::PropertyButtonMode::SetOnPress,
                    buttonValue);
                statusBar()->showMessage(tr("Learn: press a MIDI note…"), 15000);
            });
    connect(m_filterEditor.get(), &FilterNodeEditorWindow::midiClearMappingRequested, this,
            [this](int bankSetIndex, int bankIndex, int cellIndex, const QString& propertyId) {
                if (!m_inputRouter) {
                    return;
                }
                auto before = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
                m_inputRouter->clearPropertyMappingForCell(bankSetIndex, bankIndex, cellIndex, propertyId);
                if (before) {
                    if (auto after = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex)) {
                        if (before->cell.propertyMappings.size() != after->cell.propertyMappings.size()) {
                            pushCellsReplaceCommand({*before}, {*after}, tr("Clear MIDI mapping"));
                        }
                    }
                }
                statusBar()->showMessage(tr("Cleared MIDI mapping for %1").arg(propertyId), 4000);
                if (m_filterEditor) {
                    m_filterEditor->refreshMidiMapOverlays();
                }
            });

    m_layerFadeTimer = new QTimer(this);
    m_layerFadeTimer->setInterval(16);
    connect(m_layerFadeTimer, &QTimer::timeout, this, &MainWindow::tickLayerFade);

    m_midiInspectorDebounceTimer = new QTimer(this);
    m_midiInspectorDebounceTimer->setSingleShot(true);
    m_midiInspectorDebounceTimer->setInterval(33);
    connect(m_midiInspectorDebounceTimer, &QTimer::timeout, this, [this]() {
        if (!m_inspector) {
            return;
        }
        syncInspectorLayerKeyingOverride();
        m_inspector->refreshFromModel();
    });

    m_mixerUpdateDebounceTimer = new QTimer(this);
    m_mixerUpdateDebounceTimer->setSingleShot(true);
    m_mixerUpdateDebounceTimer->setInterval(16);
    // Leading-edge throttle with trailing edge: apply the latest pending update
    // (e.g. final transparency value at the end of a drag) and restart the cooldown
    // so further updates inside the next window are coalesced again.
    connect(m_mixerUpdateDebounceTimer, &QTimer::timeout, this, [this]() {
        if (!m_mixerUpdatePending) {
            return;
        }
        m_mixerUpdatePending = false;
        updateMixerFromPlayingCells();
        syncMixerToFullscreen();
        m_mixerUpdateDebounceTimer->start();
    });

    m_feedbackMixerSyncTimer = new QTimer(this);
    m_feedbackMixerSyncTimer->setSingleShot(true);
    m_feedbackMixerSyncTimer->setInterval(33);
    connect(m_feedbackMixerSyncTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingFeedbackSyncBankSet < 0) {
            return;
        }
        const int bs = m_pendingFeedbackSyncBankSet;
        const int b = m_pendingFeedbackSyncBank;
        const int c = m_pendingFeedbackSyncCell;
        m_pendingFeedbackSyncBankSet = -1;
        syncPlayingFeedbackCellToMixer(bs, b, c);
    });

    m_outputScreenIndex =
        QSettings().value(QLatin1String(kOutputScreenIndex), 0).toInt();

    m_outputProcessingDialog = std::make_unique<OutputProcessingDialog>(this);
    connect(m_outputProcessingDialog.get(), &OutputProcessingDialog::filterChainChanged,
            this, &MainWindow::syncOutputFilterChainToMixers);

    setupMenus();
    setupInputMapping();
    rebindUiToProject(/*preloadMedia=*/false);

    for (int i = 0; i < kMixLayers; ++i) {
        connect(m_decoders[i].get(), &pvj::video::VideoDecoder::frameReady,
                this, [this, i](QImage frame, qint64 pts) {
            m_lastFrames[static_cast<size_t>(i)] = frame;
            const bool selHere = selectionMatchesSlot(m_layerSlots[static_cast<size_t>(i)]);
            const bool peekHere = (m_clipPeekLayer == i);
            // Large clip preview (left): only while right-click peek is active — not on mere left-click / selection.
            if (peekHere && m_previewA) {
                m_previewA->setFrame(frame, pts);
            }
            if (selHere && m_inspector) {
                m_inspector->setVisualThumbnail(frame);
            }
            m_previewB->setFrame(i, frame, pts);
            if (m_fullscreenOut && m_fullscreenOut->isVisible()) {
                m_fullscreenOut->mixerWidget()->setFrame(i, frame, pts);
            }
        }, Qt::QueuedConnection);

        connect(m_decoders[i].get(), &pvj::video::VideoDecoder::errorOccurred,
                this, [this, i](const QString& msg) {
            statusBar()->showMessage(tr("Playback error (layer %1): %2")
                                     .arg(i + 1).arg(msg), 5000);
        });
        m_decoders[i]->setLooping(true);
    }

    connect(m_previewB, &pvj::render::RhiMixerWidget::feedbackRepaintTick, this, [this]() {
        if (m_fullscreenOut && m_fullscreenOut->isVisible()) {
            if (pvj::render::RhiMixerWidget* dst = m_fullscreenOut->mixerWidget()) {
                dst->update();
            }
        }
    });

    connect(m_inspector, &ParameterInspector::visualSeekStepRequested,
            this, &MainWindow::onInspectorVisualSeekStep);
    connect(m_inspector, &ParameterInspector::scratchApplyRequested,
            this, &MainWindow::onInspectorScratchApply);
    connect(m_inspector, &ParameterInspector::midiLearnCcRequested,
            this, &MainWindow::onInspectorMidiLearnCc);
    connect(m_inspector, &ParameterInspector::midiLearnNoteRequested,
            this, &MainWindow::onInspectorMidiLearnNote);
    connect(m_inspector, &ParameterInspector::midiClearMappingRequested,
            this, &MainWindow::onInspectorMidiClearMapping);

    applyStagePixelSize(loadStagePixelSize());

    statusBar()->showMessage(tr("Ready"));
    updateWindowTitle();

    QTimer::singleShot(0, this, &MainWindow::promptOpenLastProjectIfNeeded);
}

MainWindow::~MainWindow()
{
    shutdownUndoStack();
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void MainWindow::shutdownUndoStack()
{
    if (m_filterParamsUndoTimer) {
        m_filterParamsUndoTimer->stop();
        disconnect(m_filterParamsUndoTimer, nullptr, this, nullptr);
    }
    if (!m_undoStack) {
        return;
    }
    // indexChanged would otherwise refresh UI / touch project lists during teardown.
    disconnect(m_undoStack, nullptr, this, nullptr);
    m_undoStack->clear();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_closing) {
        QMainWindow::closeEvent(event);
        return;
    }
    if (!maybeSave()) {
        event->ignore();
        return;
    }
    m_closing = true;
    rememberLastProjectPath();
    shutdownUndoStack();
    saveViewSettings();
    if (m_actMidiMappingEdit && m_actMidiMappingEdit->isChecked()) {
        QSignalBlocker blocker(m_actMidiMappingEdit);
        m_actMidiMappingEdit->setChecked(false);
    }
    if (m_fullscreenOut && m_fullscreenOut->isVisible()) {
        m_fullscreenOut->leaveFullscreen();
    }
    if (m_filterEditor && m_filterEditor->isVisible()) {
        m_filterEditor->close();
    }
    closeMediaPreloadHint();
    QMainWindow::closeEvent(event);
}

void MainWindow::setupCentralLayout()
{
    // Right-side dock: media library
    m_mediaDock = new MediaLibraryDock(this);
    addDockWidget(Qt::RightDockWidgetArea, m_mediaDock);
    connect(m_mediaDock, &MediaLibraryDock::mediaActivated,
            this, &MainWindow::onMediaActivated);

    // Central area: vertical split between (parameter tabs + previews) and bank grid
    auto* central = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(4, 4, 4, 4);
    centralLayout->setSpacing(4);

    m_mainSplit = new PvjSplitter(Qt::Vertical, central);
    m_mainSplit->setChildrenCollapsible(false);

    m_upperSplit = new PvjSplitter(Qt::Horizontal, m_mainSplit);
    m_upperSplit->setChildrenCollapsible(true);

    // Left of upper split: parameter inspector tabs
    m_inspector = new ParameterInspector(m_upperSplit);
    m_upperSplit->addWidget(m_inspector);
    connect(m_inspector, &ParameterInspector::cellChanged,
            this, &MainWindow::onInspectorDiscreteCellChanged);
    connect(m_inspector, &ParameterInspector::cellPlaybackChanged,
            this, &MainWindow::onCellPlaybackChanged);
    connect(m_inspector, &ParameterInspector::continuousEditBegan,
            this, &MainWindow::beginInspectorContinuousEdit);
    connect(m_inspector, &ParameterInspector::continuousEditEnded,
            this, &MainWindow::endInspectorContinuousEdit);

    // Right of upper split: layer preview + mixer output
    auto* previewHost = new QWidget(m_upperSplit);
    auto* previewLayout = new QHBoxLayout(previewHost);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(4);

    m_clipPreviewFrame = new QFrame(previewHost);
    m_clipPreviewFrame->setObjectName(QStringLiteral("pvjClipPreviewFrame"));
    setClipPreviewPeekChrome(false);
    auto* clipPreviewLay = new QVBoxLayout(m_clipPreviewFrame);
    clipPreviewLay->setContentsMargins(0, 0, 0, 0);
    clipPreviewLay->setSpacing(0);
    m_previewA = new pvj::render::RhiPreviewWidget(m_clipPreviewFrame);
    m_previewA->setLabel(tr("Clip preview"));
    clipPreviewLay->addWidget(m_previewA, 1);
    m_previewB = new pvj::render::RhiMixerWidget(previewHost);
    m_previewB->setLabel(tr("Mixer"));
    previewLayout->addWidget(m_clipPreviewFrame, 1);
    previewLayout->addWidget(m_previewB, 1);

    m_upperSplit->addWidget(previewHost);
    m_upperSplit->setStretchFactor(0, 2);
    m_upperSplit->setStretchFactor(1, 5);
    m_mainSplit->addWidget(m_upperSplit);

    // Bottom: bank grid
    auto* bankHost = new QWidget(m_mainSplit);
    auto* bankHostLayout = new QVBoxLayout(bankHost);
    bankHostLayout->setContentsMargins(0, 0, 0, 0);
    bankHostLayout->setSpacing(4);
    m_bankGrid = new BankGridWidget(bankHost);
    m_bankGrid->setMinimumHeight(280);
    bankHostLayout->addWidget(m_bankGrid, 1);

    m_mainSplit->addWidget(bankHost);
    m_mainSplit->setStretchFactor(0, 5);
    m_mainSplit->setStretchFactor(1, 2);
    centralLayout->addWidget(m_mainSplit, 1);

    connect(m_bankGrid, &BankGridWidget::cellSelected,
            this, [this](int bankSetIndex, int bankIndex, int cellIndex) {
        onCellSelected(bankSetIndex, bankIndex, cellIndex);
    });
    connect(m_bankGrid, &BankGridWidget::cellPreviewCacheUpdated,
            this, [this](const QUuid& mediaId) {
        if (!m_inspector || !m_project) {
            return;
        }
        const Cell* c = cellAtDeck({m_inspector->selectedBankSetIndex(),
                                    m_inspector->selectedBankIndex(),
                                    m_inspector->selectedCellIndex()});
        if (c && c->visual.mediaId == mediaId) {
            refreshPreviewForSelectedCell();
        }
    });
    connect(m_bankGrid, &BankGridWidget::mediaDroppedOnCell,
            this, &MainWindow::onMediaDroppedOnCell);
    connect(m_bankGrid, &BankGridWidget::bankSelected,
            this, [this](int bankSetIndex, int bankIndex) {
        if (m_inspector && m_bankGrid) {
            m_inspector->setSelection(bankSetIndex, bankIndex, m_bankGrid->selectedCellIndex());
            refreshPreviewForSelectedCell();
        }
    });
    connect(m_bankGrid, &BankGridWidget::cellTriggered, this,
            [this](int bankSetIndex, int bankIndex, int cellIndex) {
                onCellTriggered(bankSetIndex, bankIndex, cellIndex, true);
            });
    connect(m_bankGrid, &BankGridWidget::cellEditRequested,
            this, &MainWindow::onCellEditRequested);
    connect(m_bankGrid, &BankGridWidget::cellPeekPreviewRequested,
            this, &MainWindow::onCellPeekPreviewRequested);
    connect(m_bankGrid, &BankGridWidget::cellContextMenuRequested,
            this, &MainWindow::onCellContextMenu);
    connect(m_bankGrid, &BankGridWidget::bankContextMenuRequested,
            this, &MainWindow::onBankContextMenu);
    connect(m_bankGrid, &BankGridWidget::midiLearnCellTriggerRequested,
            this, &MainWindow::onMidiLearnCellTriggerFromGrid);
    connect(m_bankGrid, &BankGridWidget::mediaPreloadProgress,
            this, &MainWindow::onMediaPreloadProgress);
    connect(m_bankGrid, &BankGridWidget::mediaPreloadFinished,
            this, &MainWindow::onMediaPreloadFinished);
    setCentralWidget(central);
}

void MainWindow::setupMenus()
{
    auto* fileMenu = menuBar()->addMenu(tr("&File"));

    auto* actNew = fileMenu->addAction(tr("&New Project"), this, &MainWindow::onFileNew);
    actNew->setShortcut(QKeySequence::New);

    auto* actOpen = fileMenu->addAction(tr("&Open..."), this, &MainWindow::onFileOpen);
    actOpen->setShortcut(QKeySequence::Open);

    m_recentFilesMenu = fileMenu->addMenu(tr("Recent"));
    connect(m_recentFilesMenu, &QMenu::aboutToShow, this, &MainWindow::updateRecentFilesMenu);
    updateRecentFilesMenu();

    fileMenu->addSeparator();

    auto* importMenu = fileMenu->addMenu(tr("&Import"));
    importMenu->addAction(tr("GrandVJ Project (.vj2)..."), this, &MainWindow::onImportVj2);
    importMenu->addAction(tr("GrandVJ into bank… (.vj2)..."), this, &MainWindow::onImportVj2IntoBank);
    importMenu->addAction(tr("Resolume Composition (.avc)..."), this, &MainWindow::onImportAvc);

    fileMenu->addSeparator();

    auto* actSave = fileMenu->addAction(tr("&Save"), this, &MainWindow::onFileSave);
    actSave->setShortcut(QKeySequence::Save);

    auto* actSaveAs = fileMenu->addAction(tr("Save &As..."), this, &MainWindow::onFileSaveAs);
    actSaveAs->setShortcut(QKeySequence::SaveAs);

    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QMainWindow::close);

    auto* editMenu = menuBar()->addMenu(tr("&Edit"));
    auto* actUndo = m_undoStack->createUndoAction(this, tr("&Undo"));
    actUndo->setShortcuts(QKeySequence::Undo);
    editMenu->addAction(actUndo);
    auto* actRedo = m_undoStack->createRedoAction(this, tr("&Redo"));
    actRedo->setShortcuts(QKeySequence::Redo);
    editMenu->addAction(actRedo);
    editMenu->addSeparator();
    editMenu->addAction(tr("&Copy cell"), this, &MainWindow::copySelectedCell)
        ->setShortcut(QKeySequence::Copy);
    editMenu->addAction(tr("&Paste (with MIDI)"), this, &MainWindow::pasteIntoSelectedCellWithMidi)
        ->setShortcut(QKeySequence::Paste);
    editMenu->addAction(tr("Paste &without MIDI"), this, &MainWindow::pasteIntoSelectedCellWithoutMidi)
        ->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+V")));
    editMenu->addAction(tr("Paste &parameters + MIDI"), this, &MainWindow::pasteIntoSelectedCellParamsWithMidi)
        ->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+V")));
    editMenu->addSeparator();
    auto* actPrefs = editMenu->addAction(tr("&Preferences..."), this, &MainWindow::onEditPreferences);
    actPrefs->setShortcut(QKeySequence::Preferences);
    actPrefs->setMenuRole(QAction::PreferencesRole);

    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_mediaDock->toggleViewAction());
    m_actToggleInspector = viewMenu->addAction(tr("&Parameter inspector"));
    m_actToggleInspector->setCheckable(true);
    m_actToggleInspector->setChecked(true);
    m_actToggleInspector->setShortcut(QKeySequence(QStringLiteral("Ctrl+I")));
    m_actToggleInspector->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_actToggleInspector, &QAction::toggled, this, &MainWindow::setParameterInspectorVisible);
    viewMenu->addAction(tr("Bank grid size..."), this, &MainWindow::onConfigureBankGrid);

    restoreViewSettings();

    m_outputMenu = menuBar()->addMenu(tr("&Output"));
    connect(m_outputMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildOutputScreenMenu);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, &MainWindow::rebuildOutputScreenMenu);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &MainWindow::rebuildOutputScreenMenu);

    auto* mapMenu = menuBar()->addMenu(tr("&Mapping"));
    m_actMidiMappingEdit = mapMenu->addAction(tr("Edit &mapping mode"));
    m_actMidiMappingEdit->setCheckable(true);
    m_actMidiMappingEdit->setShortcut(QKeySequence(QStringLiteral("Ctrl+M")));
    m_actMidiMappingEdit->setShortcutContext(Qt::ApplicationShortcut);
    m_actMidiMappingEdit->setToolTip(
        tr("Ctrl+M: green bank grid. Click a cell, then press a keyboard key or MIDI note to assign a clip trigger."));
    connect(m_actMidiMappingEdit, &QAction::toggled, this, &MainWindow::onMidiMappingEditToggled);
    mapMenu->addSeparator();
    mapMenu->addAction(tr("Learn cell trigger…"), this, &MainWindow::onLearnCellTrigger);
    mapMenu->addAction(tr("Learn MIDI CC → property…"), this, &MainWindow::onLearnPropertyCc);
    mapMenu->addAction(tr("Learn MIDI CC → fade (transparency)…"),
                       this, &MainWindow::onLearnMidiFadeTransparency);
    mapMenu->addSeparator();
    auto* bankLearn = mapMenu->addMenu(tr("Learn bank (MIDI note)…"));
    bankLearn->addAction(tr("Bank &next…"), this, &MainWindow::onLearnBankNext);
    bankLearn->addAction(tr("Bank &previous…"), this, &MainWindow::onLearnBankPrev);
    bankLearn->addAction(tr("&Select bank…"), this, &MainWindow::onLearnBankSelect);
    m_midiPortMenu = mapMenu->addMenu(tr("MIDI input port"));
    mapMenu->addSeparator();
    auto* applyMenu = mapMenu->addMenu(tr("Copy to all banks"));
    applyMenu->addAction(tr("Keyboard mapping (all cells)…"), this,
                         &MainWindow::onCopyKeyboardMappingToAllBanks);
    applyMenu->addAction(tr("Layer selection (all cells)…"), this,
                         &MainWindow::onCopyLayerMappingToAllBanks);
    applyMenu->addAction(tr("MIDI mapping (all cells)…"), this,
                         &MainWindow::onCopyMidiMappingToAllBanks);
    applyMenu->addSeparator();
    m_actApplyMappingAllBanks = applyMenu->addAction(tr("All cell settings (legacy)"));
    m_actApplyMappingAllBanks->setCheckable(true);
    m_actApplyMappingAllBanks->setToolTip(
        tr("Copy the selected cell's MIDI mappings, layer settings (preferred mix "
           "layer, keying, mixing, matte role), and playback settings (not the video) "
           "to the same cell on every bank."));
    connect(m_actApplyMappingAllBanks, &QAction::toggled,
            this, &MainWindow::onApplyMappingToAllBanksToggled);
    mapMenu->addSeparator();
    mapMenu->addAction(tr("Cancel learn"), this, &MainWindow::onCancelLearn);
    mapMenu->addSeparator();
    mapMenu->addAction(tr("Mix slot shortcuts…"), this, [this] {
        QMessageBox::information(
            this,
            tr("Mix slot shortcuts"),
            tr("<p><b>Cell triggers (Ctrl+M):</b> enable mapping mode, click a cell, then press a "
               "keyboard key or MIDI note. The key appears on the cell (top right).</p>"
               "<p><b>Mix slots (MIDI):</b> channel 16, notes <b>60–72</b> (C4–C5) → slots 1–13.</p>"
               "<p><b>Grid:</b> <span style=\"color:#3a9cff\">blue</span> = clip on a mix layer; "
               "<span style=\"color:#ff913a\">orange</span> = large clip preview (right-click peek).</p>"
               "<p><b>Right-click</b> a cell: context menu (peek preview, copy, paste). "
               "<b>Ctrl+C</b> copies the selected cell. "
               "<b>Ctrl+V</b> paste with MIDI, <b>Ctrl+Shift+V</b> without MIDI, "
               "<b>Ctrl+Alt+V</b> parameters + MIDI. "
               "<b>Ctrl+Z</b> / <b>Ctrl+Shift+Z</b> undo / redo.</p>"));
    });

    menuBar()->addMenu(tr("&Help"));
}

void MainWindow::rebindUiToProject(bool preloadMedia)
{
    if (preloadMedia) {
        showProjectLoadProgress(tr("Preparing project…"), 0);
    }

    Project* p = m_project.get();
    p->ensureSingleBankSet();
    p->resizeBanksForGrid(p->settings.matrix.gridRows, p->settings.matrix.gridCols);
    m_bankGrid->setProject(p);
    m_inspector->setProject(p);
    if (m_outputProcessingDialog) {
        m_outputProcessingDialog->setProject(p);
    }
    m_mediaDock->setProject(p);
    if (m_inputRouter) {
        m_inputRouter->setProject(p);
    }
    clearClipPeek();
    m_inspector->setSelection(m_bankGrid->activeBankSetIndex(),
                              m_bankGrid->activeBankIndex(),
                              -1);
    refreshPreviewForSelectedCell();
    syncMixSlotHighlightsToBankGrid();
    syncOutputFilterChainToMixers();
    if (preloadMedia) {
        showMediaPreloadHint();
        if (m_bankGrid) {
            m_bankGrid->preloadProjectMediaThumbnails();
            // If the batch finished synchronously, mediaPreloadFinished already
            // closed the dialog and reset counters to 0. Calling progress with
            // (0,0) would reopen an empty progress window that never closes.
            if (m_bankGrid->mediaPreloadTotal() > 0) {
                onMediaPreloadProgress(m_bankGrid->mediaPreloadCompleted(),
                                       m_bankGrid->mediaPreloadTotal());
            }
        }
    }
}

void MainWindow::showProjectLoadProgress(const QString& label, int percent)
{
    if (!m_mediaPreloadDialog) {
        m_mediaPreloadDialog = new QProgressDialog(this);
        m_mediaPreloadDialog->setWindowTitle(tr("Loading project"));
        m_mediaPreloadDialog->setWindowModality(Qt::ApplicationModal);
        m_mediaPreloadDialog->setMinimumDuration(0);
        m_mediaPreloadDialog->setAutoClose(false);
        m_mediaPreloadDialog->setAutoReset(false);
        m_mediaPreloadDialog->setCancelButton(nullptr);
        m_mediaPreloadDialog->setAttribute(Qt::WA_QuitOnClose, false);
        m_mediaPreloadDialog->setMinimumWidth(360);
        Qt::WindowFlags flags = Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint;
        flags &= ~Qt::WindowCloseButtonHint;
        flags &= ~Qt::WindowMaximizeButtonHint;
        flags &= ~Qt::WindowMinimizeButtonHint;
        flags &= ~Qt::WindowSystemMenuHint;
        m_mediaPreloadDialog->setWindowFlags(flags);
    }

    if (percent < 0) {
        m_mediaPreloadDialog->setRange(0, 0);
    } else {
        m_mediaPreloadDialog->setRange(0, 100);
        m_mediaPreloadDialog->setValue(qBound(0, percent, 100));
    }
    m_mediaPreloadDialog->setLabelText(label);
    if (!m_mediaPreloadDialog->isVisible()) {
        m_mediaPreloadDialog->show();
    }
    m_mediaPreloadDialog->raise();
    m_mediaPreloadDialog->activateWindow();
    // Force an immediate paint so the dialog does not stay blank/white until the
    // first thumbnail finishes (setValue(0) on an already-zero value is a no-op
    // and would not trigger a repaint on its own).
    m_mediaPreloadDialog->repaint();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void MainWindow::showMediaPreloadHint()
{
    showProjectLoadProgress(tr("Loading video previews… %1%").arg(0), 0);
}

void MainWindow::onMediaPreloadProgress(int completed, int total)
{
    if (total <= 0) {
        return;
    }
    const int percent = mediaPreloadPercent(completed, total);
    showProjectLoadProgress(tr("Loading video previews… %1%").arg(percent), percent);
}

void MainWindow::closeMediaPreloadHint()
{
    if (!m_mediaPreloadDialog) {
        return;
    }
    m_mediaPreloadDialog->hide();
    m_mediaPreloadDialog->reset();
}

void MainWindow::onMediaPreloadFinished()
{
    closeMediaPreloadHint();
    scheduleResolveMissingProjectMedia();
}

void MainWindow::promptOpenLastProjectIfNeeded()
{
    if (m_startupProjectPromptDone) {
        return;
    }
    m_startupProjectPromptDone = true;

    const QString path = loadLastProjectPathSetting();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        tr("Open project"),
        tr("Load the last opened project?\n\n%1").arg(path),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (answer == QMessageBox::Yes) {
        openProjectFromPath(path);
    }
}

namespace {

bool mediaFileExists(const QString& path)
{
    return !path.isEmpty() && QFileInfo::exists(path);
}

} // namespace

void MainWindow::clearCellsUsingMedia(const QUuid& mediaId)
{
    if (!m_project || mediaId.isNull()) {
        return;
    }
    using pvj::core::Cell;
    using pvj::core::GeneratorKind;
    using pvj::core::VisualType;

    for (int si = 0; si < m_project->bankSets.size(); ++si) {
        auto& set = m_project->bankSets[si];
        for (int bi = 0; bi < set.banks.size(); ++bi) {
            auto& bank = set.banks[bi];
            for (int ci = 0; ci < bank.cells.size(); ++ci) {
                Cell& c = bank.cells[ci];
                if (c.visual.type == VisualType::Media && c.visual.mediaId == mediaId) {
                    c.visual.type      = VisualType::Empty;
                    c.visual.mediaId   = {};
                    c.visual.generator = GeneratorKind::None;
                }
            }
        }
    }
}

MainWindow::MissingMediaAction MainWindow::promptMissingMediaFile(const QString& path)
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowModality(Qt::ApplicationModal);
    box.setWindowTitle(tr("Media file not found"));
    box.setText(tr("The media file could not be found:"));
    box.setInformativeText(path);
    auto* locateBtn    = box.addButton(tr("Locate…"), QMessageBox::AcceptRole);
    auto* skipBtn      = box.addButton(tr("Skip"), QMessageBox::ActionRole);
    auto* skipAllBtn   = box.addButton(tr("Skip all"), QMessageBox::ActionRole);
    auto* deleteBtn    = box.addButton(tr("Delete"), QMessageBox::DestructiveRole);
    auto* deleteAllBtn = box.addButton(tr("Delete all"), QMessageBox::DestructiveRole);
    box.setDefaultButton(locateBtn);
    box.exec();

    const QAbstractButton* clicked = box.clickedButton();
    if (clicked == locateBtn) {
        return MissingMediaAction::Locate;
    }
    if (clicked == skipAllBtn) {
        return MissingMediaAction::SkipAll;
    }
    if (clicked == deleteBtn) {
        return MissingMediaAction::Delete;
    }
    if (clicked == deleteAllBtn) {
        return MissingMediaAction::DeleteAll;
    }
    if (clicked == skipBtn) {
        return MissingMediaAction::Skip;
    }
    return MissingMediaAction::Skip;
}

bool MainWindow::tryLocateMissingMedia(const QUuid& mediaId, const QString& oldPath)
{
    if (!m_project || mediaId.isNull()) {
        return false;
    }
    pvj::core::MediaItem* item = m_project->findMedia(mediaId);
    if (!item) {
        return false;
    }

    const QString startDir = QFileInfo(oldPath).absolutePath();
    const QString filter =
        tr("Video files (*.mp4 *.mov *.avi *.mkv *.webm *.m4v *.mpg *.mpeg);;All files (*.*)");
    const QString chosen = QFileDialog::getOpenFileName(
        this,
        tr("Locate media file"),
        startDir.isEmpty() ? QDir::homePath() : startDir,
        filter);
    if (chosen.isEmpty() || !QFileInfo(chosen).isFile()) {
        return false;
    }

    item->path        = chosen;
    item->displayName = QFileInfo(chosen).fileName();
    if (m_mediaDock) {
        m_mediaDock->refreshProjectMedia();
    }
    if (m_bankGrid) {
        m_bankGrid->requestThumbnailForMedia(mediaId);
    }
    return true;
}

bool MainWindow::ensureMediaFileAvailable(const QUuid& mediaId, const QString& path)
{
    if (mediaFileExists(path)) {
        return true;
    }
    if (!m_project || mediaId.isNull()) {
        return false;
    }

    // Never open nested missing-media dialogs (e.g. while resolve is already prompting,
    // or while QMessageBox::exec() is pumping events and playback tries to open media).
    if (m_missingMediaPassActive || m_missingMediaDialogOpen) {
        return false;
    }

    if (m_skipAllMissingMedia) {
        return false;
    }
    if (m_deleteAllMissingMedia) {
        clearCellsUsingMedia(mediaId);
        if (m_bankGrid) {
            m_bankGrid->refreshCellLabels();
        }
        updateMixerFromPlayingCells();
        refreshPreviewForSelectedCell();
        return false;
    }

    m_missingMediaDialogOpen = true;
    const MissingMediaAction act = promptMissingMediaFile(path);
    m_missingMediaDialogOpen = false;

    switch (act) {
    case MissingMediaAction::Locate:
        if (tryLocateMissingMedia(mediaId, path)) {
            return true;
        }
        return false;
    case MissingMediaAction::Skip:
        return false;
    case MissingMediaAction::SkipAll:
        m_skipAllMissingMedia = true;
        return false;
    case MissingMediaAction::Delete:
        clearCellsUsingMedia(mediaId);
        break;
    case MissingMediaAction::DeleteAll:
        m_deleteAllMissingMedia = true;
        clearCellsUsingMedia(mediaId);
        break;
    }
    if (m_bankGrid) {
        m_bankGrid->refresh();
    }
    updateMixerFromPlayingCells();
    refreshPreviewForSelectedCell();
    markProjectDirty();
    return false;
}

void MainWindow::scheduleResolveMissingProjectMedia()
{
    if (m_missingMediaResolveScheduled) {
        return;
    }
    m_missingMediaResolveScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_missingMediaResolveScheduled = false;
        resolveMissingProjectMedia();
    });
}

void MainWindow::resolveMissingProjectMedia()
{
    if (!m_project) {
        return;
    }
    // Already walking the queue — don't start a parallel pass.
    if (m_missingMediaPassActive || m_missingMediaDialogOpen) {
        return;
    }

    m_skipAllMissingMedia = false;
    m_deleteAllMissingMedia = false;
    m_missingMediaCellsCleared = false;
    m_missingMediaUpdated = false;

    QSet<QUuid> missingIdSet;
    for (const pvj::core::MediaItem& item : m_project->mediaLibrary) {
        if (!item.id.isNull() && !mediaFileExists(item.path)) {
            missingIdSet.insert(item.id);
        }
    }

    using pvj::core::VisualType;
    for (const auto& set : m_project->bankSets) {
        for (const auto& bank : set.banks) {
            for (const auto& cell : bank.cells) {
                if (cell.visual.type != VisualType::Media || cell.visual.mediaId.isNull()) {
                    continue;
                }
                const pvj::core::MediaItem* m = m_project->findMedia(cell.visual.mediaId);
                if (!m || !mediaFileExists(m->path)) {
                    missingIdSet.insert(cell.visual.mediaId);
                }
            }
        }
    }

    m_missingMediaQueue = missingIdSet.values();
    std::sort(m_missingMediaQueue.begin(), m_missingMediaQueue.end(),
              [](const QUuid& a, const QUuid& b) { return a.toString() < b.toString(); });

    if (m_missingMediaQueue.isEmpty()) {
        return;
    }

    m_missingMediaPassActive = true;
    processNextMissingMedia();
}

void MainWindow::finishMissingMediaPass()
{
    m_missingMediaPassActive = false;
    m_missingMediaQueue.clear();

    if (!m_missingMediaCellsCleared && !m_missingMediaUpdated) {
        return;
    }
    if (m_bankGrid) {
        m_bankGrid->refresh();
    }
    if (m_mediaDock && m_missingMediaUpdated) {
        m_mediaDock->refreshProjectMedia();
    }
    if (m_missingMediaCellsCleared) {
        updateMixerFromPlayingCells();
        refreshPreviewForSelectedCell();
        markProjectDirty();
    }
}

void MainWindow::processNextMissingMedia()
{
    if (!m_project) {
        finishMissingMediaPass();
        return;
    }

    // Advance past entries that were fixed meanwhile or are no longer missing.
    while (!m_missingMediaQueue.isEmpty()) {
        const QUuid id = m_missingMediaQueue.first();
        const pvj::core::MediaItem* item = m_project->findMedia(id);
        if (item && mediaFileExists(item->path)) {
            m_missingMediaQueue.removeFirst();
            continue;
        }
        break;
    }

    if (m_missingMediaQueue.isEmpty() || m_skipAllMissingMedia) {
        finishMissingMediaPass();
        return;
    }

    if (m_deleteAllMissingMedia) {
        for (const QUuid& id : m_missingMediaQueue) {
            clearCellsUsingMedia(id);
            m_missingMediaCellsCleared = true;
        }
        m_missingMediaQueue.clear();
        finishMissingMediaPass();
        return;
    }

    if (m_missingMediaDialogOpen) {
        return;
    }

    const QUuid id = m_missingMediaQueue.takeFirst();
    const pvj::core::MediaItem* item = m_project->findMedia(id);
    const QString path = item ? item->path : QStringLiteral("(unknown)");

    m_missingMediaDialogOpen = true;
    const MissingMediaAction act = promptMissingMediaFile(path);
    m_missingMediaDialogOpen = false;

    switch (act) {
    case MissingMediaAction::Locate:
        if (tryLocateMissingMedia(id, path)) {
            m_missingMediaUpdated = true;
        }
        break;
    case MissingMediaAction::Skip:
        break;
    case MissingMediaAction::SkipAll:
        m_skipAllMissingMedia = true;
        m_missingMediaQueue.clear();
        break;
    case MissingMediaAction::Delete:
        clearCellsUsingMedia(id);
        m_missingMediaCellsCleared = true;
        break;
    case MissingMediaAction::DeleteAll:
        m_deleteAllMissingMedia = true;
        clearCellsUsingMedia(id);
        m_missingMediaCellsCleared = true;
        break;
    }

    // Continue on the next event-loop turn so nested processEvents cannot re-enter
    // this function while a dialog is still tearing down.
    QTimer::singleShot(0, this, &MainWindow::processNextMissingMedia);
}

void MainWindow::applyBankGridDimensions(int rows, int cols)
{
    if (!m_project || !m_bankGrid) {
        return;
    }
    m_project->resizeBanksForGrid(rows, cols);
    m_bankGrid->setGridDimensions(m_project->settings.matrix.gridRows, m_project->settings.matrix.gridCols);
    m_bankGrid->refresh();
    if (m_inspector) {
        m_inspector->setSelection(m_bankGrid->activeBankSetIndex(),
                                  m_bankGrid->activeBankIndex(),
                                  m_bankGrid->selectedCellIndex());
    }
    refreshPreviewForSelectedCell();
}

void MainWindow::applyStagePixelSize(QSize px)
{
    if (px.width() < 16)  px.setWidth(16);
    if (px.height() < 16) px.setHeight(16);
    m_stagePixelSize = px;
    if (m_previewB) {
        m_previewB->setStagePixelSize(px);
    }
    if (m_fullscreenOut && m_fullscreenOut->mixerWidget()) {
        m_fullscreenOut->mixerWidget()->setStagePixelSize(px);
    }
}

void MainWindow::onEditPreferences()
{
    PreferencesDialog dlg(this);
    dlg.setStagePixelSize(m_stagePixelSize.isValid() && !m_stagePixelSize.isEmpty()
                          ? m_stagePixelSize
                          : loadStagePixelSize());
    connect(&dlg, &PreferencesDialog::stagePixelSizeChanged, this, [this](QSize px) {
        applyStagePixelSize(px);
        saveStagePixelSize(px);
        if (statusBar()) {
            statusBar()->showMessage(
                tr("Stage resolution: %1×%2 px").arg(px.width()).arg(px.height()), 3000);
        }
    });
    syncMidiPreferencesDialog(dlg);
    connect(&dlg, &PreferencesDialog::midiDevicesRefreshRequested, this, [this, &dlg]() {
        syncMidiPreferencesDialog(dlg);
    });
    connect(&dlg, &PreferencesDialog::midiInputPortsChanged, this, [this, &dlg](const QStringList& names) {
        applyMidiInputPorts(names);
        syncMidiPreferencesDialog(dlg);
    });

    QMetaObject::Connection midiActivityConn;
    if (m_midiInput) {
        midiActivityConn = connect(
            m_midiInput.get(), &pvj::input::MidiInput::messageReceived, &dlg,
            [&dlg](const QByteArray& bytes) { dlg.reportMidiInputActivity(bytes); },
            Qt::QueuedConnection);
    }

    dlg.exec();

    if (midiActivityConn) {
        disconnect(midiActivityConn);
    }
}

bool MainWindow::applyMidiInputPorts(const QStringList& portNames)
{
    if (!m_midiInput) {
        return false;
    }
    if (portNames.isEmpty()) {
        m_midiInput->closeAllPorts();
        pvj::input::MidiInput::clearPreferredPorts();
        populateMidiPortMenu();
        showMidiInputStatus();
        return true;
    }
    if (m_midiInput->openPortsByNames(portNames)) {
        populateMidiPortMenu();
        showMidiInputStatus();
        return true;
    }
    populateMidiPortMenu();
    if (statusBar()) {
        statusBar()->showMessage(
            tr("Could not open MIDI port(s): %1").arg(portNames.join(QStringLiteral(", "))),
            8000);
    }
    showMidiInputStatus();
    return false;
}

void MainWindow::syncMidiPreferencesDialog(PreferencesDialog& dlg)
{
    if (!m_midiInput) {
        dlg.setMidiInputPorts({}, {}, {});
        return;
    }
    const QStringList ports    = m_midiInput->portNames();
    const QStringList open     = m_midiInput->openPortNames();
    QStringList       selected = pvj::input::MidiInput::savedPortNames();
    if (selected.isEmpty() && !open.isEmpty()) {
        selected = open;
    }
    dlg.setMidiInputPorts(ports, selected, open);
}

void MainWindow::saveViewSettings()
{
    QSettings settings;
    if (m_mainSplit) {
        settings.setValue(QLatin1String(kUiMainSplitterState), m_mainSplit->saveState());
    }
    if (m_upperSplit) {
        settings.setValue(QLatin1String(kUiUpperSplitterState), m_upperSplit->saveState());
    }
    if (m_inspector) {
        settings.setValue(QLatin1String(kUiParameterInspectorVisible), m_inspector->isVisible());
    }
    if (m_mediaDock) {
        settings.setValue(QLatin1String(kUiMediaLibraryVisible), !m_mediaDock->isHidden());
    }
}

void MainWindow::restoreViewSettings()
{
    QSettings settings;

    const bool inspectorVisible =
        settings.value(QLatin1String(kUiParameterInspectorVisible), true).toBool();
    if (m_actToggleInspector) {
        QSignalBlocker blocker(m_actToggleInspector);
        m_actToggleInspector->setChecked(inspectorVisible);
    }
    if (m_inspector) {
        m_inspector->setVisible(inspectorVisible);
    }

    const bool mediaVisible =
        settings.value(QLatin1String(kUiMediaLibraryVisible), true).toBool();
    if (m_mediaDock) {
        m_mediaDock->setVisible(mediaVisible);
    }

    if (m_mainSplit) {
        const QByteArray state = settings.value(QLatin1String(kUiMainSplitterState)).toByteArray();
        if (!state.isEmpty()) {
            m_mainSplit->restoreState(state);
        }
    }
    if (m_upperSplit) {
        const QByteArray state = settings.value(QLatin1String(kUiUpperSplitterState)).toByteArray();
        if (!state.isEmpty()) {
            m_upperSplit->restoreState(state);
        } else if (!inspectorVisible) {
            setParameterInspectorVisible(false);
        }
    }
}

void MainWindow::setParameterInspectorVisible(bool visible)
{
    if (!m_inspector) {
        return;
    }
    m_inspector->setVisible(visible);
    if (m_upperSplit) {
        // Keep preview pane usable when inspector is hidden.
        QList<int> sizes = m_upperSplit->sizes();
        if (sizes.size() >= 2) {
            if (!visible) {
                sizes[0] = 0;
                if (sizes[1] <= 0) {
                    sizes[1] = 400;
                }
            } else if (sizes[0] <= 0) {
                const int total = qMax(1, sizes[0] + sizes[1]);
                sizes[0] = total * 2 / 7;
                sizes[1] = total - sizes[0];
            }
            m_upperSplit->setSizes(sizes);
        }
    }
    if (m_actToggleInspector && m_actToggleInspector->isChecked() != visible) {
        QSignalBlocker blocker(m_actToggleInspector);
        m_actToggleInspector->setChecked(visible);
    }
}

void MainWindow::updateWindowTitle()
{
    QString title = QStringLiteral("PerformanieVJ");
    if (m_project && !m_project->filePath.isEmpty()) {
        title += QStringLiteral(" - ") + QFileInfo(m_project->filePath).fileName();
    } else {
        title += QStringLiteral(" - ") + tr("Untitled");
    }
    if (m_project && m_project->sourceFormat != QLatin1String("pvj")) {
        title += QStringLiteral(" [imported from ") + m_project->sourceFormat.toUpper() + QLatin1Char(']');
    }
    if (isProjectDirty()) {
        title += QLatin1Char('*');
    }
    if (m_actMidiMappingEdit && m_actMidiMappingEdit->isChecked()) {
        title += tr(" — Map");
    }
    setWindowTitle(title);
    setWindowModified(isProjectDirty());
}

void MainWindow::markProjectDirty()
{
    if (m_projectDirty) {
        return;
    }
    m_projectDirty = true;
    updateWindowTitle();
}

void MainWindow::clearProjectDirty()
{
    m_projectDirty = false;
    if (m_undoStack) {
        m_undoStack->setClean();
    }
    updateWindowTitle();
}

bool MainWindow::isProjectDirty() const
{
    if (m_projectDirty) {
        return true;
    }
    return m_undoStack && !m_undoStack->isClean();
}

bool MainWindow::maybeSave()
{
    if (!isProjectDirty()) {
        return true;
    }

    const QString name = (m_project && !m_project->filePath.isEmpty())
                             ? QFileInfo(m_project->filePath).fileName()
                             : tr("Untitled");
    const auto answer = QMessageBox::warning(
        this,
        tr("Unsaved changes"),
        tr("Save changes to \"%1\"?").arg(name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (answer == QMessageBox::Save) {
        return saveProject();
    }
    if (answer == QMessageBox::Cancel) {
        return false;
    }
    return true;
}

void MainWindow::rememberLastProjectPath()
{
    if (!m_project) {
        return;
    }
    if (m_project->filePath.isEmpty() || m_project->sourceFormat != QLatin1String("pvj")) {
        return;
    }
    saveLastProjectPathSetting(m_project->filePath);
}

void MainWindow::stopAllPlaybackAndClear()
{
    clearClipPeek();
    if (m_layerFadeTimer) {
        m_layerFadeTimer->stop();
    }
    m_layerFadeAnimating.fill(false);

    for (int i = 0; i < kMixLayers; ++i) {
        m_audioDecoders[i]->close();
        if (m_audioEngine) {
            m_audioEngine->setLayerActive(i, false);
        }
        m_decoders[i]->stop();
        m_previewB->clearFrame(i);
        m_lastFrames[static_cast<size_t>(i)] = QImage();
        if (m_fullscreenOut) {
            m_fullscreenOut->mixerWidget()->clearFrame(i);
        }
        m_layerSlots[i] = {};
        m_layerKeying[static_cast<size_t>(i)].valid = false;
    }
    m_inspectorEditLayer = -1;
    m_mixerSlotHadMedia.fill(false);
    m_activeMixerFilterCells.clear();
    m_previewA->clearFrame();
    updateMixerFromPlayingCells();
    syncOutputFilterChainToMixers();
    syncMixerFilterHighlightsToBankGrid();
}

// File operations -------------------------------------------------------------

void MainWindow::onFileNew()
{
    if (!maybeSave()) {
        return;
    }
    stopAllPlaybackAndClear();

    clearUndoStack();
    m_project = std::make_unique<Project>();
    m_project->initializeDefault();
    rebindUiToProject(/*preloadMedia=*/false);
    clearProjectDirty();
    updateWindowTitle();
    statusBar()->showMessage(tr("New project"), 3000);
}

void MainWindow::onFileOpen()
{
    if (!maybeSave()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Project"),
                                                      {}, QString::fromLatin1(kPvjFilter));
    if (path.isEmpty()) return;
    openProjectFromPath(path);
}

void MainWindow::openProjectFromPath(const QString& path)
{
    showProjectLoadProgress(tr("Opening project…"), -1);

    auto next = std::make_unique<Project>();
    auto res = PvjSerializer::load(*next, path);
    if (!res.ok) {
        closeMediaPreloadHint();
        QMessageBox::warning(this, tr("Open failed"), res.errorMessage);
        return;
    }
    stopAllPlaybackAndClear();
    if (m_bankGrid) {
        m_bankGrid->setProject(nullptr);
    }
    if (m_inspector) {
        m_inspector->setProject(nullptr);
    }
    if (m_mediaDock) {
        m_mediaDock->setProject(nullptr);
    }
    if (m_inputRouter) {
        m_inputRouter->setProject(nullptr);
    }

    clearUndoStack();
    m_project = std::move(next);
    rebindUiToProject();
    clearProjectDirty();
    updateWindowTitle();
    statusBar()->showMessage(tr("Opened %1").arg(path), 5000);
    rememberRecentProject(path);
    saveLastProjectPathSetting(path);
}

void MainWindow::rememberRecentProject(const QString& path)
{
    pushRecentProjectPath(path);
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    if (!m_recentFilesMenu) {
        return;
    }
    m_recentFilesMenu->clear();
    QStringList paths = loadRecentProjectPathsSetting();
    QStringList existing;
    for (const QString& p : paths) {
        if (QFileInfo::exists(p)) {
            existing.append(p);
        }
    }
    if (existing.size() != paths.size()) {
        saveRecentProjectPathsSetting(existing);
        paths = existing;
    }
    if (paths.isEmpty()) {
        auto* placeholder = m_recentFilesMenu->addAction(tr("(no recent projects)"));
        placeholder->setEnabled(false);
        return;
    }
    for (const QString& p : paths) {
        const QString label = QFileInfo(p).fileName();
        auto* act           = m_recentFilesMenu->addAction(label);
        act->setData(p);
        act->setStatusTip(p);
        act->setToolTip(p);
        connect(act, &QAction::triggered, this, &MainWindow::onRecentFileTriggered);
    }
}

void MainWindow::onRecentFileTriggered()
{
    auto* a = qobject_cast<QAction*>(sender());
    if (!a) {
        return;
    }
    const QString path = a->data().toString();
    const bool exists = !path.isEmpty() && QFileInfo::exists(path);
    if (exists) {
        if (!maybeSave()) {
            return;
        }
        openProjectFromPath(path);
    } else if (!path.isEmpty()) {
        QMessageBox::warning(this, tr("Open failed"),
                             tr("Project file not found:\n%1").arg(path));
    }
}

bool MainWindow::saveToPath(const QString& path)
{
    auto res = PvjSerializer::save(*m_project, path);
    if (!res.ok) {
        QMessageBox::warning(this, tr("Save failed"), res.errorMessage);
        return false;
    }
    m_project->filePath = path;
    m_project->sourceFormat = QStringLiteral("pvj");
    clearProjectDirty();
    updateWindowTitle();
    statusBar()->showMessage(tr("Saved %1").arg(path), 5000);
    rememberRecentProject(path);
    saveLastProjectPathSetting(path);
    return true;
}

bool MainWindow::saveProject()
{
    if (!m_project) {
        return false;
    }
    if (m_project->filePath.isEmpty() || m_project->sourceFormat != QLatin1String("pvj")) {
        return saveProjectAs();
    }
    return saveToPath(m_project->filePath);
}

bool MainWindow::saveProjectAs()
{
    if (!m_project) {
        return false;
    }
    QString suggested = m_project->filePath;
    if (suggested.isEmpty()) {
        suggested = QStringLiteral("Untitled.pvj");
    } else if (!suggested.endsWith(QLatin1String(".pvj"), Qt::CaseInsensitive)) {
        QFileInfo fi(suggested);
        suggested = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".pvj");
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Project As"),
                                                      suggested, QString::fromLatin1(kPvjFilter));
    if (path.isEmpty()) {
        return false;
    }
    return saveToPath(path);
}

void MainWindow::onFileSave()
{
    saveProject();
}

void MainWindow::onFileSaveAs()
{
    saveProjectAs();
}

void MainWindow::onConfigureBankGrid()
{
    if (!m_project) {
        return;
    }
    const int currentRows = qBound(1, m_project->settings.matrix.gridRows, 16);
    const int currentCols = qBound(1, m_project->settings.matrix.gridCols, 16);

    bool ok = false;
    const int rows = QInputDialog::getInt(this,
                                          tr("Bank grid"),
                                          tr("Rows per bank:"),
                                          currentRows,
                                          1,
                                          16,
                                          1,
                                          &ok);
    if (!ok) {
        return;
    }
    const int cols = QInputDialog::getInt(this,
                                          tr("Bank grid"),
                                          tr("Columns per bank:"),
                                          currentCols,
                                          1,
                                          16,
                                          1,
                                          &ok);
    if (!ok) {
        return;
    }
    if (rows == currentRows && cols == currentCols) {
        return;
    }

    const int nextCellCount = rows * cols;
    const int oldCellCount = currentRows * currentCols;
    if (nextCellCount < oldCellCount) {
        const auto answer = QMessageBox::question(
            this,
            tr("Reduce bank grid?"),
            tr("Reducing the grid from %1 to %2 cells per bank removes cells beyond the new size.\n\nContinue?")
                .arg(oldCellCount)
                .arg(nextCellCount));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    const Project before = *m_project;
    applyBankGridDimensions(rows, cols);
    if (m_undoStack) {
        m_undoStack->push(new ProjectReplaceCommand(m_project.get(), before, *m_project,
                                                    tr("Resize bank grid")));
    }
    statusBar()->showMessage(tr("Bank grid set to %1 x %2").arg(rows).arg(cols), 4000);
}

void MainWindow::onImportVj2()
{
    if (!maybeSave()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Import GrandVJ Project"),
                                                      {}, QString::fromLatin1(kVj2Filter));
    if (path.isEmpty()) return;

    showProjectLoadProgress(tr("Importing GrandVJ project…"), -1);

    auto next = std::make_unique<Project>();
    auto res = Vj2Importer::importFile(*next, path);
    if (!res.ok) {
        closeMediaPreloadHint();
        QMessageBox::warning(this, tr("Import failed"), res.errorMessage);
        return;
    }
    stopAllPlaybackAndClear();

    clearUndoStack();
    m_project = std::move(next);
    rebindUiToProject();
    applyBankGridDimensions(m_project->settings.matrix.gridRows, m_project->settings.matrix.gridCols);
    markProjectDirty();
    updateWindowTitle();
    if (!res.warnings.isEmpty()) {
        QMessageBox::information(this, tr("Import warnings"), res.warnings.join(QLatin1Char('\n')));
    }
    statusBar()->showMessage(
        tr("Imported %1 — bank grid %2×%3 (save as .pvj to keep changes)")
            .arg(path)
            .arg(m_project->settings.matrix.gridRows)
            .arg(m_project->settings.matrix.gridCols),
        8000);
}

void MainWindow::onImportVj2IntoBank()
{
    if (!m_project) {
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this, tr("Import GrandVJ into bank"),
                                                      {}, QString::fromLatin1(kVj2Filter));
    if (path.isEmpty()) {
        return;
    }

    m_project->ensureSingleBankSet();
    const int bankCount = m_project->bankSets.isEmpty() ? 64 : m_project->bankSets[0].banks.size();
    const int currentBank1Based =
        m_bankGrid ? qBound(1, m_bankGrid->activeBankIndex() + 1, qMax(1, bankCount)) : 1;

    bool ok = false;
    const int destBank1 = QInputDialog::getInt(
        this,
        tr("Import into bank"),
        tr("Destination start bank (existing banks from this index are overwritten):"),
        currentBank1Based,
        1,
        256,
        1,
        &ok);
    if (!ok) {
        return;
    }

    const int sourceBank1 = QInputDialog::getInt(
        this,
        tr("Import from bank"),
        tr("Source start bank in the GrandVJ file (1 = first bank):"),
        1,
        1,
        256,
        1,
        &ok);
    if (!ok) {
        return;
    }

    showProjectLoadProgress(tr("Importing GrandVJ banks…"), -1);

    const Project before = *m_project;
    auto res = Vj2Importer::mergeFile(*m_project, path, destBank1 - 1, sourceBank1 - 1);
    if (!res.ok) {
        closeMediaPreloadHint();
        QMessageBox::warning(this, tr("Import failed"), res.errorMessage);
        return;
    }

    if (m_undoStack) {
        m_undoStack->push(new ProjectReplaceCommand(m_project.get(), before, *m_project,
                                                    tr("Import GrandVJ into bank")));
    }

    stopAllPlaybackAndClear();
    rebindUiToProject();
    applyBankGridDimensions(m_project->settings.matrix.gridRows, m_project->settings.matrix.gridCols);
    if (m_bankGrid) {
        m_bankGrid->selectBank(destBank1 - 1);
    }
    updateWindowTitle();
    if (!res.warnings.isEmpty()) {
        QMessageBox::information(this, tr("Import warnings"), res.warnings.join(QLatin1Char('\n')));
    }
    statusBar()->showMessage(
        tr("Merged %1 banks from %2 into bank %3 (save as .pvj to keep changes)")
            .arg(res.banksMerged)
            .arg(path)
            .arg(destBank1),
        8000);
}

void MainWindow::onImportAvc()
{
    if (!maybeSave()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Import Resolume Composition"),
                                                      {}, QString::fromLatin1(kAvcFilter));
    if (path.isEmpty()) return;

    showProjectLoadProgress(tr("Importing Resolume composition…"), -1);

    auto next = std::make_unique<Project>();
    auto res = AvcImporter::importFile(*next, path);
    if (!res.ok) {
        closeMediaPreloadHint();
        QMessageBox::warning(this, tr("Import failed"), res.errorMessage);
        return;
    }
    stopAllPlaybackAndClear();

    clearUndoStack();
    m_project = std::move(next);
    rebindUiToProject();
    markProjectDirty();
    updateWindowTitle();
    if (!res.warnings.isEmpty()) {
        QMessageBox::information(this, tr("Import warnings"),
                                 tr("Only clip references and column/layer layout were imported.\n\n")
                                 + res.warnings.join(QLatin1Char('\n')));
    }
    statusBar()->showMessage(tr("Imported %1 (save as .pvj to keep changes)").arg(path), 8000);
}

// Selection / triggering ------------------------------------------------------

void MainWindow::onCellSelected(int bankSetIndex, int bankIndex, int cellIndex)
{
    // Left-click elsewhere clears peek; selecting the same cell as the peek target does not.
    if (!cellMatchesPeekSelection(bankSetIndex, bankIndex, cellIndex)) {
        clearClipPeek();
    }
    syncInspectorEditLayerForCell(bankSetIndex, bankIndex, cellIndex);
    syncInspectorLayerKeyingOverride();
    if (m_inspector) {
        m_inspector->setSelection(bankSetIndex, bankIndex, cellIndex);
    }
    m_inspectorEditBefore = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
    refreshPreviewForSelectedCell();
}

bool MainWindow::cellMatchesPeekSelection(int bankSetIndex, int bankIndex, int cellIndex) const
{
    if (m_clipPeekLayer < 0 || m_clipPeekLayer >= kMixLayers) {
        return false;
    }
    const DeckSlot& d = m_layerSlots[static_cast<size_t>(m_clipPeekLayer)];
    return d.bankSet == bankSetIndex && d.bank == bankIndex && d.cell == cellIndex;
}

void MainWindow::onCellEditRequested(int bankSetIndex, int bankIndex, int cellIndex)
{
    if (!m_filterEditor || !m_project) {
        return;
    }
    if (bankSetIndex < 0 || bankIndex < 0 || cellIndex < 0) {
        return;
    }
    if (bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    auto& set = m_project->bankSets[bankSetIndex];
    if (bankIndex >= set.banks.size()) {
        return;
    }
    auto& bank = set.banks[bankIndex];
    if (cellIndex >= bank.cells.size()) {
        return;
    }
    m_filterEditBefore = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
    m_filterEditor->openForCell(bankSetIndex, bankIndex, cellIndex,
                                &bank.cells[cellIndex], m_project.get());
}

void MainWindow::onCellTriggered(int bankSetIndex, int bankIndex, int cellIndex, bool toggleIfPlaying)
{
    if (bankSetIndex < 0 || bankIndex < 0 || cellIndex < 0) return;
    if (bankSetIndex >= m_project->bankSets.size()) return;
    auto& set  = m_project->bankSets[bankSetIndex];
    if (bankIndex >= set.banks.size()) return;
    auto& bank = set.banks[bankIndex];
    if (cellIndex >= bank.cells.size()) return;

    const auto& cell = bank.cells[cellIndex];

    if (cellIsMixerFilterType(cell)) {
        toggleMixerFilterCell(bankSetIndex, bankIndex, cellIndex);
        refreshPreviewForSelectedCell();
        return;
    }

    if (!cellIsPlayable(cell)) {
        const int layer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
        if (layer < 0) {
            refreshPreviewForSelectedCell();
            return;
        }
        stopMixLayer(layer);
        syncMixLayerFromCell(layer);
        syncMixerToFullscreen();
        syncMixSlotHighlightsToBankGrid();
        refreshPreviewForSelectedCell();
        return;
    }

    // Left-click / keyboard toggle: same cell already on the mixer → stop.
    if (toggleIfPlaying) {
        const int playingLayer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
        if (playingLayer >= 0) {
            stopMixLayer(playingLayer);
            syncMixLayerFromCell(playingLayer);
            syncMixerToFullscreen();
            syncMixSlotHighlightsToBankGrid();
            refreshPreviewForSelectedCell();
            return;
        }
    }

    const int layer = pickMixSlotForTrigger(bankSetIndex, bankIndex, cellIndex);
    startCellOnMixLayer(layer, bankSetIndex, bankIndex, cellIndex);
    syncMixerToFullscreen();
    syncMixSlotHighlightsToBankGrid();
    refreshPreviewForSelectedCell();
}

void MainWindow::stopMixLayer(int layer)
{
    if (layer < kUserLayerMin || layer >= kMixLayers) {
        return;
    }
    m_audioDecoders[layer]->close();
    if (m_audioEngine) {
        m_audioEngine->setLayerActive(layer, false);
    }
    m_decoders[layer]->stop();
    m_previewB->clearFrame(layer);
    m_previewB->setLayerFeedback(layer, false, {});
    m_lastFrames[static_cast<size_t>(layer)] = QImage();
    if (m_fullscreenOut && m_fullscreenOut->mixerWidget()) {
        m_fullscreenOut->mixerWidget()->clearFrame(layer);
    }
    if (m_clipPeekLayer == layer) {
        clearClipPeek();
    }
    m_layerSlots[layer] = {};
    m_layerKeying[static_cast<size_t>(layer)].valid = false;
    m_mixerSlotHadMedia[static_cast<size_t>(layer)] = false;
    m_layerFadeAnimating[static_cast<size_t>(layer)] = false;
}

bool MainWindow::cellIsPlayable(const Cell& cell)
{
    const bool isFeedbackCell = cell.visual.type == VisualType::Generator
        && cell.visual.generator == GeneratorKind::InternalFeedback;
    const bool isPlayableMedia = cell.visual.type == VisualType::Media
        && !cell.visual.mediaId.isNull();
    return isFeedbackCell || isPlayableMedia;
}

bool MainWindow::cellIsMixerFilterType(const Cell& cell)
{
    return cell.visual.type == VisualType::MixerFilter;
}

bool MainWindow::isMixerFilterCellActive(int bankSetIndex, int bankIndex, int cellIndex) const
{
    for (const DeckSlot& slot : m_activeMixerFilterCells) {
        if (slot.bankSet == bankSetIndex && slot.bank == bankIndex && slot.cell == cellIndex) {
            return true;
        }
    }
    return false;
}

void MainWindow::pruneActiveMixerFilterCells()
{
    bool changed = false;
    for (int i = m_activeMixerFilterCells.size() - 1; i >= 0; --i) {
        const Cell* c = cellAtDeck(m_activeMixerFilterCells[i]);
        if (!c || !cellIsMixerFilter(*c)) {
            m_activeMixerFilterCells.removeAt(i);
            changed = true;
        }
    }
    if (changed) {
        syncOutputFilterChainToMixers();
        syncMixerToFullscreen();
        syncMixerFilterHighlightsToBankGrid();
    }
}

void MainWindow::toggleMixerFilterCell(int bankSetIndex, int bankIndex, int cellIndex)
{
    const Cell* cell = cellAtDeck({bankSetIndex, bankIndex, cellIndex});
    if (!cell || !cellIsMixerFilterType(*cell)) {
        return;
    }

    for (int i = 0; i < m_activeMixerFilterCells.size(); ++i) {
        const DeckSlot& slot = m_activeMixerFilterCells[i];
        if (slot.bankSet == bankSetIndex && slot.bank == bankIndex && slot.cell == cellIndex) {
            m_activeMixerFilterCells.removeAt(i);
            syncOutputFilterChainToMixers();
            syncMixerToFullscreen();
            syncMixerFilterHighlightsToBankGrid();
            statusBar()->showMessage(tr("Mixer filter off (cell %1)").arg(cellIndex + 1), 2500);
            return;
        }
    }

    if (!cellIsMixerFilter(*cell)) {
        statusBar()->showMessage(tr("Choose a filter for this mixer-filter cell first (double-click to edit)"),
                                4000);
        return;
    }

    m_activeMixerFilterCells.append({bankSetIndex, bankIndex, cellIndex});
    syncOutputFilterChainToMixers();
    syncMixerToFullscreen();
    syncMixerFilterHighlightsToBankGrid();
    statusBar()->showMessage(tr("Mixer filter on (cell %1)").arg(cellIndex + 1), 2500);
}

QList<pvj::core::CellFilterNode> MainWindow::buildMergedOutputFilterChain() const
{
    if (!m_project) {
        return {};
    }
    QList<QList<pvj::core::CellFilterNode>> activeChains;
    activeChains.reserve(m_activeMixerFilterCells.size());
    for (const DeckSlot& slot : m_activeMixerFilterCells) {
        const Cell* c = cellAtDeck(slot);
        if (c && !c->filterChain.isEmpty()) {
            activeChains.append(c->filterChain);
        }
    }
    return pvj::core::buildEffectiveOutputFilterChain(m_project->settings.output.filterChain,
                                                      activeChains);
}

bool MainWindow::startCellOnMixLayer(int layer, int bankSetIndex, int bankIndex, int cellIndex)
{
    if (layer < kUserLayerMin || layer >= kMixLayers || !m_project) {
        return false;
    }
    const Cell* cell = cellAtDeck({bankSetIndex, bankIndex, cellIndex});
    if (!cell || !cellIsPlayable(*cell)) {
        return false;
    }

    const DeckSlot& existing = m_layerSlots[static_cast<size_t>(layer)];
    if (existing.bankSet >= 0
        && (existing.bankSet != bankSetIndex || existing.bank != bankIndex
            || existing.cell != cellIndex)) {
        stopMixLayer(layer);
    }

    const bool isFeedbackCell = cell->visual.type == VisualType::Generator
        && cell->visual.generator == GeneratorKind::InternalFeedback;

    if (isFeedbackCell) {
        m_layerSlots[layer] = {bankSetIndex, bankIndex, cellIndex};
        snapshotLayerKeyingFromCell(layer, cell);
        m_inspectorEditLayer = layer;
        syncInspectorLayerKeyingOverride();
        m_previewB->clearFrame(layer);
        m_lastFrames[static_cast<size_t>(layer)] = QImage();
        if (m_fullscreenOut && m_fullscreenOut->mixerWidget()) {
            m_fullscreenOut->mixerWidget()->clearFrame(layer);
        }
        m_layerFadeAnimating[static_cast<size_t>(layer)] = false;
        reapplyCurrentMidiControllersToPlayingCell(bankSetIndex, bankIndex, cellIndex);
        syncMixLayerFromCell(layer);
        return true;
    }

    for (const auto& m : m_project->mediaLibrary) {
        if (m.id != cell->visual.mediaId) {
            continue;
        }
        m_layerSlots[layer] = {bankSetIndex, bankIndex, cellIndex};
        snapshotLayerKeyingFromCell(layer, cell);
        m_inspectorEditLayer = layer;
        syncInspectorLayerKeyingOverride();
        playMediaOnLayer(layer, m.path);
        m_layerFadeAnimating[static_cast<size_t>(layer)] = false;
        reapplyCurrentMidiControllersToPlayingCell(bankSetIndex, bankIndex, cellIndex);
        syncMixLayerFromCell(layer);
        return true;
    }
    return false;
}

void MainWindow::reapplyCurrentMidiControllersToPlayingCell(int bankSetIndex, int bankIndex,
                                                            int cellIndex)
{
    if (!m_inputRouter || !m_project) {
        return;
    }
    if (findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex) < 0) {
        return;
    }
    if (bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    const auto& set = m_project->bankSets[bankSetIndex];
    if (bankIndex < 0 || bankIndex >= set.banks.size()) {
        return;
    }
    const auto& bank = set.banks[bankIndex];
    if (cellIndex < 0 || cellIndex >= bank.cells.size()) {
        return;
    }
    const Cell& cell = bank.cells[cellIndex];
    for (const auto& pm : cell.propertyMappings) {
        if (pm.input != pvj::core::InputType::MidiCC
            && pm.input != pvj::core::InputType::MidiAftertouch) {
            continue;
        }
        const std::optional<int> raw =
            m_inputRouter->lastContinuousValue(pm.input, pm.channel, pm.number);
        if (!raw.has_value()) {
            continue;
        }
        QString prop;
        double value = 0.0;
        if (!m_inputRouter->scaleContinuousMapping(pm, *raw, &prop, &value)) {
            continue;
        }
        onInputPropertyMapped(bankSetIndex, bankIndex, cellIndex, prop, value);
    }
}

void MainWindow::reapplyPlayingCell(int bankSetIndex, int bankIndex, int cellIndex)
{
    const int currentLayer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
    if (currentLayer < 0) {
        updateMixerFromPlayingCells();
        return;
    }

    const Cell* cell = cellAtDeck({bankSetIndex, bankIndex, cellIndex});
    if (!cell || !cellIsPlayable(*cell)) {
        stopMixLayer(currentLayer);
        updateMixerFromPlayingCells();
        syncMixerToFullscreen();
        syncMixSlotHighlightsToBankGrid();
        refreshPreviewForSelectedCell();
        return;
    }

    const int targetLayer = gpuLayerFromPreferred(preferredLayerFromCell(cell));
    stopMixLayer(currentLayer);
    startCellOnMixLayer(targetLayer, bankSetIndex, bankIndex, cellIndex);
    syncMixerToFullscreen();
    syncMixSlotHighlightsToBankGrid();
    refreshPreviewForSelectedCell();
}

void MainWindow::onCellPlaybackChanged(int bankSetIndex, int bankIndex, int cellIndex)
{
    reapplyPlayingCell(bankSetIndex, bankIndex, cellIndex);
}

void MainWindow::playMediaOnLayer(int layer, const QString& path)
{
    if (layer < kUserLayerMin || layer >= kMixLayers) return;
    if (path.isEmpty()) return;

    const Cell* c = cellAtDeck(m_layerSlots[static_cast<size_t>(layer)]);
    QString mediaPath = path;
    if (!mediaFileExists(mediaPath) && c && !c->visual.mediaId.isNull()) {
        if (!ensureMediaFileAvailable(c->visual.mediaId, mediaPath)) {
            return;
        }
        if (const pvj::core::MediaItem* m = m_project->findMedia(c->visual.mediaId)) {
            mediaPath = m->path;
        }
    }
    if (!mediaFileExists(mediaPath)) {
        return;
    }

    m_audioDecoders[layer]->close();
    if (m_audioEngine) {
        m_audioEngine->setLayerActive(layer, false);
    }

    if (!m_decoders[layer]->open(mediaPath)) {
        statusBar()->showMessage(tr("Cannot open %1").arg(mediaPath), 5000);
        return;
    }
    const double speed = qBound(0.0, c ? c->props.movieSpeed : 1.0, 4.0);
    const bool loop    = c ? playModeUsesLoop(c->props.playMode) : true;

    m_decoders[layer]->setLooping(loop);
    m_decoders[layer]->setPlaybackSpeed(speed);

    const qint64 dur = m_decoders[layer]->durationMs();
    if (dur > 0 && c) {
        const double a = qBound(0.0, c->props.segmentInU, 1.0);
        const double b = qBound(0.0, c->props.segmentOutU, 1.0);
        if (b > a + 1e-6) {
            const qint64 t = qint64(a * double(dur - 1));
            m_decoders[layer]->seek(t);
        }
    }

    if (c && c->props.clipPaused) {
        m_decoders[layer]->pause();
    } else {
        m_decoders[layer]->play();
    }

    if (m_audioDecoders[layer]->open(path)) {
        m_audioDecoders[layer]->setLooping(loop);
        m_audioDecoders[layer]->setPlaybackSpeed(speed);
        if (dur > 0 && c) {
            const double a = qBound(0.0, c->props.segmentInU, 1.0);
            const double b = qBound(0.0, c->props.segmentOutU, 1.0);
            if (b > a + 1e-6) {
                const qint64 t = qint64(a * double(dur - 1));
                m_audioDecoders[layer]->seek(t);
            }
        }
        if (c && c->props.clipPaused) {
            m_audioDecoders[layer]->pause();
        } else {
            m_audioDecoders[layer]->play();
        }
        if (m_audioEngine) {
            m_audioEngine->setLayerGain(layer, effectiveLayerAudioGain(c));
            m_audioEngine->setLayerActive(layer, true);
        }
    } else if (m_audioEngine) {
        m_audioEngine->setLayerActive(layer, false);
    }

    if (m_clipPeekLayer == layer && m_previewA) {
        m_previewA->setLabel(QFileInfo(path).fileName());
    }
}

const Cell* MainWindow::cellAtDeck(const DeckSlot& slot) const
{
    if (!m_project) return nullptr;
    if (slot.bankSet < 0 || slot.bankSet >= m_project->bankSets.size()) return nullptr;
    const auto& set = m_project->bankSets[slot.bankSet];
    if (slot.bank < 0 || slot.bank >= set.banks.size()) return nullptr;
    const auto& bank = set.banks[slot.bank];
    if (slot.cell < 0 || slot.cell >= bank.cells.size()) return nullptr;
    return &bank.cells[slot.cell];
}

void MainWindow::snapshotLayerKeyingFromCell(int layer, const Cell* cell)
{
    if (layer < 0 || layer >= kMixLayers) {
        return;
    }
    LayerKeyingState& s = m_layerKeying[static_cast<size_t>(layer)];
    if (!cell) {
        s.valid = false;
        return;
    }
    s.valid = true;
    s.keyingEnabled = cell->props.keyingEnabled;
    s.keyingMode = cell->props.keyingMode;
    s.keyThreshold = cell->props.keyThreshold;
    s.keySoftness = cell->props.keySoftness;
    s.keyLumaCenter = cell->props.keyLumaCenter;
    s.keyLumaInvert = cell->props.keyLumaInvert;
    s.keyChromaHue = cell->props.keyChromaHue;
    s.keyChromaInvert = cell->props.keyChromaInvert;
    s.keyChannelR = cell->props.keyChannelR;
    s.keyChannelG = cell->props.keyChannelG;
    s.keyChannelB = cell->props.keyChannelB;
    s.maskType = cell->props.maskType;
    s.maskFeather = cell->props.maskFeather;
    s.maskRectWidth = cell->props.maskRectWidth;
    s.maskRectHeight = cell->props.maskRectHeight;
    s.maskRadius = cell->props.maskRadius;
    s.maskEllipseX = cell->props.maskEllipseX;
    s.maskEllipseY = cell->props.maskEllipseY;
}

void MainWindow::applyLayerKeyingStateToCell(Cell& cell, const LayerKeyingState& state)
{
    if (!state.valid) {
        return;
    }
    pvj::core::CellProps& p = cell.props;
    p.keyingEnabled  = state.keyingEnabled;
    p.keyingMode     = state.keyingMode;
    p.keyThreshold   = state.keyThreshold;
    p.keySoftness    = state.keySoftness;
    p.keyLumaCenter  = state.keyLumaCenter;
    p.keyLumaInvert  = state.keyLumaInvert;
    p.keyChromaHue   = state.keyChromaHue;
    p.keyChromaInvert = state.keyChromaInvert;
    p.keyChannelR    = state.keyChannelR;
    p.keyChannelG    = state.keyChannelG;
    p.keyChannelB    = state.keyChannelB;
    p.maskType       = state.maskType;
    p.maskFeather    = state.maskFeather;
    p.maskRectWidth  = state.maskRectWidth;
    p.maskRectHeight = state.maskRectHeight;
    p.maskRadius     = state.maskRadius;
    p.maskEllipseX   = state.maskEllipseX;
    p.maskEllipseY   = state.maskEllipseY;
}

void MainWindow::syncInspectorLayerKeyingOverride()
{
    if (!m_inspector) {
        return;
    }
    const LayerKeyingState* overridePtr = nullptr;
    if (m_inspectorEditLayer >= kUserLayerMin && m_inspectorEditLayer < kMixLayers) {
        const DeckSlot& slot = m_layerSlots[static_cast<size_t>(m_inspectorEditLayer)];
        const bool selectionMatches = slot.bankSet >= 0
            && slot.bankSet == m_inspector->selectedBankSetIndex()
            && slot.bank == m_inspector->selectedBankIndex()
            && slot.cell == m_inspector->selectedCellIndex();
        if (selectionMatches) {
            const LayerKeyingState& state = m_layerKeying[static_cast<size_t>(m_inspectorEditLayer)];
            if (state.valid) {
                overridePtr = &state;
            }
        }
    }
    m_inspector->setLayerKeyingOverride(overridePtr);
}

void MainWindow::syncLiveMixerOpacityForCell(int bankSetIndex, int bankIndex, int cellIndex)
{
    if (!m_previewB) {
        return;
    }
    const int layer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
    if (layer < kUserLayerMin || layer >= kMixLayers) {
        return;
    }
    const Cell* c = cellAtDeck(m_layerSlots[static_cast<size_t>(layer)]);
    if (!c) {
        return;
    }
    const bool isFeedbackCell = c->visual.type == VisualType::Generator
        && c->visual.generator == GeneratorKind::InternalFeedback;

    if (!m_layerFadeAnimating[static_cast<size_t>(layer)]) {
        const float opacity = float(c->props.transparency);
        m_previewB->setLayerOpacity(layer, opacity, false);
        if (m_fullscreenOut && m_fullscreenOut->isVisible()) {
            if (pvj::render::RhiMixerWidget* dst = m_fullscreenOut->mixerWidget()) {
                dst->setLayerOpacity(layer, opacity, false);
            }
        }
    }

    // Feedback loops advance only when the mixer repaints; video layers get that from
    // frameReady, but feedback does not. Repaint once (ring steps every render frame).
    if (isFeedbackCell && m_previewB->layerFeedbackEnabled(layer)) {
        m_previewB->update();
        if (m_fullscreenOut && m_fullscreenOut->isVisible()) {
            if (pvj::render::RhiMixerWidget* dst = m_fullscreenOut->mixerWidget()) {
                dst->update();
            }
        }
    }
}

void MainWindow::scheduleMixerUpdateFromCells()
{
    if (!m_mixerUpdateDebounceTimer) {
        updateMixerFromPlayingCells();
        syncMixerToFullscreen();
        return;
    }
    // Leading-edge throttle: previously this used QTimer::start() on every call, which
    // restarts the singleshot countdown. With slider events or MIDI faders arriving
    // faster than the 16 ms interval (common on Windows where mouse moves can be ≥125 Hz),
    // the timer never fired during a drag and the mixer only updated once the user
    // released the slider — making transparency and other parameter sliders feel jerky,
    // especially with filters applied where each frame is heavier.
    //
    // Now: the first call applies the update immediately and starts the cooldown.
    // Subsequent calls during the cooldown only set the pending flag and the timer's
    // timeout slot applies the most recent model state when the window closes.
    if (m_mixerUpdateDebounceTimer->isActive()) {
        m_mixerUpdatePending = true;
        return;
    }
    m_mixerUpdatePending = false;
    updateMixerFromPlayingCells();
    syncMixerToFullscreen();
    m_mixerUpdateDebounceTimer->start();
}

void MainWindow::syncFilterParamsToMixer(int bankSetIndex, int bankIndex, int cellIndex)
{
    const int layer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
    if (layer < kUserLayerMin || layer >= kMixLayers || !m_previewB) {
        return;
    }
    const Cell* c = cellAtDeck(m_layerSlots[static_cast<size_t>(layer)]);
    if (!c) {
        return;
    }
    const LayerKeyingState& layerKeying = m_layerKeying[static_cast<size_t>(layer)];
    const LayerKeyingState* keyingPtr = layerKeying.valid ? &layerKeying : nullptr;
    const QList<pvj::core::CellFilterNode> chain = effectiveFilterChainForMixer(c, keyingPtr);
    m_previewB->setLayerFilterChain(layer, chain);

    if (m_fullscreenOut && m_fullscreenOut->isVisible()) {
        if (pvj::render::RhiMixerWidget* dst = m_fullscreenOut->mixerWidget()) {
            dst->setLayerFilterChain(layer, chain);
        }
    }
}

void MainWindow::schedulePlayingFeedbackCellMixerSync(int bankSetIndex, int bankIndex,
                                                      int cellIndex)
{
    m_pendingFeedbackSyncBankSet = bankSetIndex;
    m_pendingFeedbackSyncBank = bankIndex;
    m_pendingFeedbackSyncCell = cellIndex;
    if (m_feedbackMixerSyncTimer) {
        m_feedbackMixerSyncTimer->start();
    }
}

void MainWindow::syncPlayingFeedbackCellAfterPropertyChange(int bankSetIndex, int bankIndex,
                                                            int cellIndex,
                                                            const QString& propertyName)
{
    const QString resolved = pvj::core::PropertyRegistry::resolvePropertyId(propertyName);
    if (resolved.compare(QStringLiteral("transparency"), Qt::CaseInsensitive) == 0) {
        syncLiveMixerOpacityForCell(bankSetIndex, bankIndex, cellIndex);
        return;
    }
    if (pvj::core::PropertyRegistry::isFilterParamProperty(resolved)) {
        syncFilterParamsToMixer(bankSetIndex, bankIndex, cellIndex);
    }
    syncPlayingFeedbackCellToMixer(bankSetIndex, bankIndex, cellIndex);
}

void MainWindow::syncPlayingFeedbackCellToMixer(int bankSetIndex, int bankIndex, int cellIndex)
{
    const int layer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
    if (layer < kUserLayerMin || layer >= kMixLayers || !m_previewB) {
        return;
    }
    const Cell* c = cellAtDeck(m_layerSlots[static_cast<size_t>(layer)]);
    if (!c || c->visual.type != VisualType::Generator
        || c->visual.generator != GeneratorKind::InternalFeedback) {
        return;
    }
    const LayerKeyingState& layerKeying = m_layerKeying[static_cast<size_t>(layer)];
    const LayerKeyingState* keyingPtr = layerKeying.valid ? &layerKeying : nullptr;
    const QList<pvj::core::CellFilterNode> chain = effectiveFilterChainForMixer(c, keyingPtr);
    const double kr = keyingPtr ? keyingPtr->keyChannelR : c->props.keyChannelR;
    const double kg = keyingPtr ? keyingPtr->keyChannelG : c->props.keyChannelG;
    const double kb = keyingPtr ? keyingPtr->keyChannelB : c->props.keyChannelB;
    const float opacity = float(c->props.transparency);
    const bool fadeAnimating = m_layerFadeAnimating[static_cast<size_t>(layer)];

    const auto apply = [&](pvj::render::RhiMixerWidget* mix) {
        if (!mix) {
            return;
        }
        mix->setLayerActive(layer, true);
        mix->setLayerCopyMode(layer, c->props.copyMode);
        mix->setLayerMatteRole(layer, c->props.matteRole);
        mix->setLayerPicture(layer, pictureParamsForMixer(*c));
        mix->setLayerFilterChain(layer, chain);
        mix->setLayerKeyChannels(layer, float(kr), float(kg), float(kb));
        if (!fadeAnimating) {
            mix->setLayerOpacity(layer, opacity, false);
        }
        mix->setLayerFeedback(layer, true, c->props.feedback);
    };

    apply(m_previewB);
    if (m_fullscreenOut && m_fullscreenOut->isVisible()) {
        apply(m_fullscreenOut->mixerWidget());
    }
    if (m_previewB) {
        m_previewB->update();
    }
    if (m_fullscreenOut && m_fullscreenOut->isVisible()) {
        if (pvj::render::RhiMixerWidget* dst = m_fullscreenOut->mixerWidget()) {
            dst->update();
        }
    }
}

void MainWindow::syncInspectorEditLayerForCell(int bankSetIndex, int bankIndex, int cellIndex)
{
    if (m_inspectorEditLayer >= kUserLayerMin && m_inspectorEditLayer < kMixLayers) {
        const DeckSlot& slot = m_layerSlots[static_cast<size_t>(m_inspectorEditLayer)];
        if (slot.bankSet == bankSetIndex && slot.bank == bankIndex && slot.cell == cellIndex) {
            return;
        }
    }
    m_inspectorEditLayer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
}

void MainWindow::syncMixLayerFromCell(int layer, bool requestRepaint)
{
    if (layer < kUserLayerMin || layer >= kMixLayers || !m_previewB) {
        return;
    }
    const Cell* c = cellAtDeck(m_layerSlots[static_cast<size_t>(layer)]);
    const LayerKeyingState& layerKeying = m_layerKeying[static_cast<size_t>(layer)];
    const LayerKeyingState* keyingPtr = layerKeying.valid ? &layerKeying : nullptr;
    const bool isFeedbackCell = c && c->visual.type == VisualType::Generator
        && c->visual.generator == GeneratorKind::InternalFeedback;
    const bool mediaSource =
        c && c->visual.type == VisualType::Media && !c->visual.mediaId.isNull();

    if (isFeedbackCell) {
        m_previewB->setLayerActive(layer, true);
        m_previewB->setLayerCopyMode(layer, c->props.copyMode);
        m_previewB->setLayerMatteRole(layer, c->props.matteRole);
        m_previewB->setLayerPicture(layer, pictureParamsForMixer(*c));
        m_previewB->setLayerFilterChain(layer, effectiveFilterChainForMixer(c, keyingPtr));
        const double kr = keyingPtr ? keyingPtr->keyChannelR : c->props.keyChannelR;
        const double kg = keyingPtr ? keyingPtr->keyChannelG : c->props.keyChannelG;
        const double kb = keyingPtr ? keyingPtr->keyChannelB : c->props.keyChannelB;
        m_previewB->setLayerKeyChannels(layer, float(kr), float(kg), float(kb));
        if (!m_layerFadeAnimating[static_cast<size_t>(layer)]) {
            m_previewB->setLayerOpacity(layer, float(c->props.transparency), false);
        }
        m_previewB->setLayerFeedback(layer, true, c->props.feedback);
        if (m_audioEngine) {
            m_audioEngine->setLayerActive(layer, false);
        }
        if (requestRepaint) {
            m_previewB->update();
        }
        return;
    }

    m_previewB->setLayerFeedback(layer, false, {});
    if (!c || !mediaSource) {
        if (m_mixerSlotHadMedia[static_cast<size_t>(layer)]) {
            m_decoders[layer]->stop();
            m_audioDecoders[layer]->close();
        }
        m_mixerSlotHadMedia[static_cast<size_t>(layer)] = false;
        m_layerFadeAnimating[static_cast<size_t>(layer)] = false;
        m_previewB->setLayerActive(layer, false);
        m_previewB->setLayerOpacity(layer, 1.0f, false);
        m_previewB->setLayerMatteRole(layer, pvj::core::LayerMatteRole::None);
        m_previewB->setLayerFilterChain(layer, {});
        m_previewB->setLayerKeyChannels(layer, 1.f, 1.f, 1.f);
        if (m_audioEngine) {
            m_audioEngine->setLayerActive(layer, false);
        }
        if (requestRepaint) {
            m_previewB->update();
        }
        return;
    }

    m_previewB->setLayerActive(layer, true);
    m_previewB->setLayerCopyMode(layer, c->props.copyMode);
    m_previewB->setLayerMatteRole(layer, c->props.matteRole);
    m_previewB->setLayerPicture(layer, pictureParamsForMixer(*c));
    m_previewB->setLayerFilterChain(layer, effectiveFilterChainForMixer(c, keyingPtr));
    const double kr = keyingPtr ? keyingPtr->keyChannelR : c->props.keyChannelR;
    const double kg = keyingPtr ? keyingPtr->keyChannelG : c->props.keyChannelG;
    const double kb = keyingPtr ? keyingPtr->keyChannelB : c->props.keyChannelB;
    m_previewB->setLayerKeyChannels(layer, float(kr), float(kg), float(kb));
    if (!m_layerFadeAnimating[static_cast<size_t>(layer)]) {
        m_previewB->setLayerOpacity(layer, float(c->props.transparency), false);
    }

    m_mixerSlotHadMedia[static_cast<size_t>(layer)] = true;
    m_decoders[layer]->setLooping(playModeUsesLoop(c->props.playMode));
    if (m_audioDecoders[layer]->isOpen()) {
        m_audioDecoders[layer]->setLooping(playModeUsesLoop(c->props.playMode));
    }
    const double speed = qBound(0.0, c->props.movieSpeed, 4.0);
    m_decoders[layer]->setPlaybackSpeed(speed);
    if (m_audioDecoders[layer]->isOpen()) {
        m_audioDecoders[layer]->setPlaybackSpeed(speed);
    }

    if (m_audioEngine) {
        if (!m_layerFadeAnimating[static_cast<size_t>(layer)]) {
            m_audioEngine->setLayerGain(layer, effectiveLayerAudioGain(c));
        }
        m_audioEngine->setLayerActive(layer, m_audioDecoders[layer]->isOpen());
    }
    if (requestRepaint) {
        m_previewB->update();
    }
}

void MainWindow::updateMixerFromPlayingCells()
{
    for (int i = kUserLayerMin; i < kMixLayers; ++i) {
        syncMixLayerFromCell(i, false);
    }
    updateDeckAPreviewRotation();
    syncMixerToFullscreen();
    syncMixSlotHighlightsToBankGrid();
    if (m_previewB) {
        m_previewB->update();
    }
}

void MainWindow::syncMixSlotHighlightsToBankGrid()
{
    if (!m_bankGrid || !m_previewB) {
        return;
    }
    std::array<MixSlotCellRef, kMixSlotCount> arr{};
    for (int i = kUserLayerMin; i < kMixLayers; ++i) {
        const DeckSlot& d = m_layerSlots[static_cast<size_t>(i)];
        if (d.bankSet < 0) {
            continue;
        }
        arr[static_cast<size_t>(i)] = MixSlotCellRef{d.bankSet, d.bank, d.cell};
    }
    m_bankGrid->setMixSlotPlayback(arr);
    syncMixerFilterHighlightsToBankGrid();
    syncPeekHighlightToBankGrid();
}

void MainWindow::syncMixerFilterHighlightsToBankGrid()
{
    if (!m_bankGrid) {
        return;
    }
    QList<MixSlotCellRef> refs;
    refs.reserve(m_activeMixerFilterCells.size());
    for (const DeckSlot& slot : m_activeMixerFilterCells) {
        if (slot.bankSet >= 0) {
            refs.append(MixSlotCellRef{slot.bankSet, slot.bank, slot.cell});
        }
    }
    m_bankGrid->setActiveMixerFilterCells(refs);
}

void MainWindow::syncPeekHighlightToBankGrid()
{
    if (!m_bankGrid) {
        return;
    }
    if (m_clipPeekLayer < 0 || m_clipPeekLayer >= kMixLayers) {
        m_bankGrid->setPeekCellHighlight(MixSlotCellRef{});
        return;
    }
    const DeckSlot& d = m_layerSlots[static_cast<size_t>(m_clipPeekLayer)];
    if (d.bankSet < 0) {
        m_bankGrid->setPeekCellHighlight(MixSlotCellRef{});
        return;
    }
    m_bankGrid->setPeekCellHighlight(MixSlotCellRef{d.bankSet, d.bank, d.cell});
}

void MainWindow::onMixLayerDirect(int userSlotIndex)
{
    static constexpr int kUserLayerDirectCount = 13;
    if (userSlotIndex < 0 || userSlotIndex >= kUserLayerDirectCount || !m_bankGrid) {
        return;
    }
    const int gpuLayer = userSlotIndex + kUserLayerMin;
    m_inspectorEditLayer = gpuLayer;
    syncInspectorLayerKeyingOverride();
    const DeckSlot& s = m_layerSlots[static_cast<size_t>(gpuLayer)];
    if (s.bankSet < 0) {
        statusBar()->showMessage(tr("Mix slot %1 is empty").arg(gpuLayer), 2000);
        return;
    }
    m_bankGrid->revealAndSelectCell(s.bankSet, s.bank, s.cell);
    onCellTriggered(s.bankSet, s.bank, s.cell);
}

void MainWindow::startLayerFadeIn(int layer, float targetTransparency, float fadeParam)
{
    if (layer < kUserLayerMin || layer >= kMixLayers) {
        return;
    }
    const int durMs = int(5000.0 * double(fadeParam) + 0.5);
    if (durMs <= 0) {
        updateMixerFromPlayingCells();
        return;
    }
    m_layerFadeAnimating[static_cast<size_t>(layer)] = true;
    m_layerFadeTarget[static_cast<size_t>(layer)]     = targetTransparency;
    float audioTarget = targetTransparency;
    if (const Cell* c = cellAtDeck(m_layerSlots[static_cast<size_t>(layer)])) {
        audioTarget = float(c->props.transparency * effectiveLayerAudioGain(c));
    }
    m_layerFadeTargetAudio[static_cast<size_t>(layer)] = audioTarget;
    m_layerFadeElapsedMs[static_cast<size_t>(layer)]  = 0;
    m_layerFadeDurationMs[static_cast<size_t>(layer)] = std::max(1, durMs);

    m_previewB->setLayerOpacity(layer, 0.f);
    m_previewB->setLayerActive(layer, true);
    if (const Cell* c = cellAtDeck(m_layerSlots[static_cast<size_t>(layer)])) {
        m_previewB->setLayerCopyMode(layer, c->props.copyMode);
    } else {
        m_previewB->setLayerCopyMode(layer, pvj::core::CopyMode::Normal);
    }
    if (m_audioEngine && m_audioDecoders[layer]->isOpen()) {
        m_audioEngine->setLayerGain(layer, 0.f);
        m_audioEngine->setLayerActive(layer, true);
    }
    syncMixerToFullscreen();
    syncMixSlotHighlightsToBankGrid();
    if (m_layerFadeTimer && !m_layerFadeTimer->isActive()) {
        m_layerFadeTimer->start();
    }
}

void MainWindow::tickLayerFade()
{
    bool any = false;
    for (int i = kUserLayerMin; i < kMixLayers; ++i) {
        if (!m_layerFadeAnimating[static_cast<size_t>(i)]) {
            continue;
        }
        any = true;
        m_layerFadeElapsedMs[static_cast<size_t>(i)] += 16;
        const int dur = m_layerFadeDurationMs[static_cast<size_t>(i)];
        float t = float(m_layerFadeElapsedMs[static_cast<size_t>(i)]) / float(std::max(1, dur));
        if (t >= 1.f) {
            t = 1.f;
            m_layerFadeAnimating[static_cast<size_t>(i)] = false;
        }
        const float opacity = t * m_layerFadeTarget[static_cast<size_t>(i)];
        m_previewB->setLayerOpacity(i, opacity);
        if (m_audioEngine && m_audioDecoders[i]->isOpen()) {
            m_audioEngine->setLayerGain(i, t * m_layerFadeTargetAudio[static_cast<size_t>(i)]);
        }
        m_previewB->setLayerActive(i, true);
    }
    syncMixerToFullscreen();
    syncMixSlotHighlightsToBankGrid();
    if (!any) {
        if (m_layerFadeTimer) {
            m_layerFadeTimer->stop();
        }
        updateMixerFromPlayingCells();
    }
}

void MainWindow::updateDeckAPreviewRotation()
{
    float rad = 0.f;
    // Rotation applies only to the large clip preview while right-click peek is active.
    if (m_clipPeekLayer >= 0 && m_clipPeekLayer < kMixLayers) {
        if (const Cell* c = cellAtDeck(m_layerSlots[static_cast<size_t>(m_clipPeekLayer)])) {
            rad = float(c->props.rotationZ * M_PI);
        }
    }
    if (m_previewA) {
        m_previewA->setRotationZ(rad);
    }
}

void MainWindow::syncOutputFilterChainToMixers()
{
    if (!m_project || !m_previewB) {
        return;
    }
    const QList<pvj::core::CellFilterNode> chain = buildMergedOutputFilterChain();
    m_previewB->setOutputFilterChain(chain);
    if (m_fullscreenOut && m_fullscreenOut->mixerWidget()) {
        m_fullscreenOut->mixerWidget()->setOutputFilterChain(chain);
    }
}

void MainWindow::syncMixerToFullscreen()
{
    if (!m_fullscreenOut || !m_fullscreenOut->isVisible()) return;

    pvj::render::RhiMixerWidget* dst = m_fullscreenOut->mixerWidget();
    const pvj::render::RhiMixerWidget* src = m_previewB;
    for (int i = 0; i < pvj::render::RhiMixerWidget::LayerCount; ++i) {
        dst->setLayerOpacity(i, src->layerOpacity(i));
        dst->setLayerCopyMode(i, src->layerCopyMode(i));
        dst->setLayerActive(i, src->layerActive(i));
        const QImage& fr = m_lastFrames[static_cast<size_t>(i)];
        if (!fr.isNull()) {
            dst->setFrame(i, fr, 0);
        } else {
            dst->clearFrame(i);
        }
        dst->setLayerPicture(i, src->layerPicture(i));
        dst->setLayerFeedback(i, src->layerFeedbackEnabled(i), src->layerFeedback(i));
        if (const Cell* c = cellAtDeck(m_layerSlots[static_cast<size_t>(i)])) {
            const LayerKeyingState& layerKeying = m_layerKeying[static_cast<size_t>(i)];
            const LayerKeyingState* keyingPtr = layerKeying.valid ? &layerKeying : nullptr;
            dst->setLayerFilterChain(i, effectiveFilterChainForMixer(c, keyingPtr));
            const double kr = keyingPtr ? keyingPtr->keyChannelR : c->props.keyChannelR;
            const double kg = keyingPtr ? keyingPtr->keyChannelG : c->props.keyChannelG;
            const double kb = keyingPtr ? keyingPtr->keyChannelB : c->props.keyChannelB;
            dst->setLayerKeyChannels(i, float(kr), float(kg), float(kb));
        } else {
            dst->setLayerFilterChain(i, {});
            dst->setLayerKeyChannels(i, 1.f, 1.f, 1.f);
        }
    }
    syncOutputFilterChainToMixers();
}

bool MainWindow::selectionMatchesSlot(const DeckSlot& s) const
{
    if (!m_inspector) {
        return false;
    }
    return s.bankSet == m_inspector->selectedBankSetIndex()
        && s.bank == m_inspector->selectedBankIndex()
        && s.cell == m_inspector->selectedCellIndex();
}

int MainWindow::findLayerPlayingCell(int bankSet, int bank, int cell) const
{
    for (int i = kUserLayerMin; i < kMixLayers; ++i) {
        const DeckSlot& s = m_layerSlots[static_cast<size_t>(i)];
        if (s.bankSet == bankSet && s.bank == bank && s.cell == cell) {
            return i;
        }
    }
    return -1;
}

int MainWindow::resolveCellTriggerBankIndex(int mappingBankIndex) const
{
    if (mappingBankIndex == pvj::core::kBankIndexAllBanks) {
        return m_bankGrid ? m_bankGrid->activeBankIndex() : 0;
    }
    return mappingBankIndex;
}

int MainWindow::applyCellLayerSettingsToAllBanks(int bankSetIndex, int sourceBankIndex,
                                                 int cellIndex, const Cell& src)
{
    if (!m_project || bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return 0;
    }
    auto& set = m_project->bankSets[bankSetIndex];
    int copied = 0;
    for (int i = 0; i < set.banks.size(); ++i) {
        if (i == sourceBankIndex) {
            continue;
        }
        Bank& bank = set.banks[i];
        if (cellIndex < 0 || cellIndex >= bank.cells.size()) {
            continue;
        }
        pvj::core::copyCellLayerSettings(bank.cells[cellIndex], src);
        ++copied;
        if (findLayerPlayingCell(bankSetIndex, i, cellIndex) >= 0) {
            reapplyPlayingCell(bankSetIndex, i, cellIndex);
        }
    }
    return copied;
}

int MainWindow::applyPreferredLayerToAllBanks(int bankSetIndex, int sourceBankIndex, int cellIndex,
                                              int preferredLayer)
{
    if (!m_project || bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return 0;
    }
    auto& set = m_project->bankSets[bankSetIndex];
    int copied = 0;
    for (int i = 0; i < set.banks.size(); ++i) {
        if (i == sourceBankIndex) {
            continue;
        }
        Bank& bank = set.banks[i];
        if (cellIndex < 0 || cellIndex >= bank.cells.size()) {
            continue;
        }
        bank.cells[cellIndex].props.preferredLayer = preferredLayer;
        ++copied;
    }
    return copied;
}

int MainWindow::applyMidiMappingsToAllBanks(int bankSetIndex, int sourceBankIndex, int cellIndex,
                                            const Cell& src)
{
    if (!m_project || bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return 0;
    }
    auto& set = m_project->bankSets[bankSetIndex];
    int copied = 0;
    for (int i = 0; i < set.banks.size(); ++i) {
        if (i == sourceBankIndex) {
            continue;
        }
        Bank& bank = set.banks[i];
        if (cellIndex < 0 || cellIndex >= bank.cells.size()) {
            continue;
        }
        pvj::core::copyCellMidiPropertyMappings(bank.cells[cellIndex], src);
        ++copied;
    }
    return copied;
}

bool MainWindow::applyKeyboardTriggerToAllBanks(int bankSetIndex, int bankIndex, int cellIndex)
{
    if (!m_project) {
        return false;
    }
    const QString keyText = m_project->keyboardTriggerForCell(bankSetIndex, bankIndex, cellIndex);
    int midiCh = -1;
    int midiNote = -1;
    m_project->midiCellTriggerForCell(bankSetIndex, bankIndex, cellIndex, &midiCh, &midiNote);

    int qtKey = 0;
    for (const auto& t : m_project->triggerMappings) {
        if (t.input != pvj::core::InputType::Key || t.cellIndex != cellIndex
            || t.bankSetIndex != bankSetIndex || t.keyText.isEmpty()) {
            continue;
        }
        if (!keyText.isEmpty() && t.keyText != keyText) {
            continue;
        }
        qtKey = t.number;
        if (t.bankIndex == pvj::core::kBankIndexAllBanks || t.bankIndex == bankIndex) {
            break;
        }
    }

    const bool hadKey = !keyText.isEmpty();
    const bool hadMidi = midiCh >= 0 && midiNote >= 0;
    if (!hadKey && !hadMidi) {
        return false;
    }

    if (hadKey) {
        m_project->setKeyboardTriggerForCell(bankSetIndex, bankIndex, cellIndex, keyText, qtKey);
    }
    if (hadMidi) {
        m_project->setMidiCellTriggerForCell(bankSetIndex, cellIndex, midiCh, midiNote);
    }
    return true;
}

void MainWindow::refreshAfterMappingApplyToAllBanks(int bankSetIndex, int bankIndex, int cellIndex,
                                                    int playingLayer)
{
    if (m_bankGrid) {
        m_bankGrid->refresh();
    }
    if (playingLayer >= 0) {
        reapplyPlayingCell(bankSetIndex, bankIndex, cellIndex);
    }
    if (m_inspector) {
        m_inspector->setSelection(bankSetIndex, bankIndex, cellIndex);
    }
    m_inspectorEditBefore = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
    refreshPreviewForSelectedCell();
    onCellEdited(bankSetIndex, bankIndex, cellIndex);
    if (m_filterEditor) {
        m_filterEditor->refreshMidiMapOverlays();
    }
}

void MainWindow::clearUndoStack()
{
    if (m_undoStack) {
        m_undoStack->clear();
    }
    m_inspectorContinuousEdit = false;
    m_inspectorEditBefore.reset();
    m_learnBaselineValid = false;
    m_learnCellBefore.reset();
    m_filterEditBefore.reset();
    m_suppressInspectorUndo = false;
    m_refreshingUiAfterEdit = false;
    if (m_filterParamsUndoTimer) {
        m_filterParamsUndoTimer->stop();
    }
    m_filterParamsUndoBankSet = -1;
}

void MainWindow::refreshUiAfterProjectEdit()
{
    if (m_closing || !m_project || m_refreshingUiAfterEdit) {
        return;
    }
    m_refreshingUiAfterEdit = true;
    m_suppressInspectorUndo = true;
    if (m_bankGrid) {
        m_bankGrid->setGridDimensions(m_project->settings.matrix.gridRows,
                                      m_project->settings.matrix.gridCols);
        m_bankGrid->refresh();
    }
    if (m_inspector) {
        const int bs = m_bankGrid ? m_bankGrid->activeBankSetIndex() : 0;
        const int b  = m_bankGrid ? m_bankGrid->activeBankIndex() : 0;
        const int c  = m_bankGrid ? m_bankGrid->selectedCellIndex() : -1;
        m_inspector->setSelection(bs, b, c);
        m_inspectorEditBefore = snapshotSingleCell(bs, b, c);
    }
    if (m_mediaDock) {
        m_mediaDock->refreshProjectMedia();
    }
    if (m_filterEditor) {
        m_filterEditor->refreshMidiMapOverlays();
        if (m_filterEditBefore) {
            m_filterEditBefore = snapshotSingleCell(m_filterEditBefore->ref.bankSet,
                                                    m_filterEditBefore->ref.bank,
                                                    m_filterEditBefore->ref.cell);
        }
    }
    refreshPreviewForSelectedCell();
    updateMixerFromPlayingCells();
    syncOutputFilterChainToMixers();
    syncMixerFilterHighlightsToBankGrid();
    updateWindowTitle();
    m_suppressInspectorUndo = false;
    m_refreshingUiAfterEdit = false;
}

void MainWindow::pushCellsReplaceCommand(QVector<CellSnapshot> before, QVector<CellSnapshot> after,
                                         const QString& text)
{
    if (!m_undoStack || !m_project || before.isEmpty() || before.size() != after.size()) {
        return;
    }
    m_undoStack->push(new CellsReplaceCommand(m_project.get(), std::move(before), std::move(after), text));
}

QVector<CellSnapshot> MainWindow::snapshotCellAcrossBanks(int bankSetIndex, int /*bankIndex*/,
                                                          int cellIndex) const
{
    QVector<CellSnapshot> snaps;
    if (!m_project || bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return snaps;
    }
    const auto& set = m_project->bankSets[bankSetIndex];
    snaps.reserve(set.banks.size());
    for (int i = 0; i < set.banks.size(); ++i) {
        if (cellIndex < 0 || cellIndex >= set.banks[i].cells.size()) {
            continue;
        }
        CellSnapshot snap;
        snap.ref = {bankSetIndex, i, cellIndex};
        snap.cell = set.banks[i].cells[cellIndex];
        snaps.append(std::move(snap));
    }
    return snaps;
}

QVector<CellSnapshot> MainWindow::snapshotAllCellsInBankSet(int bankSetIndex) const
{
    QVector<CellSnapshot> snaps;
    if (!m_project || bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return snaps;
    }
    const auto& set = m_project->bankSets[bankSetIndex];
    for (int bi = 0; bi < set.banks.size(); ++bi) {
        const auto& bank = set.banks[bi];
        for (int ci = 0; ci < bank.cells.size(); ++ci) {
            CellSnapshot snap;
            snap.ref = {bankSetIndex, bi, ci};
            snap.cell = bank.cells[ci];
            snaps.append(std::move(snap));
        }
    }
    return snaps;
}

std::optional<CellSnapshot> MainWindow::snapshotSingleCell(int bankSetIndex, int bankIndex,
                                                           int cellIndex) const
{
    if (!m_project || bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return std::nullopt;
    }
    const auto& set = m_project->bankSets[bankSetIndex];
    if (bankIndex < 0 || bankIndex >= set.banks.size()) {
        return std::nullopt;
    }
    const auto& bank = set.banks[bankIndex];
    if (cellIndex < 0 || cellIndex >= bank.cells.size()) {
        return std::nullopt;
    }
    CellSnapshot snap;
    snap.ref = {bankSetIndex, bankIndex, cellIndex};
    snap.cell = bank.cells[cellIndex];
    return snap;
}

void MainWindow::syncInspectorEditBaselines()
{
    if (!m_project) {
        return;
    }
    if (m_inspector) {
        const int bs = m_inspector->selectedBankSetIndex();
        const int b  = m_inspector->selectedBankIndex();
        const int c  = m_inspector->selectedCellIndex();
        if (c >= 0) {
            m_inspectorEditBefore = snapshotSingleCell(bs, b, c);
        }
    }
    if (m_filterEditBefore) {
        m_filterEditBefore = snapshotSingleCell(m_filterEditBefore->ref.bankSet,
                                                m_filterEditBefore->ref.bank,
                                                m_filterEditBefore->ref.cell);
    }
}

void MainWindow::beginInspectorContinuousEdit()
{
    if (!m_inspector) {
        return;
    }
    m_inspectorContinuousEdit = true;
    // Always capture at gesture start so MIDI learns during/before the drag cannot poison undo.
    m_inspectorEditBefore = snapshotSingleCell(m_inspector->selectedBankSetIndex(),
                                               m_inspector->selectedBankIndex(),
                                               m_inspector->selectedCellIndex());
}

void MainWindow::endInspectorContinuousEdit()
{
    if (!m_inspectorContinuousEdit) {
        return;
    }
    m_inspectorContinuousEdit = false;
    if (m_suppressInspectorUndo || !m_inspector || !m_inspectorEditBefore) {
        if (m_inspector) {
            m_inspectorEditBefore = snapshotSingleCell(m_inspector->selectedBankSetIndex(),
                                                       m_inspector->selectedBankIndex(),
                                                       m_inspector->selectedCellIndex());
        }
        return;
    }
    const int bs = m_inspector->selectedBankSetIndex();
    const int b  = m_inspector->selectedBankIndex();
    const int c  = m_inspector->selectedCellIndex();
    auto after = snapshotSingleCell(bs, b, c);
    if (!after) {
        return;
    }
    // Inspector parameter edits must not undo MIDI mappings learned outside this gesture.
    CellSnapshot before = *m_inspectorEditBefore;
    before.cell.propertyMappings = after->cell.propertyMappings;

    // Avoid stacking no-ops: compare a few high-signal fields (not MIDI mappings).
    const Cell& a = before.cell;
    const Cell& bCell = after->cell;
    const bool changed = a.name != bCell.name
        || a.visual.type != bCell.visual.type
        || a.visual.mediaId != bCell.visual.mediaId
        || a.props.transparency != bCell.props.transparency
        || a.props.movieSpeed != bCell.props.movieSpeed
        || a.props.fade != bCell.props.fade
        || a.props.rotationZ != bCell.props.rotationZ
        || a.props.copyMode != bCell.props.copyMode
        || a.props.keyingEnabled != bCell.props.keyingEnabled
        || a.props.keyThreshold != bCell.props.keyThreshold
        || a.props.keySoftness != bCell.props.keySoftness
        || a.props.preferredLayer != bCell.props.preferredLayer
        || a.filterChain.size() != bCell.filterChain.size();
    if (changed) {
        pushCellsReplaceCommand({before}, {*after}, tr("Edit cell parameters"));
    }
    m_inspectorEditBefore = after;
    onCellEdited(bs, b, c);
}

void MainWindow::onInspectorDiscreteCellChanged(int bankSetIndex, int bankIndex, int cellIndex)
{
    if (m_inspectorContinuousEdit) {
        onCellEdited(bankSetIndex, bankIndex, cellIndex);
        return;
    }
    auto after = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
    if (m_suppressInspectorUndo) {
        if (after) {
            m_inspectorEditBefore = after;
        }
        onCellEdited(bankSetIndex, bankIndex, cellIndex);
        return;
    }
    if (m_inspectorEditBefore && after
        && m_inspectorEditBefore->ref.bankSet == bankSetIndex
        && m_inspectorEditBefore->ref.bank == bankIndex
        && m_inspectorEditBefore->ref.cell == cellIndex) {
        // Keep current MIDI mappings on undo of unrelated inspector field edits.
        CellSnapshot before = *m_inspectorEditBefore;
        before.cell.propertyMappings = after->cell.propertyMappings;
        pushCellsReplaceCommand({before}, {*after}, tr("Edit cell"));
        m_inspectorEditBefore = after;
    } else if (after) {
        m_inspectorEditBefore = after;
    }
    onCellEdited(bankSetIndex, bankIndex, cellIndex);
    if (m_bankGrid) {
        m_bankGrid->refreshCellLabels();
    }
}

void MainWindow::captureLearnBaseline()
{
    m_learnBaselineValid = false;
    m_learnCellBefore.reset();
    m_learnTriggersBefore.clear();
    if (!m_project) {
        return;
    }
    m_learnTriggersBefore = m_project->triggerMappings;
    if (m_bankGrid) {
        const int ci = m_bankGrid->selectedCellIndex();
        if (ci >= 0) {
            m_learnCellBefore = snapshotSingleCell(m_bankGrid->activeBankSetIndex(),
                                                   m_bankGrid->activeBankIndex(),
                                                   ci);
        }
    }
    // Filter-editor learn uses explicit bank/cell in beginLearn* — also set from callers.
    m_learnBaselineValid = true;
}

void MainWindow::commitLearnUndoIfNeeded(const QString& text)
{
    if (!m_learnBaselineValid || !m_undoStack || !m_project) {
        m_learnBaselineValid = false;
        return;
    }
    m_learnBaselineValid = false;

    const auto& afterTriggers = m_project->triggerMappings;
    bool triggersChanged = m_learnTriggersBefore.size() != afterTriggers.size();
    if (!triggersChanged) {
        for (int i = 0; i < afterTriggers.size(); ++i) {
            const auto& a = m_learnTriggersBefore[i];
            const auto& b = afterTriggers[i];
            if (a.input != b.input || a.channel != b.channel || a.number != b.number
                || a.keyText != b.keyText || a.target != b.target
                || a.bankSetIndex != b.bankSetIndex || a.bankIndex != b.bankIndex
                || a.cellIndex != b.cellIndex || a.propertyName != b.propertyName) {
                triggersChanged = true;
                break;
            }
        }
    }
    if (triggersChanged) {
        m_undoStack->push(new TriggerMappingsReplaceCommand(
            m_project.get(), m_learnTriggersBefore, m_project->triggerMappings, text));
    }

    if (m_learnCellBefore) {
        if (auto after = snapshotSingleCell(m_learnCellBefore->ref.bankSet,
                                            m_learnCellBefore->ref.bank,
                                            m_learnCellBefore->ref.cell)) {
            if (m_learnCellBefore->cell.propertyMappings.size()
                != after->cell.propertyMappings.size()) {
                pushCellsReplaceCommand({*m_learnCellBefore}, {*after}, text);
            } else {
                bool mappingsChanged = false;
                for (int i = 0; i < after->cell.propertyMappings.size(); ++i) {
                    const auto& a = m_learnCellBefore->cell.propertyMappings[i];
                    const auto& b = after->cell.propertyMappings[i];
                    if (a.property != b.property || a.input != b.input || a.channel != b.channel
                        || a.number != b.number || a.minValue != b.minValue || a.maxValue != b.maxValue
                        || a.buttonMode != b.buttonMode || a.buttonValue != b.buttonValue) {
                        mappingsChanged = true;
                        break;
                    }
                }
                if (mappingsChanged) {
                    pushCellsReplaceCommand({*m_learnCellBefore}, {*after}, text);
                }
            }
        }
    }
    m_learnCellBefore.reset();
}

void MainWindow::commitFilterCellUndo(int bankSetIndex, int bankIndex, int cellIndex,
                                      const QString& text)
{
    auto after = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
    if (!after) {
        return;
    }
    if (!m_filterEditBefore
        || m_filterEditBefore->ref.bankSet != bankSetIndex
        || m_filterEditBefore->ref.bank != bankIndex
        || m_filterEditBefore->ref.cell != cellIndex) {
        m_filterEditBefore = after;
        return;
    }
    // Filter-param undo must not roll back MIDI mappings learned while the debounce was open.
    CellSnapshot before = *m_filterEditBefore;
    before.cell.propertyMappings = after->cell.propertyMappings;
    pushCellsReplaceCommand({before}, {*after}, text);
    m_filterEditBefore = after;
}

void MainWindow::scheduleFilterParamsUndo(int bankSetIndex, int bankIndex, int cellIndex)
{
    if (!m_filterEditBefore
        || m_filterEditBefore->ref.bankSet != bankSetIndex
        || m_filterEditBefore->ref.bank != bankIndex
        || m_filterEditBefore->ref.cell != cellIndex) {
        m_filterEditBefore = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
    }
    m_filterParamsUndoBankSet = bankSetIndex;
    m_filterParamsUndoBank = bankIndex;
    m_filterParamsUndoCell = cellIndex;
    if (m_filterParamsUndoTimer) {
        m_filterParamsUndoTimer->start();
    }
}

int MainWindow::pickMixSlotForTrigger(int bankSet, int bank, int cell)
{
    const int existing = findLayerPlayingCell(bankSet, bank, cell);
    if (existing >= 0) {
        return existing;
    }
    const int preferred = preferredLayerFromCell(cellAtDeck(DeckSlot{bankSet, bank, cell}));
    return gpuLayerFromPreferred(preferred);
}

void MainWindow::refreshPreviewForSelectedCell()
{
    if (!m_inspector) {
        return;
    }

    QImage thumb;
    for (int i = 0; i < kMixLayers; ++i) {
        if (!selectionMatchesSlot(m_layerSlots[static_cast<size_t>(i)])) {
            continue;
        }
        const QImage& fr = m_lastFrames[static_cast<size_t>(i)];
        if (!fr.isNull()) {
            thumb = fr;
        }
        break;
    }
    if (thumb.isNull() && m_bankGrid) {
        thumb = m_bankGrid->cachedCellPreview(m_inspector->selectedBankSetIndex(),
                                              m_inspector->selectedBankIndex(),
                                              m_inspector->selectedCellIndex());
    }
    if (!thumb.isNull()) {
        m_inspector->setVisualThumbnail(thumb);
    } else {
        m_inspector->clearVisualThumbnail();
    }

    applyClipPreviewPane();
    updateDeckAPreviewRotation();
}

void MainWindow::applyClipPreviewPane()
{
    if (!m_previewA || !m_project) {
        return;
    }

    if (m_clipPeekLayer >= 0 && m_clipPeekLayer < kMixLayers) {
        const QImage& fr = m_lastFrames[static_cast<size_t>(m_clipPeekLayer)];
        if (!fr.isNull()) {
            m_previewA->setFrame(fr, 0);
        } else {
            m_previewA->clearFrame();
        }
        m_previewA->setLabel(tr("Clip preview"));
        if (const Cell* c = cellAtDeck(m_layerSlots[static_cast<size_t>(m_clipPeekLayer)])) {
            if (c->visual.type == VisualType::Media && !c->visual.mediaId.isNull()) {
                if (const auto* media = m_project->findMedia(c->visual.mediaId)) {
                    m_previewA->setLabel(QFileInfo(media->path).fileName());
                }
            } else if (c->visual.type == VisualType::Generator) {
                m_previewA->setLabel(tr("Generator"));
            }
        }
        return;
    }

    // No peek: do not mirror the selected mix layer into the large preview — that is reserved for
    // right-click peek only (left-click is mixer + inspector thumbnail elsewhere).
    m_previewA->clearFrame();
    m_previewA->setLabel(tr("Clip preview"));
}

void MainWindow::setClipPreviewPeekChrome(bool active)
{
    if (!m_clipPreviewFrame) {
        return;
    }
    m_clipPreviewFrame->setObjectName(QStringLiteral("pvjClipPreviewFrame"));
    if (active) {
        m_clipPreviewFrame->setStyleSheet(QStringLiteral(
            "#pvjClipPreviewFrame { border: 2px solid #ff9140; border-radius: 8px; background-color: #121418; }"));
    } else {
        m_clipPreviewFrame->setStyleSheet(QStringLiteral(
            "#pvjClipPreviewFrame { border: 2px solid transparent; border-radius: 8px; background-color: transparent; }"));
    }
}

void MainWindow::clearClipPeek()
{
    m_clipPeekLayer = -1;
    setClipPreviewPeekChrome(false);
    syncPeekHighlightToBankGrid();
}

void MainWindow::onCellPeekPreviewRequested(int bankSetIndex, int bankIndex, int cellIndex)
{
    const int layer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
    if (layer < 0) {
        statusBar()->showMessage(
            tr("Right-click preview: this cell is not playing on any mix slot."), 4000);
        return;
    }
    if (m_clipPeekLayer == layer) {
        clearClipPeek();
        refreshPreviewForSelectedCell();
        statusBar()->showMessage(tr("Right-click preview off."), 2000);
        return;
    }
    m_clipPeekLayer = layer;
    setClipPreviewPeekChrome(true);
    // Sync bank tab, grid selection, and inspector to this cell (peek stays on: onCellSelected skips clearClipPeek).
    if (m_bankGrid) {
        m_bankGrid->revealAndSelectCell(bankSetIndex, bankIndex, cellIndex);
    } else if (m_inspector) {
        m_inspector->setSelection(bankSetIndex, bankIndex, cellIndex);
        refreshPreviewForSelectedCell();
    }
    applyClipPreviewPane();
    updateDeckAPreviewRotation();
    syncPeekHighlightToBankGrid();
    statusBar()->showMessage(
        tr("Clip preview (orange). Same cell: right-click again to hide. Inspector follows this clip."), 4000);
}

int MainWindow::activeFullscreenScreenIndex() const
{
    if (!m_fullscreenOut || !m_fullscreenOut->isVisible()) {
        return -1;
    }
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (QScreen* target = m_fullscreenOut->targetScreen()) {
        const int idx = screens.indexOf(target);
        if (idx >= 0) {
            return idx;
        }
    }
    return m_outputScreenIndex;
}

void MainWindow::rebuildOutputScreenMenu()
{
    if (!m_outputMenu) {
        return;
    }
    m_outputMenu->clear();
    if (m_outputScreenGroup) {
        m_outputScreenGroup->deleteLater();
        m_outputScreenGroup = nullptr;
    }
    m_outputScreenGroup = new QActionGroup(this);
    m_outputScreenGroup->setExclusive(true);
    m_actCloseFullscreenOutput = nullptr;

    const QList<QScreen*> screens = QGuiApplication::screens();
    const int activeIdx = activeFullscreenScreenIndex();

    if (screens.isEmpty()) {
        QAction* none = m_outputMenu->addAction(tr("(no displays detected)"));
        none->setEnabled(false);
    } else {
        for (int i = 0; i < screens.size(); ++i) {
            const QScreen* s = screens[i];
            const QString name = s->name();
            const QRect g = s->geometry();
            const QString label = tr("Screen %1: %2 (%3×%4)")
                                      .arg(i + 1)
                                      .arg(name.isEmpty() ? tr("Display") : name)
                                      .arg(g.width())
                                      .arg(g.height());
            QAction* act = m_outputMenu->addAction(label);
            act->setCheckable(true);
            act->setChecked(i == activeIdx);
            m_outputScreenGroup->addAction(act);
            connect(act, &QAction::triggered, this, [this, i]() {
                openFullscreenOutputOnScreen(i);
            });
        }
    }

    m_outputMenu->addSeparator();
    m_actCloseFullscreenOutput =
        m_outputMenu->addAction(tr("Close fullscreen output"), this, &MainWindow::closeFullscreenOutput);
    m_actCloseFullscreenOutput->setEnabled(activeIdx >= 0);

    m_outputMenu->addSeparator();
    m_outputMenu->addAction(tr("Post-processing filters…"), this,
                            &MainWindow::onOutputProcessingFilters);
}

void MainWindow::openFullscreenOutputOnScreen(int screenIndex)
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        return;
    }

    const int idx = qBound(0, screenIndex, screens.size() - 1);
    m_outputScreenIndex = idx;
    QSettings().setValue(QLatin1String(kOutputScreenIndex), idx);

    if (!m_fullscreenOut) {
        m_fullscreenOut = std::make_unique<pvj::render::FullscreenOutputWindow>();
        if (m_stagePixelSize.isValid() && !m_stagePixelSize.isEmpty()
            && m_fullscreenOut->mixerWidget()) {
            m_fullscreenOut->mixerWidget()->setStagePixelSize(m_stagePixelSize);
        }
    }

    QScreen* s = screens.at(idx);
    m_fullscreenOut->setTargetScreen(s);
    m_fullscreenOut->enterFullscreen();
    syncMixerToFullscreen();
    statusBar()->showMessage(tr("Fullscreen output on %1").arg(s->name()), 3000);
}

void MainWindow::closeFullscreenOutput()
{
    if (!m_fullscreenOut || !m_fullscreenOut->isVisible()) {
        return;
    }
    m_fullscreenOut->hide();
    statusBar()->showMessage(tr("Fullscreen output closed"), 2000);
    if (m_outputScreenGroup) {
        for (QAction* a : m_outputScreenGroup->actions()) {
            if (a) {
                a->setChecked(false);
            }
        }
    }
}

void MainWindow::onOutputProcessingFilters()
{
    if (!m_outputProcessingDialog) {
        return;
    }
    m_outputProcessingDialog->setProject(m_project.get());
    m_outputProcessingDialog->show();
    m_outputProcessingDialog->raise();
    m_outputProcessingDialog->activateWindow();
}

void MainWindow::assignMediaToCell(int bankSetIndex, int bankIndex, int cellIndex, const QString& absolutePath)
{
    if (!m_project || !m_bankGrid || absolutePath.isEmpty()) {
        return;
    }
    if (!QFileInfo(absolutePath).isFile()) {
        statusBar()->showMessage(tr("Not a file: %1").arg(absolutePath), 4000);
        return;
    }
    if (bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    auto& set = m_project->bankSets[bankSetIndex];
    if (bankIndex < 0 || bankIndex >= set.banks.size()) {
        return;
    }
    auto& bank = set.banks[bankIndex];
    if (cellIndex < 0 || cellIndex >= bank.cells.size()) {
        return;
    }

    auto before = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);

    QUuid id;
    for (const auto& m : m_project->mediaLibrary) {
        if (m.path.compare(absolutePath, Qt::CaseInsensitive) == 0) {
            id = m.id;
            break;
        }
    }
    if (id.isNull()) {
        MediaItem m;
        m.id = QUuid::createUuid();
        m.path = absolutePath;
        m.displayName = QFileInfo(absolutePath).fileName();
        m_project->mediaLibrary.append(m);
        if (m_mediaDock) {
            m_mediaDock->refreshProjectMedia();
        }
        id = m.id;
    }

    auto& cellRef = bank.cells[cellIndex];
    cellRef.visual.type = VisualType::Media;
    cellRef.visual.mediaId = id;
    if (before) {
        if (auto after = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex)) {
            pushCellsReplaceCommand({*before}, {*after}, tr("Assign media"));
        }
    }
    m_bankGrid->selectCell(cellIndex);
    m_bankGrid->refresh();
    m_inspector->setSelection(bankSetIndex, bankIndex, cellIndex);
    m_inspectorEditBefore = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
    refreshPreviewForSelectedCell();
    reapplyPlayingCell(bankSetIndex, bankIndex, cellIndex);
    if (m_previewA) {
        m_previewA->setLabel(QFileInfo(absolutePath).fileName());
    }
    statusBar()->showMessage(tr("Assigned %1 to bank %2 cell %3")
                             .arg(QFileInfo(absolutePath).fileName())
                             .arg(bankIndex + 1).arg(cellIndex + 1), 5000);
}

void MainWindow::onMediaActivated(const QString& absolutePath)
{
    const int set  = m_bankGrid->activeBankSetIndex();
    const int bank = m_bankGrid->activeBankIndex();
    const int cell = m_bankGrid->selectedCellIndex();
    if (cell < 0) {
        statusBar()->showMessage(tr("Select a cell first to assign \"%1\"")
                                 .arg(QFileInfo(absolutePath).fileName()), 5000);
        return;
    }
    assignMediaToCell(set, bank, cell, absolutePath);
}

void MainWindow::onMediaDroppedOnCell(int bankSetIndex, int bankIndex, int cellIndex, const QString& absolutePath)
{
    assignMediaToCell(bankSetIndex, bankIndex, cellIndex, absolutePath);
}

void MainWindow::copySelectedCell()
{
    if (!m_project || !m_bankGrid) {
        return;
    }
    const int cellIndex = m_bankGrid->selectedCellIndex();
    if (cellIndex < 0) {
        statusBar()->showMessage(tr("Select a cell to copy"), 3000);
        return;
    }
    const Cell* src = cellAtDeck({m_bankGrid->activeBankSetIndex(),
                                  m_bankGrid->activeBankIndex(),
                                  cellIndex});
    if (!src) {
        return;
    }
    m_cellClipboard = *src;
    m_cellClipboardValid = true;
    statusBar()->showMessage(tr("Copied cell %1").arg(cellIndex + 1), 3000);
}

void MainWindow::pasteIntoSelectedCell()
{
    pasteIntoSelectedCell(pvj::core::CellPasteMode::FullWithMidi);
}

void MainWindow::pasteIntoSelectedCellWithMidi()
{
    pasteIntoSelectedCell(pvj::core::CellPasteMode::FullWithMidi);
}

void MainWindow::pasteIntoSelectedCellWithoutMidi()
{
    pasteIntoSelectedCell(pvj::core::CellPasteMode::FullWithoutMidi);
}

void MainWindow::pasteIntoSelectedCellParamsWithMidi()
{
    pasteIntoSelectedCell(pvj::core::CellPasteMode::ParamsWithMidi);
}

void MainWindow::pasteIntoSelectedCell(pvj::core::CellPasteMode mode)
{
    if (!m_cellClipboardValid) {
        statusBar()->showMessage(tr("Copy a cell first (Ctrl+C)"), 3000);
        return;
    }
    if (!m_project || !m_bankGrid) {
        return;
    }
    const int bankSetIndex = m_bankGrid->activeBankSetIndex();
    const int bankIndex    = m_bankGrid->activeBankIndex();
    const int cellIndex    = m_bankGrid->selectedCellIndex();
    if (cellIndex < 0) {
        statusBar()->showMessage(tr("Select a cell to paste into"), 3000);
        return;
    }
    Cell* dst = const_cast<Cell*>(cellAtDeck({bankSetIndex, bankIndex, cellIndex}));
    if (!dst) {
        return;
    }

    QVector<CellSnapshot> before = snapshotCellAcrossBanks(bankSetIndex, bankIndex, cellIndex);
    pvj::core::pasteCellContent(*dst, m_cellClipboard, mode);
    const int layerBanks =
        applyCellLayerSettingsToAllBanks(bankSetIndex, bankIndex, cellIndex, *dst);
    QVector<CellSnapshot> after = snapshotCellAcrossBanks(bankSetIndex, bankIndex, cellIndex);
    QString undoText;
    switch (mode) {
    case pvj::core::CellPasteMode::FullWithMidi:
        undoText = tr("Paste cell (with MIDI)");
        break;
    case pvj::core::CellPasteMode::FullWithoutMidi:
        undoText = tr("Paste cell (without MIDI)");
        break;
    case pvj::core::CellPasteMode::ParamsWithMidi:
        undoText = tr("Paste parameters + MIDI");
        break;
    }
    pushCellsReplaceCommand(std::move(before), std::move(after), undoText);

    m_bankGrid->refresh();
    if (m_inspector) {
        m_inspector->setSelection(bankSetIndex, bankIndex, cellIndex);
    }
    m_inspectorEditBefore = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
    refreshPreviewForSelectedCell();
    reapplyPlayingCell(bankSetIndex, bankIndex, cellIndex);
    onCellEdited(bankSetIndex, bankIndex, cellIndex);
    if (layerBanks > 0) {
        statusBar()->showMessage(
            tr("Pasted into cell %1; layer settings applied to %2 other banks")
                .arg(cellIndex + 1)
                .arg(layerBanks),
            3000);
    } else {
        statusBar()->showMessage(tr("Pasted into cell %1").arg(cellIndex + 1), 3000);
    }
}

void MainWindow::onCellContextMenu(int bankSetIndex, int bankIndex, int cellIndex, QPoint globalPos)
{
    QMenu menu(this);
    if (m_project && bankSetIndex >= 0 && bankSetIndex < m_project->bankSets.size()) {
        const auto& set = m_project->bankSets[bankSetIndex];
        if (bankIndex >= 0 && bankIndex < set.banks.size()
            && cellIndex >= 0 && cellIndex < set.banks[bankIndex].cells.size()) {
            const Cell& cell = set.banks[bankIndex].cells[cellIndex];
            if (cell.visual.type != VisualType::MixerFilter) {
                menu.addAction(tr("Use as mixer filter cell"), this,
                               [this, bankSetIndex, bankIndex, cellIndex]() {
                                   if (!m_project) {
                                       return;
                                   }
                                   auto before = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
                                   auto& setRef = m_project->bankSets[bankSetIndex];
                                   Cell& c = setRef.banks[bankIndex].cells[cellIndex];
                                   c.visual.type = VisualType::MixerFilter;
                                   c.visual.generator = GeneratorKind::None;
                                   c.visual.mediaId = {};
                                   if (before) {
                                       if (auto after = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex)) {
                                           pushCellsReplaceCommand({*before}, {*after},
                                                                   tr("Set mixer filter cell"));
                                       }
                                   }
                                   if (m_bankGrid) {
                                       m_bankGrid->refresh();
                                   }
                                   if (m_inspector) {
                                       m_inspector->refreshFromModel();
                                   }
                                   m_inspectorEditBefore =
                                       snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
                                   statusBar()->showMessage(
                                       tr("Cell %1 is now a mixer filter cell — pick a filter (double-click)")
                                           .arg(cellIndex + 1),
                                       5000);
                               });
                menu.addSeparator();
            }
        }
    }
    menu.addAction(tr("Peek preview"), this, [this, bankSetIndex, bankIndex, cellIndex]() {
        onCellPeekPreviewRequested(bankSetIndex, bankIndex, cellIndex);
    });
    menu.addSeparator();
    menu.addAction(tr("Copy cell"), this, &MainWindow::copySelectedCell);
    auto* pasteWithMidi = menu.addAction(tr("Paste (with MIDI)"), this,
                                         &MainWindow::pasteIntoSelectedCellWithMidi);
    pasteWithMidi->setEnabled(m_cellClipboardValid);
    auto* pasteWithoutMidi = menu.addAction(tr("Paste without MIDI"), this,
                                            &MainWindow::pasteIntoSelectedCellWithoutMidi);
    pasteWithoutMidi->setEnabled(m_cellClipboardValid);
    auto* pasteParams = menu.addAction(tr("Paste parameters + MIDI"), this,
                                       &MainWindow::pasteIntoSelectedCellParamsWithMidi);
    pasteParams->setEnabled(m_cellClipboardValid);
    if (m_project && bankSetIndex >= 0 && bankSetIndex < m_project->bankSets.size()) {
        const auto& set = m_project->bankSets[bankSetIndex];
        if (bankIndex >= 0 && bankIndex < set.banks.size()
            && cellIndex >= 0 && cellIndex < set.banks[bankIndex].cells.size()) {
            const Cell& cell = set.banks[bankIndex].cells[cellIndex];
            if (cellAllowsRename(cell)) {
                menu.addSeparator();
                menu.addAction(tr("Rename cell…"), this,
                               [this, bankSetIndex, bankIndex, cellIndex]() {
                                   renameCell(bankSetIndex, bankIndex, cellIndex);
                               });
            }
        }
    }
    menu.exec(globalPos);
}

void MainWindow::onBankContextMenu(int bankSetIndex, int bankIndex, QPoint globalPos)
{
    QMenu menu(this);
    menu.addAction(tr("Copy bank"), this, [this, bankSetIndex, bankIndex]() {
        copyBank(bankSetIndex, bankIndex);
    });
    auto* pasteAct = menu.addAction(tr("Paste bank"), this, &MainWindow::pasteBankIntoActive);
    pasteAct->setEnabled(m_bankClipboardValid);
    menu.addSeparator();
    menu.addAction(tr("Rename bank…"), this, [this, bankSetIndex, bankIndex]() {
        renameBank(bankSetIndex, bankIndex);
    });
    menu.exec(globalPos);
}

void MainWindow::copyBank(int bankSetIndex, int bankIndex)
{
    if (!m_project) {
        return;
    }
    if (bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    const auto& set = m_project->bankSets[bankSetIndex];
    if (bankIndex < 0 || bankIndex >= set.banks.size()) {
        return;
    }
    m_bankClipboard      = set.banks[bankIndex];
    m_bankClipboardValid = true;
    const QString label = set.banks[bankIndex].name.isEmpty()
                              ? tr("Bank %1").arg(bankIndex + 1)
                              : set.banks[bankIndex].name;
    statusBar()->showMessage(tr("Copied bank “%1”").arg(label), 3000);
}

void MainWindow::pasteBankIntoActive()
{
    if (!m_bankClipboardValid || !m_project || !m_bankGrid) {
        statusBar()->showMessage(tr("Copy a bank first (bank tab context menu)"), 3000);
        return;
    }
    const int bankSetIndex = m_bankGrid->activeBankSetIndex();
    const int bankIndex    = m_bankGrid->activeBankIndex();
    if (bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    auto& set = m_project->bankSets[bankSetIndex];
    if (bankIndex < 0 || bankIndex >= set.banks.size()) {
        return;
    }

    const Project before = *m_project;
    Bank& dst = set.banks[bankIndex];
    pvj::core::copyBankContent(dst, m_bankClipboard);
    int layerCells = 0;
    for (int i = 0; i < dst.cells.size(); ++i) {
        layerCells += applyCellLayerSettingsToAllBanks(bankSetIndex, bankIndex, i, dst.cells[i]);
    }
    if (m_undoStack) {
        m_undoStack->push(new ProjectReplaceCommand(m_project.get(), before, *m_project,
                                                    tr("Paste bank")));
    }
    m_bankGrid->refresh();
    for (int i = 0; i < dst.cells.size(); ++i) {
        if (findLayerPlayingCell(bankSetIndex, bankIndex, i) >= 0) {
            reapplyPlayingCell(bankSetIndex, bankIndex, i);
        }
    }
    if (m_inspector) {
        m_inspector->setSelection(bankSetIndex, bankIndex, m_bankGrid->selectedCellIndex());
    }
    m_inspectorEditBefore = snapshotSingleCell(bankSetIndex, bankIndex, m_bankGrid->selectedCellIndex());
    refreshPreviewForSelectedCell();
    const QString label = dst.name.isEmpty() ? tr("Bank %1").arg(bankIndex + 1) : dst.name;
    if (layerCells > 0) {
        statusBar()->showMessage(
            tr("Pasted bank into “%1”; layer settings applied across banks (%2 cell updates)")
                .arg(label)
                .arg(layerCells),
            3000);
    } else {
        statusBar()->showMessage(tr("Pasted bank into “%1”").arg(label), 3000);
    }
}

void MainWindow::renameBank(int bankSetIndex, int bankIndex)
{
    if (!m_project || !m_bankGrid) {
        return;
    }
    if (bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    auto& set = m_project->bankSets[bankSetIndex];
    if (bankIndex < 0 || bankIndex >= set.banks.size()) {
        return;
    }
    Bank& bank = set.banks[bankIndex];
    const Bank beforeBank = bank;
    const QString current =
        bank.name.isEmpty() ? tr("Bank %1").arg(bankIndex + 1) : bank.name;
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Rename bank"), tr("Bank name:"),
                                         QLineEdit::Normal, current, &ok);
    if (!ok) {
        return;
    }
    name = name.trimmed();
    if (name.isEmpty()) {
        name = QStringLiteral("Bank %1").arg(bankIndex + 1);
    }
    bank.name = name;
    if (m_undoStack) {
        m_undoStack->push(new BankReplaceCommand(m_project.get(), bankSetIndex, bankIndex,
                                                 beforeBank, bank, tr("Rename bank")));
    }
    m_bankGrid->refresh();
    statusBar()->showMessage(tr("Renamed bank to “%1”").arg(name), 3000);
}

void MainWindow::renameCell(int bankSetIndex, int bankIndex, int cellIndex)
{
    if (!m_project || !m_bankGrid) {
        return;
    }
    if (bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    auto& set = m_project->bankSets[bankSetIndex];
    if (bankIndex < 0 || bankIndex >= set.banks.size()) {
        return;
    }
    auto& bank = set.banks[bankIndex];
    if (cellIndex < 0 || cellIndex >= bank.cells.size()) {
        return;
    }
    Cell& cell = bank.cells[cellIndex];
    if (!cellAllowsRename(cell)) {
        return;
    }
    auto before = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Rename cell"), tr("Cell name:"),
                                         QLineEdit::Normal, cell.name, &ok);
    if (!ok) {
        return;
    }
    cell.name = name.trimmed();
    if (before) {
        if (auto after = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex)) {
            pushCellsReplaceCommand({*before}, {*after}, tr("Rename cell"));
        }
    }
    m_bankGrid->refresh();
    if (m_inspector) {
        m_inspector->setSelection(bankSetIndex, bankIndex, cellIndex);
    }
    m_inspectorEditBefore = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
    statusBar()->showMessage(cell.name.isEmpty()
                                 ? tr("Cleared cell name")
                                 : tr("Renamed cell to “%1”").arg(cell.name),
                             3000);
}

void MainWindow::onCellEdited(int bankSetIndex, int bankIndex, int cellIndex)
{
    // Do not call m_bankGrid->refresh() here: it clears/rebuilds all filmstrip thumbnails
    // and stalls the UI on every inspector slider tick while video keeps decoding.
    if (m_inspectorEditLayer >= kUserLayerMin && m_inspectorEditLayer < kMixLayers) {
        const DeckSlot& slot = m_layerSlots[static_cast<size_t>(m_inspectorEditLayer)];
        if (slot.bankSet == bankSetIndex && slot.bank == bankIndex && slot.cell == cellIndex) {
            snapshotLayerKeyingFromCell(m_inspectorEditLayer, cellAtDeck(slot));
            syncInspectorLayerKeyingOverride();
        }
    } else {
        const int layer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
        if (layer >= 0) {
            snapshotLayerKeyingFromCell(layer, cellAtDeck(m_layerSlots[static_cast<size_t>(layer)]));
            m_inspectorEditLayer = layer;
            syncInspectorLayerKeyingOverride();
        }
    }
    const int layer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
    if (layer >= 0) {
        if (const Cell* c = cellAtDeck(m_layerSlots[static_cast<size_t>(layer)])) {
            if (c->visual.type == VisualType::Media) {
                m_decoders[layer]->setLooping(playModeUsesLoop(c->props.playMode));
                if (m_audioDecoders[layer]->isOpen()) {
                    m_audioDecoders[layer]->setLooping(playModeUsesLoop(c->props.playMode));
                }
                if (c->props.clipPaused) {
                    m_decoders[layer]->pause();
                    m_audioDecoders[layer]->pause();
                } else {
                    m_decoders[layer]->play();
                    if (m_audioDecoders[layer]->isOpen()) {
                        m_audioDecoders[layer]->play();
                    }
                }
            }
        }
    }
    pruneActiveMixerFilterCells();
    const Cell* playingCell =
        layer >= 0 ? cellAtDeck(m_layerSlots[static_cast<size_t>(layer)]) : nullptr;
    const bool feedbackPlaying = playingCell
        && playingCell->visual.type == VisualType::Generator
        && playingCell->visual.generator == GeneratorKind::InternalFeedback;
    if (feedbackPlaying) {
        syncLiveMixerOpacityForCell(bankSetIndex, bankIndex, cellIndex);
        syncPlayingFeedbackCellToMixer(bankSetIndex, bankIndex, cellIndex);
    } else {
        syncLiveMixerOpacityForCell(bankSetIndex, bankIndex, cellIndex);
        scheduleMixerUpdateFromCells();
    }
}

void MainWindow::onInspectorVisualSeekStep(int seconds)
{
    if (!m_inspector) {
        return;
    }
    const int bs = m_inspector->selectedBankSetIndex();
    const int b  = m_inspector->selectedBankIndex();
    const int ci = m_inspector->selectedCellIndex();
    const int layer = findLayerPlayingCell(bs, b, ci);
    if (layer < 0 || !m_decoders[layer]->isOpen()) {
        return;
    }
    qint64 cur = m_decoders[layer]->lastPresentedPtsMs();
    if (cur < 0) {
        cur = 0;
    }
    qint64 tgt = cur + qint64(seconds) * 1000;
    const qint64 dur = m_decoders[layer]->durationMs();
    if (dur > 1) {
        tgt = qBound(qint64(0), tgt, dur - 1);
    } else {
        tgt = qMax(qint64(0), tgt);
    }
    m_decoders[layer]->seek(tgt);
    if (m_audioDecoders[layer]->isOpen()) {
        m_audioDecoders[layer]->seek(tgt);
    }
}

void MainWindow::onInspectorScratchApply()
{
    if (!m_inspector) {
        return;
    }
    const int bs = m_inspector->selectedBankSetIndex();
    const int b  = m_inspector->selectedBankIndex();
    const int ci = m_inspector->selectedCellIndex();
    const int layer = findLayerPlayingCell(bs, b, ci);
    if (layer < 0 || !m_decoders[layer]->isOpen()) {
        return;
    }
    const Cell* c = cellAtDeck(m_layerSlots[static_cast<size_t>(layer)]);
    if (!c) {
        return;
    }
    const qint64 dur = m_decoders[layer]->durationMs();
    if (dur <= 0) {
        return;
    }
    const qint64 t = qBound(qint64(0),
                            qint64(qBound(0.0, c->props.scratchHeadU, 1.0) * double(dur - 1)),
                            dur - 1);
    m_decoders[layer]->seek(t);
    if (m_audioDecoders[layer]->isOpen()) {
        m_audioDecoders[layer]->seek(t);
    }
}

void MainWindow::setupInputMapping()
{
    m_inputRouter = std::make_unique<pvj::input::InputRouter>(this);
    m_midiInput   = std::make_unique<pvj::input::MidiInput>(this);

    connect(m_midiInput.get(), &pvj::input::MidiInput::messageReceived,
            this,
            [this](const QByteArray& b) {
                if (!m_inputRouter) {
                    return;
                }
                m_inputRouter->handleMidiBytes(
                    reinterpret_cast<const unsigned char*>(b.constData()),
                    size_t(b.size()));
            },
            Qt::DirectConnection);

    connect(m_inputRouter.get(), &pvj::input::InputRouter::triggerCell,
            this, &MainWindow::onInputTriggerCell);
    connect(m_inputRouter.get(), &pvj::input::InputRouter::releaseCell,
            this, &MainWindow::onInputReleaseCell);
    connect(m_inputRouter.get(), &pvj::input::InputRouter::bankNext,
            this, &MainWindow::onInputBankNext);
    connect(m_inputRouter.get(), &pvj::input::InputRouter::bankPrev,
            this, &MainWindow::onInputBankPrev);
    connect(m_inputRouter.get(), &pvj::input::InputRouter::bankSelect,
            this, &MainWindow::onInputBankSelect);
    connect(m_inputRouter.get(), &pvj::input::InputRouter::propertyValueChanged,
            this, &MainWindow::onInputPropertyMapped);
    connect(m_inputRouter.get(), &pvj::input::InputRouter::propertyToggleRequested,
            this, &MainWindow::onInputPropertyToggle);
    connect(m_inputRouter.get(), &pvj::input::InputRouter::mixLayerDirect,
            this, &MainWindow::onMixLayerDirect);

    connect(m_inputRouter.get(), &pvj::input::InputRouter::learnFinished,
            this, [this](const QString& msg) {
        commitLearnUndoIfNeeded(tr("Learn mapping"));
        syncInspectorEditBaselines();
        statusBar()->showMessage(msg, 6000);
        if (m_inspector) {
            m_inspector->refreshFromModel();
        }
        if (m_bankGrid) {
            m_bankGrid->refresh();
        }
        if (m_filterEditor) {
            m_filterEditor->refreshMidiMapOverlays();
        }
    });
    connect(m_inputRouter.get(), &pvj::input::InputRouter::triggerMappingsChanged,
            this, [this]() {
        syncInspectorEditBaselines();
        if (m_inspector) {
            m_inspector->refreshFromModel();
        }
        if (m_bankGrid) {
            m_bankGrid->refresh();
        }
        if (m_filterEditor) {
            m_filterEditor->refreshMidiMapOverlays();
        }
    });
    connect(m_inputRouter.get(), &pvj::input::InputRouter::propertyMappingsChanged,
            this, [this]() {
        syncInspectorEditBaselines();
        if (m_inspector) {
            m_inspector->refreshFromModel();
        }
        if (m_bankGrid) {
            m_bankGrid->refresh();
        }
        if (m_filterEditor) {
            m_filterEditor->refreshMidiMapOverlays();
        }
    });
    connect(m_inputRouter.get(), &pvj::input::InputRouter::learnCancelled,
            this, [this]() {
        m_learnBaselineValid = false;
        m_learnCellBefore.reset();
        statusBar()->showMessage(tr("Learn cancelled"), 3000);
    });
    connect(m_inputRouter.get(), &pvj::input::InputRouter::learnHint,
            this, [this](const QString& msg) {
        statusBar()->showMessage(msg, 5000);
    });

    qApp->installEventFilter(this);
    m_midiInput->start();
    populateMidiPortMenu();
    showMidiInputStatus();
}

void MainWindow::populateMidiPortMenu()
{
    if (!m_midiPortMenu || !m_midiInput) {
        return;
    }
    m_midiPortMenu->clear();

    const QStringList ports = m_midiInput->portNames();
    const QStringList open  = m_midiInput->openPortNames();

    if (ports.isEmpty()) {
        QAction* none = m_midiPortMenu->addAction(tr("(no MIDI inputs detected)"));
        none->setEnabled(false);
    } else {
        for (const QString& name : ports) {
            QAction* act = m_midiPortMenu->addAction(name);
            act->setCheckable(true);
            act->setChecked(open.contains(name));
            act->setData(name);
            connect(act, &QAction::triggered, this, [this]() {
                QStringList next;
                for (QAction* a : m_midiPortMenu->actions()) {
                    if (a && a->isCheckable() && a->isChecked()) {
                        next.append(a->text());
                    }
                }
                applyMidiInputPorts(next);
            });
        }
    }

    m_midiPortMenu->addSeparator();
    QAction* refresh = m_midiPortMenu->addAction(tr("Refresh ports"));
    connect(refresh, &QAction::triggered, this, [this]() {
        m_midiInput->reopenPreferredPort();
        populateMidiPortMenu();
        showMidiInputStatus();
    });
    m_midiPortMenu->addAction(tr("Choose in Preferences…"), this, &MainWindow::onEditPreferences);
}

void MainWindow::showMidiInputStatus()
{
    if (!m_midiInput) {
        return;
    }
    const QStringList open = m_midiInput->openPortNames();
    if (!open.isEmpty()) {
        statusBar()->showMessage(
            tr("MIDI input (%1): %2").arg(open.size()).arg(open.join(QStringLiteral(", "))),
            6000);
        return;
    }
    if (m_midiInput->portNames().isEmpty()) {
        statusBar()->showMessage(
            tr("MIDI: no input devices found. Connect a controller or install a virtual MIDI cable."),
            10000);
    } else {
        statusBar()->showMessage(
            tr("MIDI: port could not be opened — choose Edit → Preferences or Mapping → MIDI input port."),
            10000);
    }
}

bool MainWindow::eventFilter(QObject* /*watched*/, QEvent* event)
{
    if (event->type() != QEvent::KeyPress) {
        return false;
    }
    if (!m_inputRouter) {
        return false;
    }
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->isAutoRepeat()) {
        return false;
    }
    if (!focusAllowsGlobalShortcuts()) {
        return false;
    }
    const bool ctrl = (ke->modifiers() & Qt::ControlModifier);
    const bool shift = (ke->modifiers() & Qt::ShiftModifier);
    const bool alt = (ke->modifiers() & Qt::AltModifier);
    const bool meta = (ke->modifiers() & Qt::MetaModifier);
    if (ctrl && !meta) {
        if (ke->key() == Qt::Key_C && !shift && !alt) {
            copySelectedCell();
            return true;
        }
        if (ke->key() == Qt::Key_V && !shift && !alt) {
            pasteIntoSelectedCellWithMidi();
            return true;
        }
        if (ke->key() == Qt::Key_V && shift && !alt) {
            pasteIntoSelectedCellWithoutMidi();
            return true;
        }
        if (ke->key() == Qt::Key_V && alt && !shift) {
            pasteIntoSelectedCellParamsWithMidi();
            return true;
        }
    }
    if (m_inputRouter->handleKeyEvent(ke->keyCombination(), true)) {
        return true;
    }
    return false;
}

void MainWindow::onLearnCellTrigger()
{
    if (!m_inputRouter || !m_bankGrid) {
        return;
    }
    const int ci = m_bankGrid->selectedCellIndex();
    if (ci < 0) {
        QMessageBox::information(this, tr("Learn"),
                                 tr("Select a cell in the bank grid first."));
        return;
    }
    captureLearnBaseline();
    m_inputRouter->beginLearnCellTrigger(m_bankGrid->activeBankSetIndex(),
                                         m_bankGrid->activeBankIndex(),
                                         ci);
    statusBar()->showMessage(tr("Learn: press a MIDI note or a keyboard key…"), 15000);
}

void MainWindow::onLearnPropertyCc()
{
    if (!m_inputRouter || !m_bankGrid) {
        return;
    }
    const int ci = m_bankGrid->selectedCellIndex();
    if (ci < 0) {
        QMessageBox::information(this, tr("Learn"),
                                 tr("Select a cell in the bank grid first."));
        return;
    }
    const QStringList items = pvj::core::PropertyRegistry::allPropertyNames();
    QStringList display;
    display.reserve(items.size());
    for (const QString& id : items) {
        display.append(QStringLiteral("%1 — %2").arg(id, pvj::core::PropertyRegistry::labelFor(id)));
    }
    bool ok = false;
    const QString pick = QInputDialog::getItem(this, tr("Property"),
                                               tr("Map MIDI CC to:"),
                                               display, 0, false, &ok);
    if (!ok) {
        return;
    }
    const int idx = display.indexOf(pick);
    const QString prop = (idx >= 0) ? items.at(idx) : pick.section(QStringLiteral(" — "), 0, 0);
    captureLearnBaseline();
    m_inputRouter->beginLearnPropertyCc(m_bankGrid->activeBankSetIndex(),
                                        m_bankGrid->activeBankIndex(),
                                        ci,
                                        prop);
    statusBar()->showMessage(tr("Learn: move a MIDI CC (0–127)…"), 15000);
}

void MainWindow::onLearnMidiFadeTransparency()
{
    if (!m_inputRouter || !m_bankGrid) {
        return;
    }
    const int ci = m_bankGrid->selectedCellIndex();
    if (ci < 0) {
        QMessageBox::information(this, tr("Learn"),
                                 tr("Select a cell in the bank grid first."));
        return;
    }
    captureLearnBaseline();
    m_inputRouter->beginLearnPropertyCc(m_bankGrid->activeBankSetIndex(),
                                        m_bankGrid->activeBankIndex(),
                                        ci,
                                        QStringLiteral("transparency"));
    statusBar()->showMessage(
        tr("Learn: move a MIDI CC — it will control transparency (fade) for the selected cell."), 15000);
}

void MainWindow::onCancelLearn()
{
    if (m_inputRouter) {
        m_inputRouter->cancelLearn();
    }
}

void MainWindow::onMidiMappingEditToggled(bool on)
{
    if (m_bankGrid) {
        m_bankGrid->setMidiMappingEditMode(on);
    }
    if (m_inspector) {
        m_inspector->setMidiMappingEditMode(on);
    }
    if (m_filterEditor) {
        m_filterEditor->setMidiMappingEditMode(on);
    }
    if (m_inputRouter && !on) {
        m_inputRouter->cancelLearn();
    }
    if (on) {
        statusBar()->showMessage(
            tr("Mapping mode (Ctrl+M): click a cell in the green grid, then press a keyboard key or MIDI note "
               "to assign a clip trigger. Inspector: right-click a control to learn CC / note."),
            12000);
    } else {
        statusBar()->showMessage(tr("Mapping mode off."), 3000);
    }
    updateWindowTitle();
}

void MainWindow::onCopyKeyboardMappingToAllBanks()
{
    if (!m_project || !m_bankGrid) {
        return;
    }
    const int bankSetIndex = m_bankGrid->activeBankSetIndex();
    const int bankIndex    = m_bankGrid->activeBankIndex();
    if (bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    const auto& set = m_project->bankSets[bankSetIndex];
    if (bankIndex < 0 || bankIndex >= set.banks.size()) {
        return;
    }

    const QList<pvj::core::TriggerMapping> beforeTriggers = m_project->triggerMappings;
    int applied = 0;
    const int cellCount = set.banks[bankIndex].cells.size();
    for (int ci = 0; ci < cellCount; ++ci) {
        if (applyKeyboardTriggerToAllBanks(bankSetIndex, bankIndex, ci)) {
            ++applied;
        }
    }
    if (applied == 0) {
        statusBar()->showMessage(tr("No keyboard or MIDI note triggers on this bank"), 4000);
        return;
    }

    if (m_undoStack) {
        m_undoStack->push(new TriggerMappingsReplaceCommand(
            m_project.get(), beforeTriggers, m_project->triggerMappings,
            tr("Copy keyboard mapping to all banks")));
    }
    markProjectDirty();
    if (m_bankGrid) {
        m_bankGrid->refresh();
    }
    statusBar()->showMessage(
        tr("Applied keyboard/MIDI note triggers for %1 cells to all banks").arg(applied),
        4000);
}

void MainWindow::onCopyLayerMappingToAllBanks()
{
    if (!m_project || !m_bankGrid) {
        return;
    }
    const int bankSetIndex = m_bankGrid->activeBankSetIndex();
    const int bankIndex    = m_bankGrid->activeBankIndex();
    if (bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    const auto& set = m_project->bankSets[bankSetIndex];
    if (bankIndex < 0 || bankIndex >= set.banks.size()) {
        return;
    }

    QVector<CellSnapshot> before = snapshotAllCellsInBankSet(bankSetIndex);
    int cellUpdates = 0;
    const Bank& srcBank = set.banks[bankIndex];
    for (int ci = 0; ci < srcBank.cells.size(); ++ci) {
        const int preferred = qBound(0, srcBank.cells[ci].props.preferredLayer, 12);
        cellUpdates +=
            applyPreferredLayerToAllBanks(bankSetIndex, bankIndex, ci, preferred);
    }
    QVector<CellSnapshot> after = snapshotAllCellsInBankSet(bankSetIndex);
    pushCellsReplaceCommand(std::move(before), std::move(after),
                            tr("Copy layer selection to all banks"));

    if (m_bankGrid) {
        m_bankGrid->refresh();
    }
    if (m_inspector) {
        m_inspector->refreshFromModel();
        m_inspectorEditBefore = snapshotSingleCell(bankSetIndex, bankIndex,
                                                   m_bankGrid->selectedCellIndex());
    }
    updateMixerFromPlayingCells();
    statusBar()->showMessage(
        tr("Copied layer selection for all cells to other banks (%1 cell updates)")
            .arg(cellUpdates),
        4000);
}

void MainWindow::onCopyMidiMappingToAllBanks()
{
    if (!m_project || !m_bankGrid) {
        return;
    }
    const int bankSetIndex = m_bankGrid->activeBankSetIndex();
    const int bankIndex    = m_bankGrid->activeBankIndex();
    if (bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    const auto& set = m_project->bankSets[bankSetIndex];
    if (bankIndex < 0 || bankIndex >= set.banks.size()) {
        return;
    }

    QVector<CellSnapshot> before = snapshotAllCellsInBankSet(bankSetIndex);
    int cellUpdates = 0;
    int cellsWithMidi = 0;
    const Bank& srcBank = set.banks[bankIndex];
    for (int ci = 0; ci < srcBank.cells.size(); ++ci) {
        const Cell& src = srcBank.cells[ci];
        if (src.propertyMappings.isEmpty()) {
            // Still clear/copy empty mappings so other banks match source.
            cellUpdates += applyMidiMappingsToAllBanks(bankSetIndex, bankIndex, ci, src);
            continue;
        }
        ++cellsWithMidi;
        cellUpdates += applyMidiMappingsToAllBanks(bankSetIndex, bankIndex, ci, src);
    }
    QVector<CellSnapshot> after = snapshotAllCellsInBankSet(bankSetIndex);
    pushCellsReplaceCommand(std::move(before), std::move(after),
                            tr("Copy MIDI mapping to all banks"));

    if (m_bankGrid) {
        m_bankGrid->refresh();
    }
    if (m_inspector) {
        m_inspector->refreshFromModel();
        m_inspectorEditBefore = snapshotSingleCell(bankSetIndex, bankIndex,
                                                   m_bankGrid->selectedCellIndex());
    }
    if (m_filterEditor) {
        m_filterEditor->refreshMidiMapOverlays();
    }
    statusBar()->showMessage(
        tr("Copied MIDI mappings for all cells (%1 with mappings, %2 bank cell updates)")
            .arg(cellsWithMidi)
            .arg(cellUpdates),
        4000);
}

void MainWindow::onApplyMappingToAllBanksToggled(bool on)
{
    if (!on) {
        return;
    }

    auto resetCheckbox = [this]() {
        if (m_actApplyMappingAllBanks) {
            QSignalBlocker blocker(m_actApplyMappingAllBanks);
            m_actApplyMappingAllBanks->setChecked(false);
        }
    };

    if (!m_project || !m_bankGrid) {
        resetCheckbox();
        return;
    }

    const int bankSetIndex = m_bankGrid->activeBankSetIndex();
    const int bankIndex    = m_bankGrid->activeBankIndex();
    const int cellIndex    = m_bankGrid->selectedCellIndex();
    if (cellIndex < 0) {
        statusBar()->showMessage(tr("Select a cell first"), 3000);
        resetCheckbox();
        return;
    }

    Cell* srcCell = const_cast<Cell*>(cellAtDeck({bankSetIndex, bankIndex, cellIndex}));
    if (!srcCell) {
        resetCheckbox();
        return;
    }

    QVector<CellSnapshot> before = snapshotCellAcrossBanks(bankSetIndex, bankIndex, cellIndex);

    const int playingLayer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
    if (playingLayer >= kUserLayerMin && playingLayer < kMixLayers) {
        const LayerKeyingState& layerKeying =
            m_layerKeying[static_cast<size_t>(playingLayer)];
        if (layerKeying.valid) {
            applyLayerKeyingStateToCell(*srcCell, layerKeying);
        }
    }

    const Cell src = *srcCell;

    const int copied = applyCellLayerSettingsToAllBanks(bankSetIndex, bankIndex, cellIndex, src);
    QVector<CellSnapshot> after = snapshotCellAcrossBanks(bankSetIndex, bankIndex, cellIndex);
    pushCellsReplaceCommand(std::move(before), std::move(after), tr("Apply mapping to all banks"));

    refreshAfterMappingApplyToAllBanks(bankSetIndex, bankIndex, cellIndex, playingLayer);

    statusBar()->showMessage(
        tr("Applied cell %1 mappings, layer, and playback settings to %2 banks")
            .arg(cellIndex + 1)
            .arg(copied),
        3000);
    resetCheckbox();
}

void MainWindow::onInspectorMidiLearnCc(const QString& propertyName)
{
    if (!m_inputRouter || !m_bankGrid) {
        return;
    }
    const int ci = m_bankGrid->selectedCellIndex();
    if (ci < 0) {
        QMessageBox::information(this, tr("Learn"),
                                 tr("Select a cell in the bank grid first."));
        return;
    }
    captureLearnBaseline();
    m_inputRouter->beginLearnPropertyCc(m_bankGrid->activeBankSetIndex(),
                                        m_bankGrid->activeBankIndex(),
                                        ci,
                                        propertyName);
    statusBar()->showMessage(tr("Learn: move a MIDI CC (0–127)…"), 15000);
}

void MainWindow::onInspectorMidiLearnNote(const QString& propertyName, bool toggle, double buttonValue)
{
    if (!m_inputRouter || !m_bankGrid) {
        return;
    }
    const int ci = m_bankGrid->selectedCellIndex();
    if (ci < 0) {
        QMessageBox::information(this, tr("Learn"),
                                 tr("Select a cell in the bank grid first."));
        return;
    }
    captureLearnBaseline();
    m_inputRouter->beginLearnPropertyNote(
        m_bankGrid->activeBankSetIndex(),
        m_bankGrid->activeBankIndex(),
        ci,
        propertyName,
        toggle ? pvj::core::PropertyButtonMode::Toggle : pvj::core::PropertyButtonMode::SetOnPress,
        buttonValue);
    statusBar()->showMessage(tr("Learn: press a MIDI note…"), 15000);
}

void MainWindow::onInspectorMidiClearMapping(const QString& propertyName)
{
    if (!m_inputRouter || !m_bankGrid) {
        return;
    }
    const int ci = m_bankGrid->selectedCellIndex();
    if (ci < 0) {
        return;
    }
    const int bs = m_bankGrid->activeBankSetIndex();
    const int bi = m_bankGrid->activeBankIndex();
    auto before = snapshotSingleCell(bs, bi, ci);
    m_inputRouter->clearPropertyMappingForCell(bs, bi, ci, propertyName);
    if (before) {
        if (auto after = snapshotSingleCell(bs, bi, ci)) {
            if (before->cell.propertyMappings.size() != after->cell.propertyMappings.size()) {
                pushCellsReplaceCommand({*before}, {*after}, tr("Clear MIDI mapping"));
            }
        }
    }
    statusBar()->showMessage(tr("Cleared MIDI mapping for %1").arg(propertyName), 4000);
    if (m_inspector) {
        m_inspector->refreshFromModel();
    }
    if (m_bankGrid) {
        m_bankGrid->refresh();
    }
    if (m_filterEditor) {
        m_filterEditor->refreshMidiMapOverlays();
    }
}

void MainWindow::onLearnBankNext()
{
    if (!m_inputRouter || !m_bankGrid) {
        return;
    }
    captureLearnBaseline();
    m_inputRouter->beginLearnBankNav(pvj::core::TriggerTarget::BankNext, m_bankGrid->activeBankSetIndex());
    statusBar()->showMessage(tr("Learn: press a MIDI note for bank next…"), 15000);
}

void MainWindow::onLearnBankPrev()
{
    if (!m_inputRouter || !m_bankGrid) {
        return;
    }
    captureLearnBaseline();
    m_inputRouter->beginLearnBankNav(pvj::core::TriggerTarget::BankPrev, m_bankGrid->activeBankSetIndex());
    statusBar()->showMessage(tr("Learn: press a MIDI note for bank previous…"), 15000);
}

void MainWindow::onLearnBankSelect()
{
    if (!m_inputRouter || !m_bankGrid || !m_project) {
        return;
    }
    const int bs = m_bankGrid->activeBankSetIndex();
    if (bs < 0 || bs >= m_project->bankSets.size()) {
        return;
    }
    const int nBanks = m_project->bankSets[bs].banks.size();
    if (nBanks <= 0) {
        return;
    }
    bool ok = false;
    const int oneBased = QInputDialog::getInt(this, tr("Select bank"),
                                              tr("Bank number (1–%1):").arg(nBanks),
                                              m_bankGrid->activeBankIndex() + 1, 1, nBanks, 1, &ok);
    if (!ok) {
        return;
    }
    captureLearnBaseline();
    m_inputRouter->beginLearnBankNav(pvj::core::TriggerTarget::BankSelect, bs, oneBased - 1);
    statusBar()->showMessage(tr("Learn: press a MIDI note to jump to this bank…"), 15000);
}

void MainWindow::onMidiLearnCellTriggerFromGrid(int bankSetIndex, int bankIndex, int cellIndex)
{
    if (!m_inputRouter || !m_bankGrid) {
        return;
    }
    m_bankGrid->selectCell(cellIndex);
    if (m_inspector) {
        m_inspector->setSelection(bankSetIndex, bankIndex, cellIndex);
    }
    captureLearnBaseline();
    m_learnCellBefore = snapshotSingleCell(bankSetIndex, bankIndex, cellIndex);
    m_inputRouter->beginLearnCellTrigger(bankSetIndex, bankIndex, cellIndex);
    statusBar()->showMessage(tr("Mapping: press a keyboard key or MIDI note for this cell…"), 15000);
}

void MainWindow::onInputTriggerCell(int bankSetIndex, int bankIndex, int cellIndex, bool fromMidiNote)
{
    if (bankSetIndex < 0 || cellIndex < 0) {
        return;
    }
    const int bank = resolveCellTriggerBankIndex(bankIndex);
    if (bank < 0) {
        return;
    }

    if (m_bankGrid) {
        m_bankGrid->revealAndSelectCell(bankSetIndex, bank, cellIndex);
    } else if (m_inspector) {
        m_inspector->setSelection(bankSetIndex, bank, cellIndex);
    }

    if (fromMidiNote) {
        // Momentary MIDI: note-on starts; note-off stops (no toggle on repeat note-on).
        if (findLayerPlayingCell(bankSetIndex, bank, cellIndex) >= 0) {
            return;
        }
        onCellTriggered(bankSetIndex, bank, cellIndex, false);
        return;
    }

    onCellTriggered(bankSetIndex, bank, cellIndex, true);
}

void MainWindow::onInputReleaseCell(int bankSetIndex, int bankIndex, int cellIndex)
{
    if (bankSetIndex < 0 || cellIndex < 0) {
        return;
    }
    const int bank = resolveCellTriggerBankIndex(bankIndex);
    if (bank < 0) {
        return;
    }

    const int playingLayer = findLayerPlayingCell(bankSetIndex, bank, cellIndex);
    if (playingLayer < 0) {
        return;
    }

    stopMixLayer(playingLayer);
    syncMixLayerFromCell(playingLayer);
    syncMixerToFullscreen();
    syncMixSlotHighlightsToBankGrid();
    refreshPreviewForSelectedCell();
}

void MainWindow::onInputBankNext(int bankSetIndex)
{
    if (!m_bankGrid) {
        return;
    }
    if (m_bankGrid->activeBankSetIndex() != bankSetIndex) {
        m_bankGrid->setBankSetIndex(bankSetIndex);
    }
    m_bankGrid->stepBank(1);
}

void MainWindow::onInputBankPrev(int bankSetIndex)
{
    if (!m_bankGrid) {
        return;
    }
    if (m_bankGrid->activeBankSetIndex() != bankSetIndex) {
        m_bankGrid->setBankSetIndex(bankSetIndex);
    }
    m_bankGrid->stepBank(-1);
}

void MainWindow::onInputBankSelect(int bankSetIndex, int bankIndex)
{
    if (!m_bankGrid) {
        return;
    }
    m_bankGrid->setBankSetIndex(bankSetIndex);
    m_bankGrid->selectBank(bankIndex);
}

void MainWindow::onInputPropertyMapped(int bankSetIndex, int bankIndex, int cellIndex,
                                     const QString& propertyName, double value)
{
    if (!m_project) {
        return;
    }
    // MIDI property writes apply only to cells currently on a mix layer.
    if (findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex) < 0) {
        return;
    }
    if (propertyName.compare(QStringLiteral("transparency"), Qt::CaseInsensitive) == 0) {
        for (int i = 0; i < kMixLayers; ++i) {
            const auto& s = m_layerSlots[static_cast<size_t>(i)];
            if (s.bankSet == bankSetIndex && s.bank == bankIndex && s.cell == cellIndex) {
                m_layerFadeAnimating[static_cast<size_t>(i)] = false;
                break;
            }
        }
    }
    if (bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    auto& set = m_project->bankSets[bankSetIndex];
    if (bankIndex < 0 || bankIndex >= set.banks.size()) {
        return;
    }
    auto& bank = set.banks[bankIndex];
    if (cellIndex < 0 || cellIndex >= bank.cells.size()) {
        return;
    }
    if (!applyMappedProperty(bank.cells[cellIndex], propertyName, value)) {
        return;
    }
    markProjectDirty();

    for (int i = kUserLayerMin; i < kMixLayers; ++i) {
        const DeckSlot& slot = m_layerSlots[static_cast<size_t>(i)];
        if (slot.bankSet == bankSetIndex && slot.bank == bankIndex && slot.cell == cellIndex) {
            snapshotLayerKeyingFromCell(i, cellAtDeck(slot));
        }
    }

    const bool inspectorShowsMappedCell =
        m_inspector && m_inspector->selectedBankSetIndex() == bankSetIndex
        && m_inspector->selectedBankIndex() == bankIndex && m_inspector->selectedCellIndex() == cellIndex;
    if (inspectorShowsMappedCell && m_midiInspectorDebounceTimer) {
        m_midiInspectorDebounceTimer->start(33);
    }

    const int playingLayer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
    const Cell* playingCell = playingLayer >= 0
        ? cellAtDeck(m_layerSlots[static_cast<size_t>(playingLayer)]) : nullptr;
    const bool feedbackPlaying = playingCell
        && playingCell->visual.type == VisualType::Generator
        && playingCell->visual.generator == GeneratorKind::InternalFeedback;
    if (feedbackPlaying) {
        syncPlayingFeedbackCellAfterPropertyChange(bankSetIndex, bankIndex, cellIndex,
                                                   propertyName);
        return;
    }
    syncLiveMixerOpacityForCell(bankSetIndex, bankIndex, cellIndex);
    const QString resolved = pvj::core::PropertyRegistry::resolvePropertyId(propertyName);
    if (pvj::core::PropertyRegistry::isFilterParamProperty(resolved)) {
        syncFilterParamsToMixer(bankSetIndex, bankIndex, cellIndex);
    } else if (resolved.compare(QStringLiteral("transparency"), Qt::CaseInsensitive) != 0) {
        scheduleMixerUpdateFromCells();
    }
}

void MainWindow::onInputPropertyToggle(int bankSetIndex, int bankIndex, int cellIndex,
                                       const QString& propertyName)
{
    if (!m_project) {
        return;
    }
    if (findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex) < 0) {
        return;
    }
    if (propertyName.compare(QStringLiteral("transparency"), Qt::CaseInsensitive) == 0) {
        for (int i = 0; i < kMixLayers; ++i) {
            const auto& s = m_layerSlots[static_cast<size_t>(i)];
            if (s.bankSet == bankSetIndex && s.bank == bankIndex && s.cell == cellIndex) {
                m_layerFadeAnimating[static_cast<size_t>(i)] = false;
                break;
            }
        }
    }
    if (bankSetIndex < 0 || bankSetIndex >= m_project->bankSets.size()) {
        return;
    }
    auto& set = m_project->bankSets[bankSetIndex];
    if (bankIndex < 0 || bankIndex >= set.banks.size()) {
        return;
    }
    auto& bank = set.banks[bankIndex];
    if (cellIndex < 0 || cellIndex >= bank.cells.size()) {
        return;
    }
    if (!pvj::core::PropertyRegistry::toggleValue(bank.cells[cellIndex], propertyName)) {
        return;
    }
    m_inspector->refreshFromModel();
    m_bankGrid->refresh();
    const int playingLayer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
    const Cell* playingCell = playingLayer >= 0
        ? cellAtDeck(m_layerSlots[static_cast<size_t>(playingLayer)]) : nullptr;
    const bool feedbackPlaying = playingCell
        && playingCell->visual.type == VisualType::Generator
        && playingCell->visual.generator == GeneratorKind::InternalFeedback;
    if (feedbackPlaying) {
        syncPlayingFeedbackCellAfterPropertyChange(bankSetIndex, bankIndex, cellIndex,
                                                   propertyName);
    } else {
        const QString resolved = pvj::core::PropertyRegistry::resolvePropertyId(propertyName);
        if (pvj::core::PropertyRegistry::isFilterParamProperty(resolved)) {
            syncFilterParamsToMixer(bankSetIndex, bankIndex, cellIndex);
            syncMixerToFullscreen();
        } else {
            updateMixerFromPlayingCells();
        }
    }
}

} // namespace pvj::app

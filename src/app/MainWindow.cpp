#include "MainWindow.h"

#include "BankGridWidget.h"
#include "FilterNodeEditorWindow.h"
#include "MediaLibraryDock.h"
#include "ParameterInspector.h"
#include "PreferencesDialog.h"
#include "SplitterHelpers.h"

#include "core/AvcImporter.h"
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

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QCloseEvent>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QEvent>
#include <QFrame>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QScreen>
#include <QStatusBar>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

#include <QtMath>

#include <algorithm>
#include <limits>

namespace pvj::app {

using pvj::core::AvcImporter;
using pvj::core::MediaItem;
using pvj::core::Project;
using pvj::core::PvjSerializer;
using pvj::core::Vj2Importer;
using pvj::core::Cell;
using pvj::core::PlayMode;
using pvj::core::VisualType;
using pvj::core::GeneratorKind;

namespace {
constexpr const char* kPvjFilter = "PerformanieVJ Project (*.pvj)";
constexpr const char* kVj2Filter = "GrandVJ Project (*.vj2)";
constexpr const char* kAvcFilter = "Resolume Composition (*.avc)";
constexpr const char* kUiMainSplitterState = "ui/mainSplitterState";
constexpr const char* kOutputStageWidth    = "output/stageWidth";
constexpr const char* kOutputStageHeight   = "output/stageHeight";
constexpr int kDefaultStageWidth  = 1920;
constexpr int kDefaultStageHeight = 1080;
constexpr const char* kRecentProjectPaths = "recent/projectPaths";
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
    const QString p = raw.toLower();
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

/// Mix-slot direct keys (no Ctrl/Alt/Meta): 1–9, 0, minus, equal → slots 1–12 (0-based indices 0–11).
int mixSlotFromQtKey(int key)
{
    // Qualify with ::Qt — Qt 6.7+ qtextdocument.h introduces a nested `namespace Qt`
    // for mightBeRichText helpers; unqualified `Qt::Key_*` can bind there and fail to compile.
    // Keypad digits use the same Key_1…Key_9 / Key_0 codes as the main keyboard.
    switch (key) {
    case ::Qt::Key_1:
        return 0;
    case ::Qt::Key_2:
        return 1;
    case ::Qt::Key_3:
        return 2;
    case ::Qt::Key_4:
        return 3;
    case ::Qt::Key_5:
        return 4;
    case ::Qt::Key_6:
        return 5;
    case ::Qt::Key_7:
        return 6;
    case ::Qt::Key_8:
        return 7;
    case ::Qt::Key_9:
        return 8;
    case ::Qt::Key_0:
        return 9;
    case ::Qt::Key_Minus:
        return 10;
    case ::Qt::Key_Equal:
        return 11;
    default:
        return -1;
    }
}

int preferredLayerFromCell(const Cell* c)
{
    if (!c) {
        return 4;
    }
    return qBound(0, c->props.preferredLayer, 11);
}

} // namespace

int MainWindow::gpuLayerFromPreferred(int preferredLayer)
{
    return qBound(kUserLayerMin, preferredLayer + 1, kUserLayerMax);
}

namespace {

// #region agent log
void agentDebugLog(const char* location, const char* message, const char* hypothesisId,
                   const QJsonObject& data)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("sessionId"), QStringLiteral("f36697"));
    obj.insert(QStringLiteral("runId"), QStringLiteral("load-crash-3"));
    obj.insert(QStringLiteral("hypothesisId"), QString::fromUtf8(hypothesisId));
    obj.insert(QStringLiteral("location"), QString::fromUtf8(location));
    obj.insert(QStringLiteral("message"), QString::fromUtf8(message));
    obj.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
    obj.insert(QStringLiteral("data"), data);
    QFile f(QStringLiteral("d:/PerformanieVJ/debug-f36697.log"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        f.write("\n");
    }
}
// #endregion

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
        for (auto& n : chain) {
            const QString t = n.typeId.toLower();
            if (t == QLatin1String("chroma_key") || t == QLatin1String("luma_key")) {
                syncKeyNodeFromView(n, kv);
            }
        }
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
            [this](int /*bankSetIndex*/, int /*bankIndex*/, int /*cellIndex*/) {
                if (m_bankGrid) {
                    m_bankGrid->refresh();
                }
                if (m_inspector) {
                    m_inspector->refreshFromModel();
                }
                updateMixerFromPlayingCells();
            });
    connect(m_filterEditor.get(), &FilterNodeEditorWindow::midiLearnCcRequested, this,
            [this](int bankSetIndex, int bankIndex, int cellIndex, const QString& propertyId) {
                if (!m_inputRouter) {
                    return;
                }
                m_inputRouter->beginLearnPropertyCc(bankSetIndex, bankIndex, cellIndex, propertyId);
                statusBar()->showMessage(tr("Learn: move a MIDI CC (0–127)…"), 15000);
            });
    connect(m_filterEditor.get(), &FilterNodeEditorWindow::midiLearnNoteRequested, this,
            [this](int bankSetIndex, int bankIndex, int cellIndex, const QString& propertyId,
                   bool toggle, double buttonValue) {
                if (!m_inputRouter) {
                    return;
                }
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
                m_inputRouter->clearPropertyMappingForCell(bankSetIndex, bankIndex, cellIndex, propertyId);
                statusBar()->showMessage(tr("Cleared MIDI mapping for %1").arg(propertyId), 4000);
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
    connect(m_mixerUpdateDebounceTimer, &QTimer::timeout, this, [this]() {
        updateMixerFromPlayingCells();
        syncMixerToFullscreen();
    });

    setupMenus();
    setupInputMapping();
    rebindUiToProject();

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

    connect(m_inspector, &ParameterInspector::fullscreenOutputToggled,
            this, &MainWindow::onFullscreenOutputToggled);
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
}

MainWindow::~MainWindow()
{
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_mainSplit) {
        QSettings settings;
        settings.setValue(QLatin1String(kUiMainSplitterState), m_mainSplit->saveState());
    }
    if (m_actMidiMappingEdit && m_actMidiMappingEdit->isChecked()) {
        m_actMidiMappingEdit->setChecked(false);
    }
    if (m_fullscreenOut && m_fullscreenOut->isVisible()) {
        m_fullscreenOut->leaveFullscreen();
    }
    if (m_filterEditor && m_filterEditor->isVisible()) {
        m_filterEditor->close();
    }
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

    auto* upperSplit = new PvjSplitter(Qt::Horizontal, m_mainSplit);

    // Left of upper split: parameter inspector tabs
    m_inspector = new ParameterInspector(upperSplit);
    upperSplit->addWidget(m_inspector);
    connect(m_inspector, &ParameterInspector::cellChanged,
            this, &MainWindow::onCellEdited);
    connect(m_inspector, &ParameterInspector::cellPlaybackChanged,
            this, &MainWindow::onCellPlaybackChanged);

    // Right of upper split: layer preview + mixer output
    auto* previewHost = new QWidget(upperSplit);
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

    upperSplit->addWidget(previewHost);
    upperSplit->setStretchFactor(0, 2);
    upperSplit->setStretchFactor(1, 5);
    m_mainSplit->addWidget(upperSplit);

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
    {
        QSettings settings;
        const QByteArray state = settings.value(QLatin1String(kUiMainSplitterState)).toByteArray();
        if (!state.isEmpty()) {
            m_mainSplit->restoreState(state);
        }
    }
    centralLayout->addWidget(m_mainSplit, 1);

    connect(m_bankGrid, &BankGridWidget::cellSelected,
            this, [this](int bankSetIndex, int bankIndex, int cellIndex) {
        onCellSelected(bankSetIndex, bankIndex, cellIndex);
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
    connect(m_bankGrid, &BankGridWidget::cellTriggered,
            this, &MainWindow::onCellTriggered);
    connect(m_bankGrid, &BankGridWidget::cellEditRequested,
            this, &MainWindow::onCellEditRequested);
    connect(m_bankGrid, &BankGridWidget::cellPeekPreviewRequested,
            this, &MainWindow::onCellPeekPreviewRequested);
    connect(m_bankGrid, &BankGridWidget::midiLearnCellTriggerRequested,
            this, &MainWindow::onMidiLearnCellTriggerFromGrid);

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
    importMenu->addAction(tr("GrandVJ Project (.vj2)..."),      this, &MainWindow::onImportVj2);
    importMenu->addAction(tr("Resolume Composition (.avc)..."), this, &MainWindow::onImportAvc);

    fileMenu->addSeparator();

    auto* actSave = fileMenu->addAction(tr("&Save"), this, &MainWindow::onFileSave);
    actSave->setShortcut(QKeySequence::Save);

    auto* actSaveAs = fileMenu->addAction(tr("Save &As..."), this, &MainWindow::onFileSaveAs);
    actSaveAs->setShortcut(QKeySequence::SaveAs);

    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QMainWindow::close);

    auto* editMenu = menuBar()->addMenu(tr("&Edit"));
    auto* actPrefs = editMenu->addAction(tr("&Preferences..."), this, &MainWindow::onEditPreferences);
    actPrefs->setShortcut(QKeySequence::Preferences);
    actPrefs->setMenuRole(QAction::PreferencesRole);

    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_mediaDock->toggleViewAction());
    viewMenu->addAction(tr("Bank grid size..."), this, &MainWindow::onConfigureBankGrid);

    auto* mapMenu = menuBar()->addMenu(tr("&Mapping"));
    m_actMidiMappingEdit = mapMenu->addAction(tr("Edit MIDI &mapping mode"));
    m_actMidiMappingEdit->setCheckable(true);
    m_actMidiMappingEdit->setShortcut(QKeySequence(QStringLiteral("Ctrl+M")));
    m_actMidiMappingEdit->setShortcutContext(Qt::ApplicationShortcut);
    m_actMidiMappingEdit->setToolTip(
        tr("Highlights the bank grid in green. Click a cell, then press a MIDI note or key to assign a clip trigger. "
           "Use “Learn MIDI CC → property…” for faders on the selected cell."));
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
    mapMenu->addSeparator();
    mapMenu->addAction(tr("Cancel learn"), this, &MainWindow::onCancelLearn);
    mapMenu->addSeparator();
    mapMenu->addAction(tr("Mix slot shortcuts…"), this, [this] {
        QMessageBox::information(
            this,
            tr("Mix slot shortcuts"),
            tr("<p><b>Keyboard:</b> <b>1</b>–<b>9</b>, <b>0</b>, <b>-</b>, <b>=</b> "
               "(without Ctrl, Alt, or Meta) → mix slots 1–12; same keys on the keypad.</p>"
               "<p><b>MIDI:</b> <b>Channel 16</b>, notes <b>60–71</b> (C4–B4) → mix slots 1–12. "
               "These are handled before project trigger mappings.</p>"
               "<p><b>Grid:</b> <span style=\"color:#3a9cff\">blue</span> = clip on a mix layer; "
               "<span style=\"color:#ff913a\">orange</span> = large clip preview (right-click peek).</p>"
               "<p><b>Right-click</b> a <b>playing</b> cell: preview + Inspector for that cell; "
               "right-click again to clear peek.</p>"));
    });

    menuBar()->addMenu(tr("&Help"));
}

void MainWindow::rebindUiToProject()
{
    Project* p = m_project.get();
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("rebind-start"));
        agentDebugLog("MainWindow.cpp:rebindUiToProject", "rebind step", "H5", d);
    }
  // #endregion
    p->ensureSingleBankSet();
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("after-ensureSingleBankSet"));
        d.insert(QStringLiteral("bankSets"), p->bankSets.size());
        if (!p->bankSets.isEmpty()) {
            d.insert(QStringLiteral("banks"), p->bankSets[0].banks.size());
            if (!p->bankSets[0].banks.isEmpty()) {
                d.insert(QStringLiteral("cells"), p->bankSets[0].banks[0].cells.size());
            }
        }
        d.insert(QStringLiteral("triggerMappings"), p->triggerMappings.size());
        agentDebugLog("MainWindow.cpp:rebindUiToProject", "rebind step", "H5", d);
    }
  // #endregion
    p->resizeBanksForGrid(p->settings.matrix.gridRows, p->settings.matrix.gridCols);
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("after-resizeBanksForGrid"));
        d.insert(QStringLiteral("gridRows"), p->settings.matrix.gridRows);
        d.insert(QStringLiteral("gridCols"), p->settings.matrix.gridCols);
        if (!p->bankSets.isEmpty() && !p->bankSets[0].banks.isEmpty()) {
            d.insert(QStringLiteral("cells"), p->bankSets[0].banks[0].cells.size());
        }
        agentDebugLog("MainWindow.cpp:rebindUiToProject", "rebind step", "H5", d);
    }
  // #endregion
    m_bankGrid ->setProject(p);
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("after-bankGrid-setProject"));
        agentDebugLog("MainWindow.cpp:rebindUiToProject", "rebind step", "H6", d);
    }
  // #endregion
    m_inspector->setProject(p);
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("after-inspector-setProject"));
        agentDebugLog("MainWindow.cpp:rebindUiToProject", "rebind step", "H7", d);
    }
  // #endregion
    m_mediaDock->setProject(p);
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("after-mediaDock-setProject"));
        agentDebugLog("MainWindow.cpp:rebindUiToProject", "rebind step", "H8", d);
    }
  // #endregion
    if (m_inputRouter) {
        m_inputRouter->setProject(p);
    }
    clearClipPeek();
    m_inspector->setSelection(m_bankGrid->activeBankSetIndex(),
                              m_bankGrid->activeBankIndex(),
                              -1);
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("after-inspector-setSelection"));
        agentDebugLog("MainWindow.cpp:rebindUiToProject", "rebind step", "H7", d);
    }
  // #endregion
    refreshPreviewForSelectedCell();
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("after-refreshPreviewForSelectedCell"));
        agentDebugLog("MainWindow.cpp:rebindUiToProject", "rebind step", "H9", d);
    }
  // #endregion
    syncMixSlotHighlightsToBankGrid();
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("after-syncMixSlotHighlights"));
        agentDebugLog("MainWindow.cpp:rebindUiToProject", "rebind step", "H10", d);
    }
  // #endregion
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
    dlg.exec();
}

void MainWindow::updateWindowTitle()
{
    QString title = QStringLiteral("PerformanieVJ");
    if (!m_project->filePath.isEmpty()) {
        title += QStringLiteral(" - ") + QFileInfo(m_project->filePath).fileName();
    } else {
        title += QStringLiteral(" - ") + tr("Untitled");
    }
    if (m_project->sourceFormat != QLatin1String("pvj")) {
        title += QStringLiteral(" [imported from ") + m_project->sourceFormat.toUpper() + QLatin1Char(']');
    }
    if (m_actMidiMappingEdit && m_actMidiMappingEdit->isChecked()) {
        title += tr(" — MIDI map");
    }
    setWindowTitle(title);
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
    m_previewA->clearFrame();
    updateMixerFromPlayingCells();
}

// File operations -------------------------------------------------------------

void MainWindow::onFileNew()
{
    stopAllPlaybackAndClear();

    m_project = std::make_unique<Project>();
    m_project->initializeDefault();
    rebindUiToProject();
    updateWindowTitle();
    statusBar()->showMessage(tr("New project"), 3000);
}

void MainWindow::onFileOpen()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Project"),
                                                      {}, QString::fromLatin1(kPvjFilter));
    if (path.isEmpty()) return;
    openProjectFromPath(path);
}

void MainWindow::openProjectFromPath(const QString& path)
{
    auto next = std::make_unique<Project>();
    auto res = PvjSerializer::load(*next, path);
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("path"), path);
        d.insert(QStringLiteral("ok"), res.ok);
        d.insert(QStringLiteral("error"), res.errorMessage);
        agentDebugLog("MainWindow.cpp:openProjectFromPath", "project load attempt", "H1", d);
    }
  // #endregion
    if (!res.ok) {
        QMessageBox::warning(this, tr("Open failed"), res.errorMessage);
        return;
    }
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("loaded-before-stopAll"));
        d.insert(QStringLiteral("bankSets"), next->bankSets.size());
        if (!next->bankSets.isEmpty()) {
            d.insert(QStringLiteral("banks"), next->bankSets[0].banks.size());
            if (!next->bankSets[0].banks.isEmpty()) {
                d.insert(QStringLiteral("cells"), next->bankSets[0].banks[0].cells.size());
            }
        }
        d.insert(QStringLiteral("mediaLibrary"), next->mediaLibrary.size());
        agentDebugLog("MainWindow.cpp:openProjectFromPath", "load step", "H5", d);
    }
  // #endregion
    stopAllPlaybackAndClear();
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("after-stopAllPlaybackAndClear"));
        agentDebugLog("MainWindow.cpp:openProjectFromPath", "load step", "H5", d);
    }
  // #endregion
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

    m_project = std::move(next);
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("after-project-move"));
        agentDebugLog("MainWindow.cpp:openProjectFromPath", "load step", "H13", d);
    }
  // #endregion
    rebindUiToProject();
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("after-rebindUiToProject"));
        agentDebugLog("MainWindow.cpp:openProjectFromPath", "load step", "H11", d);
    }
  // #endregion
    updateWindowTitle();
    statusBar()->showMessage(tr("Opened %1").arg(path), 5000);
    rememberRecentProject(path);
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("after-rememberRecentProject"));
        agentDebugLog("MainWindow.cpp:openProjectFromPath", "load step", "H11", d);
    }
    QTimer::singleShot(500, this, [this]() {
        QJsonObject d;
        d.insert(QStringLiteral("step"), QStringLiteral("deferred-500ms-after-load"));
        agentDebugLog("MainWindow.cpp:openProjectFromPath", "load step", "H12", d);
    });
  // #endregion
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
  // #region agent log
    {
        QJsonObject d;
        d.insert(QStringLiteral("path"), path);
        d.insert(QStringLiteral("exists"), exists);
        agentDebugLog("MainWindow.cpp:onRecentFileTriggered", "recent project open", "H2", d);
    }
  // #endregion
    if (exists) {
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
    updateWindowTitle();
    statusBar()->showMessage(tr("Saved %1").arg(path), 5000);
    rememberRecentProject(path);
    return true;
}

void MainWindow::onFileSave()
{
    if (m_project->filePath.isEmpty() || m_project->sourceFormat != QLatin1String("pvj")) {
        onFileSaveAs();
        return;
    }
    saveToPath(m_project->filePath);
}

void MainWindow::onFileSaveAs()
{
    QString suggested = m_project->filePath;
    if (suggested.isEmpty()) {
        suggested = QStringLiteral("Untitled.pvj");
    } else if (!suggested.endsWith(QLatin1String(".pvj"), Qt::CaseInsensitive)) {
        QFileInfo fi(suggested);
        suggested = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".pvj");
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Project As"),
                                                      suggested, QString::fromLatin1(kPvjFilter));
    if (path.isEmpty()) return;
    saveToPath(path);
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

    applyBankGridDimensions(rows, cols);
    statusBar()->showMessage(tr("Bank grid set to %1 x %2").arg(rows).arg(cols), 4000);
}

void MainWindow::onImportVj2()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Import GrandVJ Project"),
                                                      {}, QString::fromLatin1(kVj2Filter));
    if (path.isEmpty()) return;
    auto next = std::make_unique<Project>();
    auto res = Vj2Importer::importFile(*next, path);
    if (!res.ok) {
        QMessageBox::warning(this, tr("Import failed"), res.errorMessage);
        return;
    }
    stopAllPlaybackAndClear();

    m_project = std::move(next);
    rebindUiToProject();
    updateWindowTitle();
    if (!res.warnings.isEmpty()) {
        QMessageBox::information(this, tr("Import warnings"), res.warnings.join(QLatin1Char('\n')));
    }
    statusBar()->showMessage(tr("Imported %1 (save as .pvj to keep changes)").arg(path), 8000);
}

void MainWindow::onImportAvc()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Import Resolume Composition"),
                                                      {}, QString::fromLatin1(kAvcFilter));
    if (path.isEmpty()) return;
    auto next = std::make_unique<Project>();
    auto res = AvcImporter::importFile(*next, path);
    if (!res.ok) {
        QMessageBox::warning(this, tr("Import failed"), res.errorMessage);
        return;
    }
    stopAllPlaybackAndClear();

    m_project = std::move(next);
    rebindUiToProject();
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
    m_filterEditor->openForCell(bankSetIndex, bankIndex, cellIndex,
                                &bank.cells[cellIndex], m_project.get());
}

void MainWindow::onCellTriggered(int bankSetIndex, int bankIndex, int cellIndex)
{
    if (bankSetIndex < 0 || bankIndex < 0 || cellIndex < 0) return;
    if (bankSetIndex >= m_project->bankSets.size()) return;
    auto& set  = m_project->bankSets[bankSetIndex];
    if (bankIndex >= set.banks.size()) return;
    auto& bank = set.banks[bankIndex];
    if (cellIndex >= bank.cells.size()) return;

    const auto& cell = bank.cells[cellIndex];

    if (!cellIsPlayable(cell)) {
        const int layer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
        if (layer < 0) {
            refreshPreviewForSelectedCell();
            return;
        }
        stopMixLayer(layer);
        updateMixerFromPlayingCells();
        syncMixerToFullscreen();
        syncMixSlotHighlightsToBankGrid();
        refreshPreviewForSelectedCell();
        return;
    }

    // Left-click toggle: same cell already on the mixer → stop.
    const int playingLayer = findLayerPlayingCell(bankSetIndex, bankIndex, cellIndex);
    if (playingLayer >= 0) {
        stopMixLayer(playingLayer);
        updateMixerFromPlayingCells();
        syncMixerToFullscreen();
        syncMixSlotHighlightsToBankGrid();
        refreshPreviewForSelectedCell();
        return;
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
        if (cell->props.fade > 1e-6) {
            startLayerFadeIn(layer, float(cell->props.transparency), float(cell->props.fade));
        } else {
            updateMixerFromPlayingCells();
        }
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
        if (cell->props.fade > 1e-6) {
            startLayerFadeIn(layer, float(cell->props.transparency), float(cell->props.fade));
        } else {
            updateMixerFromPlayingCells();
        }
        return true;
    }
    return false;
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

    m_audioDecoders[layer]->close();
    if (m_audioEngine) {
        m_audioEngine->setLayerActive(layer, false);
    }

    if (!m_decoders[layer]->open(path)) {
      // #region agent log
        {
            QJsonObject d;
            d.insert(QStringLiteral("layer"), layer);
            d.insert(QStringLiteral("path"), path);
            agentDebugLog("MainWindow.cpp:playMediaOnLayer", "media decoder open failed", "H3", d);
        }
      // #endregion
        statusBar()->showMessage(tr("Cannot open %1").arg(path), 5000);
        return;
    }

    const Cell* c = cellAtDeck(m_layerSlots[static_cast<size_t>(layer)]);
    const double speed = c ? c->props.movieSpeed : 1.0;
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
            m_audioEngine->setLayerGain(layer, c ? float(c->props.audioGain) : 1.f);
            m_audioEngine->setLayerActive(layer, true);
        }
    } else if (m_audioEngine) {
        m_audioEngine->setLayerActive(layer, false);
    }

    if (m_clipPeekLayer == layer && m_previewA) {
        m_previewA->setLabel(QFileInfo(path).fileName());
    }
}

const Cell* MainWindow::cellAtDeck(const DeckSlot& slot)
{
    if (!m_project) return nullptr;
    if (slot.bankSet < 0 || slot.bankSet >= m_project->bankSets.size()) return nullptr;
    const auto& set = m_project->bankSets[slot.bankSet];
    if (slot.bank < 0 || slot.bank >= set.banks.size()) return nullptr;
    auto& bank = set.banks[slot.bank];
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

void MainWindow::scheduleMixerUpdateFromCells()
{
    if (!m_mixerUpdateDebounceTimer) {
        updateMixerFromPlayingCells();
        syncMixerToFullscreen();
        return;
    }
    m_mixerUpdateDebounceTimer->start();
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

void MainWindow::updateMixerFromPlayingCells()
{
    for (int i = kUserLayerMin; i < kMixLayers; ++i) {
        const Cell* c = cellAtDeck(m_layerSlots[i]);
        const LayerKeyingState& layerKeying = m_layerKeying[static_cast<size_t>(i)];
        const LayerKeyingState* keyingPtr = layerKeying.valid ? &layerKeying : nullptr;
        const bool isFeedbackCell = c
            && c->visual.type == VisualType::Generator
            && c->visual.generator == GeneratorKind::InternalFeedback;
        const bool mediaSource = c
            && c->visual.type == VisualType::Media
            && !c->visual.mediaId.isNull();
        if (isFeedbackCell) {
            m_previewB->setLayerActive(i, true);
            m_previewB->setLayerCopyMode(i, c->props.copyMode);
            m_previewB->setLayerMatteRole(i, c->props.matteRole);
            m_previewB->setLayerPicture(i, c->props.picture);
            const QList<pvj::core::CellFilterNode> effectiveChain = effectiveFilterChainForMixer(c, keyingPtr);
            m_previewB->setLayerFilterChain(i, effectiveChain);
            const double kr = keyingPtr ? keyingPtr->keyChannelR : c->props.keyChannelR;
            const double kg = keyingPtr ? keyingPtr->keyChannelG : c->props.keyChannelG;
            const double kb = keyingPtr ? keyingPtr->keyChannelB : c->props.keyChannelB;
            m_previewB->setLayerKeyChannels(i, float(kr), float(kg), float(kb));
            if (!m_layerFadeAnimating[static_cast<size_t>(i)]) {
                m_previewB->setLayerOpacity(i, float(c->props.transparency));
            }
            m_previewB->setLayerFeedback(i, true, c->props.feedback);
            if (m_audioEngine) {
                m_audioEngine->setLayerActive(i, false);
            }
            continue;
        }
        m_previewB->setLayerFeedback(i, false, {});
        if (!c || !mediaSource) {
            if (m_mixerSlotHadMedia[static_cast<size_t>(i)]) {
                m_decoders[i]->stop();
                m_audioDecoders[i]->close();
            }
            m_mixerSlotHadMedia[static_cast<size_t>(i)] = false;
            m_layerFadeAnimating[static_cast<size_t>(i)] = false;
            m_previewB->setLayerActive(i, false);
            m_previewB->setLayerOpacity(i, 1.0f);
            m_previewB->setLayerMatteRole(i, pvj::core::LayerMatteRole::None);
            m_previewB->setLayerFilterChain(i, {});
            m_previewB->setLayerKeyChannels(i, 1.f, 1.f, 1.f);
            if (m_audioEngine) {
                m_audioEngine->setLayerActive(i, false);
            }
            continue;
        }
        m_previewB->setLayerActive(i, true);
        m_previewB->setLayerCopyMode(i, c->props.copyMode);
        m_previewB->setLayerMatteRole(i, c->props.matteRole);
        m_previewB->setLayerPicture(i, c->props.picture);
        const QList<pvj::core::CellFilterNode> effectiveChain = effectiveFilterChainForMixer(c, keyingPtr);
        m_previewB->setLayerFilterChain(i, effectiveChain);
        const double kr = keyingPtr ? keyingPtr->keyChannelR : c->props.keyChannelR;
        const double kg = keyingPtr ? keyingPtr->keyChannelG : c->props.keyChannelG;
        const double kb = keyingPtr ? keyingPtr->keyChannelB : c->props.keyChannelB;
        m_previewB->setLayerKeyChannels(i, float(kr), float(kg), float(kb));
        if (!m_layerFadeAnimating[static_cast<size_t>(i)]) {
            m_previewB->setLayerOpacity(i, float(c->props.transparency));
        }

        if (mediaSource) {
            m_mixerSlotHadMedia[static_cast<size_t>(i)] = true;
            m_decoders[i]->setLooping(playModeUsesLoop(c->props.playMode));
            if (m_audioDecoders[i]->isOpen()) {
                m_audioDecoders[i]->setLooping(playModeUsesLoop(c->props.playMode));
            }

            m_decoders[i]->setPlaybackSpeed(c->props.movieSpeed);
            if (m_audioDecoders[i]->isOpen()) {
                m_audioDecoders[i]->setPlaybackSpeed(c->props.movieSpeed);
            }
        } else if (m_mixerSlotHadMedia[static_cast<size_t>(i)]) {
            m_decoders[i]->stop();
            m_audioDecoders[i]->close();
            m_mixerSlotHadMedia[static_cast<size_t>(i)] = false;
        }

        if (m_audioEngine) {
            if (mediaSource && !m_layerFadeAnimating[static_cast<size_t>(i)]) {
                m_audioEngine->setLayerGain(i, float(c->props.audioGain));
            }
            m_audioEngine->setLayerActive(i, mediaSource && m_audioDecoders[i]->isOpen());
        }
    }
    updateDeckAPreviewRotation();
    syncMixerToFullscreen();
    syncMixSlotHighlightsToBankGrid();

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
    syncPeekHighlightToBankGrid();
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
    static constexpr int kUserLayerDirectCount = 12;
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
        audioTarget = float(c->props.transparency * c->props.audioGain);
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
    m_inspector->clearVisualThumbnail();
    for (int i = 0; i < kMixLayers; ++i) {
        if (!selectionMatchesSlot(m_layerSlots[static_cast<size_t>(i)])) {
            continue;
        }
        const QImage& fr = m_lastFrames[static_cast<size_t>(i)];
        if (!fr.isNull()) {
            m_inspector->setVisualThumbnail(fr);
        }
        break;
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

void MainWindow::onFullscreenOutputToggled()
{
    if (!m_fullscreenOut) {
        m_fullscreenOut = std::make_unique<pvj::render::FullscreenOutputWindow>();
        if (m_stagePixelSize.isValid() && !m_stagePixelSize.isEmpty()
            && m_fullscreenOut->mixerWidget()) {
            m_fullscreenOut->mixerWidget()->setStagePixelSize(m_stagePixelSize);
        }
    }

    if (m_fullscreenOut->isVisible()) {
        m_fullscreenOut->hide();
        statusBar()->showMessage(tr("Fullscreen output closed"), 2000);
        return;
    }

    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        return;
    }
    const int idx = m_inspector->outputScreenIndex();
    QScreen* s = screens.at(qBound(0, idx, screens.size() - 1));

    m_fullscreenOut->setTargetScreen(s);
    m_fullscreenOut->enterFullscreen();
    syncMixerToFullscreen();
    statusBar()->showMessage(tr("Fullscreen output on %1").arg(s->name()), 3000);
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
    m_bankGrid->selectCell(cellIndex);
    m_bankGrid->refresh();
    m_inspector->setSelection(bankSetIndex, bankIndex, cellIndex);
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
    scheduleMixerUpdateFromCells();
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
            this, [this](const QByteArray& b) {
        if (!m_inputRouter || !m_project) {
            return;
        }
        m_inputRouter->handleMidiBytes(
            reinterpret_cast<const unsigned char*>(b.constData()),
            size_t(b.size()));
    });

    connect(m_inputRouter.get(), &pvj::input::InputRouter::triggerCell,
            this, &MainWindow::onInputTriggerCell);
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
        statusBar()->showMessage(msg, 6000);
        if (m_inspector) {
            m_inspector->refreshFromModel();
        }
        if (m_bankGrid) {
            m_bankGrid->refresh();
        }
    });
    connect(m_inputRouter.get(), &pvj::input::InputRouter::learnCancelled,
            this, [this]() {
        statusBar()->showMessage(tr("Learn cancelled"), 3000);
    });

    qApp->installEventFilter(this);
    m_midiInput->start();
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
    if (!(ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        const int slot = mixSlotFromQtKey(ke->key());
        if (slot >= 0) {
            onMixLayerDirect(slot);
            return true;
        }
    }
    if (m_inputRouter->handleKeyEvent(ke->key(), int(ke->modifiers()), true)) {
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
    if (m_inputRouter && !on) {
        m_inputRouter->cancelLearn();
    }
    if (on) {
        statusBar()->showMessage(
            tr("MIDI mapping mode: grid cells are green — click a cell, then press a MIDI note or key for that trigger. "
               "Inspector: right-click a control (or left-click while in this mode) to learn CC / note. "
               "CC faders: Mapping → “Learn MIDI CC → property…” on the selected cell."),
            12000);
    } else {
        statusBar()->showMessage(tr("MIDI mapping mode off."), 3000);
    }
    updateWindowTitle();
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
    m_inputRouter->clearPropertyMappingForCell(m_bankGrid->activeBankSetIndex(),
                                                 m_bankGrid->activeBankIndex(),
                                                 ci,
                                                 propertyName);
    statusBar()->showMessage(tr("Cleared MIDI mapping for %1").arg(propertyName), 4000);
    if (m_inspector) {
        m_inspector->refreshFromModel();
    }
    if (m_bankGrid) {
        m_bankGrid->refresh();
    }
}

void MainWindow::onLearnBankNext()
{
    if (!m_inputRouter || !m_bankGrid) {
        return;
    }
    m_inputRouter->beginLearnBankNav(pvj::core::TriggerTarget::BankNext, m_bankGrid->activeBankSetIndex());
    statusBar()->showMessage(tr("Learn: press a MIDI note for bank next…"), 15000);
}

void MainWindow::onLearnBankPrev()
{
    if (!m_inputRouter || !m_bankGrid) {
        return;
    }
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
    m_inputRouter->beginLearnCellTrigger(bankSetIndex, bankIndex, cellIndex);
    statusBar()->showMessage(tr("MIDI map: press a MIDI note or keyboard key for this cell…"), 15000);
}

void MainWindow::onInputTriggerCell(int bankSetIndex, int bankIndex, int cellIndex)
{
    onCellTriggered(bankSetIndex, bankIndex, cellIndex);
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

    scheduleMixerUpdateFromCells();
}

void MainWindow::onInputPropertyToggle(int bankSetIndex, int bankIndex, int cellIndex,
                                       const QString& propertyName)
{
    if (!m_project) {
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
    updateMixerFromPlayingCells();
}

} // namespace pvj::app

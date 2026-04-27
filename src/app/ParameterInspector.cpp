#include "ParameterInspector.h"

#include "MidiLearnMenu.h"

#include "core/EnumStrings.h"
#include "core/Project.h"
#include "core/PropertyRegistry.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDial>
#include <QFont>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QAbstractButton>
#include <QContextMenuEvent>
#include <QEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

namespace pvj::app {

using pvj::core::CopyMode;
using pvj::core::GeneratorKind;
using pvj::core::LayerBand;
using pvj::core::MaskType;
using pvj::core::PlayMode;
using pvj::core::Project;
using pvj::core::VisualType;
using pvj::core::WrapMode;

class VisualThumbnailLabel final : public QWidget
{
public:
    explicit VisualThumbnailLabel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(160, 90);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFocusPolicy(Qt::NoFocus);
    }

    void setFrame(const QImage& frame);
    void clearThumb();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_frame;
};

void VisualThumbnailLabel::setFrame(const QImage& frame)
{
    if (frame.isNull()) {
        clearThumb();
        return;
    }
    const QImage scaled = frame.scaled(320, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_frame = scaled.convertToFormat(QImage::Format_RGB32);
    update();
}

void VisualThumbnailLabel::clearThumb()
{
    m_frame = QImage();
    update();
}

void VisualThumbnailLabel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath clip;
    clip.addRoundedRect(rect().adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
    p.fillPath(clip, QColor(0x14, 0x16, 0x1c));
    p.setPen(QPen(QColor(0x3d, 0x44, 0x53), 1));
    p.drawPath(clip);

    if (m_frame.isNull()) {
        p.setPen(QColor(0x8b, 0x92, 0xa3));
        p.drawText(rect(), Qt::AlignCenter, tr("\u2014"));
        return;
    }

    const QRectF inner = clip.boundingRect().adjusted(2, 2, -2, -2);
    const QSizeF sz = QSizeF(m_frame.size()).scaled(inner.size(), Qt::KeepAspectRatio);
    QRectF dr(QPointF(inner.center() - QPointF(sz.width() / 2, sz.height() / 2)), sz);
    p.setClipPath(clip);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(dr, m_frame);
}

namespace {

constexpr int kUnitSliderMax = 1000;

int unitToSlider(double u)
{
    return int(qRound(qBound(0.0, u, 1.0) * double(kUnitSliderMax)));
}

double sliderToUnit(int s)
{
    return qBound(0.0, double(s) / double(kUnitSliderMax), 1.0);
}

int rangeToSlider(double value, double minValue, double maxValue)
{
    const double t = (qBound(minValue, value, maxValue) - minValue) / (maxValue - minValue);
    return unitToSlider(t);
}

double sliderToRange(int sliderValue, double minValue, double maxValue)
{
    return minValue + (maxValue - minValue) * sliderToUnit(sliderValue);
}

QString formatUnit(double u)
{
    return QString::number(u, 'f', 3);
}

QString formatSignedUnit(double u)
{
    return QString::number(u, 'f', 3);
}

QString formatKeyPercent(double u)
{
    return QString::number(int(qRound(qBound(0.0, u, 1.0) * 100.0))) + QLatin1Char('%');
}

int movieSpeedToSlider(double s)
{
    const double t = qBound(-4.0, s, 4.0);
    return int(qRound((t + 4.0) / 8.0 * double(kUnitSliderMax)));
}

double sliderToMovieSpeed(int v)
{
    return -4.0 + 8.0 * (double(v) / double(kUnitSliderMax));
}

QString formatMovieSpeed(double s)
{
    return QString::number(s, 'f', 2) + QLatin1Char('x');
}

QString playModeTip(pvj::core::PlayMode m)
{
    using pvj::core::PlayMode;
    switch (m) {
    case PlayMode::LoopForward:
        return QObject::tr("Loop forward");
    case PlayMode::LoopReverse:
        return QObject::tr("Loop reverse (engine: loop until reverse decode exists)");
    case PlayMode::Once:
        return QObject::tr("Play once");
    case PlayMode::PingPong:
        return QObject::tr("Ping-pong (engine: loop)");
    case PlayMode::Shuffle:
        return QObject::tr("Shuffle (stored; engine: loop)");
    case PlayMode::TimecodeSync:
        return QObject::tr("Timecode sync (stored; use TC Start)");
    case PlayMode::LoopSegment:
        return QObject::tr("Loop segment (uses In/Out trim)");
    case PlayMode::HoldLastFrame:
        return QObject::tr("Hold last frame at end");
    case PlayMode::PlayBackwardOnce:
        return QObject::tr("Play backward once (stored)");
    case PlayMode::RandomAccess:
        return QObject::tr("Random access (stored)");
    case PlayMode::StepFrame:
        return QObject::tr("Step / strobe (stored)");
    default:
        return QObject::tr("Playback mode");
    }
}

struct MixPreset {
    CopyMode cm;
    MaskType mt;
    double transparency;
    double maskW;
    double maskS;
    double kr;
    double kg;
    double kb;
};

// Stored preset row index = combo index (1-based into this table).
static const MixPreset kMixPresets[] = {
    { CopyMode::Normal, MaskType::None, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0 },
    { CopyMode::Over, MaskType::None, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0 },
    { CopyMode::Add, MaskType::None, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0 },
    { CopyMode::Difference, MaskType::SoftEdge, 1.0, 0.55, 0.2, 1.0, 1.0, 1.0 },
};

constexpr int kMixPresetCount = int(sizeof(kMixPresets) / sizeof(kMixPresets[0]));

void fillCopyModeCombo(QComboBox* cb)
{
    cb->clear();
    const auto addCm = [cb](const QString& label, CopyMode m) {
        cb->addItem(label, int(m));
    };
    addCm(QObject::tr("Normal"), CopyMode::Normal);
    addCm(QObject::tr("Add"), CopyMode::Add);
    addCm(QObject::tr("Multiply"), CopyMode::Multiply);
    addCm(QObject::tr("Screen"), CopyMode::Screen);
    addCm(QObject::tr("Lighten"), CopyMode::Lighten);
    addCm(QObject::tr("Darken"), CopyMode::Darken);
    cb->insertSeparator(cb->count());
    addCm(QObject::tr("Difference"), CopyMode::Difference);
    addCm(QObject::tr("Difference Vivid"), CopyMode::DifferenceVivid);
    addCm(QObject::tr("Difference RGB"), CopyMode::DifferenceRgb);
    addCm(QObject::tr("Overlay"), CopyMode::Overlay);
    addCm(QObject::tr("Atop"), CopyMode::Atop);
    addCm(QObject::tr("Average"), CopyMode::Average);
    addCm(QObject::tr("Brightest"), CopyMode::Brightest);
    addCm(QObject::tr("Burn Color"), CopyMode::BurnColor);
    addCm(QObject::tr("Burn Linear"), CopyMode::BurnLinear);
    addCm(QObject::tr("Chroma Difference"), CopyMode::ChromaDifference);
    addCm(QObject::tr("Color"), CopyMode::ColorBlend);
    addCm(QObject::tr("Darker Color"), CopyMode::DarkerColor);
    addCm(QObject::tr("Dimmest"), CopyMode::Dimmest);
    addCm(QObject::tr("Divide"), CopyMode::Divide);
    addCm(QObject::tr("Dodge"), CopyMode::Dodge);
    addCm(QObject::tr("Exclude"), CopyMode::Exclude);
    addCm(QObject::tr("Freeze"), CopyMode::Freeze);
    addCm(QObject::tr("Glow"), CopyMode::Glow);
    addCm(QObject::tr("Hard Light"), CopyMode::HardLight);
    addCm(QObject::tr("Hard Mix"), CopyMode::HardMix);
    addCm(QObject::tr("Heat"), CopyMode::Heat);
    addCm(QObject::tr("Hue"), CopyMode::HueBlend);
    addCm(QObject::tr("Inside"), CopyMode::Inside);
    addCm(QObject::tr("Inside Luminance"), CopyMode::InsideLuminance);
    addCm(QObject::tr("Inverse"), CopyMode::Inverse);
    addCm(QObject::tr("Lighter Color"), CopyMode::LighterColor);
    addCm(QObject::tr("Luminance Difference"), CopyMode::LuminanceDifference);
    addCm(QObject::tr("Maximum"), CopyMode::Maximum);
    addCm(QObject::tr("Minimum"), CopyMode::Minimum);
    addCm(QObject::tr("Negate"), CopyMode::Negate);
    addCm(QObject::tr("Outside"), CopyMode::Outside);
    addCm(QObject::tr("Outside Luminance"), CopyMode::OutsideLuminance);
    cb->insertSeparator(cb->count());
    addCm(QObject::tr("Over"), CopyMode::Over);
    addCm(QObject::tr("Pinlight"), CopyMode::Pinlight);
    addCm(QObject::tr("Reflect"), CopyMode::Reflect);
    addCm(QObject::tr("Soft Light"), CopyMode::SoftLight);
    addCm(QObject::tr("Linear Light"), CopyMode::LinearLight);
    addCm(QObject::tr("Stencil Luminance"), CopyMode::StencilLuminance);
    addCm(QObject::tr("Subtract"), CopyMode::Subtract);
    addCm(QObject::tr("Subtractive"), CopyMode::Subtractive);
    addCm(QObject::tr("Under"), CopyMode::Under);
    addCm(QObject::tr("Vivid Light"), CopyMode::VividLight);
    addCm(QObject::tr("Xor"), CopyMode::Xor);
    cb->insertSeparator(cb->count());
    addCm(QObject::tr("Y Film"), CopyMode::YFilm);
    addCm(QObject::tr("Z Film"), CopyMode::ZFilm);
}

} // namespace

ParameterInspector::ParameterInspector(QWidget* parent)
    : QTabWidget(parent)
{
    setDocumentMode(true);
    setTabPosition(QTabWidget::North);

    addTab(buildVisualTab(), tr("Visual"));
    addTab(buildTransitionTab(), tr("Transition"));
    addTab(buildMixingTab(), tr("Mixing"));
    addTab(buildPositionTab(), tr("Position"));
    addTab(buildOutputTab(), tr("Output"));

    rebuildScreenList();

    connect(qGuiApp, &QGuiApplication::screenAdded,
            this, [this] { rebuildScreenList(); });
    connect(qGuiApp, &QGuiApplication::screenRemoved,
            this, [this] { rebuildScreenList(); });

    registerMidiWidgets();

    refreshFromCell();
}

QWidget* ParameterInspector::buildVisualTab()
{
    auto* host = new QWidget(this);
    auto* root = new QVBoxLayout(host);
    root->setContentsMargins(0, 0, 0, 0);

    m_visualStandardSection = new QWidget(host);
    auto* stdLay = new QVBoxLayout(m_visualStandardSection);
    stdLay->setContentsMargins(0, 0, 0, 0);

    {
        auto* top = new QWidget(m_visualStandardSection);
        auto* topGrid = new QGridLayout(top);
        topGrid->setColumnStretch(1, 1);
        topGrid->addWidget(new QLabel(tr("Visual"), top), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
        auto* visRow = new QWidget(top);
        auto* vh     = new QHBoxLayout(visRow);
        vh->setContentsMargins(0, 0, 0, 0);
        m_visualPrevBtn = new QToolButton(visRow);
        m_visualPrevBtn->setAutoRaise(true);
        m_visualPrevBtn->setText(QStringLiteral("<"));
        m_visualPrevBtn->setToolTip(tr("Seek −1 s on the playing layer for this cell"));
        connect(m_visualPrevBtn, &QToolButton::clicked, this, [this] {
            emit visualSeekStepRequested(-1);
        });
        m_visualThumb = new VisualThumbnailLabel(visRow);
        m_visualNextBtn = new QToolButton(visRow);
        m_visualNextBtn->setAutoRaise(true);
        m_visualNextBtn->setText(QStringLiteral(">"));
        m_visualNextBtn->setToolTip(tr("Seek +1 s on the playing layer for this cell"));
        connect(m_visualNextBtn, &QToolButton::clicked, this, [this] {
            emit visualSeekStepRequested(1);
        });
        m_feedbackPresetApplyBtn = new QToolButton(visRow);
        m_feedbackPresetApplyBtn->setText(QStringLiteral("FB"));
        m_feedbackPresetApplyBtn->setAutoRaise(true);
        m_feedbackPresetApplyBtn->setToolTip(
            tr("Apply feedback source preset to this cell (without circular preset mode)."));
        connect(m_feedbackPresetApplyBtn, &QToolButton::clicked,
                this, &ParameterInspector::onFeedbackPresetApplyClicked);
        m_audioDial = new QDial(visRow);
        m_audioDial->setRange(0, 400);
        m_audioDial->setValue(100);
        m_audioDial->setFixedSize(72, 72);
        m_audioDial->setNotchesVisible(true);
        m_audioDial->setWrapping(false);
        m_audioDial->setToolTip(tr("Audio gain for this clip (independent of video transparency)."));
        m_audioDialValue = new QLabel(QStringLiteral("1.00"), visRow);
        m_audioDialValue->setMinimumWidth(40);
        connect(m_audioDial, &QDial::valueChanged, this, &ParameterInspector::onAudioDialChanged);
        vh->addWidget(m_visualPrevBtn);
        vh->addWidget(m_visualThumb, 0, Qt::AlignCenter);
        vh->addWidget(m_visualNextBtn);
        vh->addWidget(m_feedbackPresetApplyBtn);
        vh->addWidget(m_audioDial);
        vh->addWidget(m_audioDialValue);
        vh->addStretch(1);
        topGrid->addWidget(visRow, 0, 1);
        stdLay->addWidget(top);
    }

    m_visualLabel = new QLabel(tr("(no cell selected)"), m_visualStandardSection);
    m_visualLabel->setWordWrap(true);
    m_visualLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    stdLay->addWidget(m_visualLabel);

    m_visualClipSection = new QWidget(m_visualStandardSection);
    auto* grid = new QGridLayout(m_visualClipSection);
    m_visualGrid = grid;
    grid->setColumnStretch(1, 1);
    int row = 0;

    grid->addWidget(new QLabel(tr("Play mode"), m_visualClipSection), row, 0, Qt::AlignRight | Qt::AlignVCenter);
    {
        auto* pmRow = new QWidget(m_visualClipSection);
        auto* pmh    = new QHBoxLayout(pmRow);
        pmh->setContentsMargins(0, 0, 0, 0);
        m_playModeGroup = new QButtonGroup(m_visualClipSection);
        m_playModeGroup->setExclusive(true);
        for (int i = 0; i <= int(PlayMode::StepFrame); ++i) {
            auto* tb = new QToolButton(pmRow);
            tb->setCheckable(true);
            tb->setAutoRaise(true);
            tb->setFixedSize(28, 26);
            tb->setText(QString::number(i + 1));
            tb->setToolTip(playModeTip(static_cast<PlayMode>(i)));
            m_playModeGroup->addButton(tb, i);
            pmh->addWidget(tb);
        }
        connect(m_playModeGroup, &QButtonGroup::idClicked,
                this, &ParameterInspector::onPlayModeGroupClicked);
        pmh->addStretch(1);
        grid->addWidget(pmRow, row, 1);
    }
    ++row;

    grid->addWidget(new QLabel(tr("Speed"), m_visualClipSection), row, 0, Qt::AlignRight | Qt::AlignVCenter);
    {
        auto* spRow = new QWidget(m_visualClipSection);
        auto* sph   = new QHBoxLayout(spRow);
        sph->setContentsMargins(0, 0, 0, 0);
        m_speedSlider = new QSlider(Qt::Horizontal, spRow);
        m_speedSlider->setRange(0, kUnitSliderMax);
        m_speedSlider->setSingleStep(10);
        m_speedSlider->setPageStep(50);
        m_speedSlider->setFocusPolicy(Qt::StrongFocus);
        m_speedSlider->setToolTip(tr("Playback rate (−4× … +4×)."));
        m_speedValueLabel = new QLabel(formatMovieSpeed(1.0), spRow);
        m_speedValueLabel->setMinimumWidth(52);
        m_pauseClipBtn = new QToolButton(spRow);
        m_pauseClipBtn->setCheckable(true);
        m_pauseClipBtn->setAutoRaise(true);
        m_pauseClipBtn->setText(QStringLiteral("||"));
        m_pauseClipBtn->setToolTip(tr("Pause / resume this clip when it is on a mix layer"));
        connect(m_speedSlider, &QSlider::valueChanged,
                this, &ParameterInspector::onMovieSpeedSliderChanged);
        connect(m_pauseClipBtn, &QToolButton::toggled, this, &ParameterInspector::onClipPauseToggled);
        sph->addWidget(m_speedSlider, 1);
        sph->addWidget(m_speedValueLabel);
        sph->addWidget(m_pauseClipBtn);
        grid->addWidget(spRow, row, 1);
    }
    ++row;

    grid->addWidget(new QLabel(tr("Segment"), m_visualClipSection), row, 0, Qt::AlignRight | Qt::AlignVCenter);
    {
        auto* sgRow = new QWidget(m_visualClipSection);
        auto* sgh   = new QVBoxLayout(sgRow);
        sgh->setContentsMargins(0, 0, 0, 0);
        auto* inRow = new QWidget(sgRow);
        auto* inh   = new QHBoxLayout(inRow);
        inh->setContentsMargins(0, 0, 0, 0);
        inh->addWidget(new QLabel(tr("In"), inRow));
        m_segmentInSlider = new QSlider(Qt::Horizontal, inRow);
        m_segmentInSlider->setRange(0, kUnitSliderMax);
        connect(m_segmentInSlider, &QSlider::valueChanged,
                this, &ParameterInspector::onSegmentInChanged);
        inh->addWidget(m_segmentInSlider, 1);
        sgh->addWidget(inRow);
        auto* outRow = new QWidget(sgRow);
        auto* outh   = new QHBoxLayout(outRow);
        outh->setContentsMargins(0, 0, 0, 0);
        outh->addWidget(new QLabel(tr("Out"), outRow));
        m_segmentOutSlider = new QSlider(Qt::Horizontal, outRow);
        m_segmentOutSlider->setRange(0, kUnitSliderMax);
        connect(m_segmentOutSlider, &QSlider::valueChanged,
                this, &ParameterInspector::onSegmentOutChanged);
        outh->addWidget(m_segmentOutSlider, 1);
        sgh->addWidget(outRow);
        grid->addWidget(sgRow, row, 1);
    }
    ++row;

    grid->addWidget(new QLabel(tr("Scratch"), m_visualClipSection), row, 0, Qt::AlignRight | Qt::AlignVCenter);
    {
        auto* scRow = new QWidget(m_visualClipSection);
        auto* sch   = new QHBoxLayout(scRow);
        sch->setContentsMargins(0, 0, 0, 0);
        m_scratchSlider = new QSlider(Qt::Horizontal, scRow);
        m_scratchSlider->setRange(0, kUnitSliderMax);
        m_scratchSlider->setToolTip(
            tr("Playhead position in the file (0–100%). Release to seek on the playing layer."));
        connect(m_scratchSlider, &QSlider::valueChanged,
                this, &ParameterInspector::onScratchSliderChanged);
        connect(m_scratchSlider, &QSlider::sliderReleased,
                this, &ParameterInspector::onScratchSliderReleased);
        sch->addWidget(m_scratchSlider, 1);
        grid->addWidget(scRow, row, 1);
    }
    ++row;

    grid->addWidget(new QLabel(tr("Text"), m_visualClipSection), row, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_overlayLineEdit = new QLineEdit(m_visualClipSection);
    m_overlayLineEdit->setPlaceholderText(tr("Overlay / cue text"));
    connect(m_overlayLineEdit, &QLineEdit::editingFinished,
            this, [this] {
                if (m_overlayLineEdit) {
                    onOverlayTextEdited(m_overlayLineEdit->text());
                }
            });
    grid->addWidget(m_overlayLineEdit, row, 1);
    ++row;

    grid->addWidget(new QLabel(tr("TC Start"), m_visualClipSection), row, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_tcStartEdit = new QLineEdit(m_visualClipSection);
    m_tcStartEdit->setPlaceholderText(QStringLiteral("00:00:00:00"));
    connect(m_tcStartEdit, &QLineEdit::editingFinished, this, [this] {
        if (m_tcStartEdit) {
            onTcStartEdited(m_tcStartEdit->text());
        }
    });
    grid->addWidget(m_tcStartEdit, row, 1);
    ++row;

    grid->addWidget(new QLabel(tr("Fade"), m_visualClipSection), row, 0, Qt::AlignRight | Qt::AlignVCenter);
    {
        auto* fdRow = new QWidget(m_visualClipSection);
        auto* fh    = new QHBoxLayout(fdRow);
        fh->setContentsMargins(0, 0, 0, 0);
        m_fadeSlider = new QSlider(Qt::Horizontal, fdRow);
        m_fadeSlider->setRange(0, kUnitSliderMax);
        m_fadeSlider->setSingleStep(10);
        m_fadeSlider->setPageStep(50);
        m_fadeSlider->setFocusPolicy(Qt::StrongFocus);
        m_fadeSlider->setToolTip(
            tr("Fade-in time when triggering the cell (0 = instant, 1 = long).\n"
               "Continuous fade in/out via MIDI: Mapping → "
               "\"Learn MIDI CC → fade (transparency)…\"."));
        m_fadeValue = new QLabel(formatUnit(0.0), fdRow);
        m_fadeValue->setMinimumWidth(48);
        m_fadeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        connect(m_fadeSlider, &QSlider::valueChanged,
                this, &ParameterInspector::onFadeSliderChanged);
        fh->addWidget(m_fadeSlider, 1);
        fh->addWidget(m_fadeValue);
        grid->addWidget(fdRow, row, 1);
    }
    ++row;

    auto* note = new QLabel(
        tr("Video opacity is in the Mixing tab. Audio gain is the dial next to the clip thumbnail."),
        m_visualClipSection);
    m_visualNote = note;
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #8b92a3;"));
    grid->addWidget(note, row, 0, 1, 2);

    stdLay->addWidget(m_visualClipSection);

    m_feedbackGroup = new QGroupBox(tr("Feedback"), m_visualStandardSection);
    auto* fbGrid = new QGridLayout(m_feedbackGroup);
    int fr = 0;
    m_feedbackEnabled = new QCheckBox(tr("Feedback enabled"), m_feedbackGroup);
    connect(m_feedbackEnabled, &QCheckBox::toggled, this, &ParameterInspector::onFeedbackEnabledToggled);
    fbGrid->addWidget(m_feedbackEnabled, fr++, 0, 1, 3);

    auto addFbSlider = [&](const QString& lab, QSlider** sOut, QLabel** vOut, const QString& tip) {
        fbGrid->addWidget(new QLabel(lab, m_feedbackGroup), fr, 0, Qt::AlignRight | Qt::AlignVCenter);
        auto* s = new QSlider(Qt::Horizontal, m_feedbackGroup);
        s->setRange(0, kUnitSliderMax);
        s->setFocusPolicy(Qt::StrongFocus);
        s->setToolTip(tip);
        auto* v = new QLabel(QStringLiteral("0"), m_feedbackGroup);
        v->setMinimumWidth(44);
        v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        fbGrid->addWidget(s, fr, 1);
        fbGrid->addWidget(v, fr, 2);
        *sOut = s;
        *vOut = v;
        ++fr;
    };

    addFbSlider(tr("Strength"), &m_feedbackStrengthSlider, &m_feedbackStrengthValue,
                tr("Blend weight of the feedback history (0–1)."));
    connect(m_feedbackStrengthSlider, &QSlider::valueChanged,
            this, &ParameterInspector::onFeedbackStrengthChanged);
    addFbSlider(tr("Zoom"), &m_feedbackZoomSlider, &m_feedbackZoomValue,
                tr("Per-frame scale of the history sample (0.5–2)."));
    connect(m_feedbackZoomSlider, &QSlider::valueChanged, this, &ParameterInspector::onFeedbackZoomChanged);
    fbGrid->addWidget(new QLabel(tr("Rotation (°)"), m_feedbackGroup), fr, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_feedbackRotationSlider = new QSlider(Qt::Horizontal, m_feedbackGroup);
    m_feedbackRotationSlider->setRange(0, kUnitSliderMax);
    m_feedbackRotationSlider->setToolTip(
        tr("Fixed ° per frame, or °/s when “Circular rotation” is on."));
    m_feedbackRotationValue = new QLabel(QStringLiteral("0.00"), m_feedbackGroup);
    m_feedbackRotationValue->setMinimumWidth(44);
    m_feedbackRotationValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    connect(m_feedbackRotationSlider, &QSlider::valueChanged,
            this, &ParameterInspector::onFeedbackRotationChanged);
    fbGrid->addWidget(m_feedbackRotationSlider, fr, 1);
    fbGrid->addWidget(m_feedbackRotationValue, fr, 2);
    ++fr;
    m_feedbackRotationAnimated = new QCheckBox(tr("Circular rotation (°/s)"), m_feedbackGroup);
    connect(m_feedbackRotationAnimated, &QCheckBox::toggled,
            this, &ParameterInspector::onFeedbackRotationAnimatedToggled);
    fbGrid->addWidget(m_feedbackRotationAnimated, fr++, 0, 1, 3);
    addFbSlider(tr("Decay"), &m_feedbackDecaySlider, &m_feedbackDecayValue,
                tr("Offset added to history (−0.1 … +0.1)."));
    connect(m_feedbackDecaySlider, &QSlider::valueChanged, this, &ParameterInspector::onFeedbackDecayChanged);
    addFbSlider(tr("Hist. brightness"), &m_feedbackBrightnessSlider, &m_feedbackBrightnessValue,
                tr("Additive brightness on feedback history (−1 … +1)."));
    connect(m_feedbackBrightnessSlider, &QSlider::valueChanged,
            this, &ParameterInspector::onFeedbackBrightnessChanged);
    addFbSlider(tr("Hist. saturation"), &m_feedbackSaturationSlider, &m_feedbackSaturationValue,
                tr("Saturation on history (0–2)."));
    connect(m_feedbackSaturationSlider, &QSlider::valueChanged,
            this, &ParameterInspector::onFeedbackSaturationChanged);
    addFbSlider(tr("Hist. gamma"), &m_feedbackGammaSlider, &m_feedbackGammaValue, tr("Gamma on history (0.1–4)."));
    connect(m_feedbackGammaSlider, &QSlider::valueChanged, this, &ParameterInspector::onFeedbackGammaChanged);
    addFbSlider(tr("Hist. contrast"), &m_feedbackContrastSlider, &m_feedbackContrastValue,
                tr("Contrast on history (0–2)."));
    connect(m_feedbackContrastSlider, &QSlider::valueChanged, this, &ParameterInspector::onFeedbackContrastChanged);

    {
        auto* layerHdr = new QLabel(tr("Layer output"), m_feedbackGroup);
        QFont hf = layerHdr->font();
        hf.setBold(true);
        layerHdr->setFont(hf);
        fbGrid->addWidget(layerHdr, fr++, 0, 1, 3);
    }
    addFbSlider(tr("Layer brightness"), &m_feedbackLayerBrightnessSlider, &m_feedbackLayerBrightnessValue,
                tr("After mix (−1 … +1)."));
    connect(m_feedbackLayerBrightnessSlider, &QSlider::valueChanged,
            this, &ParameterInspector::onFeedbackLayerBrightnessChanged);
    addFbSlider(tr("Layer saturation"), &m_feedbackLayerSaturationSlider, &m_feedbackLayerSaturationValue,
                tr("After mix (0–2)."));
    connect(m_feedbackLayerSaturationSlider, &QSlider::valueChanged,
            this, &ParameterInspector::onFeedbackLayerSaturationChanged);
    addFbSlider(tr("Layer gamma"), &m_feedbackLayerGammaSlider, &m_feedbackLayerGammaValue,
                tr("After mix (0.1–4)."));
    connect(m_feedbackLayerGammaSlider, &QSlider::valueChanged,
            this, &ParameterInspector::onFeedbackLayerGammaChanged);
    addFbSlider(tr("Layer contrast"), &m_feedbackLayerContrastSlider, &m_feedbackLayerContrastValue,
                tr("After mix (0–2)."));
    connect(m_feedbackLayerContrastSlider, &QSlider::valueChanged,
            this, &ParameterInspector::onFeedbackLayerContrastChanged);

    fbGrid->addWidget(new QLabel(tr("Wrap mode"), m_feedbackGroup), fr, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_feedbackWrapMode = new QComboBox(m_feedbackGroup);
    m_feedbackWrapMode->addItem(tr("Clamp"), int(WrapMode::Clamp));
    m_feedbackWrapMode->addItem(tr("Repeat"), int(WrapMode::Repeat));
    m_feedbackWrapMode->addItem(tr("Mirror"), int(WrapMode::Mirror));
    m_feedbackWrapMode->addItem(tr("Tile"), int(WrapMode::Tile));
    m_feedbackWrapMode->setToolTip(tr("UV addressing when sampling feedback history."));
    connect(m_feedbackWrapMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ParameterInspector::onFeedbackWrapModeChanged);
    fbGrid->addWidget(m_feedbackWrapMode, fr, 1, 1, 2);

    m_feedbackGroup->setVisible(false);
    stdLay->addWidget(m_feedbackGroup);

    root->addWidget(m_visualStandardSection);
    {
        auto* layerBandRow = new QWidget(host);
        auto* lbh = new QHBoxLayout(layerBandRow);
        lbh->setContentsMargins(0, 0, 0, 0);
        lbh->addWidget(new QLabel(tr("Layer band"), layerBandRow));
        m_layerBandGroup = new QButtonGroup(host);
        m_layerBandGroup->setExclusive(true);
        auto mkBand = [&](LayerBand value, const QString& text, const QString& tip) {
            auto* tb = new QToolButton(layerBandRow);
            tb->setCheckable(true);
            tb->setAutoRaise(true);
            tb->setText(text);
            tb->setToolTip(tip);
            m_layerBandGroup->addButton(tb, int(value));
            lbh->addWidget(tb);
        };
        mkBand(LayerBand::Back, tr("Back"), tr("Uses mix layers 1-4"));
        mkBand(LayerBand::Mid, tr("Mid"), tr("Uses mix layers 5-8 (default)"));
        mkBand(LayerBand::Front, tr("Front"), tr("Uses mix layers 9-12"));
        connect(m_layerBandGroup, &QButtonGroup::idClicked,
                this, &ParameterInspector::onLayerBandGroupClicked);
        lbh->addStretch(1);
        root->addWidget(layerBandRow);
    }

    return host;
}

QWidget* ParameterInspector::buildTransitionTab()
{
    auto* host = new QWidget(this);
    auto* layout = new QVBoxLayout(host);
    auto* note = new QLabel(
        tr("Clips triggered from the bank grid are mixed automatically: up to six videos "
           "composite in the mixer using transparency (alpha) and the Copy mode from the "
           "Mixing tab. "
           "When all slots are busy, the clip with the lowest priority value is replaced."),
        host);
    note->setWordWrap(true);
    note->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    note->setStyleSheet(QStringLiteral("color: #8b92a3;"));
    layout->addWidget(note);
    return host;
}

QWidget* ParameterInspector::buildMixingTab()
{
    auto* host = new QWidget(this);
    auto* root = new QVBoxLayout(host);
    root->setSpacing(10);

    auto addSection = [&](const QString& title) {
        auto* lab = new QLabel(QStringLiteral("<b>%1</b>").arg(title), host);
        root->addWidget(lab);
    };

    addSection(tr("Preset"));
    m_mixingPreset = new QComboBox(host);
    m_mixingPreset->setMaxVisibleItems(16);
    m_mixingPreset->addItem(tr("Custom"), 0);
    m_mixingPreset->addItem(tr("Replace (Normal, no mask)"), 1);
    m_mixingPreset->addItem(tr("Stack on top (Over)"), 2);
    m_mixingPreset->addItem(tr("Additive glow (Add)"), 3);
    m_mixingPreset->addItem(tr("Chroma-style (Difference + soft mask)"), 4);
    connect(m_mixingPreset, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ParameterInspector::onMixingPresetChanged);
    root->addWidget(m_mixingPreset);

    addSection(tr("Transparency"));
    {
        auto* row = new QWidget(host);
        auto* h   = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        m_transparencySlider = new QSlider(Qt::Horizontal, row);
        m_transparencySlider->setRange(0, kUnitSliderMax);
        m_transparencySlider->setSingleStep(10);
        m_transparencySlider->setPageStep(50);
        m_transparencySlider->setFocusPolicy(Qt::StrongFocus);
        m_transparencySlider->setToolTip(
            tr("Transparency for the layer in the mixer (0 = fully transparent, 1 = full image)."));
        m_transparencyValue = new QLabel(formatUnit(1.0), row);
        m_transparencyValue->setMinimumWidth(48);
        m_transparencyValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        connect(m_transparencySlider, &QSlider::valueChanged,
                this, &ParameterInspector::onTransparencySliderChanged);
        h->addWidget(m_transparencySlider, 1);
        h->addWidget(m_transparencyValue);
        root->addWidget(row);
    }

    addSection(tr("Copy mode"));
    m_copyMode = new QComboBox(host);
    m_copyMode->setMaxVisibleItems(24);
    fillCopyModeCombo(m_copyMode);
    connect(m_copyMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ParameterInspector::onCopyModeChanged);
    root->addWidget(m_copyMode);

    addSection(tr("Mask"));
    {
        auto* row = new QWidget(host);
        auto* h   = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        m_maskTypeGroup = new QButtonGroup(host);
        m_maskTypeGroup->setExclusive(true);

        const struct {
            MaskType mt;
            QString sym;
        } masks[] = {
            { MaskType::None, QString(QChar(0x2014)) },
            { MaskType::Rectangle, QString(QChar(0x25AD)) },
            { MaskType::Circle, QString(QChar(0x25EF)) },
            { MaskType::SoftEdge, QString(QChar(0x25C9)) },
            { MaskType::Ellipse, QString(QChar(0x2B2D)) },
            { MaskType::Custom, QString(QChar(0x2699)) },
        };
        for (const auto& m : masks) {
            auto* tb = new QToolButton(row);
            tb->setText(m.sym);
            tb->setCheckable(true);
            tb->setAutoRaise(true);
            QString tip;
            switch (m.mt) {
            case MaskType::None:
                tip = tr("No mask");
                break;
            case MaskType::Rectangle:
                tip = tr("Rectangle");
                break;
            case MaskType::Circle:
                tip = tr("Circle");
                break;
            case MaskType::SoftEdge:
                tip = tr("Soft edge");
                break;
            case MaskType::Ellipse:
                tip = tr("Ellipse");
                break;
            case MaskType::Custom:
                tip = tr("Custom");
                break;
            }
            tb->setToolTip(tip);
            tb->setMinimumSize(36, 28);
            tb->setFocusPolicy(Qt::StrongFocus);
            m_maskTypeGroup->addButton(tb, int(m.mt));
            h->addWidget(tb);
        }
        connect(m_maskTypeGroup, &QButtonGroup::idClicked,
                this, &ParameterInspector::onMaskTypeGroupIdClicked);
        h->addStretch(1);
        root->addWidget(row);
    }

    addSection(tr("Keying range"));
    {
        auto* hint = new QLabel(
            tr("Drives geometric / luma mask strength: hardness → mask width, feather → mask smoothness."),
            host);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("color: #8b92a3; font-size: 11px;"));
        root->addWidget(hint);
        auto* row1 = new QWidget(host);
        auto* h1   = new QHBoxLayout(row1);
        h1->setContentsMargins(0, 0, 0, 0);
        h1->addWidget(new QLabel(tr("Hardness"), row1));
        m_keyHardnessSlider = new QSlider(Qt::Horizontal, row1);
        m_keyHardnessSlider->setRange(0, kUnitSliderMax);
        m_keyHardnessSlider->setSingleStep(10);
        m_keyHardnessSlider->setPageStep(50);
        m_keyHardnessValue = new QLabel(formatUnit(0.0), row1);
        m_keyHardnessValue->setMinimumWidth(44);
        m_keyHardnessValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        connect(m_keyHardnessSlider, &QSlider::valueChanged,
                this, &ParameterInspector::onKeyHardnessSliderChanged);
        h1->addWidget(m_keyHardnessSlider, 1);
        h1->addWidget(m_keyHardnessValue);
        root->addWidget(row1);

        auto* row2 = new QWidget(host);
        auto* h2   = new QHBoxLayout(row2);
        h2->setContentsMargins(0, 0, 0, 0);
        h2->addWidget(new QLabel(tr("Feather"), row2));
        m_keyFeatherSlider = new QSlider(Qt::Horizontal, row2);
        m_keyFeatherSlider->setRange(0, kUnitSliderMax);
        m_keyFeatherSlider->setSingleStep(10);
        m_keyFeatherSlider->setPageStep(50);
        m_keyFeatherValue = new QLabel(formatUnit(0.0), row2);
        m_keyFeatherValue->setMinimumWidth(44);
        m_keyFeatherValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        connect(m_keyFeatherSlider, &QSlider::valueChanged,
                this, &ParameterInspector::onKeyFeatherSliderChanged);
        h2->addWidget(m_keyFeatherSlider, 1);
        h2->addWidget(m_keyFeatherValue);
        root->addWidget(row2);
    }

    addSection(tr("RGB key channels"));
    {
        auto* rgbHint = new QLabel(
            tr("Per-channel weights for future GPU keying (stored in the project; mixer shader can use them later)."),
            host);
        rgbHint->setWordWrap(true);
        rgbHint->setStyleSheet(QStringLiteral("color: #8b92a3; font-size: 11px;"));
        root->addWidget(rgbHint);

        auto makeRgbRow = [&](const QString& letter, QSlider** slider, QLabel** value,
                              void (ParameterInspector::*slot)(int)) {
            auto* row = new QWidget(host);
            auto* h   = new QHBoxLayout(row);
            h->setContentsMargins(0, 0, 0, 0);
            h->addWidget(new QLabel(letter, row));
            *slider = new QSlider(Qt::Horizontal, row);
            (*slider)->setRange(0, kUnitSliderMax);
            (*slider)->setSingleStep(10);
            (*slider)->setPageStep(50);
            *value = new QLabel(formatKeyPercent(1.0), row);
            (*value)->setMinimumWidth(40);
            (*value)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            connect(*slider, &QSlider::valueChanged, this, slot);
            h->addWidget(*slider, 1);
            h->addWidget(*value);
            root->addWidget(row);
        };
        makeRgbRow(tr("R"), &m_keyRSlider, &m_keyRValue, &ParameterInspector::onKeyChannelRChanged);
        makeRgbRow(tr("G"), &m_keyGSlider, &m_keyGValue, &ParameterInspector::onKeyChannelGChanged);
        makeRgbRow(tr("B"), &m_keyBSlider, &m_keyBValue, &ParameterInspector::onKeyChannelBChanged);
    }

    root->addStretch(1);
    return host;
}

QWidget* ParameterInspector::buildPositionTab()
{
    auto* host = new QWidget(this);
    auto* form = new QFormLayout(host);

    m_rotation = new QDoubleSpinBox(host);
    m_rotation->setRange(-1.0, 1.0);
    m_rotation->setSingleStep(0.05);
    m_rotation->setDecimals(3);
    connect(m_rotation, &QDoubleSpinBox::valueChanged,
            this, &ParameterInspector::onRotationChanged);
    form->addRow(tr("Rotation Z"), m_rotation);

    auto* note = new QLabel(tr("Position / scale / pivot controls - M5"), host);
    note->setStyleSheet(QStringLiteral("color: #8b92a3;"));
    form->addRow(QString(), note);

    return host;
}

QWidget* ParameterInspector::buildOutputTab()
{
    auto* host = new QWidget(this);
    auto* form = new QFormLayout(host);

    m_outputScreen = new QComboBox(host);
    m_outputScreen->setToolTip(tr("Monitor used for fullscreen output"));
    form->addRow(tr("Fullscreen screen"), m_outputScreen);

    m_fullscreenBtn = new QPushButton(tr("Toggle fullscreen output"), host);
    m_fullscreenBtn->setToolTip(tr("Show or hide the mixer output fullscreen on the selected screen"));
    connect(m_fullscreenBtn, &QPushButton::clicked,
            this, &ParameterInspector::fullscreenOutputToggled);
    form->addRow(QString(), m_fullscreenBtn);

    return host;
}

void ParameterInspector::rebuildScreenList()
{
    if (!m_outputScreen) {
        return;
    }
    m_outputScreen->clear();
    const auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        const auto* s = screens[i];
        const QString name = s->name();
        const auto g = s->geometry();
        m_outputScreen->addItem(tr("Screen %1: %2 (%3×%4)")
                                    .arg(i + 1)
                                    .arg(name.isEmpty() ? tr("Display") : name)
                                    .arg(g.width())
                                    .arg(g.height()));
    }
    if (m_outputScreen->count() > 0) {
        m_outputScreen->setCurrentIndex(0);
    }
}

int ParameterInspector::outputScreenIndex() const
{
    if (!m_outputScreen) {
        return 0;
    }
    return m_outputScreen->currentIndex();
}

void ParameterInspector::setProject(pvj::core::Project* project)
{
    m_project = project;
    refreshFromCell();
}

void ParameterInspector::setSelection(int bankSetIndex, int bankIndex, int cellIndex)
{
    m_bankSetIndex = bankSetIndex;
    m_bankIndex    = bankIndex;
    m_cellIndex    = cellIndex;
    refreshFromCell();
}

void ParameterInspector::setMidiMappingEditMode(bool on)
{
    if (m_midiMappingEditMode == on) {
        return;
    }
    m_midiMappingEditMode = on;
    if (on) {
        setStyleSheet(QStringLiteral(
            "QTabWidget::pane { border: 2px solid rgba(80, 220, 120, 0.75); border-radius: 4px; }"));
    } else {
        setStyleSheet(QString());
    }
}

void ParameterInspector::refreshFromModel()
{
    refreshFromCell();
}

pvj::core::Cell* ParameterInspector::currentCell()
{
    if (!m_project) return nullptr;
    if (m_bankSetIndex < 0 || m_bankSetIndex >= m_project->bankSets.size()) return nullptr;
    auto& set = m_project->bankSets[m_bankSetIndex];
    if (m_bankIndex < 0 || m_bankIndex >= set.banks.size()) return nullptr;
    auto& bank = set.banks[m_bankIndex];
    if (m_cellIndex < 0 || m_cellIndex >= bank.cells.size()) return nullptr;
    return &bank.cells[m_cellIndex];
}

void ParameterInspector::markMixingCustom()
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.mixingPresetIndex = 0;
    }
    QSignalBlocker b(m_mixingPreset);
    m_mixingPreset->setCurrentIndex(0);
}

void ParameterInspector::applyMixingPreset(int comboIndex)
{
    if (comboIndex <= 0 || comboIndex > kMixPresetCount) {
        return;
    }
    auto* cell = currentCell();
    if (!cell) {
        return;
    }
    const MixPreset& pr = kMixPresets[comboIndex - 1];
    cell->props.copyMode       = pr.cm;
    cell->props.maskType       = pr.mt;
    cell->props.transparency   = pr.transparency;
    cell->props.maskWidth      = pr.maskW;
    cell->props.maskSmoothness = pr.maskS;
    cell->props.keyChannelR    = pr.kr;
    cell->props.keyChannelG    = pr.kg;
    cell->props.keyChannelB    = pr.kb;
    cell->props.mixingPresetIndex = comboIndex;

    m_loading = true;
    QSignalBlocker b0(m_transparencySlider);
    QSignalBlocker b1(m_copyMode);
    QSignalBlocker b2(m_keyHardnessSlider);
    QSignalBlocker b3(m_keyFeatherSlider);
    QSignalBlocker b4(m_keyRSlider);
    QSignalBlocker b5(m_keyGSlider);
    QSignalBlocker b6(m_keyBSlider);
    QSignalBlocker b7(m_mixingPreset);

    m_transparencySlider->setValue(unitToSlider(cell->props.transparency));
    m_transparencyValue->setText(formatUnit(cell->props.transparency));
    const auto setCombo = [](QComboBox* cb, int value) {
        for (int i = 0; i < cb->count(); ++i) {
            if (cb->itemData(i).toInt() == value) {
                cb->setCurrentIndex(i);
                return;
            }
        }
    };
    setCombo(m_copyMode, int(cell->props.copyMode));
    m_keyHardnessSlider->setValue(unitToSlider(cell->props.maskWidth));
    m_keyHardnessValue->setText(formatUnit(cell->props.maskWidth));
    m_keyFeatherSlider->setValue(unitToSlider(cell->props.maskSmoothness));
    m_keyFeatherValue->setText(formatUnit(cell->props.maskSmoothness));
    m_keyRSlider->setValue(unitToSlider(cell->props.keyChannelR));
    m_keyRValue->setText(formatKeyPercent(cell->props.keyChannelR));
    m_keyGSlider->setValue(unitToSlider(cell->props.keyChannelG));
    m_keyGValue->setText(formatKeyPercent(cell->props.keyChannelG));
    m_keyBSlider->setValue(unitToSlider(cell->props.keyChannelB));
    m_keyBValue->setText(formatKeyPercent(cell->props.keyChannelB));
    m_mixingPreset->setCurrentIndex(comboIndex);
    syncMaskTypeButtons();

    m_loading = false;
    emitChanged();
}

void ParameterInspector::syncMaskTypeButtons()
{
    if (!m_maskTypeGroup) {
        return;
    }
    QSignalBlocker b(m_maskTypeGroup);
    const auto* cell = currentCell();
    const int id     = cell ? int(cell->props.maskType) : int(MaskType::None);
    if (QAbstractButton* btn = m_maskTypeGroup->button(id)) {
        btn->setChecked(true);
    }
}

void ParameterInspector::syncPlayModeButtons()
{
    if (!m_playModeGroup) {
        return;
    }
    QSignalBlocker b(m_playModeGroup);
    const auto* cell = currentCell();
    const int id     = cell ? int(cell->props.playMode) : int(PlayMode::LoopForward);
    if (QAbstractButton* btn = m_playModeGroup->button(id)) {
        btn->setChecked(true);
    }
}

void ParameterInspector::syncPriorityButtons()
{
    if (!m_priorityGroup) {
        return;
    }
    QSignalBlocker b(m_priorityGroup);
    for (QAbstractButton* btn : m_priorityGroup->buttons()) {
        btn->setChecked(false);
    }
    const auto* cell = currentCell();
    const int pri    = cell ? cell->props.priority : 0;
    QAbstractButton* pick = nullptr;
    if (pri <= -5) {
        pick = m_priorityGroup->button(-10);
    } else if (pri >= 5) {
        pick = m_priorityGroup->button(10);
    } else if (pri == 0) {
        pick = m_priorityGroup->button(0);
    }
    if (pick) {
        pick->setChecked(true);
    }
}

void ParameterInspector::syncVisualRowVisibility(const pvj::core::Cell* cell)
{
    const bool isFeedbackVisual = cell && cell->visual.type == VisualType::Generator
        && cell->visual.generator == GeneratorKind::Feedback;
    if (m_visualClipSection) {
        m_visualClipSection->setVisible(!isFeedbackVisual);
    }
    if (m_feedbackGroup) {
        m_feedbackGroup->setVisible(isFeedbackVisual);
    }
    if (m_visualPrevBtn) {
        m_visualPrevBtn->setVisible(!isFeedbackVisual);
    }
    if (m_visualNextBtn) {
        m_visualNextBtn->setVisible(!isFeedbackVisual);
    }
    if (m_audioDial) {
        m_audioDial->setVisible(!isFeedbackVisual);
    }
    if (m_audioDialValue) {
        m_audioDialValue->setVisible(!isFeedbackVisual);
    }
}

void ParameterInspector::refreshFromCell()
{
    m_loading = true;

    QSignalBlocker b0(m_speedSlider);
    QSignalBlocker b1(m_fadeSlider);
    QSignalBlocker b2(m_rotation);
    QSignalBlocker b3(m_copyMode);
    QSignalBlocker b4(m_transparencySlider);
    QSignalBlocker b5(m_keyHardnessSlider);
    QSignalBlocker b6(m_keyFeatherSlider);
    QSignalBlocker b7(m_keyRSlider);
    QSignalBlocker b8(m_keyGSlider);
    QSignalBlocker b9(m_keyBSlider);
    QSignalBlocker b10(m_mixingPreset);
    QSignalBlocker b12(m_audioDial);
    QSignalBlocker b13(m_segmentInSlider);
    QSignalBlocker b14(m_segmentOutSlider);
    QSignalBlocker b15(m_scratchSlider);
    QSignalBlocker b16(m_pauseClipBtn);
    QSignalBlocker b17(m_overlayLineEdit);
    QSignalBlocker b18(m_tcStartEdit);
    QSignalBlocker b33(m_layerBandGroup);
    QSignalBlocker bfb0(m_feedbackEnabled);
    QSignalBlocker bfb1(m_feedbackStrengthSlider);
    QSignalBlocker bfb2(m_feedbackZoomSlider);
    QSignalBlocker bfb3(m_feedbackDecaySlider);
    QSignalBlocker bfb4(m_feedbackBrightnessSlider);
    QSignalBlocker bfb5(m_feedbackSaturationSlider);
    QSignalBlocker bfb6(m_feedbackGammaSlider);
    QSignalBlocker bfb7(m_feedbackContrastSlider);
    QSignalBlocker bfb8(m_feedbackLayerBrightnessSlider);
    QSignalBlocker bfb9(m_feedbackLayerSaturationSlider);
    QSignalBlocker bfb10(m_feedbackLayerGammaSlider);
    QSignalBlocker bfb11(m_feedbackLayerContrastSlider);
    QSignalBlocker bfb12(m_feedbackWrapMode);
    QSignalBlocker bfb13(m_feedbackRotationSlider);
    QSignalBlocker bfb14(m_feedbackRotationAnimated);

    auto* cell = currentCell();
    const bool hasCell = (cell != nullptr);
    const bool isFeedbackVisual = hasCell && cell->visual.type == VisualType::Generator
        && cell->visual.generator == GeneratorKind::Feedback;
    syncVisualRowVisibility(cell);

    for (auto* w : {static_cast<QWidget*>(m_visualPrevBtn),
                     static_cast<QWidget*>(m_visualNextBtn),
                     static_cast<QWidget*>(m_speedSlider),
                     static_cast<QWidget*>(m_pauseClipBtn),
                     static_cast<QWidget*>(m_segmentInSlider),
                     static_cast<QWidget*>(m_segmentOutSlider),
                     static_cast<QWidget*>(m_scratchSlider),
                     static_cast<QWidget*>(m_overlayLineEdit),
                     static_cast<QWidget*>(m_tcStartEdit),
                     static_cast<QWidget*>(m_fadeSlider),
                     static_cast<QWidget*>(m_fadeValue),
                     static_cast<QWidget*>(m_rotation),
                     static_cast<QWidget*>(m_copyMode),
                     static_cast<QWidget*>(m_transparencySlider),
                     static_cast<QWidget*>(m_transparencyValue),
                     static_cast<QWidget*>(m_mixingPreset),
                     static_cast<QWidget*>(m_keyHardnessSlider),
                     static_cast<QWidget*>(m_keyHardnessValue),
                     static_cast<QWidget*>(m_keyFeatherSlider),
                     static_cast<QWidget*>(m_keyFeatherValue),
                     static_cast<QWidget*>(m_keyRSlider),
                     static_cast<QWidget*>(m_keyGSlider),
                     static_cast<QWidget*>(m_keyBSlider),
                     static_cast<QWidget*>(m_keyRValue),
                     static_cast<QWidget*>(m_keyGValue),
                     static_cast<QWidget*>(m_keyBValue),
                     static_cast<QWidget*>(m_audioDial),
                     static_cast<QWidget*>(m_audioDialValue)}) {
        if (w) {
            w->setEnabled(hasCell && !isFeedbackVisual);
        }
    }
    if (m_visualThumb) {
        m_visualThumb->setEnabled(true);
    }
    if (m_maskTypeGroup) {
        const auto buttons = m_maskTypeGroup->buttons();
        for (QAbstractButton* btn : buttons) {
            btn->setEnabled(hasCell);
        }
    }
    if (m_playModeGroup) {
        const auto buttons = m_playModeGroup->buttons();
        for (QAbstractButton* btn : buttons) {
            btn->setEnabled(hasCell && !isFeedbackVisual);
        }
    }
    if (m_priorityGroup) {
        const auto buttons = m_priorityGroup->buttons();
        for (QAbstractButton* btn : buttons) {
            btn->setEnabled(hasCell);
        }
    }
    if (m_layerBandGroup) {
        const auto buttons = m_layerBandGroup->buttons();
        for (QAbstractButton* btn : buttons) {
            btn->setEnabled(hasCell);
        }
    }

    const auto setFbEnabled = [hasCell, isFeedbackVisual](QWidget* w) {
        if (w) {
            w->setEnabled(hasCell && isFeedbackVisual);
        }
    };
    setFbEnabled(m_feedbackEnabled);
    setFbEnabled(m_feedbackStrengthSlider);
    setFbEnabled(m_feedbackStrengthValue);
    setFbEnabled(m_feedbackZoomSlider);
    setFbEnabled(m_feedbackZoomValue);
    setFbEnabled(m_feedbackRotationSlider);
    setFbEnabled(m_feedbackRotationValue);
    setFbEnabled(m_feedbackRotationAnimated);
    setFbEnabled(m_feedbackDecaySlider);
    setFbEnabled(m_feedbackDecayValue);
    setFbEnabled(m_feedbackBrightnessSlider);
    setFbEnabled(m_feedbackBrightnessValue);
    setFbEnabled(m_feedbackSaturationSlider);
    setFbEnabled(m_feedbackSaturationValue);
    setFbEnabled(m_feedbackGammaSlider);
    setFbEnabled(m_feedbackGammaValue);
    setFbEnabled(m_feedbackContrastSlider);
    setFbEnabled(m_feedbackContrastValue);
    setFbEnabled(m_feedbackLayerBrightnessSlider);
    setFbEnabled(m_feedbackLayerBrightnessValue);
    setFbEnabled(m_feedbackLayerSaturationSlider);
    setFbEnabled(m_feedbackLayerSaturationValue);
    setFbEnabled(m_feedbackLayerGammaSlider);
    setFbEnabled(m_feedbackLayerGammaValue);
    setFbEnabled(m_feedbackLayerContrastSlider);
    setFbEnabled(m_feedbackLayerContrastValue);
    setFbEnabled(m_feedbackWrapMode);

    if (!hasCell) {
        m_visualLabel->setText(tr("(no cell selected)"));
        clearVisualThumbnail();
        m_transparencySlider->setValue(unitToSlider(1.0));
        m_transparencyValue->setText(formatUnit(1.0));
        m_speedSlider->setValue(movieSpeedToSlider(1.0));
        m_speedValueLabel->setText(formatMovieSpeed(1.0));
        m_fadeSlider->setValue(unitToSlider(0.0));
        m_fadeValue->setText(formatUnit(0.0));
        m_rotation->setValue(0.0);
        m_audioDial->setValue(100);
        m_audioDialValue->setText(QStringLiteral("1.00"));
        m_segmentInSlider->setValue(0);
        m_segmentOutSlider->setValue(kUnitSliderMax);
        m_scratchSlider->setValue(unitToSlider(0.5));
        m_pauseClipBtn->setChecked(false);
        m_overlayLineEdit->clear();
        m_tcStartEdit->setText(QStringLiteral("00:00:00:00"));
        m_keyHardnessSlider->setValue(unitToSlider(0.0));
        m_keyHardnessValue->setText(formatUnit(0.0));
        m_keyFeatherSlider->setValue(unitToSlider(0.0));
        m_keyFeatherValue->setText(formatUnit(0.0));
        m_keyRSlider->setValue(unitToSlider(1.0));
        m_keyGSlider->setValue(unitToSlider(1.0));
        m_keyBSlider->setValue(unitToSlider(1.0));
        m_keyRValue->setText(formatKeyPercent(1.0));
        m_keyGValue->setText(formatKeyPercent(1.0));
        m_keyBValue->setText(formatKeyPercent(1.0));
        m_mixingPreset->setCurrentIndex(0);
        const auto setCombo = [](QComboBox* cb, int value) {
            for (int i = 0; i < cb->count(); ++i) {
                if (cb->itemData(i).toInt() == value) {
                    cb->setCurrentIndex(i);
                    return;
                }
            }
        };
        setCombo(m_copyMode, int(CopyMode::Normal));
        syncMaskTypeButtons();
        syncPlayModeButtons();
        syncPriorityButtons();
        if (m_layerBandGroup) {
            if (QAbstractButton* btn = m_layerBandGroup->button(int(LayerBand::Mid))) {
                btn->setChecked(true);
            }
        }
        if (m_feedbackEnabled) {
            const pvj::core::FeedbackParams d;
            m_feedbackEnabled->setChecked(d.enabled);
            m_feedbackStrengthSlider->setValue(unitToSlider(d.strength));
            m_feedbackStrengthValue->setText(formatUnit(d.strength));
            m_feedbackZoomSlider->setValue(rangeToSlider(d.zoom, 0.5, 2.0));
            m_feedbackZoomValue->setText(QString::number(d.zoom, 'f', 3));
            m_feedbackRotationSlider->setValue(rangeToSlider(d.rotationDeg, -180.0, 180.0));
            m_feedbackRotationValue->setText(QString::number(d.rotationDeg, 'f', 2));
            m_feedbackRotationAnimated->setChecked(d.rotationAnimated);
            m_feedbackDecaySlider->setValue(rangeToSlider(d.decay, -0.1, 0.1));
            m_feedbackDecayValue->setText(QString::number(d.decay, 'f', 3));
            m_feedbackBrightnessSlider->setValue(rangeToSlider(d.brightness, -1.0, 1.0));
            m_feedbackBrightnessValue->setText(formatSignedUnit(d.brightness));
            m_feedbackSaturationSlider->setValue(rangeToSlider(d.saturation, 0.0, 2.0));
            m_feedbackSaturationValue->setText(QString::number(d.saturation, 'f', 2));
            m_feedbackGammaSlider->setValue(rangeToSlider(d.gamma, 0.1, 4.0));
            m_feedbackGammaValue->setText(QString::number(d.gamma, 'f', 2));
            m_feedbackContrastSlider->setValue(rangeToSlider(d.contrast, 0.0, 2.0));
            m_feedbackContrastValue->setText(QString::number(d.contrast, 'f', 2));
            m_feedbackLayerBrightnessSlider->setValue(rangeToSlider(d.layerBrightness, -1.0, 1.0));
            m_feedbackLayerBrightnessValue->setText(formatSignedUnit(d.layerBrightness));
            m_feedbackLayerSaturationSlider->setValue(rangeToSlider(d.layerSaturation, 0.0, 2.0));
            m_feedbackLayerSaturationValue->setText(QString::number(d.layerSaturation, 'f', 2));
            m_feedbackLayerGammaSlider->setValue(rangeToSlider(d.layerGamma, 0.1, 4.0));
            m_feedbackLayerGammaValue->setText(QString::number(d.layerGamma, 'f', 2));
            m_feedbackLayerContrastSlider->setValue(rangeToSlider(d.layerContrast, 0.0, 2.0));
            m_feedbackLayerContrastValue->setText(QString::number(d.layerContrast, 'f', 2));
            const auto setWrapCombo = [](QComboBox* cb, int value) {
                for (int i = 0; i < cb->count(); ++i) {
                    if (cb->itemData(i).toInt() == value) {
                        cb->setCurrentIndex(i);
                        return;
                    }
                }
            };
            setWrapCombo(m_feedbackWrapMode, int(d.wrapMode));
        }
        if (m_feedbackPresetApplyBtn) {
            m_feedbackPresetApplyBtn->setEnabled(false);
        }
        m_loading = false;
        return;
    }

    QString visualText;
    switch (cell->visual.type) {
    case VisualType::Empty:
        visualText = tr("(empty)");
        break;
    case VisualType::Media:
        if (const auto* media = m_project->findMedia(cell->visual.mediaId)) {
            visualText = QFileInfo(media->path).fileName();
        } else {
            visualText = tr("(missing media reference)");
        }
        break;
    case VisualType::Generator:
        visualText = tr("Generator: %1")
                         .arg(pvj::core::enums::toString(cell->visual.generator));
        break;
    }
    m_visualLabel->setText(visualText);

    const auto setCombo = [](QComboBox* cb, int value) {
        for (int i = 0; i < cb->count(); ++i) {
            if (cb->itemData(i).toInt() == value) {
                cb->setCurrentIndex(i);
                return;
            }
        }
    };

    m_transparencySlider->setValue(unitToSlider(cell->props.transparency));
    m_transparencyValue->setText(formatUnit(cell->props.transparency));
    m_speedSlider->setValue(movieSpeedToSlider(cell->props.movieSpeed));
    m_speedValueLabel->setText(formatMovieSpeed(cell->props.movieSpeed));
    m_fadeSlider->setValue(unitToSlider(cell->props.fade));
    m_fadeValue->setText(formatUnit(cell->props.fade));
    m_rotation->setValue(cell->props.rotationZ);
    {
        const int dv = int(qRound(qBound(0.0, cell->props.audioGain, 4.0) * 100.0));
        m_audioDial->setValue(dv);
        m_audioDialValue->setText(QString::number(cell->props.audioGain, 'f', 2));
    }
    m_segmentInSlider->setValue(unitToSlider(cell->props.segmentInU));
    m_segmentOutSlider->setValue(unitToSlider(cell->props.segmentOutU));
    m_scratchSlider->setValue(unitToSlider(cell->props.scratchHeadU));
    m_pauseClipBtn->setChecked(cell->props.clipPaused);
    m_overlayLineEdit->setText(cell->props.overlayText);
    m_tcStartEdit->setText(cell->props.tcStart);
    m_keyHardnessSlider->setValue(unitToSlider(cell->props.maskWidth));
    m_keyHardnessValue->setText(formatUnit(cell->props.maskWidth));
    m_keyFeatherSlider->setValue(unitToSlider(cell->props.maskSmoothness));
    m_keyFeatherValue->setText(formatUnit(cell->props.maskSmoothness));
    m_keyRSlider->setValue(unitToSlider(cell->props.keyChannelR));
    m_keyGSlider->setValue(unitToSlider(cell->props.keyChannelG));
    m_keyBSlider->setValue(unitToSlider(cell->props.keyChannelB));
    m_keyRValue->setText(formatKeyPercent(cell->props.keyChannelR));
    m_keyGValue->setText(formatKeyPercent(cell->props.keyChannelG));
    m_keyBValue->setText(formatKeyPercent(cell->props.keyChannelB));
    setCombo(m_copyMode, int(cell->props.copyMode));
    syncMaskTypeButtons();
    syncPlayModeButtons();
    syncPriorityButtons();
    if (m_layerBandGroup) {
        const int band = qBound(int(LayerBand::Back), int(cell->props.layerBand), int(LayerBand::Front));
        if (QAbstractButton* btn = m_layerBandGroup->button(band)) {
            btn->setChecked(true);
        }
    }

    const int presetIdx = qBound(0, cell->props.mixingPresetIndex, m_mixingPreset->count() - 1);
    m_mixingPreset->setCurrentIndex(presetIdx);

    if (m_feedbackEnabled) {
        const pvj::core::FeedbackParams& fb = cell->props.feedback;
        m_feedbackEnabled->setChecked(fb.enabled);
        m_feedbackStrengthSlider->setValue(unitToSlider(fb.strength));
        m_feedbackStrengthValue->setText(formatUnit(fb.strength));
        m_feedbackZoomSlider->setValue(rangeToSlider(fb.zoom, 0.5, 2.0));
        m_feedbackZoomValue->setText(QString::number(fb.zoom, 'f', 3));
        m_feedbackRotationSlider->setValue(rangeToSlider(fb.rotationDeg, -180.0, 180.0));
        m_feedbackRotationValue->setText(QString::number(fb.rotationDeg, 'f', 2));
        m_feedbackRotationAnimated->setChecked(fb.rotationAnimated);
        m_feedbackDecaySlider->setValue(rangeToSlider(fb.decay, -0.1, 0.1));
        m_feedbackDecayValue->setText(QString::number(fb.decay, 'f', 3));
        m_feedbackBrightnessSlider->setValue(rangeToSlider(fb.brightness, -1.0, 1.0));
        m_feedbackBrightnessValue->setText(formatSignedUnit(fb.brightness));
        m_feedbackSaturationSlider->setValue(rangeToSlider(fb.saturation, 0.0, 2.0));
        m_feedbackSaturationValue->setText(QString::number(fb.saturation, 'f', 2));
        m_feedbackGammaSlider->setValue(rangeToSlider(fb.gamma, 0.1, 4.0));
        m_feedbackGammaValue->setText(QString::number(fb.gamma, 'f', 2));
        m_feedbackContrastSlider->setValue(rangeToSlider(fb.contrast, 0.0, 2.0));
        m_feedbackContrastValue->setText(QString::number(fb.contrast, 'f', 2));
        m_feedbackLayerBrightnessSlider->setValue(rangeToSlider(fb.layerBrightness, -1.0, 1.0));
        m_feedbackLayerBrightnessValue->setText(formatSignedUnit(fb.layerBrightness));
        m_feedbackLayerSaturationSlider->setValue(rangeToSlider(fb.layerSaturation, 0.0, 2.0));
        m_feedbackLayerSaturationValue->setText(QString::number(fb.layerSaturation, 'f', 2));
        m_feedbackLayerGammaSlider->setValue(rangeToSlider(fb.layerGamma, 0.1, 4.0));
        m_feedbackLayerGammaValue->setText(QString::number(fb.layerGamma, 'f', 2));
        m_feedbackLayerContrastSlider->setValue(rangeToSlider(fb.layerContrast, 0.0, 2.0));
        m_feedbackLayerContrastValue->setText(QString::number(fb.layerContrast, 'f', 2));
        setCombo(m_feedbackWrapMode, int(fb.wrapMode));
    }
    if (m_feedbackPresetApplyBtn) {
        m_feedbackPresetApplyBtn->setEnabled(true);
    }

    m_loading = false;
}

void ParameterInspector::emitChanged()
{
    emit cellChanged(m_bankSetIndex, m_bankIndex, m_cellIndex);
}

void ParameterInspector::onMixingPresetChanged(int idx)
{
    if (m_loading) {
        return;
    }
    if (idx <= 0) {
        if (auto* c = currentCell()) {
            c->props.mixingPresetIndex = 0;
            emitChanged();
        }
        return;
    }
    applyMixingPreset(idx);
}

void ParameterInspector::onTransparencySliderChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_transparencyValue) {
        m_transparencyValue->setText(formatUnit(u));
    }
    if (auto* c = currentCell()) {
        c->props.transparency = u;
        markMixingCustom();
        emitChanged();
    }
}
void ParameterInspector::onMovieSpeedSliderChanged(int v)
{
    if (m_loading) return;
    const double s = sliderToMovieSpeed(v);
    if (m_speedValueLabel) {
        m_speedValueLabel->setText(formatMovieSpeed(s));
    }
    if (auto* c = currentCell()) {
        c->props.movieSpeed = s;
        emitChanged();
    }
}

void ParameterInspector::onAudioDialChanged(int v)
{
    if (m_loading) return;
    const double g = qBound(0.0, double(v) / 100.0, 4.0);
    if (m_audioDialValue) {
        m_audioDialValue->setText(QString::number(g, 'f', 2));
    }
    if (auto* c = currentCell()) {
        c->props.audioGain = g;
        emitChanged();
    }
}

void ParameterInspector::onPlayModeGroupClicked(int id)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.playMode = static_cast<PlayMode>(id);
        emitChanged();
    }
}

void ParameterInspector::onPriorityGroupClicked(int id)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.priority = id;
        emitChanged();
    }
}

void ParameterInspector::onLayerBandGroupClicked(int id)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        if (id < int(LayerBand::Back) || id > int(LayerBand::Front)) {
            id = int(LayerBand::Mid);
        }
        c->props.layerBand = static_cast<LayerBand>(id);
        emitChanged();
    }
}

void ParameterInspector::onClipPauseToggled(bool checked)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.clipPaused = checked;
        emitChanged();
    }
}

void ParameterInspector::onSegmentInChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (auto* c = currentCell()) {
        c->props.segmentInU = u;
        if (c->props.segmentOutU < u + 0.01) {
            c->props.segmentOutU = qMin(1.0, u + 0.01);
            QSignalBlocker b(m_segmentOutSlider);
            m_segmentOutSlider->setValue(unitToSlider(c->props.segmentOutU));
        }
        emitChanged();
    }
}

void ParameterInspector::onSegmentOutChanged(int v)
{
    if (m_loading) return;
    double u = sliderToUnit(v);
    if (auto* c = currentCell()) {
        if (u < c->props.segmentInU + 0.01) {
            u = qMin(1.0, c->props.segmentInU + 0.01);
            QSignalBlocker b(m_segmentOutSlider);
            m_segmentOutSlider->setValue(unitToSlider(u));
        }
        c->props.segmentOutU = u;
        emitChanged();
    }
}

void ParameterInspector::onScratchSliderChanged(int v)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.scratchHeadU = sliderToUnit(v);
        emitChanged();
    }
}

void ParameterInspector::onScratchSliderReleased()
{
    emit scratchApplyRequested();
}

void ParameterInspector::onFeedbackPresetApplyClicked()
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->visual.type = VisualType::Generator;
        c->visual.generator = GeneratorKind::Feedback;
        c->visual.mediaId = {};
        c->props.feedback.enabled = true;
        c->props.feedback.rotationAnimated = false;
        emitChanged();
        refreshFromCell();
    }
}

void ParameterInspector::onFeedbackEnabledToggled(bool checked)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.feedback.enabled = checked;
        emitChanged();
    }
}

void ParameterInspector::onFeedbackStrengthChanged(int v)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.feedback.strength = sliderToUnit(v);
        if (m_feedbackStrengthValue) {
            m_feedbackStrengthValue->setText(formatUnit(c->props.feedback.strength));
        }
        emitChanged();
    }
}

void ParameterInspector::onFeedbackZoomChanged(int v)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.feedback.zoom = sliderToRange(v, 0.5, 2.0);
        if (m_feedbackZoomValue) {
            m_feedbackZoomValue->setText(QString::number(c->props.feedback.zoom, 'f', 3));
        }
        emitChanged();
    }
}

void ParameterInspector::onFeedbackRotationChanged(int v)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.feedback.rotationDeg = sliderToRange(v, -180.0, 180.0);
        if (m_feedbackRotationValue) {
            m_feedbackRotationValue->setText(QString::number(c->props.feedback.rotationDeg, 'f', 2));
        }
        emitChanged();
    }
}

void ParameterInspector::onFeedbackRotationAnimatedToggled(bool checked)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.feedback.rotationAnimated = checked;
        emitChanged();
    }
}

void ParameterInspector::onFeedbackDecayChanged(int v)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.feedback.decay = sliderToRange(v, -0.1, 0.1);
        if (m_feedbackDecayValue) {
            m_feedbackDecayValue->setText(QString::number(c->props.feedback.decay, 'f', 3));
        }
        emitChanged();
    }
}

void ParameterInspector::onFeedbackBrightnessChanged(int v)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.feedback.brightness = sliderToRange(v, -1.0, 1.0);
        if (m_feedbackBrightnessValue) {
            m_feedbackBrightnessValue->setText(formatSignedUnit(c->props.feedback.brightness));
        }
        emitChanged();
    }
}

void ParameterInspector::onFeedbackSaturationChanged(int v)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.feedback.saturation = sliderToRange(v, 0.0, 2.0);
        if (m_feedbackSaturationValue) {
            m_feedbackSaturationValue->setText(QString::number(c->props.feedback.saturation, 'f', 2));
        }
        emitChanged();
    }
}

void ParameterInspector::onFeedbackGammaChanged(int v)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.feedback.gamma = sliderToRange(v, 0.1, 4.0);
        if (m_feedbackGammaValue) {
            m_feedbackGammaValue->setText(QString::number(c->props.feedback.gamma, 'f', 2));
        }
        emitChanged();
    }
}

void ParameterInspector::onFeedbackContrastChanged(int v)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.feedback.contrast = sliderToRange(v, 0.0, 2.0);
        if (m_feedbackContrastValue) {
            m_feedbackContrastValue->setText(QString::number(c->props.feedback.contrast, 'f', 2));
        }
        emitChanged();
    }
}

void ParameterInspector::onFeedbackLayerBrightnessChanged(int v)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.feedback.layerBrightness = sliderToRange(v, -1.0, 1.0);
        if (m_feedbackLayerBrightnessValue) {
            m_feedbackLayerBrightnessValue->setText(formatSignedUnit(c->props.feedback.layerBrightness));
        }
        emitChanged();
    }
}

void ParameterInspector::onFeedbackLayerSaturationChanged(int v)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.feedback.layerSaturation = sliderToRange(v, 0.0, 2.0);
        if (m_feedbackLayerSaturationValue) {
            m_feedbackLayerSaturationValue->setText(QString::number(c->props.feedback.layerSaturation, 'f', 2));
        }
        emitChanged();
    }
}

void ParameterInspector::onFeedbackLayerGammaChanged(int v)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.feedback.layerGamma = sliderToRange(v, 0.1, 4.0);
        if (m_feedbackLayerGammaValue) {
            m_feedbackLayerGammaValue->setText(QString::number(c->props.feedback.layerGamma, 'f', 2));
        }
        emitChanged();
    }
}

void ParameterInspector::onFeedbackLayerContrastChanged(int v)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        c->props.feedback.layerContrast = sliderToRange(v, 0.0, 2.0);
        if (m_feedbackLayerContrastValue) {
            m_feedbackLayerContrastValue->setText(QString::number(c->props.feedback.layerContrast, 'f', 2));
        }
        emitChanged();
    }
}

void ParameterInspector::onFeedbackWrapModeChanged(int /*idx*/)
{
    if (m_loading) {
        return;
    }
    if (auto* c = currentCell()) {
        if (!m_feedbackWrapMode) {
            return;
        }
        c->props.feedback.wrapMode = static_cast<WrapMode>(m_feedbackWrapMode->currentData().toInt());
        emitChanged();
    }
}

void ParameterInspector::onOverlayTextEdited(const QString& t)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.overlayText = t;
        emitChanged();
    }
}

void ParameterInspector::onTcStartEdited(const QString& t)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.tcStart = t;
        emitChanged();
    }
}

void ParameterInspector::setVisualThumbnail(const QImage& frame)
{
    if (!m_visualThumb) {
        return;
    }
    if (frame.isNull()) {
        clearVisualThumbnail();
        return;
    }
    m_visualThumb->setFrame(frame);
}

void ParameterInspector::clearVisualThumbnail()
{
    if (!m_visualThumb) {
        return;
    }
    m_visualThumb->clearThumb();
}
void ParameterInspector::onFadeSliderChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_fadeValue) {
        m_fadeValue->setText(formatUnit(u));
    }
    if (auto* c = currentCell()) {
        c->props.fade = u;
        emitChanged();
    }
}
void ParameterInspector::onRotationChanged(double v)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.rotationZ = v;
        emitChanged();
    }
}
void ParameterInspector::onCopyModeChanged(int idx)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        const int selectedMode = m_copyMode->itemData(idx).toInt();
        c->props.copyMode = CopyMode(selectedMode);
        markMixingCustom();
        emitChanged();
    }
}
void ParameterInspector::onMaskTypeGroupIdClicked(int id)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.maskType = MaskType(id);
        markMixingCustom();
        emitChanged();
    }
}
void ParameterInspector::onKeyHardnessSliderChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_keyHardnessValue) {
        m_keyHardnessValue->setText(formatUnit(u));
    }
    if (auto* c = currentCell()) {
        c->props.maskWidth = u;
        markMixingCustom();
        emitChanged();
    }
}
void ParameterInspector::onKeyFeatherSliderChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_keyFeatherValue) {
        m_keyFeatherValue->setText(formatUnit(u));
    }
    if (auto* c = currentCell()) {
        c->props.maskSmoothness = u;
        markMixingCustom();
        emitChanged();
    }
}

void ParameterInspector::onKeyChannelRChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_keyRValue) {
        m_keyRValue->setText(formatKeyPercent(u));
    }
    if (auto* c = currentCell()) {
        c->props.keyChannelR = u;
        markMixingCustom();
        emitChanged();
    }
}

void ParameterInspector::onKeyChannelGChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_keyGValue) {
        m_keyGValue->setText(formatKeyPercent(u));
    }
    if (auto* c = currentCell()) {
        c->props.keyChannelG = u;
        markMixingCustom();
        emitChanged();
    }
}

void ParameterInspector::onKeyChannelBChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_keyBValue) {
        m_keyBValue->setText(formatKeyPercent(u));
    }
    if (auto* c = currentCell()) {
        c->props.keyChannelB = u;
        markMixingCustom();
        emitChanged();
    }
}

void ParameterInspector::tagMidiWidget(QWidget* w, const QString& propertyId, const QVariant& noteValue)
{
    if (!w) {
        return;
    }
    MidiLearnMenu::tagMidiWidget(w, propertyId, noteValue);
    w->installEventFilter(this);
    m_midiTaggedWidgets.append(w);
}

void ParameterInspector::registerMidiWidgets()
{
    tagMidiWidget(m_audioDial, QStringLiteral("audioGain"));
    tagMidiWidget(m_speedSlider, QStringLiteral("movieSpeed"));
    tagMidiWidget(m_pauseClipBtn, QStringLiteral("clipPaused"));
    tagMidiWidget(m_segmentInSlider, QStringLiteral("segmentInU"));
    tagMidiWidget(m_segmentOutSlider, QStringLiteral("segmentOutU"));
    tagMidiWidget(m_scratchSlider, QStringLiteral("scratchHeadU"));
    tagMidiWidget(m_fadeSlider, QStringLiteral("fade"));
    tagMidiWidget(m_rotation, QStringLiteral("rotationZ"));

    if (m_playModeGroup) {
        const int playLast = int(PlayMode::StepFrame);
        const double playDen = playLast > 0 ? double(playLast) : 1.0;
        for (int i = 0; i <= playLast; ++i) {
            if (auto* b = m_playModeGroup->button(i)) {
                // Normalized 0…1 so MIDI Set-on-press matches CC scrub via applyEnumFromNormalized.
                tagMidiWidget(qobject_cast<QWidget*>(b), QStringLiteral("playMode"),
                              double(i) / playDen);
            }
        }
    }
    if (m_priorityGroup) {
        tagMidiWidget(qobject_cast<QWidget*>(m_priorityGroup->button(-10)), QStringLiteral("priority"),
                      0.0);
        tagMidiWidget(qobject_cast<QWidget*>(m_priorityGroup->button(0)), QStringLiteral("priority"),
                      0.5);
        tagMidiWidget(qobject_cast<QWidget*>(m_priorityGroup->button(10)), QStringLiteral("priority"),
                      1.0);
    }

    tagMidiWidget(m_transparencySlider, QStringLiteral("transparency"));
    tagMidiWidget(m_mixingPreset, QStringLiteral("mixingPresetIndex"));
    tagMidiWidget(m_copyMode, QStringLiteral("copyMode"));
    if (m_maskTypeGroup) {
        constexpr int kMaskEnumLast = int(MaskType::Custom);
        const double maskDen = kMaskEnumLast > 0 ? double(kMaskEnumLast) : 1.0;
        const QList<QAbstractButton*> maskBtns = m_maskTypeGroup->buttons();
        for (auto* b : maskBtns) {
            const int id = m_maskTypeGroup->id(b);
            tagMidiWidget(qobject_cast<QWidget*>(b), QStringLiteral("maskType"),
                          double(id) / maskDen);
        }
    }
    tagMidiWidget(m_keyHardnessSlider, QStringLiteral("maskWidth"));
    tagMidiWidget(m_keyFeatherSlider, QStringLiteral("maskSmoothness"));
    tagMidiWidget(m_keyRSlider, QStringLiteral("keyChannelR"));
    tagMidiWidget(m_keyGSlider, QStringLiteral("keyChannelG"));
    tagMidiWidget(m_keyBSlider, QStringLiteral("keyChannelB"));

    tagMidiWidget(m_feedbackEnabled, QStringLiteral("feedbackEnabled"));
    tagMidiWidget(m_feedbackStrengthSlider, QStringLiteral("feedbackStrength"));
    tagMidiWidget(m_feedbackZoomSlider, QStringLiteral("feedbackZoom"));
    tagMidiWidget(m_feedbackRotationSlider, QStringLiteral("feedbackRotationDeg"));
    tagMidiWidget(m_feedbackRotationAnimated, QStringLiteral("feedbackRotationAnimated"));
    tagMidiWidget(m_feedbackDecaySlider, QStringLiteral("feedbackDecay"));
    tagMidiWidget(m_feedbackBrightnessSlider, QStringLiteral("feedbackBrightness"));
    tagMidiWidget(m_feedbackSaturationSlider, QStringLiteral("feedbackSaturation"));
    tagMidiWidget(m_feedbackGammaSlider, QStringLiteral("feedbackGamma"));
    tagMidiWidget(m_feedbackContrastSlider, QStringLiteral("feedbackContrast"));
    tagMidiWidget(m_feedbackLayerBrightnessSlider, QStringLiteral("feedbackLayerBrightness"));
    tagMidiWidget(m_feedbackLayerSaturationSlider, QStringLiteral("feedbackLayerSaturation"));
    tagMidiWidget(m_feedbackLayerGammaSlider, QStringLiteral("feedbackLayerGamma"));
    tagMidiWidget(m_feedbackLayerContrastSlider, QStringLiteral("feedbackLayerContrast"));
    tagMidiWidget(m_feedbackWrapMode, QStringLiteral("feedbackWrapMode"));
}

void ParameterInspector::showMidiContextMenu(QWidget* w, const QPoint& globalPos)
{
    MidiLearnMenu::showForWidget(
        this,
        w,
        globalPos,
        [this](const QString& property) { emit midiLearnCcRequested(property); },
        [this](const QString& property, bool toggle, double buttonValue) {
            emit midiLearnNoteRequested(property, toggle, buttonValue);
        },
        [this](const QString& property) { emit midiClearMappingRequested(property); });
}

bool ParameterInspector::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::ContextMenu) {
        auto* w = qobject_cast<QWidget*>(watched);
        if (w && !w->property("pvjProperty").toString().isEmpty()) {
            auto* ce = static_cast<QContextMenuEvent*>(event);
            showMidiContextMenu(w, ce->globalPos());
            return true;
        }
    }
    if (m_midiMappingEditMode && event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            auto* w = qobject_cast<QWidget*>(watched);
            if (w && !w->property("pvjProperty").toString().isEmpty()) {
                showMidiContextMenu(w, me->globalPosition().toPoint());
                return true;
            }
        }
    }
    return QTabWidget::eventFilter(watched, event);
}

} // namespace pvj::app

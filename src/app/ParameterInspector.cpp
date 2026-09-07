#include "ParameterInspector.h"

#include "MainWindow.h"
#include "MidiLearnMenu.h"
#include "MidiMapOverlay.h"

#include "core/EnumStrings.h"
#include "core/FilterEffectIds.h"
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
#include <QMainWindow>
#include <QStatusBar>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPaintEvent>
#include <QScrollArea>
#include <QPainter>
#include <QPainterPath>
#include <QLineEdit>
#include <QPushButton>
#include <QAbstractButton>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QGroupBox>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QStandardItemModel>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>
#include <algorithm>

namespace pvj::app {

using pvj::core::CopyMode;
using pvj::core::GeneratorKind;
using pvj::core::KeyingMode;
using pvj::core::LayerMatteRole;
using pvj::core::MaskType;
using pvj::core::PlayMode;
using pvj::core::Project;
using pvj::core::VisualType;

bool cellAllowsUserName(const pvj::core::Cell& cell)
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
                               return !pvj::core::isFeedbackMarkerNode(n.typeId);
                           });
    }
    return false;
}

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

class KeyRangeWidget final : public QWidget
{
public:
    enum class GradientMode {
        Luma,
        Hue,
    };

    explicit KeyRangeWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(24);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMouseTracking(true);
    }

    void setGradientMode(GradientMode mode)
    {
        if (m_mode == mode) {
            return;
        }
        m_mode = mode;
        update();
    }

    void setRange(double minV, double maxV)
    {
        const double lo = qBound(0.0, qMin(minV, maxV), 1.0);
        const double hi = qBound(0.0, qMax(minV, maxV), 1.0);
        if (qFuzzyCompare(m_min + 1.0, lo + 1.0) && qFuzzyCompare(m_max + 1.0, hi + 1.0)) {
            return;
        }
        m_min = lo;
        m_max = hi;
        update();
    }

    double minValue() const { return m_min; }
    double maxValue() const { return m_max; }

    void setOnRangeChanged(std::function<void(double, double)> cb)
    {
        m_onRangeChanged = std::move(cb);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF groove(4.0, 8.0, qMax(10.0, width() - 8.0), 10.0);
        QLinearGradient grad(groove.left(), groove.top(), groove.right(), groove.top());
        if (m_mode == GradientMode::Hue) {
            grad.setColorAt(0.00, QColor::fromRgbF(1.0, 0.0, 0.0));
            grad.setColorAt(0.17, QColor::fromRgbF(1.0, 1.0, 0.0));
            grad.setColorAt(0.33, QColor::fromRgbF(0.0, 1.0, 0.0));
            grad.setColorAt(0.50, QColor::fromRgbF(0.0, 1.0, 1.0));
            grad.setColorAt(0.67, QColor::fromRgbF(0.0, 0.0, 1.0));
            grad.setColorAt(0.83, QColor::fromRgbF(1.0, 0.0, 1.0));
            grad.setColorAt(1.00, QColor::fromRgbF(1.0, 0.0, 0.0));
        } else {
            grad.setColorAt(0.0, QColor(10, 10, 10));
            grad.setColorAt(1.0, QColor(245, 245, 245));
        }
        p.setPen(QPen(QColor(80, 88, 100), 1.0));
        p.setBrush(grad);
        p.drawRoundedRect(groove, 2.0, 2.0);

        const double xMin = groove.left() + groove.width() * m_min;
        const double xMax = groove.left() + groove.width() * m_max;
        const QRectF selRect(xMin, groove.top(), qMax(2.0, xMax - xMin), groove.height());
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 55));
        p.drawRect(selRect);

        p.setPen(QPen(QColor(230, 230, 230), 1.5));
        drawMarker(&p, xMin, groove.top() - 2.0, true);
        drawMarker(&p, xMax, groove.bottom() + 2.0, false);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        const double x = event->position().x();
        const double left = valueToX(m_min);
        const double right = valueToX(m_max);
        if (qAbs(x - left) <= qAbs(x - right)) {
            m_draggingLeft = true;
        } else {
            m_draggingLeft = false;
        }
        handleDrag(x);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!(event->buttons() & Qt::LeftButton)) {
            return;
        }
        handleDrag(event->position().x());
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        Q_UNUSED(event);
    }

private:
    static void drawMarker(QPainter* p, double x, double y, bool down)
    {
        QPolygonF tri;
        if (down) {
            tri << QPointF(x - 4.0, y - 4.0) << QPointF(x + 4.0, y - 4.0) << QPointF(x, y + 2.0);
        } else {
            tri << QPointF(x - 4.0, y + 4.0) << QPointF(x + 4.0, y + 4.0) << QPointF(x, y - 2.0);
        }
        p->drawPolygon(tri);
    }

    double valueToX(double v) const
    {
        const double left = 4.0;
        const double w = qMax(10.0, width() - 8.0);
        return left + qBound(0.0, v, 1.0) * w;
    }

    double xToValue(double x) const
    {
        const double left = 4.0;
        const double w = qMax(10.0, width() - 8.0);
        return qBound(0.0, (x - left) / w, 1.0);
    }

    void handleDrag(double x)
    {
        const double v = xToValue(x);
        if (m_draggingLeft) {
            m_min = qMin(v, m_max);
        } else {
            m_max = qMax(v, m_min);
        }
        update();
        if (m_onRangeChanged) {
            m_onRangeChanged(m_min, m_max);
        }
    }

    GradientMode m_mode = GradientMode::Luma;
    double m_min = 0.4;
    double m_max = 0.6;
    bool m_draggingLeft = true;
    std::function<void(double, double)> m_onRangeChanged;
};

namespace {

constexpr int kUnitSliderMax = 1000;
constexpr double kKeyUnitEpsilon = 1.0 / (kUnitSliderMax * 2.0);

struct KeyingUiValues {
    bool keyingEnabled = false;
    KeyingMode keyingMode = KeyingMode::Luma;
    double keyThreshold = 0.25;
    double keySoftness = 0.12;
    double keyLumaCenter = 0.5;
    bool keyLumaInvert = false;
    double keyChromaHue = 0.33;
    bool keyChromaInvert = false;
    double keyChannelR = 1.0;
    double keyChannelG = 1.0;
    double keyChannelB = 1.0;
};

KeyingUiValues keyingUiValues(const pvj::core::Cell* cell, const LayerKeyingState* layerOverride)
{
    KeyingUiValues v;
    if (layerOverride && layerOverride->valid) {
        v.keyingEnabled = layerOverride->keyingEnabled;
        v.keyingMode = layerOverride->keyingMode;
        v.keyThreshold = layerOverride->keyThreshold;
        v.keySoftness = layerOverride->keySoftness;
        v.keyLumaCenter = layerOverride->keyLumaCenter;
        v.keyLumaInvert = layerOverride->keyLumaInvert;
        v.keyChromaHue = layerOverride->keyChromaHue;
        v.keyChromaInvert = layerOverride->keyChromaInvert;
        v.keyChannelR = layerOverride->keyChannelR;
        v.keyChannelG = layerOverride->keyChannelG;
        v.keyChannelB = layerOverride->keyChannelB;
        return v;
    }
    if (!cell) {
        return v;
    }
    v.keyingEnabled = cell->props.keyingEnabled;
    v.keyingMode = cell->props.keyingMode;
    v.keyThreshold = cell->props.keyThreshold;
    v.keySoftness = cell->props.keySoftness;
    v.keyLumaCenter = cell->props.keyLumaCenter;
    v.keyLumaInvert = cell->props.keyLumaInvert;
    v.keyChromaHue = cell->props.keyChromaHue;
    v.keyChromaInvert = cell->props.keyChromaInvert;
    v.keyChannelR = cell->props.keyChannelR;
    v.keyChannelG = cell->props.keyChannelG;
    v.keyChannelB = cell->props.keyChannelB;
    return v;
}

void applyKeyingUiValuesToCell(pvj::core::CellProps& props, const KeyingUiValues& v)
{
    props.keyingEnabled = v.keyingEnabled;
    props.keyingMode = v.keyingMode;
    props.keyThreshold = v.keyThreshold;
    props.keySoftness = v.keySoftness;
    props.keyLumaCenter = v.keyLumaCenter;
    props.keyLumaInvert = v.keyLumaInvert;
    props.keyChromaHue = v.keyChromaHue;
    props.keyChromaInvert = v.keyChromaInvert;
    props.keyChannelR = v.keyChannelR;
    props.keyChannelG = v.keyChannelG;
    props.keyChannelB = v.keyChannelB;
}

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

constexpr double kMovieSpeedMin     = 0.0;
constexpr double kMovieSpeedMax     = 4.0;
constexpr double kMovieSpeedSnap    = 1.0;
constexpr double kMovieSpeedSnapEps = (kMovieSpeedMax - kMovieSpeedMin) / (double(kUnitSliderMax) * 2.0);

int movieSpeedToSlider(double s)
{
    const double t = qBound(kMovieSpeedMin, s, kMovieSpeedMax);
    return int(qRound((t - kMovieSpeedMin) / (kMovieSpeedMax - kMovieSpeedMin)
                      * double(kUnitSliderMax)));
}

double sliderToMovieSpeed(int v)
{
    const double s = kMovieSpeedMin
        + (kMovieSpeedMax - kMovieSpeedMin) * (double(v) / double(kUnitSliderMax));
    if (qAbs(s - kMovieSpeedSnap) < kMovieSpeedSnapEps) {
        return kMovieSpeedSnap;
    }
    return s;
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
    CopyMode copyMode;
    MaskType maskType;
    KeyingMode keyingMode;
    bool keyingEnabled;
    bool keyLumaInvert;
    double keyLumaCenter;
    bool keyChromaInvert;
    double keyChromaHue;
    double keyChannelR;
    double keyChannelG;
    double keyChannelB;
    double transparency;
    double maskWidth;
    double maskSmoothness;
};

// Stored preset row index = combo index (0-based into this table).
static const MixPreset kMixPresets[] = {
    { CopyMode::Normal, MaskType::None, KeyingMode::Luma, false, false, 0.50, false, 0.33, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0 }, // Default
    { CopyMode::Normal, MaskType::None, KeyingMode::Luma, true, false, 0.20, false, 0.33, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0 }, // Reject Black
    { CopyMode::Normal, MaskType::None, KeyingMode::Luma, true, true, 0.80, false, 0.33, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0 },  // Reject White
    { CopyMode::Normal, MaskType::None, KeyingMode::Chroma, true, false, 0.50, true, 0.00, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 }, // Reject Red
    { CopyMode::Normal, MaskType::None, KeyingMode::Chroma, true, false, 0.50, true, 0.33, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0 }, // Reject Green
    { CopyMode::Normal, MaskType::None, KeyingMode::Chroma, true, false, 0.50, true, 0.66, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0 }, // Reject Blue
    { CopyMode::Normal, MaskType::None, KeyingMode::Chroma, true, false, 0.50, false, 0.00, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 }, // Red
    { CopyMode::Normal, MaskType::None, KeyingMode::Chroma, true, false, 0.50, false, 0.33, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0 }, // Green
    { CopyMode::Normal, MaskType::None, KeyingMode::Chroma, true, false, 0.50, false, 0.66, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0 }, // Blue
    { CopyMode::Normal, MaskType::None, KeyingMode::Chroma, true, false, 0.50, false, 0.83, 1.0, 0.0, 1.0, 1.0, 0.0, 0.0 }, // Purple
    { CopyMode::Normal, MaskType::None, KeyingMode::Chroma, true, false, 0.50, false, 0.16, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0 }, // Yellow
    { CopyMode::Normal, MaskType::None, KeyingMode::Chroma, true, false, 0.50, false, 0.08, 1.0, 0.5, 0.0, 1.0, 0.0, 0.0 }, // Orange
};

constexpr int kMixPresetCount = int(sizeof(kMixPresets) / sizeof(kMixPresets[0]));

void fillWrapModeCombo(QComboBox* cb)
{
    if (!cb) {
        return;
    }
    cb->clear();
    cb->addItem(QObject::tr("Clamp"), int(pvj::core::WrapMode::Clamp));
    cb->addItem(QObject::tr("Repeat"), int(pvj::core::WrapMode::Repeat));
    cb->addItem(QObject::tr("Mirror"), int(pvj::core::WrapMode::Mirror));
    cb->addItem(QObject::tr("Mirror once"), int(pvj::core::WrapMode::MirrorOnce));
    cb->addItem(QObject::tr("Black"), int(pvj::core::WrapMode::Black));
}

void fillFeedbackBlendCombo(QComboBox* cb)
{
    if (!cb) {
        return;
    }
    cb->clear();
    using BM = pvj::core::FeedbackBlendMode;
    cb->addItem(QObject::tr("Add"), int(BM::Add));
    cb->addItem(QObject::tr("Mix"), int(BM::Mix));
    cb->addItem(QObject::tr("Screen"), int(BM::Screen));
    cb->addItem(QObject::tr("Lighten"), int(BM::Lighten));
    cb->addItem(QObject::tr("Multiply"), int(BM::Multiply));
    cb->addItem(QObject::tr("Difference"), int(BM::Difference));
}

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

enum class VisualSourceKind : int {
    Empty = 0,
    Media = 1,
    TestPattern = 2,
    Solid = 3,
    Spout = 4,
    Ndi = 5,
    Feedback = 6,
    MixerFilter = 7,
};

int visualSourceKindFromCell(const pvj::core::Cell& c)
{
    if (c.visual.type == VisualType::Empty) {
        return int(VisualSourceKind::Empty);
    }
    if (c.visual.type == VisualType::Media) {
        return int(VisualSourceKind::Media);
    }
    if (c.visual.type == VisualType::MixerFilter) {
        return int(VisualSourceKind::MixerFilter);
    }
    if (c.visual.type == VisualType::Generator) {
        switch (c.visual.generator) {
        case GeneratorKind::TestPattern: return int(VisualSourceKind::TestPattern);
        case GeneratorKind::SolidColor: return int(VisualSourceKind::Solid);
        case GeneratorKind::InputSpout: return int(VisualSourceKind::Spout);
        case GeneratorKind::InputNdi: return int(VisualSourceKind::Ndi);
        case GeneratorKind::InternalFeedback: return int(VisualSourceKind::Feedback);
        default: break;
        }
    }
    return int(VisualSourceKind::Empty);
}

void applyVisualSourceKind(pvj::core::Cell& c, int kind)
{
    switch (static_cast<VisualSourceKind>(kind)) {
    case VisualSourceKind::Empty:
        c.visual.type = VisualType::Empty;
        c.visual.generator = GeneratorKind::None;
        c.visual.mediaId = {};
        break;
    case VisualSourceKind::Media:
        c.visual.type = VisualType::Media;
        c.visual.generator = GeneratorKind::None;
        break;
    case VisualSourceKind::TestPattern:
        c.visual.type = VisualType::Generator;
        c.visual.generator = GeneratorKind::TestPattern;
        c.visual.mediaId = {};
        break;
    case VisualSourceKind::Solid:
        c.visual.type = VisualType::Generator;
        c.visual.generator = GeneratorKind::SolidColor;
        c.visual.mediaId = {};
        break;
    case VisualSourceKind::Spout:
        c.visual.type = VisualType::Generator;
        c.visual.generator = GeneratorKind::InputSpout;
        c.visual.mediaId = {};
        break;
    case VisualSourceKind::Ndi:
        c.visual.type = VisualType::Generator;
        c.visual.generator = GeneratorKind::InputNdi;
        c.visual.mediaId = {};
        break;
    case VisualSourceKind::Feedback:
        c.visual.type = VisualType::Generator;
        c.visual.generator = GeneratorKind::InternalFeedback;
        c.visual.mediaId = {};
        break;
    case VisualSourceKind::MixerFilter:
        c.visual.type = VisualType::MixerFilter;
        c.visual.generator = GeneratorKind::None;
        c.visual.mediaId = {};
        break;
    }
}

int signedToSlider(double v, double minV, double maxV)
{
    return rangeToSlider(v, minV, maxV);
}

double sliderToSigned(int s, double minV, double maxV)
{
    return sliderToRange(s, minV, maxV);
}

double normalizeDegrees360(double deg)
{
    double r = std::fmod(deg, 360.0);
    if (r < 0.0) {
        r += 360.0;
    }
    return r;
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
    m_feedbackTabIndex = addTab(buildFeedbackTab(), tr("Feedback"));
    addTab(buildPositionTab(), tr("Position"));

    connect(this, &QTabWidget::currentChanged, this, [this](int) {
        const auto* cell = currentCell();
        const int kind = cell ? visualSourceKindFromCell(*cell) : int(VisualSourceKind::Empty);
        syncKeyingPanelPlacement(cell != nullptr, kind);
    });

    registerMidiWidgets();

    for (QSlider* slider : findChildren<QSlider*>()) {
        connect(slider, &QSlider::sliderPressed, this, &ParameterInspector::continuousEditBegan);
        connect(slider, &QSlider::sliderReleased, this, &ParameterInspector::continuousEditEnded);
    }
    for (QDial* dial : findChildren<QDial*>()) {
        connect(dial, &QDial::sliderPressed, this, &ParameterInspector::continuousEditBegan);
        connect(dial, &QDial::sliderReleased, this, &ParameterInspector::continuousEditEnded);
    }

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
        vh->addWidget(m_audioDial);
        vh->addWidget(m_audioDialValue);
        vh->addStretch(1);
        topGrid->addWidget(visRow, 0, 1);
        stdLay->addWidget(top);
    }

    {
        auto* srcRow = new QWidget(m_visualStandardSection);
        auto* srcGrid = new QGridLayout(srcRow);
        srcGrid->setColumnStretch(1, 1);
        srcGrid->addWidget(new QLabel(tr("Source"), srcRow), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
        m_visualSourceCombo = new QComboBox(srcRow);
        m_visualSourceCombo->addItem(tr("Empty"), int(VisualSourceKind::Empty));
        m_visualSourceCombo->addItem(tr("Media clip"), int(VisualSourceKind::Media));
        m_visualSourceCombo->addItem(tr("Test pattern"), int(VisualSourceKind::TestPattern));
        m_visualSourceCombo->addItem(tr("Solid color"), int(VisualSourceKind::Solid));
        m_visualSourceCombo->addItem(tr("Spout"), int(VisualSourceKind::Spout));
        m_visualSourceCombo->addItem(tr("NDI"), int(VisualSourceKind::Ndi));
        m_visualSourceCombo->addItem(tr("Feedback"), int(VisualSourceKind::Feedback));
        m_visualSourceCombo->addItem(tr("Mixer filter"), int(VisualSourceKind::MixerFilter));
        connect(m_visualSourceCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                this, &ParameterInspector::onVisualSourceChanged);
        srcGrid->addWidget(m_visualSourceCombo, 0, 1);
        stdLay->addWidget(srcRow);
    }

    {
        m_cellNameRow = new QWidget(m_visualStandardSection);
        auto* nameGrid = new QGridLayout(m_cellNameRow);
        nameGrid->setColumnStretch(1, 1);
        nameGrid->addWidget(new QLabel(tr("Name"), m_cellNameRow), 0, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
        m_cellNameEdit = new QLineEdit(m_cellNameRow);
        m_cellNameEdit->setPlaceholderText(tr("Optional display name"));
        connect(m_cellNameEdit, &QLineEdit::editingFinished, this, [this]() {
            if (m_cellNameEdit) {
                onCellNameEdited(m_cellNameEdit->text());
            }
        });
        nameGrid->addWidget(m_cellNameEdit, 0, 1);
        m_cellNameRow->setVisible(false);
        stdLay->addWidget(m_cellNameRow);
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
        markSliderResetDefault(m_speedSlider, movieSpeedToSlider(1.0));
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
        markSliderResetDefault(m_segmentInSlider, 0);
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
        markSliderResetDefault(m_segmentOutSlider, kUnitSliderMax);
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
        markSliderResetDefault(m_scratchSlider, unitToSlider(0.5));
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
        markSliderResetDefault(m_fadeSlider, 0);
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

    root->addWidget(m_visualStandardSection);
    {
        auto* layerRow = new QWidget(host);
        auto* lbh = new QHBoxLayout(layerRow);
        lbh->setContentsMargins(0, 0, 0, 0);
        lbh->setSpacing(6);
        lbh->addWidget(new QLabel(tr("Layer"), layerRow));
        auto* layerBtns = new QWidget(layerRow);
        auto* layerBtnLay = new QHBoxLayout(layerBtns);
        layerBtnLay->setContentsMargins(0, 0, 0, 0);
        layerBtnLay->setSpacing(3);
        m_preferredLayerGroup = new QButtonGroup(layerRow);
        m_preferredLayerGroup->setExclusive(true);
        layerBtns->setStyleSheet(QStringLiteral(
            "QToolButton {"
            "  border: 1px solid #3a404c;"
            "  border-radius: 3px;"
            "  min-width: 24px;"
            "  min-height: 24px;"
            "  padding: 0;"
            "}"
            "QToolButton:checked {"
            "  border: 2px solid #ff9140;"
            "  background: #3a2e24;"
            "}"
            "QToolButton:hover:!checked {"
            "  border-color: #606878;"
            "}"
            "QToolButton:disabled {"
            "  color: #888;"
            "  border-color: #2e323a;"
            "  background: #000000;"
            "}"));
        {
            auto* bgTb = new QToolButton(layerBtns);
            bgTb->setEnabled(false);
            bgTb->setText(QStringLiteral("BG"));
            bgTb->setToolTip(tr("Fixed black background (not assignable)"));
            layerBtnLay->addWidget(bgTb);
        }
        for (int layer = 1; layer <= 13; ++layer) {
            auto* tb = new QToolButton(layerBtns);
            tb->setCheckable(true);
            tb->setAutoRaise(false);
            tb->setText(QString::number(layer));
            tb->setToolTip(tr("Preferred mix layer %1").arg(layer));
            m_preferredLayerGroup->addButton(tb, layer - 1);
            layerBtnLay->addWidget(tb);
        }
        connect(m_preferredLayerGroup, &QButtonGroup::idClicked,
                this, &ParameterInspector::onPreferredLayerChanged);
        lbh->addWidget(layerBtns, 1);
        root->addWidget(layerRow);
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
           "Each cell can target a preferred mix layer (1-13) in the Visual tab."),
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
    m_mixingPreset->addItem(tr("Default"), 0);
    m_mixingPreset->addItem(tr("Reject Black"), 1);
    m_mixingPreset->addItem(tr("Reject White"), 2);
    m_mixingPreset->addItem(tr("Reject Red"), 3);
    m_mixingPreset->addItem(tr("Reject Green"), 4);
    m_mixingPreset->addItem(tr("Reject Blue"), 5);
    m_mixingPreset->addItem(tr("Red"), 6);
    m_mixingPreset->addItem(tr("Green"), 7);
    m_mixingPreset->addItem(tr("Blue"), 8);
    m_mixingPreset->addItem(tr("Purple"), 9);
    m_mixingPreset->addItem(tr("Yellow"), 10);
    m_mixingPreset->addItem(tr("Orange"), 11);
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
        markSliderResetDefault(m_transparencySlider, unitToSlider(1.0));
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
    {
        auto* row = new QWidget(host);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(new QLabel(tr("Layer role"), row));
        m_matteRoleCombo = new QComboBox(row);
        m_matteRoleCombo->addItem(tr("Normal"), int(LayerMatteRole::None));
        m_matteRoleCombo->addItem(tr("Luma matte (below)"), int(LayerMatteRole::LumaMatte));
        m_matteRoleCombo->addItem(tr("Alpha matte (below)"), int(LayerMatteRole::AlphaMatte));
        m_matteRoleCombo->addItem(tr("Knockout (below)"), int(LayerMatteRole::KnockOut));
        connect(m_matteRoleCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                this, &ParameterInspector::onMatteRoleChanged);
        h->addWidget(m_matteRoleCombo, 1);
        root->addWidget(row);
    }
    {
        auto addMaskRow = [&](QWidget** rowOut, const QString& label, QSlider** sliderOut, QLabel** valueOut,
                              void (ParameterInspector::*slot)(int), int resetDefault) {
            auto* row = new QWidget(host);
            auto* h   = new QHBoxLayout(row);
            h->setContentsMargins(0, 0, 0, 0);
            h->addWidget(new QLabel(label, row));
            auto* slider = new QSlider(Qt::Horizontal, row);
            slider->setRange(0, kUnitSliderMax);
            slider->setSingleStep(10);
            slider->setPageStep(50);
            auto* value = new QLabel(formatUnit(0.0), row);
            value->setMinimumWidth(44);
            value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            connect(slider, &QSlider::valueChanged, this, slot);
            h->addWidget(slider, 1);
            h->addWidget(value);
            markSliderResetDefault(slider, resetDefault);
            *rowOut = row;
            *sliderOut = slider;
            *valueOut = value;
            root->addWidget(row);
        };
        addMaskRow(&m_maskRectWidthRow, tr("Width"), &m_maskRectWidthSlider, &m_maskRectWidthValue,
                   &ParameterInspector::onMaskRectWidthChanged, unitToSlider(1.0));
        addMaskRow(&m_maskRectHeightRow, tr("Height"), &m_maskRectHeightSlider, &m_maskRectHeightValue,
                   &ParameterInspector::onMaskRectHeightChanged, unitToSlider(1.0));
        addMaskRow(&m_maskRadiusRow, tr("Radius"), &m_maskRadiusSlider, &m_maskRadiusValue,
                   &ParameterInspector::onMaskRadiusChanged, unitToSlider(0.5));
        addMaskRow(&m_maskEllipseXRow, tr("Ellipse X"), &m_maskEllipseXSlider, &m_maskEllipseXValue,
                   &ParameterInspector::onMaskEllipseXChanged, unitToSlider(0.6));
        addMaskRow(&m_maskEllipseYRow, tr("Ellipse Y"), &m_maskEllipseYSlider, &m_maskEllipseYValue,
                   &ParameterInspector::onMaskEllipseYChanged, unitToSlider(0.45));
        addMaskRow(&m_maskFeatherRow, tr("Feather"), &m_maskFeatherSlider, &m_maskFeatherValue,
                   &ParameterInspector::onMaskFeatherChanged, unitToSlider(0.1));
    }

    addSection(tr("Keying"));
    m_mixingKeyingSlot = new QWidget(host);
    auto* mixingKeyLay = new QVBoxLayout(m_mixingKeyingSlot);
    mixingKeyLay->setContentsMargins(0, 0, 0, 0);
    mixingKeyLay->setSpacing(4);
    root->addWidget(m_mixingKeyingSlot);
    buildKeyingPanel();
    mixingKeyLay->addWidget(m_keyingPanel);

    syncMaskControlVisibility();
    syncKeyingModeUi();
    root->addStretch(1);
    return host;
}

void ParameterInspector::buildKeyingPanel()
{
    if (m_keyingPanel) {
        return;
    }
    m_keyingPanel = new QWidget(this);
    auto* root = new QVBoxLayout(m_keyingPanel);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);
    QWidget* host = m_keyingPanel;

    m_keyingEnabled = new QCheckBox(tr("Enable keying"), host);
    m_keyingEnabled->setChecked(false);
    connect(m_keyingEnabled, &QCheckBox::toggled,
            this, &ParameterInspector::onKeyingEnabledToggled);
    root->addWidget(m_keyingEnabled);
    m_keyingMode = new QComboBox(host);
    m_keyingMode->addItem(tr("B/W keying (Luma)"), int(KeyingMode::Luma));
    m_keyingMode->addItem(tr("Color keying (Chroma)"), int(KeyingMode::Chroma));
    connect(m_keyingMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ParameterInspector::onKeyingModeChanged);
    root->addWidget(m_keyingMode);
    {
        m_keyLumaTargetRow = new QWidget(host);
        auto* h = new QHBoxLayout(m_keyLumaTargetRow);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(new QLabel(tr("Mask mode"), m_keyLumaTargetRow));
        m_keyLumaTargetGroup = new QButtonGroup(m_keyLumaTargetRow);
        m_keyLumaTargetGroup->setExclusive(true);
        auto* blackBtn = new QToolButton(m_keyLumaTargetRow);
        blackBtn->setText(tr("Black"));
        blackBtn->setCheckable(true);
        blackBtn->setAutoRaise(true);
        blackBtn->setToolTip(tr("Key out dark areas"));
        m_keyLumaTargetGroup->addButton(blackBtn, 0);
        auto* whiteBtn = new QToolButton(m_keyLumaTargetRow);
        whiteBtn->setText(tr("White"));
        whiteBtn->setCheckable(true);
        whiteBtn->setAutoRaise(true);
        whiteBtn->setToolTip(tr("Key out bright areas"));
        m_keyLumaTargetGroup->addButton(whiteBtn, 1);
        connect(m_keyLumaTargetGroup, &QButtonGroup::idClicked,
                this, &ParameterInspector::onKeyLumaTargetChanged);
        h->addWidget(blackBtn);
        h->addWidget(whiteBtn);
        h->addStretch(1);
        root->addWidget(m_keyLumaTargetRow);
    }
    {
        m_keyRangeRow = new QWidget(host);
        auto* h = new QHBoxLayout(m_keyRangeRow);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(new QLabel(tr("Keying range"), m_keyRangeRow));
        m_keyRangeSlider = new KeyRangeWidget(m_keyRangeRow);
        m_keyRangeSlider->setGradientMode(KeyRangeWidget::GradientMode::Luma);
        m_keyRangeValue = new QLabel(formatUnit(0.5), m_keyRangeRow);
        m_keyRangeValue->setMinimumWidth(44);
        m_keyRangeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_keyRangeSlider->setRange(0.25, 0.75);
        m_keyRangeSlider->setOnRangeChanged([this](double minV, double maxV) {
            onKeyLumaRangeChanged(minV, maxV);
        });
        h->addWidget(m_keyRangeSlider, 1);
        h->addWidget(m_keyRangeValue);
        root->addWidget(m_keyRangeRow);
    }
    {
        m_keyChromaTargetRow = new QWidget(host);
        auto* h = new QHBoxLayout(m_keyChromaTargetRow);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(new QLabel(tr("Mask mode"), m_keyChromaTargetRow));
        m_keyChromaTargetGroup = new QButtonGroup(m_keyChromaTargetRow);
        m_keyChromaTargetGroup->setExclusive(true);
        auto* colorBtn = new QToolButton(m_keyChromaTargetRow);
        colorBtn->setText(tr("Color"));
        colorBtn->setCheckable(true);
        colorBtn->setAutoRaise(true);
        colorBtn->setToolTip(tr("Key out selected hue"));
        m_keyChromaTargetGroup->addButton(colorBtn, 0);
        auto* invBtn = new QToolButton(m_keyChromaTargetRow);
        invBtn->setText(tr("Invert"));
        invBtn->setCheckable(true);
        invBtn->setAutoRaise(true);
        invBtn->setToolTip(tr("Keep selected hue and key out other colors"));
        m_keyChromaTargetGroup->addButton(invBtn, 1);
        connect(m_keyChromaTargetGroup, &QButtonGroup::idClicked,
                this, &ParameterInspector::onKeyChromaTargetChanged);
        h->addWidget(colorBtn);
        h->addWidget(invBtn);
        h->addStretch(1);
        root->addWidget(m_keyChromaTargetRow);
    }
    {
        m_keyChromaRangeRow = new QWidget(host);
        auto* h = new QHBoxLayout(m_keyChromaRangeRow);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(new QLabel(tr("Keying range"), m_keyChromaRangeRow));
        m_keyChromaRangeSlider = new KeyRangeWidget(m_keyChromaRangeRow);
        m_keyChromaRangeSlider->setGradientMode(KeyRangeWidget::GradientMode::Hue);
        m_keyChromaRangeValue = new QLabel(formatUnit(0.33), m_keyChromaRangeRow);
        m_keyChromaRangeValue->setMinimumWidth(44);
        m_keyChromaRangeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_keyChromaRangeSlider->setRange(0.23, 0.43);
        m_keyChromaRangeSlider->setOnRangeChanged([this](double minV, double maxV) {
            onKeyChromaRangeChanged(minV, maxV);
        });
        h->addWidget(m_keyChromaRangeSlider, 1);
        h->addWidget(m_keyChromaRangeValue);
        root->addWidget(m_keyChromaRangeRow);
    }

    {
        m_keyingHint = new QLabel(
            tr("B/W keying controls the luma matte. Color keying also uses RGB channel weights."),
            host);
        m_keyingHint->setWordWrap(true);
        m_keyingHint->setStyleSheet(QStringLiteral("color: #8b92a3; font-size: 11px;"));
        root->addWidget(m_keyingHint);
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
        markSliderResetDefault(m_keyHardnessSlider, 0);
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
        markSliderResetDefault(m_keyFeatherSlider, unitToSlider(0.12));
        root->addWidget(row2);
    }
    {
        m_keyRgbHint = new QLabel(
            tr("RGB channel weights for color keying (chroma)."),
            host);
        m_keyRgbHint->setWordWrap(true);
        m_keyRgbHint->setStyleSheet(QStringLiteral("color: #8b92a3; font-size: 11px;"));
        root->addWidget(m_keyRgbHint);

        auto makeRgbRow = [&](QWidget** rowOut, const QString& letter, QSlider** slider, QLabel** value,
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
            markSliderResetDefault(*slider, unitToSlider(1.0));
            *rowOut = row;
            root->addWidget(row);
        };
        makeRgbRow(&m_keyRRow, tr("R"), &m_keyRSlider, &m_keyRValue, &ParameterInspector::onKeyChannelRChanged);
        makeRgbRow(&m_keyGRow, tr("G"), &m_keyGSlider, &m_keyGValue, &ParameterInspector::onKeyChannelGChanged);
        makeRgbRow(&m_keyBRow, tr("B"), &m_keyBSlider, &m_keyBValue, &ParameterInspector::onKeyChannelBChanged);
    }
}

QWidget* ParameterInspector::buildFeedbackTab()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* host = new QWidget(scroll);
    auto* root = new QVBoxLayout(host);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(4);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

    auto styleCombo = [](QComboBox* cb) {
        cb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        cb->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        cb->setMinimumContentsLength(16);
    };

    m_feedbackInputLabel = new QLabel(tr("Layers below (live inject)"), host);
    m_feedbackInputLabel->setStyleSheet(QStringLiteral("color: palette(text);"));
    form->addRow(tr("Input"), m_feedbackInputLabel);

    m_feedbackInputModeHint = new QLabel(
        tr("blend(live, retention×history). Empty live → fade-only; opacity≤2% clears buffer."),
        host);
    m_feedbackInputModeHint->setWordWrap(true);
    m_feedbackInputModeHint->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 11px;"));
    form->addRow(QString(), m_feedbackInputModeHint);

    auto addSliderRow = [&](const QString& title, QSlider** sliderOut, QLabel** valueOut,
                            auto slot, int resetDefault) {
        auto* row = new QWidget(host);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        *sliderOut = new QSlider(Qt::Horizontal, row);
        (*sliderOut)->setRange(0, kUnitSliderMax);
        (*sliderOut)->setSingleStep(10);
        (*sliderOut)->setPageStep(50);
        *valueOut = new QLabel(formatUnit(0.0), row);
        (*valueOut)->setMinimumWidth(44);
        (*valueOut)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        connect(*sliderOut, &QSlider::valueChanged, this, slot);
        h->addWidget(*sliderOut, 1);
        h->addWidget(*valueOut);
        markSliderResetDefault(*sliderOut, resetDefault);
        form->addRow(title, row);
    };

    {
        auto* section = new QLabel(QStringLiteral("<b>%1</b>").arg(tr("Input color (pre-feedback)")), host);
        form->addRow(section);
    }
    addSliderRow(tr("Saturation"), &m_feedbackInSaturationSlider, &m_feedbackInSaturationValue,
                 &ParameterInspector::onFeedbackInSaturationChanged, rangeToSlider(1.0, 0.0, 2.0));
    addSliderRow(tr("Brightness"), &m_feedbackInBrightnessSlider, &m_feedbackInBrightnessValue,
                 &ParameterInspector::onFeedbackInBrightnessChanged, signedToSlider(0.0, -1.0, 1.0));
    addSliderRow(tr("Contrast"), &m_feedbackInContrastSlider, &m_feedbackInContrastValue,
                 &ParameterInspector::onFeedbackInContrastChanged, rangeToSlider(1.0, 0.0, 2.0));
    addSliderRow(tr("Hue"), &m_feedbackInHueShiftSlider, &m_feedbackInHueShiftValue,
                 &ParameterInspector::onFeedbackInHueShiftChanged, unitToSlider(0.0));
    addSliderRow(tr("Gamma"), &m_feedbackInGammaSlider, &m_feedbackInGammaValue,
                 &ParameterInspector::onFeedbackInGammaChanged, rangeToSlider(1.0, 0.1, 4.0));

    {
        auto* section = new QLabel(QStringLiteral("<b>%1</b>").arg(tr("Feedback color")), host);
        form->addRow(section);
    }
    addSliderRow(tr("Saturation"), &m_feedbackSaturationSlider, &m_feedbackSaturationValue,
                 &ParameterInspector::onFeedbackSaturationChanged, rangeToSlider(1.0, 0.0, 2.0));
    addSliderRow(tr("Brightness"), &m_feedbackBrightnessSlider, &m_feedbackBrightnessValue,
                 &ParameterInspector::onFeedbackBrightnessChanged, signedToSlider(0.0, -1.0, 1.0));
    addSliderRow(tr("Contrast"), &m_feedbackContrastSlider, &m_feedbackContrastValue,
                 &ParameterInspector::onFeedbackContrastChanged, rangeToSlider(1.0, 0.0, 2.0));
    addSliderRow(tr("Hue"), &m_feedbackHueShiftSlider, &m_feedbackHueShiftValue,
                 &ParameterInspector::onFeedbackHueShiftChanged, unitToSlider(0.0));
    addSliderRow(tr("Gamma"), &m_feedbackGammaSlider, &m_feedbackGammaValue,
                 &ParameterInspector::onFeedbackGammaChanged, rangeToSlider(1.0, 0.1, 4.0));
    addSliderRow(tr("Rotation"), &m_feedbackRotationSlider, &m_feedbackRotationValue,
                 &ParameterInspector::onFeedbackRotationChanged, rangeToSlider(0.0, 0.0, 360.0));
    if (m_feedbackRotationSlider) {
        m_feedbackRotationSlider->setToolTip(tr("Feedback path rotation (0–360°)."));
    }

    addSliderRow(tr("Zoom"), &m_feedbackZoomSlider, &m_feedbackZoomValue,
                 &ParameterInspector::onFeedbackZoomChanged,
                 signedToSlider(0.0, pvj::core::kFeedbackZoomMin, pvj::core::kFeedbackZoomMax));
    addSliderRow(tr("Translate X"), &m_feedbackTranslateXSlider, &m_feedbackTranslateXValue,
                 &ParameterInspector::onFeedbackTranslateXChanged,
                 signedToSlider(0.0, pvj::core::kFeedbackTranslateMin,
                                pvj::core::kFeedbackTranslateMax));
    if (m_feedbackTranslateXSlider) {
        m_feedbackTranslateXSlider->setToolTip(tr("Feedback UV pan X (−0.25…0.25)."));
    }
    addSliderRow(tr("Translate Y"), &m_feedbackTranslateYSlider, &m_feedbackTranslateYValue,
                 &ParameterInspector::onFeedbackTranslateYChanged,
                 signedToSlider(0.0, pvj::core::kFeedbackTranslateMin,
                                pvj::core::kFeedbackTranslateMax));
    if (m_feedbackTranslateYSlider) {
        m_feedbackTranslateYSlider->setToolTip(tr("Feedback UV pan Y (−0.25…0.25)."));
    }
    addSliderRow(tr("Retention"), &m_feedbackRetentionSlider, &m_feedbackRetentionValue,
                 &ParameterInspector::onFeedbackRetentionChanged, unitToSlider(0.95));
    if (m_feedbackRetentionSlider) {
        m_feedbackRetentionSlider->setToolTip(
            tr("History share 0–1 (0 = live only, 0.95 = long trail, fades when live is black)."));
    }

    m_feedbackBlendCombo = new QComboBox(host);
    fillFeedbackBlendCombo(m_feedbackBlendCombo);
    styleCombo(m_feedbackBlendCombo);
    m_feedbackBlendCombo->setToolTip(
        tr("How live inject combines with retained history inside the feedback loop."));
    connect(m_feedbackBlendCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ParameterInspector::onFeedbackBlendModeChanged);
    form->addRow(tr("Blend"), m_feedbackBlendCombo);

    m_feedbackWrapCombo = new QComboBox(host);
    fillWrapModeCombo(m_feedbackWrapCombo);
    styleCombo(m_feedbackWrapCombo);
    m_feedbackWrapCombo->setToolTip(
        tr("UV border for feedback path (live, like rotation and zoom)."));
    connect(m_feedbackWrapCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ParameterInspector::onFeedbackWrapModeChanged);
    form->addRow(tr("Wrap"), m_feedbackWrapCombo);

    {
        auto* row = new QWidget(host);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        m_feedbackFrameDelaySlider = new QSlider(Qt::Horizontal, row);
        m_feedbackFrameDelaySlider->setRange(0, pvj::core::kFeedbackMaxFrameDelay);
        m_feedbackFrameDelaySlider->setSingleStep(1);
        m_feedbackFrameDelaySlider->setPageStep(1);
        m_feedbackFrameDelaySlider->setToolTip(
            tr("Classic ping-pong ignores frame delay (kept for project compatibility)."));
        m_feedbackFrameDelayValue = new QLabel(QStringLiteral("0"), row);
        m_feedbackFrameDelayValue->setMinimumWidth(44);
        m_feedbackFrameDelayValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        connect(m_feedbackFrameDelaySlider, &QSlider::valueChanged, this,
                &ParameterInspector::onFeedbackFrameDelayChanged);
        h->addWidget(m_feedbackFrameDelaySlider, 1);
        h->addWidget(m_feedbackFrameDelayValue);
        markSliderResetDefault(m_feedbackFrameDelaySlider, 0);
        form->addRow(tr("Frame delay"), row);
    }

    root->addLayout(form);

    m_feedbackKeyingSection = new QWidget(host);
    auto* fbKeyRoot = new QVBoxLayout(m_feedbackKeyingSection);
    fbKeyRoot->setContentsMargins(0, 0, 0, 0);
    fbKeyRoot->setSpacing(4);
    fbKeyRoot->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(tr("Keying")), host));
    m_feedbackKeyingSlot = new QWidget(host);
    auto* fbKeyLay = new QVBoxLayout(m_feedbackKeyingSlot);
    fbKeyLay->setContentsMargins(0, 0, 0, 0);
    fbKeyLay->setSpacing(4);
    fbKeyRoot->addWidget(m_feedbackKeyingSlot);
    root->addWidget(m_feedbackKeyingSection);
    buildKeyingPanel();

    root->addStretch(1);
    scroll->setWidget(host);
    return scroll;
}

QWidget* ParameterInspector::buildPositionTab()
{
    auto* host = new QWidget(this);
    auto* form = new QFormLayout(host);

    m_rotation = new QDoubleSpinBox(host);
    // UI in degrees; stored as rotationZ −1…+1 (GrandVJ). Full MIDI CC range = 360°.
    m_rotation->setRange(-180.0, 180.0);
    m_rotation->setSingleStep(1.0);
    m_rotation->setDecimals(1);
    m_rotation->setSuffix(tr("°"));
    m_rotation->setToolTip(tr("Layer rotation (−180° … +180°). MIDI CC 0→−180°, 64→0°, 127→+180°."));
    connect(m_rotation, &QDoubleSpinBox::valueChanged,
            this, &ParameterInspector::onRotationChanged);
    form->addRow(tr("Rotation Z"), m_rotation);

    m_pictureWrapCombo = new QComboBox(host);
    fillWrapModeCombo(m_pictureWrapCombo);
    m_pictureWrapCombo->setToolTip(
        tr("UV border behaviour for layer zoom/rotation (when this cell is on a mix layer)."));
    connect(m_pictureWrapCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ParameterInspector::onPictureWrapModeChanged);
    form->addRow(tr("UV wrap"), m_pictureWrapCombo);

    auto* note = new QLabel(tr("Position / scale / pivot controls - M5"), host);
    note->setStyleSheet(QStringLiteral("color: #8b92a3;"));
    form->addRow(QString(), note);

    return host;
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
    if (m_midiMappingEditMode) {
        applyMidiMapOverlays();
    }
}

void ParameterInspector::setLayerKeyingOverride(const LayerKeyingState* state)
{
    if (m_layerKeyingOverride == state) {
        return;
    }
    m_layerKeyingOverride = state;
    refreshKeyingUi();
}

void ParameterInspector::refreshKeyingUi()
{
    if (m_loading) {
        return;
    }
    auto* cell = currentCell();
    m_loading = true;
    QSignalBlocker b5(m_keyHardnessSlider);
    QSignalBlocker b6(m_keyFeatherSlider);
    QSignalBlocker b7(m_keyRSlider);
    QSignalBlocker b8(m_keyGSlider);
    QSignalBlocker b9(m_keyBSlider);
    QSignalBlocker b11(m_keyingMode);
    QSignalBlocker b11a(m_keyingEnabled);
    QSignalBlocker b19(m_keyRangeSlider);
    QSignalBlocker b20(m_keyChromaRangeSlider);

    const auto setCombo = [](QComboBox* cb, int value) {
        for (int i = 0; i < cb->count(); ++i) {
            if (cb->itemData(i).toInt() == value) {
                cb->setCurrentIndex(i);
                return;
            }
        }
    };

    const KeyingUiValues keyUi = keyingUiValues(cell, m_layerKeyingOverride);
    if (m_keyHardnessSlider) {
        m_keyHardnessSlider->setValue(unitToSlider(keyUi.keyThreshold));
    }
    if (m_keyHardnessValue) {
        m_keyHardnessValue->setText(formatUnit(keyUi.keyThreshold));
    }
    if (m_keyFeatherSlider) {
        m_keyFeatherSlider->setValue(unitToSlider(keyUi.keySoftness));
    }
    if (m_keyFeatherValue) {
        m_keyFeatherValue->setText(formatUnit(keyUi.keySoftness));
    }
    if (m_keyRSlider) {
        m_keyRSlider->setValue(unitToSlider(keyUi.keyChannelR));
    }
    if (m_keyGSlider) {
        m_keyGSlider->setValue(unitToSlider(keyUi.keyChannelG));
    }
    if (m_keyBSlider) {
        m_keyBSlider->setValue(unitToSlider(keyUi.keyChannelB));
    }
    if (m_keyRValue) {
        m_keyRValue->setText(formatKeyPercent(keyUi.keyChannelR));
    }
    if (m_keyGValue) {
        m_keyGValue->setText(formatKeyPercent(keyUi.keyChannelG));
    }
    if (m_keyBValue) {
        m_keyBValue->setText(formatKeyPercent(keyUi.keyChannelB));
    }
    if (m_keyingEnabled) {
        m_keyingEnabled->setChecked(keyUi.keyingEnabled);
    }
    if (m_keyingMode) {
        setCombo(m_keyingMode, int(keyUi.keyingMode));
    }
    if (m_keyRangeSlider) {
        m_keyRangeSlider->setRange(
            qBound(0.0, keyUi.keyLumaCenter - keyUi.keyThreshold, 1.0),
            qBound(0.0, keyUi.keyLumaCenter + keyUi.keyThreshold, 1.0));
    }
    if (m_keyRangeValue) {
        m_keyRangeValue->setText(formatUnit(keyUi.keyLumaCenter));
    }
    if (m_keyLumaTargetGroup) {
        if (QAbstractButton* b = m_keyLumaTargetGroup->button(keyUi.keyLumaInvert ? 1 : 0)) {
            b->setChecked(true);
        }
    }
    if (m_keyChromaRangeSlider) {
        m_keyChromaRangeSlider->setRange(
            qBound(0.0, keyUi.keyChromaHue - keyUi.keyThreshold, 1.0),
            qBound(0.0, keyUi.keyChromaHue + keyUi.keyThreshold, 1.0));
    }
    if (m_keyChromaRangeValue) {
        m_keyChromaRangeValue->setText(formatUnit(keyUi.keyChromaHue));
    }
    if (m_keyChromaTargetGroup) {
        if (QAbstractButton* b = m_keyChromaTargetGroup->button(keyUi.keyChromaInvert ? 1 : 0)) {
            b->setChecked(true);
        }
    }
    syncKeyingModeUi();
    m_loading = false;
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
    applyMidiMapOverlays();
}

QString ParameterInspector::midiLabelForWidget(QWidget* w) const
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

void ParameterInspector::applyMidiMapOverlays()
{
    for (QWidget* w : m_midiTaggedWidgets) {
        if (!w) {
            continue;
        }
        MidiMapOverlay::setActiveOn(w, m_midiMappingEditMode, midiLabelForWidget(w));
    }
}

void ParameterInspector::refreshFromModel()
{
    refreshFromCell();
    if (m_midiMappingEditMode) {
        applyMidiMapOverlays();
    }
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
    if (comboIndex < 0 || comboIndex >= kMixPresetCount) {
        return;
    }
    auto* cell = currentCell();
    if (!cell) {
        return;
    }
    const MixPreset& pr = kMixPresets[comboIndex];
    cell->props.copyMode       = pr.copyMode;
    cell->props.maskType       = pr.maskType;
    cell->props.keyingMode     = pr.keyingMode;
    cell->props.keyingEnabled  = pr.keyingEnabled;
    cell->props.keyLumaInvert  = pr.keyLumaInvert;
    cell->props.keyLumaCenter  = pr.keyLumaCenter;
    cell->props.keyChromaInvert = pr.keyChromaInvert;
    cell->props.keyChromaHue    = pr.keyChromaHue;
    cell->props.keyChannelR    = pr.keyChannelR;
    cell->props.keyChannelG    = pr.keyChannelG;
    cell->props.keyChannelB    = pr.keyChannelB;
    cell->props.transparency   = pr.transparency;
    cell->props.keyThreshold   = pr.maskWidth;
    cell->props.keySoftness    = pr.maskSmoothness;
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
    QSignalBlocker b8(m_keyingMode);
    QSignalBlocker b8a(m_keyingEnabled);
    QSignalBlocker b9(m_keyRangeSlider);
    QSignalBlocker b10(m_keyChromaRangeSlider);

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
    const KeyingUiValues keyUi = keyingUiValues(cell, m_layerKeyingOverride);
    m_keyHardnessSlider->setValue(unitToSlider(keyUi.keyThreshold));
    m_keyHardnessValue->setText(formatUnit(keyUi.keyThreshold));
    m_keyFeatherSlider->setValue(unitToSlider(keyUi.keySoftness));
    m_keyFeatherValue->setText(formatUnit(keyUi.keySoftness));
    m_keyRSlider->setValue(unitToSlider(keyUi.keyChannelR));
    m_keyRValue->setText(formatKeyPercent(keyUi.keyChannelR));
    m_keyGSlider->setValue(unitToSlider(keyUi.keyChannelG));
    m_keyGValue->setText(formatKeyPercent(keyUi.keyChannelG));
    m_keyBSlider->setValue(unitToSlider(keyUi.keyChannelB));
    m_keyBValue->setText(formatKeyPercent(keyUi.keyChannelB));
    if (m_keyingEnabled) {
        m_keyingEnabled->setChecked(keyUi.keyingEnabled);
    }
    setCombo(m_keyingMode, int(keyUi.keyingMode));
    m_keyRangeSlider->setRange(
        qBound(0.0, keyUi.keyLumaCenter - keyUi.keyThreshold, 1.0),
        qBound(0.0, keyUi.keyLumaCenter + keyUi.keyThreshold, 1.0));
    m_keyRangeValue->setText(formatUnit(keyUi.keyLumaCenter));
    if (m_keyLumaTargetGroup) {
        if (QAbstractButton* b = m_keyLumaTargetGroup->button(keyUi.keyLumaInvert ? 1 : 0)) {
            b->setChecked(true);
        }
    }
    m_keyChromaRangeSlider->setRange(
        qBound(0.0, keyUi.keyChromaHue - keyUi.keyThreshold, 1.0),
        qBound(0.0, keyUi.keyChromaHue + keyUi.keyThreshold, 1.0));
    m_keyChromaRangeValue->setText(formatUnit(keyUi.keyChromaHue));
    if (m_keyChromaTargetGroup) {
        if (QAbstractButton* b = m_keyChromaTargetGroup->button(keyUi.keyChromaInvert ? 1 : 0)) {
            b->setChecked(true);
        }
    }
    m_mixingPreset->setCurrentIndex(comboIndex);
    syncMaskTypeButtons();
    syncMaskControlVisibility();
    syncKeyingModeUi();

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

void ParameterInspector::syncMaskControlVisibility()
{
    const auto* cell = currentCell();
    const MaskType mt = cell ? cell->props.maskType : MaskType::None;
    const bool rectLike = (mt == MaskType::Rectangle || mt == MaskType::SoftEdge);
    const bool circleLike = (mt == MaskType::Circle || mt == MaskType::Custom);
    const bool ellipse = (mt == MaskType::Ellipse);
    const bool hasMask = (mt != MaskType::None);
    if (m_maskRectWidthRow) m_maskRectWidthRow->setVisible(rectLike);
    if (m_maskRectHeightRow) m_maskRectHeightRow->setVisible(rectLike);
    if (m_maskRadiusRow) m_maskRadiusRow->setVisible(circleLike);
    if (m_maskEllipseXRow) m_maskEllipseXRow->setVisible(ellipse);
    if (m_maskEllipseYRow) m_maskEllipseYRow->setVisible(ellipse);
    if (m_maskFeatherRow) m_maskFeatherRow->setVisible(hasMask);
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

void ParameterInspector::syncPreferredLayerButtons(int layerIndex, bool hasCell)
{
    if (!m_preferredLayerGroup) {
        return;
    }
    QSignalBlocker b(m_preferredLayerGroup);
    for (QAbstractButton* btn : m_preferredLayerGroup->buttons()) {
        btn->setEnabled(hasCell);
        btn->setChecked(false);
    }
    if (hasCell) {
        if (QAbstractButton* btn = m_preferredLayerGroup->button(layerIndex)) {
            btn->setChecked(true);
        }
    }
}

void ParameterInspector::syncFeedbackForVisualSource(bool hasCell, int visualSourceKind)
{
    const bool isMedia = visualSourceKind == int(VisualSourceKind::Media);
    const bool isMixerFilter = visualSourceKind == int(VisualSourceKind::MixerFilter);
    const bool feedbackEnabled = hasCell && !isMedia && !isMixerFilter;

    if (m_feedbackTabIndex >= 0) {
        setTabEnabled(m_feedbackTabIndex, feedbackEnabled);
        if (isMedia && currentIndex() == m_feedbackTabIndex) {
            setCurrentIndex(0);
        }
    }

    if (m_visualSourceCombo) {
        if (auto* model = qobject_cast<QStandardItemModel*>(m_visualSourceCombo->model())) {
            for (int i = 0; i < m_visualSourceCombo->count(); ++i) {
                if (m_visualSourceCombo->itemData(i).toInt() == int(VisualSourceKind::Feedback)) {
                    if (QStandardItem* item = model->item(i)) {
                        item->setEnabled(!isMedia);
                    }
                    break;
                }
            }
        }
    }

    for (auto* w : {static_cast<QWidget*>(m_feedbackInSaturationSlider),
                     static_cast<QWidget*>(m_feedbackInBrightnessSlider),
                     static_cast<QWidget*>(m_feedbackInContrastSlider),
                     static_cast<QWidget*>(m_feedbackInHueShiftSlider),
                     static_cast<QWidget*>(m_feedbackInGammaSlider),
                     static_cast<QWidget*>(m_feedbackSaturationSlider),
                     static_cast<QWidget*>(m_feedbackBrightnessSlider),
                     static_cast<QWidget*>(m_feedbackContrastSlider),
                     static_cast<QWidget*>(m_feedbackHueShiftSlider),
                     static_cast<QWidget*>(m_feedbackGammaSlider),
                     static_cast<QWidget*>(m_feedbackRotationSlider),
                     static_cast<QWidget*>(m_feedbackZoomSlider),
                     static_cast<QWidget*>(m_feedbackTranslateXSlider),
                     static_cast<QWidget*>(m_feedbackTranslateYSlider),
                     static_cast<QWidget*>(m_feedbackRetentionSlider),
                     static_cast<QWidget*>(m_feedbackFrameDelaySlider),
                     static_cast<QWidget*>(m_feedbackInputLabel),
                     static_cast<QWidget*>(m_feedbackInputModeHint),
                     static_cast<QWidget*>(m_feedbackBlendCombo),
                     static_cast<QWidget*>(m_feedbackWrapCombo)}) {
        if (w) {
            w->setEnabled(feedbackEnabled);
        }
    }
    if (m_keyingPanel) {
        m_keyingPanel->setEnabled(hasCell);
    }

    syncKeyingPanelPlacement(hasCell, visualSourceKind);
}

void ParameterInspector::syncKeyingPanelPlacement(bool hasCell, int visualSourceKind)
{
    if (!m_keyingPanel || !m_mixingKeyingSlot || !m_feedbackKeyingSlot) {
        return;
    }

    const bool isFeedback = hasCell && visualSourceKind == int(VisualSourceKind::Feedback);
  const bool showInFeedbackTab = isFeedback && m_feedbackTabIndex >= 0
        && currentIndex() == m_feedbackTabIndex;

    QWidget* targetSlot = showInFeedbackTab ? m_feedbackKeyingSlot : m_mixingKeyingSlot;
    if (m_keyingPanel->parentWidget() != targetSlot) {
        targetSlot->layout()->addWidget(m_keyingPanel);
    }

    if (m_mixingKeyingSlot) {
        m_mixingKeyingSlot->setVisible(!showInFeedbackTab);
    }
    if (m_feedbackKeyingSection) {
        m_feedbackKeyingSection->setVisible(showInFeedbackTab);
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
    QSignalBlocker b11(m_keyingMode);
    QSignalBlocker b11a(m_keyingEnabled);
    QSignalBlocker b11b(m_matteRoleCombo);
    QSignalBlocker b12(m_audioDial);
    QSignalBlocker b13(m_segmentInSlider);
    QSignalBlocker b14(m_segmentOutSlider);
    QSignalBlocker b15(m_scratchSlider);
    QSignalBlocker b16(m_pauseClipBtn);
    QSignalBlocker b17(m_overlayLineEdit);
    QSignalBlocker b18(m_tcStartEdit);
    QSignalBlocker b19(m_keyRangeSlider);
    QSignalBlocker bName(m_cellNameEdit);
    QSignalBlocker b20(m_keyChromaRangeSlider);
    QSignalBlocker b33(m_preferredLayerGroup);
    QSignalBlocker b34(m_visualSourceCombo);
    QSignalBlocker b36(m_feedbackInSaturationSlider);
    QSignalBlocker b36b(m_feedbackInBrightnessSlider);
    QSignalBlocker b36c(m_feedbackInContrastSlider);
    QSignalBlocker b36d(m_feedbackInHueShiftSlider);
    QSignalBlocker b36e(m_feedbackInGammaSlider);
    QSignalBlocker b37(m_feedbackSaturationSlider);
    QSignalBlocker b38(m_feedbackBrightnessSlider);
    QSignalBlocker b39(m_feedbackContrastSlider);
    QSignalBlocker b40(m_feedbackHueShiftSlider);
    QSignalBlocker b40g(m_feedbackGammaSlider);
    QSignalBlocker b41(m_feedbackRotationSlider);
    QSignalBlocker b42(m_feedbackZoomSlider);
    QSignalBlocker b42tx(m_feedbackTranslateXSlider);
    QSignalBlocker b42ty(m_feedbackTranslateYSlider);
    QSignalBlocker b42r(m_feedbackRetentionSlider);
    QSignalBlocker b42a(m_feedbackFrameDelaySlider);
    QSignalBlocker b43(m_feedbackBlendCombo);
    QSignalBlocker b44(m_feedbackWrapCombo);
    QSignalBlocker b45(m_pictureWrapCombo);

    auto* cell = currentCell();
    const bool hasCell = (cell != nullptr);

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
                     static_cast<QWidget*>(m_audioDial),
                     static_cast<QWidget*>(m_audioDialValue)}) {
        if (w) {
            w->setEnabled(hasCell);
        }
    }
    for (auto* w : {static_cast<QWidget*>(m_rotation),
                     static_cast<QWidget*>(m_copyMode),
                     static_cast<QWidget*>(m_keyingEnabled),
                     static_cast<QWidget*>(m_keyingMode),
                     static_cast<QWidget*>(m_matteRoleCombo),
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
                     static_cast<QWidget*>(m_keyRangeSlider),
                     static_cast<QWidget*>(m_keyRangeValue),
                     static_cast<QWidget*>(m_keyChromaRangeSlider),
                     static_cast<QWidget*>(m_keyChromaRangeValue)}) {
        if (w) {
            w->setEnabled(hasCell);
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
            btn->setEnabled(hasCell);
        }
    }
    if (m_keyLumaTargetGroup) {
        const auto buttons = m_keyLumaTargetGroup->buttons();
        for (QAbstractButton* btn : buttons) {
            btn->setEnabled(hasCell);
        }
    }
    if (m_keyChromaTargetGroup) {
        const auto buttons = m_keyChromaTargetGroup->buttons();
        for (QAbstractButton* btn : buttons) {
            btn->setEnabled(hasCell);
        }
    }
    if (m_priorityGroup) {
        const auto buttons = m_priorityGroup->buttons();
        for (QAbstractButton* btn : buttons) {
            btn->setEnabled(hasCell);
        }
    }
    if (m_preferredLayerGroup) {
        for (QAbstractButton* btn : m_preferredLayerGroup->buttons()) {
            btn->setEnabled(hasCell);
        }
    }
    if (m_visualSourceCombo) {
        m_visualSourceCombo->setEnabled(hasCell);
    }
    if (m_pictureWrapCombo) {
        m_pictureWrapCombo->setEnabled(hasCell);
    }

    const int visualKind = hasCell ? visualSourceKindFromCell(*cell) : int(VisualSourceKind::Empty);
    syncFeedbackForVisualSource(hasCell, visualKind);

    if (!hasCell) {
        m_visualLabel->setText(tr("(no cell selected)"));
        if (m_cellNameRow) {
            m_cellNameRow->setVisible(false);
        }
        if (m_cellNameEdit) {
            m_cellNameEdit->clear();
        }
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
        if (m_maskRectWidthSlider) m_maskRectWidthSlider->setValue(unitToSlider(1.0));
        if (m_maskRectHeightSlider) m_maskRectHeightSlider->setValue(unitToSlider(1.0));
        if (m_maskRadiusSlider) m_maskRadiusSlider->setValue(unitToSlider(0.5));
        if (m_maskEllipseXSlider) m_maskEllipseXSlider->setValue(unitToSlider(0.6));
        if (m_maskEllipseYSlider) m_maskEllipseYSlider->setValue(unitToSlider(0.45));
        if (m_maskFeatherSlider) m_maskFeatherSlider->setValue(unitToSlider(0.1));
        if (m_maskRectWidthValue) m_maskRectWidthValue->setText(formatUnit(1.0));
        if (m_maskRectHeightValue) m_maskRectHeightValue->setText(formatUnit(1.0));
        if (m_maskRadiusValue) m_maskRadiusValue->setText(formatUnit(0.5));
        if (m_maskEllipseXValue) m_maskEllipseXValue->setText(formatUnit(0.6));
        if (m_maskEllipseYValue) m_maskEllipseYValue->setText(formatUnit(0.45));
        if (m_maskFeatherValue) m_maskFeatherValue->setText(formatUnit(0.1));
        m_keyRSlider->setValue(unitToSlider(1.0));
        m_keyGSlider->setValue(unitToSlider(1.0));
        m_keyBSlider->setValue(unitToSlider(1.0));
        m_keyRValue->setText(formatKeyPercent(1.0));
        m_keyGValue->setText(formatKeyPercent(1.0));
        m_keyBValue->setText(formatKeyPercent(1.0));
        m_keyRangeSlider->setRange(0.25, 0.75);
        m_keyRangeValue->setText(formatUnit(0.5));
        if (m_keyingEnabled) {
            m_keyingEnabled->setChecked(false);
        }
        if (m_keyLumaTargetGroup) {
            if (QAbstractButton* b = m_keyLumaTargetGroup->button(0)) {
                b->setChecked(true);
            }
        }
        m_keyChromaRangeSlider->setRange(0.23, 0.43);
        m_keyChromaRangeValue->setText(formatUnit(0.33));
        if (m_keyChromaTargetGroup) {
            if (QAbstractButton* b = m_keyChromaTargetGroup->button(0)) {
                b->setChecked(true);
            }
        }
        m_mixingPreset->setCurrentIndex(0);
        const auto setCombo = [](QComboBox* cb, int value) {
            for (int i = 0; i < cb->count(); ++i) {
                if (cb->itemData(i).toInt() == value) {
                    cb->setCurrentIndex(i);
                    return;
                }
            }
        };
        setCombo(m_keyingMode, int(KeyingMode::Luma));
        setCombo(m_copyMode, int(CopyMode::Normal));
        setCombo(m_matteRoleCombo, int(LayerMatteRole::None));
        syncMaskTypeButtons();
        syncMaskControlVisibility();
        syncKeyingModeUi();
        syncPlayModeButtons();
        syncPriorityButtons();
        syncPreferredLayerButtons(4, false);
        if (m_visualSourceCombo) {
            m_visualSourceCombo->setCurrentIndex(0);
        }
        m_loading = false;
        return;
    }

    if (m_visualSourceCombo) {
        const int kind = visualSourceKindFromCell(*cell);
        for (int i = 0; i < m_visualSourceCombo->count(); ++i) {
            if (m_visualSourceCombo->itemData(i).toInt() == kind) {
                m_visualSourceCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    const bool showName = cellAllowsUserName(*cell);
    if (m_cellNameRow) {
        m_cellNameRow->setVisible(showName);
    }
    if (m_cellNameEdit) {
        m_cellNameEdit->setEnabled(showName);
        m_cellNameEdit->setText(showName ? cell->name : QString());
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
    m_rotation->setValue(cell->props.rotationZ * 180.0);
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
    {
        const KeyingUiValues keyUi = keyingUiValues(cell, m_layerKeyingOverride);
        m_keyHardnessSlider->setValue(unitToSlider(keyUi.keyThreshold));
        m_keyHardnessValue->setText(formatUnit(keyUi.keyThreshold));
        m_keyFeatherSlider->setValue(unitToSlider(keyUi.keySoftness));
        m_keyFeatherValue->setText(formatUnit(keyUi.keySoftness));
        m_keyRSlider->setValue(unitToSlider(keyUi.keyChannelR));
        m_keyGSlider->setValue(unitToSlider(keyUi.keyChannelG));
        m_keyBSlider->setValue(unitToSlider(keyUi.keyChannelB));
        m_keyRValue->setText(formatKeyPercent(keyUi.keyChannelR));
        m_keyGValue->setText(formatKeyPercent(keyUi.keyChannelG));
        m_keyBValue->setText(formatKeyPercent(keyUi.keyChannelB));
        m_keyRangeSlider->setRange(
            qBound(0.0, keyUi.keyLumaCenter - keyUi.keyThreshold, 1.0),
            qBound(0.0, keyUi.keyLumaCenter + keyUi.keyThreshold, 1.0));
        m_keyRangeValue->setText(formatUnit(keyUi.keyLumaCenter));
        if (m_keyLumaTargetGroup) {
            if (QAbstractButton* b = m_keyLumaTargetGroup->button(keyUi.keyLumaInvert ? 1 : 0)) {
                b->setChecked(true);
            }
        }
        m_keyChromaRangeSlider->setRange(
            qBound(0.0, keyUi.keyChromaHue - keyUi.keyThreshold, 1.0),
            qBound(0.0, keyUi.keyChromaHue + keyUi.keyThreshold, 1.0));
        m_keyChromaRangeValue->setText(formatUnit(keyUi.keyChromaHue));
        if (m_keyChromaTargetGroup) {
            if (QAbstractButton* b = m_keyChromaTargetGroup->button(keyUi.keyChromaInvert ? 1 : 0)) {
                b->setChecked(true);
            }
        }
        if (m_keyingEnabled) {
            m_keyingEnabled->setChecked(keyUi.keyingEnabled);
        }
        setCombo(m_keyingMode, int(keyUi.keyingMode));
    }
    setCombo(m_copyMode, int(cell->props.copyMode));
    setCombo(m_matteRoleCombo, int(cell->props.matteRole));
    if (m_maskRectWidthSlider) m_maskRectWidthSlider->setValue(unitToSlider(cell->props.maskRectWidth));
    if (m_maskRectHeightSlider) m_maskRectHeightSlider->setValue(unitToSlider(cell->props.maskRectHeight));
    if (m_maskRadiusSlider) m_maskRadiusSlider->setValue(unitToSlider(cell->props.maskRadius));
    if (m_maskEllipseXSlider) m_maskEllipseXSlider->setValue(unitToSlider(cell->props.maskEllipseX));
    if (m_maskEllipseYSlider) m_maskEllipseYSlider->setValue(unitToSlider(cell->props.maskEllipseY));
    if (m_maskFeatherSlider) m_maskFeatherSlider->setValue(unitToSlider(cell->props.maskFeather));
    if (m_maskRectWidthValue) m_maskRectWidthValue->setText(formatUnit(cell->props.maskRectWidth));
    if (m_maskRectHeightValue) m_maskRectHeightValue->setText(formatUnit(cell->props.maskRectHeight));
    if (m_maskRadiusValue) m_maskRadiusValue->setText(formatUnit(cell->props.maskRadius));
    if (m_maskEllipseXValue) m_maskEllipseXValue->setText(formatUnit(cell->props.maskEllipseX));
    if (m_maskEllipseYValue) m_maskEllipseYValue->setText(formatUnit(cell->props.maskEllipseY));
    if (m_maskFeatherValue) m_maskFeatherValue->setText(formatUnit(cell->props.maskFeather));
    syncMaskTypeButtons();
    syncMaskControlVisibility();
    syncKeyingModeUi();
    syncPlayModeButtons();
    syncPriorityButtons();
    syncPreferredLayerButtons(
        hasCell ? qBound(0, cell->props.preferredLayer, 12) : 4,
        hasCell);

    const int presetIdx = qBound(0, cell->props.mixingPresetIndex, m_mixingPreset->count() - 1);
    m_mixingPreset->setCurrentIndex(presetIdx);

    if (m_feedbackInSaturationSlider) {
        m_feedbackInSaturationSlider->setValue(rangeToSlider(cell->props.feedback.inSaturation, 0.0, 2.0));
        m_feedbackInSaturationValue->setText(formatUnit(cell->props.feedback.inSaturation));
    }
    if (m_feedbackInBrightnessSlider) {
        m_feedbackInBrightnessSlider->setValue(
            signedToSlider(cell->props.feedback.inBrightness, -1.0, 1.0));
        m_feedbackInBrightnessValue->setText(formatSignedUnit(cell->props.feedback.inBrightness));
    }
    if (m_feedbackInContrastSlider) {
        m_feedbackInContrastSlider->setValue(rangeToSlider(cell->props.feedback.inContrast, 0.0, 2.0));
        m_feedbackInContrastValue->setText(formatUnit(cell->props.feedback.inContrast));
    }
    if (m_feedbackInHueShiftSlider) {
        const double inHue01 = cell->props.feedback.inHueShift < 0.0
            ? qBound(0.0, (cell->props.feedback.inHueShift + 1.0) * 0.5, 1.0)
            : qBound(0.0, cell->props.feedback.inHueShift, 1.0);
        cell->props.feedback.inHueShift = inHue01;
        m_feedbackInHueShiftSlider->setValue(unitToSlider(inHue01));
        m_feedbackInHueShiftValue->setText(formatUnit(inHue01));
    }
    if (m_feedbackInGammaSlider) {
        m_feedbackInGammaSlider->setValue(rangeToSlider(cell->props.feedback.inGamma, 0.1, 4.0));
        m_feedbackInGammaValue->setText(formatUnit(cell->props.feedback.inGamma));
    }
    if (m_feedbackSaturationSlider) {
        m_feedbackSaturationSlider->setValue(rangeToSlider(cell->props.feedback.saturation, 0.0, 2.0));
        m_feedbackSaturationValue->setText(formatUnit(cell->props.feedback.saturation));
    }
    if (m_feedbackBrightnessSlider) {
        m_feedbackBrightnessSlider->setValue(signedToSlider(cell->props.feedback.brightness, -1.0, 1.0));
        m_feedbackBrightnessValue->setText(formatSignedUnit(cell->props.feedback.brightness));
    }
    if (m_feedbackContrastSlider) {
        m_feedbackContrastSlider->setValue(rangeToSlider(cell->props.feedback.contrast, 0.0, 2.0));
        m_feedbackContrastValue->setText(formatUnit(cell->props.feedback.contrast));
    }
    if (m_feedbackHueShiftSlider) {
        const double hue01 = cell->props.feedback.hueShift < 0.0
            ? qBound(0.0, (cell->props.feedback.hueShift + 1.0) * 0.5, 1.0)
            : qBound(0.0, cell->props.feedback.hueShift, 1.0);
        cell->props.feedback.hueShift = hue01;
        m_feedbackHueShiftSlider->setValue(unitToSlider(hue01));
        m_feedbackHueShiftValue->setText(formatUnit(hue01));
    }
    if (m_feedbackGammaSlider) {
        m_feedbackGammaSlider->setValue(rangeToSlider(cell->props.feedback.gamma, 0.1, 4.0));
        m_feedbackGammaValue->setText(formatUnit(cell->props.feedback.gamma));
    }
    if (m_feedbackRotationSlider) {
        const double deg = normalizeDegrees360(cell->props.feedback.rotationDeg);
        m_feedbackRotationSlider->setValue(rangeToSlider(deg, 0.0, 360.0));
        if (m_feedbackRotationValue) {
            m_feedbackRotationValue->setText(
                QStringLiteral("%1°").arg(int(deg + 0.5) % 360));
        }
    }
    if (m_feedbackZoomSlider) {
        m_feedbackZoomSlider->setValue(signedToSlider(cell->props.feedback.zoom,
                                                      pvj::core::kFeedbackZoomMin,
                                                      pvj::core::kFeedbackZoomMax));
        m_feedbackZoomValue->setText(formatSignedUnit(cell->props.feedback.zoom));
    }
    if (m_feedbackTranslateXSlider) {
        m_feedbackTranslateXSlider->setValue(
            signedToSlider(cell->props.feedback.translateX, pvj::core::kFeedbackTranslateMin,
                           pvj::core::kFeedbackTranslateMax));
        if (m_feedbackTranslateXValue) {
            m_feedbackTranslateXValue->setText(formatSignedUnit(cell->props.feedback.translateX));
        }
    }
    if (m_feedbackTranslateYSlider) {
        m_feedbackTranslateYSlider->setValue(
            signedToSlider(cell->props.feedback.translateY, pvj::core::kFeedbackTranslateMin,
                           pvj::core::kFeedbackTranslateMax));
        if (m_feedbackTranslateYValue) {
            m_feedbackTranslateYValue->setText(formatSignedUnit(cell->props.feedback.translateY));
        }
    }
    if (m_feedbackRetentionSlider) {
        m_feedbackRetentionSlider->setValue(unitToSlider(cell->props.feedback.retention));
        if (m_feedbackRetentionValue) {
            m_feedbackRetentionValue->setText(formatUnit(cell->props.feedback.retention));
        }
    }
    if (m_feedbackFrameDelaySlider) {
        const int delay = qBound(0, cell->props.feedback.frameDelay, pvj::core::kFeedbackMaxFrameDelay);
        m_feedbackFrameDelaySlider->setValue(delay);
        if (m_feedbackFrameDelayValue) {
            m_feedbackFrameDelayValue->setText(QString::number(delay));
        }
    }
    if (m_feedbackBlendCombo) {
        const int blendIdx = m_feedbackBlendCombo->findData(int(cell->props.feedback.blendMode));
        if (blendIdx >= 0) {
            m_feedbackBlendCombo->setCurrentIndex(blendIdx);
        }
    }
    if (m_feedbackWrapCombo) {
        const int wrapIdx = m_feedbackWrapCombo->findData(int(cell->props.feedback.wrapMode));
        if (wrapIdx >= 0) {
            m_feedbackWrapCombo->setCurrentIndex(wrapIdx);
        }
    }
    if (m_pictureWrapCombo) {
        const int wrapIdx = m_pictureWrapCombo->findData(int(cell->props.picture.wrapMode));
        if (wrapIdx >= 0) {
            m_pictureWrapCombo->setCurrentIndex(wrapIdx);
        }
    }

    m_loading = false;
}

void ParameterInspector::emitChanged()
{
    emit cellChanged(m_bankSetIndex, m_bankIndex, m_cellIndex);
}

void ParameterInspector::emitPlaybackChanged()
{
    emit cellPlaybackChanged(m_bankSetIndex, m_bankIndex, m_cellIndex);
}

void ParameterInspector::onVisualSourceChanged(int /*idx*/)
{
    if (m_loading || !m_visualSourceCombo) {
        return;
    }
    auto* c = currentCell();
    if (!c) {
        return;
    }
    const int kind = m_visualSourceCombo->currentData().toInt();
    if (kind == int(VisualSourceKind::Feedback)
        && c->visual.type == VisualType::Media) {
        QSignalBlocker blocker(m_visualSourceCombo);
        for (int i = 0; i < m_visualSourceCombo->count(); ++i) {
            if (m_visualSourceCombo->itemData(i).toInt() == int(VisualSourceKind::Media)) {
                m_visualSourceCombo->setCurrentIndex(i);
                break;
            }
        }
        return;
    }
    applyVisualSourceKind(*c, kind);
    syncFeedbackForVisualSource(true, kind);
    emitChanged();
    emitPlaybackChanged();
}

void ParameterInspector::onFeedbackInSaturationChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToRange(v, 0.0, 2.0);
    if (m_feedbackInSaturationValue) m_feedbackInSaturationValue->setText(formatUnit(u));
    if (auto* c = currentCell()) { c->props.feedback.inSaturation = u; emitChanged(); }
}

void ParameterInspector::onFeedbackInBrightnessChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToSigned(v, -1.0, 1.0);
    if (m_feedbackInBrightnessValue) m_feedbackInBrightnessValue->setText(formatSignedUnit(u));
    if (auto* c = currentCell()) { c->props.feedback.inBrightness = u; emitChanged(); }
}

void ParameterInspector::onFeedbackInContrastChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToRange(v, 0.0, 2.0);
    if (m_feedbackInContrastValue) m_feedbackInContrastValue->setText(formatUnit(u));
    if (auto* c = currentCell()) { c->props.feedback.inContrast = u; emitChanged(); }
}

void ParameterInspector::onFeedbackInHueShiftChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_feedbackInHueShiftValue) m_feedbackInHueShiftValue->setText(formatUnit(u));
    if (auto* c = currentCell()) { c->props.feedback.inHueShift = u; emitChanged(); }
}

void ParameterInspector::onFeedbackInGammaChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToRange(v, 0.1, 4.0);
    if (m_feedbackInGammaValue) m_feedbackInGammaValue->setText(formatUnit(u));
    if (auto* c = currentCell()) { c->props.feedback.inGamma = u; emitChanged(); }
}

void ParameterInspector::onFeedbackSaturationChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToRange(v, 0.0, 2.0);
    if (m_feedbackSaturationValue) m_feedbackSaturationValue->setText(formatUnit(u));
    if (auto* c = currentCell()) { c->props.feedback.saturation = u; emitChanged(); }
}

void ParameterInspector::onFeedbackBrightnessChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToSigned(v, -1.0, 1.0);
    if (m_feedbackBrightnessValue) m_feedbackBrightnessValue->setText(formatSignedUnit(u));
    if (auto* c = currentCell()) { c->props.feedback.brightness = u; emitChanged(); }
}

void ParameterInspector::onFeedbackContrastChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToRange(v, 0.0, 2.0);
    if (m_feedbackContrastValue) m_feedbackContrastValue->setText(formatUnit(u));
    if (auto* c = currentCell()) { c->props.feedback.contrast = u; emitChanged(); }
}

void ParameterInspector::onFeedbackHueShiftChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_feedbackHueShiftValue) m_feedbackHueShiftValue->setText(formatUnit(u));
    if (auto* c = currentCell()) { c->props.feedback.hueShift = u; emitChanged(); }
}

void ParameterInspector::onFeedbackGammaChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToRange(v, 0.1, 4.0);
    if (m_feedbackGammaValue) m_feedbackGammaValue->setText(formatUnit(u));
    if (auto* c = currentCell()) { c->props.feedback.gamma = u; emitChanged(); }
}

void ParameterInspector::onFeedbackRotationChanged(int v)
{
    if (m_loading) return;
    const double deg = normalizeDegrees360(sliderToRange(v, 0.0, 360.0));
    if (m_feedbackRotationValue) {
        m_feedbackRotationValue->setText(QStringLiteral("%1°").arg(int(deg + 0.5) % 360));
    }
    if (auto* c = currentCell()) {
        c->props.feedback.rotationDeg = deg;
        emitChanged();
    }
}

void ParameterInspector::onFeedbackZoomChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToSigned(v, pvj::core::kFeedbackZoomMin, pvj::core::kFeedbackZoomMax);
    if (m_feedbackZoomValue) m_feedbackZoomValue->setText(formatSignedUnit(u));
    if (auto* c = currentCell()) { c->props.feedback.zoom = u; emitChanged(); }
}

void ParameterInspector::onFeedbackTranslateXChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToSigned(v, pvj::core::kFeedbackTranslateMin,
                                    pvj::core::kFeedbackTranslateMax);
    if (m_feedbackTranslateXValue) m_feedbackTranslateXValue->setText(formatSignedUnit(u));
    if (auto* c = currentCell()) { c->props.feedback.translateX = u; emitChanged(); }
}

void ParameterInspector::onFeedbackTranslateYChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToSigned(v, pvj::core::kFeedbackTranslateMin,
                                    pvj::core::kFeedbackTranslateMax);
    if (m_feedbackTranslateYValue) m_feedbackTranslateYValue->setText(formatSignedUnit(u));
    if (auto* c = currentCell()) { c->props.feedback.translateY = u; emitChanged(); }
}

void ParameterInspector::onFeedbackRetentionChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_feedbackRetentionValue) m_feedbackRetentionValue->setText(formatUnit(u));
    if (auto* c = currentCell()) { c->props.feedback.retention = u; emitChanged(); }
}

void ParameterInspector::onFeedbackFrameDelayChanged(int v)
{
    if (m_loading) return;
    const int delay = qBound(0, v, pvj::core::kFeedbackMaxFrameDelay);
    if (m_feedbackFrameDelayValue) {
        m_feedbackFrameDelayValue->setText(QString::number(delay));
    }
    if (auto* c = currentCell()) {
        c->props.feedback.frameDelay = delay;
        emitChanged();
    }
}

void ParameterInspector::onFeedbackBlendModeChanged(int idx)
{
    if (m_loading || !m_feedbackBlendCombo || idx < 0) {
        return;
    }
    const auto mode =
        static_cast<pvj::core::FeedbackBlendMode>(m_feedbackBlendCombo->itemData(idx).toInt());
    if (auto* c = currentCell()) {
        c->props.feedback.blendMode = mode;
        emitChanged();
    }
}

void ParameterInspector::onFeedbackWrapModeChanged(int idx)
{
    if (m_loading || !m_feedbackWrapCombo || idx < 0) {
        return;
    }
    const auto mode = static_cast<pvj::core::WrapMode>(m_feedbackWrapCombo->itemData(idx).toInt());
    if (auto* c = currentCell()) {
        c->props.feedback.wrapMode = mode;
        emitChanged();
    }
}

void ParameterInspector::onPictureWrapModeChanged(int idx)
{
    if (m_loading || !m_pictureWrapCombo || idx < 0) {
        return;
    }
    const auto mode = static_cast<pvj::core::WrapMode>(m_pictureWrapCombo->itemData(idx).toInt());
    if (auto* c = currentCell()) {
        c->props.picture.wrapMode = mode;
        emitChanged();
    }
}

void ParameterInspector::onMixingPresetChanged(int idx)
{
    if (m_loading) {
        return;
    }
    if (idx < 0) {
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
        c->props.movieSpeed = qBound(0.0, s, 4.0);
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

void ParameterInspector::onPreferredLayerChanged(int idx)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.preferredLayer = qBound(0, idx, 12);
        emitChanged();
        emitPlaybackChanged();
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

void ParameterInspector::onOverlayTextEdited(const QString& t)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.overlayText = t;
        emitChanged();
    }
}

void ParameterInspector::onCellNameEdited(const QString& t)
{
    if (m_loading) {
        return;
    }
    auto* c = currentCell();
    if (!c || !cellAllowsUserName(*c)) {
        return;
    }
    const QString trimmed = t.trimmed();
    if (c->name == trimmed) {
        return;
    }
    c->name = trimmed;
    emitChanged();
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
        c->props.rotationZ = qBound(-1.0, v / 180.0, 1.0);
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

void ParameterInspector::onKeyingModeChanged(int idx)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        const int selectedMode = m_keyingMode->itemData(idx).toInt();
        c->props.keyingMode = static_cast<KeyingMode>(selectedMode);
        syncKeyingModeUi();
        markMixingCustom();
        emitChanged();
    }
}

void ParameterInspector::onKeyingEnabledToggled(bool checked)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.keyingEnabled = checked;
        syncKeyingModeUi();
        markMixingCustom();
        emitChanged();
    }
}

void ParameterInspector::onKeyLumaTargetChanged(int id)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.keyLumaInvert = (id == 1);
        markMixingCustom();
        emitChanged();
    }
}

void ParameterInspector::onKeyLumaRangeChanged(double minV, double maxV)
{
    if (m_loading) return;
    const double center = 0.5 * (minV + maxV);
    const double width = 0.5 * qMax(0.0, maxV - minV);
    if (auto* c = currentCell()) {
        if (qAbs(c->props.keyLumaCenter - center) <= kKeyUnitEpsilon
            && qAbs(c->props.keyThreshold - width) <= kKeyUnitEpsilon) {
            return;
        }
    }
    if (m_keyRangeValue) {
        m_keyRangeValue->setText(formatUnit(center));
    }
    if (m_keyHardnessSlider) {
        QSignalBlocker b(m_keyHardnessSlider);
        m_keyHardnessSlider->setValue(unitToSlider(width));
    }
    if (m_keyHardnessValue) {
        m_keyHardnessValue->setText(formatUnit(width));
    }
    if (auto* c = currentCell()) {
        c->props.keyLumaCenter = center;
        c->props.keyThreshold = width;
        markMixingCustom();
        emitChanged();
    }
}

void ParameterInspector::onKeyChromaTargetChanged(int id)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.keyChromaInvert = (id == 1);
        markMixingCustom();
        emitChanged();
    }
}

void ParameterInspector::onKeyChromaRangeChanged(double minV, double maxV)
{
    if (m_loading) return;
    const double center = 0.5 * (minV + maxV);
    const double width = 0.5 * qMax(0.0, maxV - minV);
    if (auto* c = currentCell()) {
        if (qAbs(c->props.keyChromaHue - center) <= kKeyUnitEpsilon
            && qAbs(c->props.keyThreshold - width) <= kKeyUnitEpsilon) {
            return;
        }
    }
    if (m_keyChromaRangeValue) {
        m_keyChromaRangeValue->setText(formatUnit(center));
    }
    if (m_keyHardnessSlider) {
        QSignalBlocker b(m_keyHardnessSlider);
        m_keyHardnessSlider->setValue(unitToSlider(width));
    }
    if (m_keyHardnessValue) {
        m_keyHardnessValue->setText(formatUnit(width));
    }
    if (auto* c = currentCell()) {
        c->props.keyChromaHue = center;
        c->props.keyThreshold = width;
        markMixingCustom();
        emitChanged();
    }
}
void ParameterInspector::onMaskTypeGroupIdClicked(int id)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.maskType = MaskType(id);
        syncMaskControlVisibility();
        markMixingCustom();
        emitChanged();
    }
}

void ParameterInspector::onMatteRoleChanged(int idx)
{
    if (m_loading) return;
    if (auto* c = currentCell()) {
        c->props.matteRole = static_cast<LayerMatteRole>(qBound(0, idx, 3));
        markMixingCustom();
        emitChanged();
    }
}
void ParameterInspector::onKeyHardnessSliderChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (auto* c = currentCell()) {
        if (qAbs(c->props.keyThreshold - u) <= kKeyUnitEpsilon) {
            return;
        }
    }
    if (m_keyHardnessValue) {
        m_keyHardnessValue->setText(formatUnit(u));
    }
    if (auto* c = currentCell()) {
        c->props.keyThreshold = u;
        if (c->props.keyingMode == KeyingMode::Luma && m_keyRangeSlider) {
            m_keyRangeSlider->setRange(
                qBound(0.0, c->props.keyLumaCenter - u, 1.0),
                qBound(0.0, c->props.keyLumaCenter + u, 1.0));
        } else if (c->props.keyingMode == KeyingMode::Chroma && m_keyChromaRangeSlider) {
            m_keyChromaRangeSlider->setRange(
                qBound(0.0, c->props.keyChromaHue - u, 1.0),
                qBound(0.0, c->props.keyChromaHue + u, 1.0));
        }
        markMixingCustom();
        emitChanged();
    }
}
void ParameterInspector::onKeyFeatherSliderChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (auto* c = currentCell()) {
        if (qAbs(c->props.keySoftness - u) <= kKeyUnitEpsilon) {
            return;
        }
    }
    if (m_keyFeatherValue) {
        m_keyFeatherValue->setText(formatUnit(u));
    }
    if (auto* c = currentCell()) {
        c->props.keySoftness = u;
        markMixingCustom();
        emitChanged();
    }
}
void ParameterInspector::onMaskRectWidthChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_maskRectWidthValue) m_maskRectWidthValue->setText(formatUnit(u));
    if (auto* c = currentCell()) {
        c->props.maskRectWidth = u;
        markMixingCustom();
        emitChanged();
    }
}
void ParameterInspector::onMaskRectHeightChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_maskRectHeightValue) m_maskRectHeightValue->setText(formatUnit(u));
    if (auto* c = currentCell()) {
        c->props.maskRectHeight = u;
        markMixingCustom();
        emitChanged();
    }
}
void ParameterInspector::onMaskRadiusChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_maskRadiusValue) m_maskRadiusValue->setText(formatUnit(u));
    if (auto* c = currentCell()) {
        c->props.maskRadius = u;
        markMixingCustom();
        emitChanged();
    }
}
void ParameterInspector::onMaskEllipseXChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_maskEllipseXValue) m_maskEllipseXValue->setText(formatUnit(u));
    if (auto* c = currentCell()) {
        c->props.maskEllipseX = u;
        markMixingCustom();
        emitChanged();
    }
}
void ParameterInspector::onMaskEllipseYChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_maskEllipseYValue) m_maskEllipseYValue->setText(formatUnit(u));
    if (auto* c = currentCell()) {
        c->props.maskEllipseY = u;
        markMixingCustom();
        emitChanged();
    }
}
void ParameterInspector::onMaskFeatherChanged(int v)
{
    if (m_loading) return;
    const double u = sliderToUnit(v);
    if (m_maskFeatherValue) m_maskFeatherValue->setText(formatUnit(u));
    if (auto* c = currentCell()) {
        c->props.maskFeather = u;
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

void ParameterInspector::syncKeyingModeUi()
{
    if (!m_keyingMode) {
        return;
    }
    const auto* cell = currentCell();
    const bool keyingEnabled = cell ? cell->props.keyingEnabled : false;
    const KeyingMode mode = cell ? cell->props.keyingMode : KeyingMode::Luma;
    const bool chromaMode = (mode == KeyingMode::Chroma);
    if (m_keyingMode) {
        m_keyingMode->setEnabled(keyingEnabled);
    }
    if (m_keyingHint) {
        m_keyingHint->setEnabled(keyingEnabled);
    }
    if (m_keyHardnessSlider) {
        m_keyHardnessSlider->setEnabled(keyingEnabled);
    }
    if (m_keyHardnessValue) {
        m_keyHardnessValue->setEnabled(keyingEnabled);
    }
    if (m_keyFeatherSlider) {
        m_keyFeatherSlider->setEnabled(keyingEnabled);
    }
    if (m_keyFeatherValue) {
        m_keyFeatherValue->setEnabled(keyingEnabled);
    }
    if (m_keyRgbHint) {
        m_keyRgbHint->setVisible(keyingEnabled && chromaMode);
    }
    if (m_keyLumaTargetRow) {
        m_keyLumaTargetRow->setVisible(keyingEnabled && !chromaMode);
    }
    if (m_keyRangeRow) {
        m_keyRangeRow->setVisible(keyingEnabled && !chromaMode);
    }
    if (m_keyChromaTargetRow) {
        m_keyChromaTargetRow->setVisible(keyingEnabled && chromaMode);
    }
    if (m_keyChromaRangeRow) {
        m_keyChromaRangeRow->setVisible(keyingEnabled && chromaMode);
    }
    if (m_keyRRow) {
        m_keyRRow->setVisible(keyingEnabled && chromaMode);
    }
    if (m_keyGRow) {
        m_keyGRow->setVisible(keyingEnabled && chromaMode);
    }
    if (m_keyBRow) {
        m_keyBRow->setVisible(keyingEnabled && chromaMode);
    }
}

void ParameterInspector::markSliderResetDefault(QSlider* slider, int defaultSliderValue)
{
    if (!slider) {
        return;
    }
    slider->setProperty("pvjResetValue", defaultSliderValue);
    slider->installEventFilter(this);
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
    tagMidiWidget(m_keyingEnabled, QStringLiteral("keyingEnabled"));
    tagMidiWidget(m_keyingMode, QStringLiteral("keyingMode"));
    if (m_keyLumaTargetGroup) {
        tagMidiWidget(qobject_cast<QWidget*>(m_keyLumaTargetGroup->button(0)),
                      QStringLiteral("keyLumaInvert"), 0.0);
        tagMidiWidget(qobject_cast<QWidget*>(m_keyLumaTargetGroup->button(1)),
                      QStringLiteral("keyLumaInvert"), 1.0);
    }
    if (m_keyChromaTargetGroup) {
        tagMidiWidget(qobject_cast<QWidget*>(m_keyChromaTargetGroup->button(0)),
                      QStringLiteral("keyChromaInvert"), 0.0);
        tagMidiWidget(qobject_cast<QWidget*>(m_keyChromaTargetGroup->button(1)),
                      QStringLiteral("keyChromaInvert"), 1.0);
    }
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
    tagMidiWidget(m_keyHardnessSlider, QStringLiteral("keyThreshold"));
    tagMidiWidget(m_keyFeatherSlider, QStringLiteral("keySoftness"));
    tagMidiWidget(m_keyRangeSlider, QStringLiteral("keyLumaCenter"));
    tagMidiWidget(m_keyChromaRangeSlider, QStringLiteral("keyChromaHue"));
    tagMidiWidget(m_maskRectWidthSlider, QStringLiteral("maskRectWidth"));
    tagMidiWidget(m_maskRectHeightSlider, QStringLiteral("maskRectHeight"));
    tagMidiWidget(m_maskRadiusSlider, QStringLiteral("maskRadius"));
    tagMidiWidget(m_maskEllipseXSlider, QStringLiteral("maskEllipseX"));
    tagMidiWidget(m_maskEllipseYSlider, QStringLiteral("maskEllipseY"));
    tagMidiWidget(m_maskFeatherSlider, QStringLiteral("maskFeather"));
    tagMidiWidget(m_keyRSlider, QStringLiteral("keyChannelR"));
    tagMidiWidget(m_keyGSlider, QStringLiteral("keyChannelG"));
    tagMidiWidget(m_keyBSlider, QStringLiteral("keyChannelB"));
    tagMidiWidget(m_feedbackInSaturationSlider, QStringLiteral("feedbackInSaturation"));
    tagMidiWidget(m_feedbackInBrightnessSlider, QStringLiteral("feedbackInBrightness"));
    tagMidiWidget(m_feedbackInContrastSlider, QStringLiteral("feedbackInContrast"));
    tagMidiWidget(m_feedbackInHueShiftSlider, QStringLiteral("feedbackInHueShift"));
    tagMidiWidget(m_feedbackInGammaSlider, QStringLiteral("feedbackInGamma"));
    tagMidiWidget(m_feedbackSaturationSlider, QStringLiteral("feedbackSaturation"));
    tagMidiWidget(m_feedbackBrightnessSlider, QStringLiteral("feedbackBrightness"));
    tagMidiWidget(m_feedbackContrastSlider, QStringLiteral("feedbackContrast"));
    tagMidiWidget(m_feedbackHueShiftSlider, QStringLiteral("feedbackHueShift"));
    tagMidiWidget(m_feedbackGammaSlider, QStringLiteral("feedbackGamma"));
    tagMidiWidget(m_feedbackRotationSlider, QStringLiteral("feedbackRotationDeg"));
    tagMidiWidget(m_feedbackZoomSlider, QStringLiteral("feedbackZoom"));
    tagMidiWidget(m_feedbackTranslateXSlider, QStringLiteral("feedbackTranslateX"));
    tagMidiWidget(m_feedbackTranslateYSlider, QStringLiteral("feedbackTranslateY"));
    tagMidiWidget(m_feedbackRetentionSlider, QStringLiteral("feedbackRetention"));
    tagMidiWidget(m_feedbackFrameDelaySlider, QStringLiteral("feedbackFrameDelay"));
    tagMidiWidget(m_feedbackBlendCombo, QStringLiteral("feedbackBlendMode"));
    tagMidiWidget(m_feedbackWrapCombo, QStringLiteral("feedbackWrapMode"));
    tagMidiWidget(m_pictureWrapCombo, QStringLiteral("pictureWrapMode"));
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
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::RightButton) {
            if (auto* slider = qobject_cast<QSlider*>(watched)) {
                const QVariant reset = slider->property("pvjResetValue");
                if (reset.isValid()) {
                    slider->setValue(reset.toInt());
                    return true;
                }
            }
        }
    }
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

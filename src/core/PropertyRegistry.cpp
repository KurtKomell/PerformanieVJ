#include "PropertyRegistry.h"
#include "FilterParamSchema.h"

#include <QHash>

#include <cmath>
#include <QtMath>

namespace pvj::core {
namespace PropertyRegistry {

namespace {

QString norm(const QString& raw)
{
    return raw.toLower();
}

bool parseFilterPropertyRaw(const QString& raw, QUuid* node, QString* paramName)
{
    const QString normalized = raw.trimmed();
    if (!normalized.startsWith(QStringLiteral("filter."), Qt::CaseInsensitive)) {
        return false;
    }
    const QStringList parts = normalized.split(QLatin1Char('.'));
    if (parts.size() != 3) {
        return false;
    }
    if (parts[0].compare(QStringLiteral("filter"), Qt::CaseInsensitive) != 0) {
        return false;
    }
    const QUuid id = QUuid(QStringLiteral("{%1}").arg(parts[1]));
    if (id.isNull()) {
        return false;
    }
    if (parts[2].isEmpty()) {
        return false;
    }
    if (node) {
        *node = id;
    }
    if (paramName) {
        *paramName = parts[2];
    }
    return true;
}

const FilterParamSpec* filterParamSpecForProperty(const QString& raw, const Cell* cell)
{
    QUuid nodeId;
    QString paramName;
    if (!parseFilterPropertyRaw(raw, &nodeId, &paramName) || !cell) {
        return nullptr;
    }
    for (const auto& node : cell->filterChain) {
        if (node.id != nodeId) {
            continue;
        }
        const auto it = filterParamSchemas().find(node.typeId);
        if (it == filterParamSchemas().end()) {
            return nullptr;
        }
        for (const auto& spec : it.value().params) {
            if (spec.name.compare(paramName, Qt::CaseInsensitive) == 0) {
                return &spec;
            }
        }
    }
    return nullptr;
}

const FilterParamSpec* findSpecByParamName(const QString& paramName)
{
    for (auto it = filterParamSchemas().cbegin(); it != filterParamSchemas().cend(); ++it) {
        for (const auto& spec : it.value().params) {
            if (spec.name.compare(paramName, Qt::CaseInsensitive) == 0) {
                return &spec;
            }
        }
    }
    return nullptr;
}

bool readFilterParamValue(const Cell& c, const QString& raw, double* out)
{
    QUuid nodeId;
    QString paramName;
    if (!parseFilterPropertyRaw(raw, &nodeId, &paramName)) {
        return false;
    }
    for (const auto& node : c.filterChain) {
        if (node.id != nodeId) {
            continue;
        }
        for (const auto& p : node.params) {
            if (p.name.compare(paramName, Qt::CaseInsensitive) == 0) {
                *out = p.value;
                return true;
            }
        }
    }
    return false;
}

bool writeFilterParamValue(Cell& c, const QString& raw, double v)
{
    QUuid nodeId;
    QString paramName;
    if (!parseFilterPropertyRaw(raw, &nodeId, &paramName)) {
        return false;
    }
    for (auto& node : c.filterChain) {
        if (node.id != nodeId) {
            continue;
        }
        const auto it = filterParamSchemas().find(node.typeId);
        if (it == filterParamSchemas().end()) {
            return false;
        }
        const FilterParamSpec* spec = nullptr;
        for (const auto& s : it.value().params) {
            if (s.name.compare(paramName, Qt::CaseInsensitive) == 0) {
                spec = &s;
                break;
            }
        }
        if (!spec) {
            return false;
        }
        double next = qBound(spec->minV, v, spec->maxV);
        if (spec->kind == FilterParamKind::Bool) {
            next = next >= 0.5 ? 1.0 : 0.0;
        }
        for (auto& p : node.params) {
            if (p.name.compare(paramName, Qt::CaseInsensitive) == 0) {
                p.value = next;
                return true;
            }
        }
        node.params.append({ spec->name, next });
        return true;
    }
    return false;
}

// CopyMode::DifferenceRgb == 50 → 51 entries
constexpr int kCopyModeCount = 51;
// PlayMode::LoopForward … PlayMode::StepFrame (11 values).
constexpr int kPlayModeCount = 11;
constexpr int kMaskTypeCount = 6;
constexpr int kWrapModeCount = 4;
constexpr int kMixingPresetCount = 4;
constexpr int kPriorityCount = 3;

void invertContinuous(double v, double minV, double maxV, double* out)
{
    const double mid = 0.5 * (minV + maxV);
    *out = qBound(minV, 2.0 * mid - v, maxV);
}

int indexFromNormalized(int enumCount, double n01)
{
    if (enumCount <= 1) {
        return 0;
    }
    const double t = qBound(0.0, n01, 1.0);
    return qMin(enumCount - 1, int(std::floor(t * double(enumCount))));
}

} // namespace

QStringList allPropertyNames()
{
    return QStringList({
        QStringLiteral("transparency"),
        QStringLiteral("audioGain"),
        QStringLiteral("movieSpeed"),
        QStringLiteral("fade"),
        QStringLiteral("rotationZ"),
        QStringLiteral("maskWidth"),
        QStringLiteral("maskSmoothness"),
        QStringLiteral("segmentInU"),
        QStringLiteral("segmentOutU"),
        QStringLiteral("scratchHeadU"),
        QStringLiteral("keyChannelR"),
        QStringLiteral("keyChannelG"),
        QStringLiteral("keyChannelB"),
        QStringLiteral("pictureZoom"),
        QStringLiteral("pictureRotationDeg"),
        QStringLiteral("pictureBrightness"),
        QStringLiteral("pictureContrast"),
        QStringLiteral("pictureSaturation"),
        QStringLiteral("pictureCircularMotion"),
        QStringLiteral("feedbackEnabled"),
        QStringLiteral("feedbackStrength"),
        QStringLiteral("feedbackZoom"),
        QStringLiteral("feedbackRotationDeg"),
        QStringLiteral("feedbackRotationAnimated"),
        QStringLiteral("feedbackDecay"),
        QStringLiteral("feedbackBrightness"),
        QStringLiteral("feedbackSaturation"),
        QStringLiteral("feedbackGamma"),
        QStringLiteral("feedbackContrast"),
        QStringLiteral("feedbackLayerBrightness"),
        QStringLiteral("feedbackLayerSaturation"),
        QStringLiteral("feedbackLayerGamma"),
        QStringLiteral("feedbackLayerContrast"),
        QStringLiteral("feedbackWrapMode"),
        QStringLiteral("playMode"),
        QStringLiteral("clipPaused"),
        QStringLiteral("copyMode"),
        QStringLiteral("maskType"),
        QStringLiteral("priority"),
        QStringLiteral("mixingPresetIndex"),
    });
}

QString labelFor(const QString& name)
{
    const QString p = norm(name);
    if (isFilterParamProperty(p)) {
        QUuid nodeId;
        QString paramName;
        if (parseFilterParamProperty(p, &nodeId, &paramName)) {
            Q_UNUSED(nodeId);
            return paramName;
        }
    }
    static const QHash<QString, QString> kLabels = {
        { QStringLiteral("transparency"), QStringLiteral("Transparency") },
        { QStringLiteral("audiogain"), QStringLiteral("Audio gain") },
        { QStringLiteral("moviespeed"), QStringLiteral("Movie speed") },
        { QStringLiteral("fade"), QStringLiteral("Fade") },
        { QStringLiteral("rotationz"), QStringLiteral("Rotation Z") },
        { QStringLiteral("maskwidth"), QStringLiteral("Key hardness") },
        { QStringLiteral("masksmoothness"), QStringLiteral("Key feather") },
        { QStringLiteral("segmentinu"), QStringLiteral("Segment in") },
        { QStringLiteral("segmentoutu"), QStringLiteral("Segment out") },
        { QStringLiteral("scratchheadu"), QStringLiteral("Scratch") },
        { QStringLiteral("keychannelr"), QStringLiteral("Key R") },
        { QStringLiteral("keychannelg"), QStringLiteral("Key G") },
        { QStringLiteral("keychannelb"), QStringLiteral("Key B") },
        { QStringLiteral("picturezoom"), QStringLiteral("Picture zoom") },
        { QStringLiteral("picturerotationdeg"), QStringLiteral("Picture rotation") },
        { QStringLiteral("picturebrightness"), QStringLiteral("Picture brightness") },
        { QStringLiteral("picturecontrast"), QStringLiteral("Picture contrast") },
        { QStringLiteral("picturesaturation"), QStringLiteral("Picture saturation") },
        { QStringLiteral("picturecircularmotion"), QStringLiteral("Picture circular motion") },
        { QStringLiteral("feedbackenabled"), QStringLiteral("Feedback enabled") },
        { QStringLiteral("feedbackstrength"), QStringLiteral("Feedback strength") },
        { QStringLiteral("feedbackzoom"), QStringLiteral("Feedback zoom") },
        { QStringLiteral("feedbackrotationdeg"), QStringLiteral("Feedback rotation") },
        { QStringLiteral("feedbackrotationanimated"), QStringLiteral("Feedback circular rotation") },
        { QStringLiteral("feedbackdecay"), QStringLiteral("Feedback decay") },
        { QStringLiteral("feedbackbrightness"), QStringLiteral("Feedback brightness") },
        { QStringLiteral("feedbacksaturation"), QStringLiteral("Feedback saturation") },
        { QStringLiteral("feedbackgamma"), QStringLiteral("Feedback gamma") },
        { QStringLiteral("feedbackcontrast"), QStringLiteral("Feedback contrast") },
        { QStringLiteral("feedbacklayerbrightness"), QStringLiteral("Feedback layer brightness") },
        { QStringLiteral("feedbacklayersaturation"), QStringLiteral("Feedback layer saturation") },
        { QStringLiteral("feedbacklayergamma"), QStringLiteral("Feedback layer gamma") },
        { QStringLiteral("feedbacklayercontrast"), QStringLiteral("Feedback layer contrast") },
        { QStringLiteral("feedbackwrapmode"), QStringLiteral("Feedback wrap mode") },
        { QStringLiteral("playmode"), QStringLiteral("Play mode") },
        { QStringLiteral("clippaused"), QStringLiteral("Clip paused") },
        { QStringLiteral("copymode"), QStringLiteral("Copy mode") },
        { QStringLiteral("masktype"), QStringLiteral("Mask type") },
        { QStringLiteral("priority"), QStringLiteral("Priority") },
        { QStringLiteral("mixingpresetindex"), QStringLiteral("Mixing preset") },
    };
    return kLabels.value(p, name);
}

Kind kindOf(const QString& name)
{
    const QString p = norm(name);
    if (isFilterParamProperty(p)) {
        QUuid nodeId;
        QString paramName;
        if (!parseFilterParamProperty(p, &nodeId, &paramName)) {
            return Kind::Continuous;
        }
        Q_UNUSED(nodeId);
        const FilterParamSpec* spec = findSpecByParamName(paramName);
        if (spec && spec->kind == FilterParamKind::Bool) {
            return Kind::Boolean;
        }
        if (spec && spec->kind == FilterParamKind::EnumIndex) {
            return Kind::Enum;
        }
        return Kind::Continuous;
    }
    if (p == QLatin1String("clippaused")
        || p == QLatin1String("feedbackenabled")
        || p == QLatin1String("feedbackrotationanimated")) {
        return Kind::Boolean;
    }
    if (p == QLatin1String("playmode") || p == QLatin1String("copymode") || p == QLatin1String("masktype")
        || p == QLatin1String("feedbackwrapmode")
        || p == QLatin1String("priority") || p == QLatin1String("mixingpresetindex")) {
        return Kind::Enum;
    }
    return Kind::Continuous;
}

int enumCountOf(const QString& name)
{
    const QString p = norm(name);
    if (p == QLatin1String("playmode")) {
        return kPlayModeCount;
    }
    if (p == QLatin1String("copymode")) {
        return kCopyModeCount;
    }
    if (p == QLatin1String("masktype")) {
        return kMaskTypeCount;
    }
    if (p == QLatin1String("feedbackwrapmode")) {
        return kWrapModeCount;
    }
    if (p == QLatin1String("priority")) {
        return kPriorityCount;
    }
    if (p == QLatin1String("mixingpresetindex")) {
        return kMixingPresetCount;
    }
    QUuid nodeId;
    QString paramName;
    if (parseFilterParamProperty(p, &nodeId, &paramName)) {
        Q_UNUSED(nodeId);
        const FilterParamSpec* spec = findSpecByParamName(paramName);
        if (spec && spec->kind == FilterParamKind::EnumIndex) {
            return qMax(0, spec->enumLabels.size());
        }
    }
    return 0;
}

void learnMinMax(const QString& name, double* minV, double* maxV)
{
    const QString p = norm(name);
    if (isFilterParamProperty(p)) {
        QUuid nodeId;
        QString paramName;
        if (parseFilterParamProperty(p, &nodeId, &paramName)) {
            Q_UNUSED(nodeId);
            if (const FilterParamSpec* spec = findSpecByParamName(paramName)) {
                *minV = spec->minV;
                *maxV = spec->maxV;
                return;
            }
        }
        *minV = 0.0;
        *maxV = 1.0;
        return;
    }
    if (kindOf(name) == Kind::Enum) {
        *minV = 0.0;
        *maxV = 1.0;
        return;
    }
    if (p == QLatin1String("moviespeed")) {
        *minV = -4.0;
        *maxV = 4.0;
    } else if (p == QLatin1String("audiogain")) {
        *minV = 0.0;
        *maxV = 4.0;
    } else if (p == QLatin1String("rotationz")) {
        *minV = -1.0;
        *maxV = 1.0;
    } else if (p == QLatin1String("picturezoom")) {
        *minV = -1.0;
        *maxV = 1.0;
    } else if (p == QLatin1String("picturerotationdeg")) {
        *minV = -180.0;
        *maxV = 180.0;
    } else if (p == QLatin1String("picturebrightness")) {
        *minV = -1.0;
        *maxV = 1.0;
    } else if (p == QLatin1String("picturecontrast") || p == QLatin1String("picturesaturation")) {
        *minV = 0.0;
        *maxV = 2.0;
    } else if (p == QLatin1String("picturecircularmotion")) {
        *minV = 0.0;
        *maxV = 1.0;
    } else if (p == QLatin1String("feedbackstrength")) {
        *minV = 0.0;
        *maxV = 1.0;
    } else if (p == QLatin1String("feedbackzoom")) {
        *minV = 0.5;
        *maxV = 2.0;
    } else if (p == QLatin1String("feedbackrotationdeg")) {
        *minV = -180.0;
        *maxV = 180.0;
    } else if (p == QLatin1String("feedbackdecay")) {
        *minV = -0.1;
        *maxV = 0.1;
    } else if (p == QLatin1String("feedbackbrightness")
               || p == QLatin1String("feedbacklayerbrightness")) {
        *minV = -1.0;
        *maxV = 1.0;
    } else if (p == QLatin1String("feedbacksaturation")
               || p == QLatin1String("feedbackcontrast")
               || p == QLatin1String("feedbacklayersaturation")
               || p == QLatin1String("feedbacklayercontrast")) {
        *minV = 0.0;
        *maxV = 2.0;
    } else if (p == QLatin1String("feedbackgamma")
               || p == QLatin1String("feedbacklayergamma")) {
        *minV = 0.1;
        *maxV = 4.0;
    } else {
        *minV = 0.0;
        *maxV = 1.0;
    }
}

bool readValue(const Cell& c, const QString& raw, double* out)
{
    const QString p = norm(raw);
    if (readFilterParamValue(c, p, out)) {
        return true;
    }
    if (p == QLatin1String("transparency")) {
        *out = c.props.transparency;
        return true;
    }
    if (p == QLatin1String("audiogain")) {
        *out = c.props.audioGain;
        return true;
    }
    if (p == QLatin1String("moviespeed")) {
        *out = c.props.movieSpeed;
        return true;
    }
    if (p == QLatin1String("fade")) {
        *out = c.props.fade;
        return true;
    }
    if (p == QLatin1String("rotationz")) {
        *out = c.props.rotationZ;
        return true;
    }
    if (p == QLatin1String("maskwidth")) {
        *out = c.props.maskWidth;
        return true;
    }
    if (p == QLatin1String("masksmoothness")) {
        *out = c.props.maskSmoothness;
        return true;
    }
    if (p == QLatin1String("segmentinu")) {
        *out = c.props.segmentInU;
        return true;
    }
    if (p == QLatin1String("segmentoutu")) {
        *out = c.props.segmentOutU;
        return true;
    }
    if (p == QLatin1String("scratchheadu")) {
        *out = c.props.scratchHeadU;
        return true;
    }
    if (p == QLatin1String("keychannelr")) {
        *out = c.props.keyChannelR;
        return true;
    }
    if (p == QLatin1String("keychannelg")) {
        *out = c.props.keyChannelG;
        return true;
    }
    if (p == QLatin1String("keychannelb")) {
        *out = c.props.keyChannelB;
        return true;
    }
    if (p == QLatin1String("picturezoom")) {
        *out = c.props.picture.zoom;
        return true;
    }
    if (p == QLatin1String("picturerotationdeg")) {
        *out = c.props.picture.rotationDeg;
        return true;
    }
    if (p == QLatin1String("picturebrightness")) {
        *out = c.props.picture.brightness;
        return true;
    }
    if (p == QLatin1String("picturecontrast")) {
        *out = c.props.picture.contrast;
        return true;
    }
    if (p == QLatin1String("picturesaturation")) {
        *out = c.props.picture.saturation;
        return true;
    }
    if (p == QLatin1String("picturecircularmotion")) {
        *out = c.props.picture.circularMotion;
        return true;
    }
    if (p == QLatin1String("feedbackenabled")) {
        *out = c.props.feedback.enabled ? 1.0 : 0.0;
        return true;
    }
    if (p == QLatin1String("feedbackstrength")) {
        *out = c.props.feedback.strength;
        return true;
    }
    if (p == QLatin1String("feedbackzoom")) {
        *out = c.props.feedback.zoom;
        return true;
    }
    if (p == QLatin1String("feedbackrotationdeg")) {
        *out = c.props.feedback.rotationDeg;
        return true;
    }
    if (p == QLatin1String("feedbackrotationanimated")) {
        *out = c.props.feedback.rotationAnimated ? 1.0 : 0.0;
        return true;
    }
    if (p == QLatin1String("feedbackdecay")) {
        *out = c.props.feedback.decay;
        return true;
    }
    if (p == QLatin1String("feedbackbrightness")) {
        *out = c.props.feedback.brightness;
        return true;
    }
    if (p == QLatin1String("feedbacksaturation")) {
        *out = c.props.feedback.saturation;
        return true;
    }
    if (p == QLatin1String("feedbackgamma")) {
        *out = c.props.feedback.gamma;
        return true;
    }
    if (p == QLatin1String("feedbackcontrast")) {
        *out = c.props.feedback.contrast;
        return true;
    }
    if (p == QLatin1String("feedbacklayerbrightness")) {
        *out = c.props.feedback.layerBrightness;
        return true;
    }
    if (p == QLatin1String("feedbacklayersaturation")) {
        *out = c.props.feedback.layerSaturation;
        return true;
    }
    if (p == QLatin1String("feedbacklayergamma")) {
        *out = c.props.feedback.layerGamma;
        return true;
    }
    if (p == QLatin1String("feedbacklayercontrast")) {
        *out = c.props.feedback.layerContrast;
        return true;
    }
    if (p == QLatin1String("feedbackwrapmode")) {
        *out = double(int(c.props.feedback.wrapMode));
        return true;
    }
    if (p == QLatin1String("playmode")) {
        *out = double(int(c.props.playMode));
        return true;
    }
    if (p == QLatin1String("clippaused")) {
        *out = c.props.clipPaused ? 1.0 : 0.0;
        return true;
    }
    if (p == QLatin1String("copymode")) {
        *out = double(int(c.props.copyMode));
        return true;
    }
    if (p == QLatin1String("masktype")) {
        *out = double(int(c.props.maskType));
        return true;
    }
    if (p == QLatin1String("priority")) {
        if (c.props.priority <= -5) {
            *out = 0.0;
        } else if (c.props.priority >= 5) {
            *out = 2.0;
        } else {
            *out = 1.0;
        }
        return true;
    }
    if (p == QLatin1String("mixingpresetindex")) {
        *out = double(c.props.mixingPresetIndex);
        return true;
    }
    return false;
}

bool applyEnumIndex(Cell& c, const QString& raw, int index)
{
    const QString p = norm(raw);
    if (isFilterParamProperty(p)) {
        return writeFilterParamValue(c, p, double(index));
    }
    if (p == QLatin1String("playmode")) {
        c.props.playMode = static_cast<PlayMode>(qBound(0, index, kPlayModeCount - 1));
        return true;
    }
    if (p == QLatin1String("copymode")) {
        c.props.copyMode = static_cast<CopyMode>(qBound(0, index, kCopyModeCount - 1));
        return true;
    }
    if (p == QLatin1String("masktype")) {
        c.props.maskType = static_cast<MaskType>(qBound(0, index, kMaskTypeCount - 1));
        return true;
    }
    if (p == QLatin1String("feedbackwrapmode")) {
        c.props.feedback.wrapMode = static_cast<WrapMode>(qBound(0, index, kWrapModeCount - 1));
        return true;
    }
    if (p == QLatin1String("priority")) {
        const int i = qBound(0, index, kPriorityCount - 1);
        c.props.priority = (i == 0) ? -10 : (i == 1) ? 0 : 10;
        return true;
    }
    if (p == QLatin1String("mixingpresetindex")) {
        c.props.mixingPresetIndex = qBound(0, index, kMixingPresetCount - 1);
        return true;
    }
    return false;
}

bool applyEnumFromNormalized(Cell& c, const QString& raw, double n01)
{
    if (isFilterParamProperty(raw)) {
        const FilterParamSpec* spec = filterParamSpecForProperty(raw, &c);
        if (!spec || spec->kind != FilterParamKind::EnumIndex) {
            return false;
        }
        const int enumCount = qMax(1, spec->enumLabels.size());
        const int idx = indexFromNormalized(enumCount, n01);
        return writeFilterParamValue(c, raw, idx);
    }
    const int n = enumCountOf(raw);
    if (n <= 0) {
        return false;
    }
    const int idx = indexFromNormalized(n, n01);
    return applyEnumIndex(c, raw, idx);
}

bool applyValue(Cell& c, const QString& raw, double v)
{
    const QString p = norm(raw);
    if (writeFilterParamValue(c, p, v)) {
        return true;
    }
    if (p == QLatin1String("transparency")) {
        c.props.transparency = qBound(0.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("audiogain")) {
        c.props.audioGain = qBound(0.0, v, 4.0);
        return true;
    }
    if (p == QLatin1String("moviespeed")) {
        c.props.movieSpeed = v;
        return true;
    }
    if (p == QLatin1String("fade")) {
        c.props.fade = qBound(0.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("rotationz")) {
        c.props.rotationZ = qBound(-1.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("maskwidth")) {
        c.props.maskWidth = qBound(0.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("masksmoothness")) {
        c.props.maskSmoothness = qBound(0.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("segmentinu")) {
        c.props.segmentInU = qBound(0.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("segmentoutu")) {
        c.props.segmentOutU = qBound(0.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("scratchheadu")) {
        c.props.scratchHeadU = qBound(0.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("keychannelr")) {
        c.props.keyChannelR = qBound(0.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("keychannelg")) {
        c.props.keyChannelG = qBound(0.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("keychannelb")) {
        c.props.keyChannelB = qBound(0.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("picturezoom")) {
        c.props.picture.zoom = qBound(-1.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("picturerotationdeg")) {
        c.props.picture.rotationDeg = qBound(-180.0, v, 180.0);
        return true;
    }
    if (p == QLatin1String("picturebrightness")) {
        c.props.picture.brightness = qBound(-1.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("picturecontrast")) {
        c.props.picture.contrast = qBound(0.0, v, 2.0);
        return true;
    }
    if (p == QLatin1String("picturesaturation")) {
        c.props.picture.saturation = qBound(0.0, v, 2.0);
        return true;
    }
    if (p == QLatin1String("picturecircularmotion")) {
        c.props.picture.circularMotion = qBound(0.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("feedbackenabled")) {
        c.props.feedback.enabled = (v >= 0.5);
        return true;
    }
    if (p == QLatin1String("feedbackstrength")) {
        c.props.feedback.strength = qBound(0.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("feedbackzoom")) {
        c.props.feedback.zoom = qBound(0.5, v, 2.0);
        return true;
    }
    if (p == QLatin1String("feedbackrotationdeg")) {
        c.props.feedback.rotationDeg = qBound(-180.0, v, 180.0);
        return true;
    }
    if (p == QLatin1String("feedbackrotationanimated")) {
        c.props.feedback.rotationAnimated = (v >= 0.5);
        return true;
    }
    if (p == QLatin1String("feedbackdecay")) {
        c.props.feedback.decay = qBound(-0.1, v, 0.1);
        return true;
    }
    if (p == QLatin1String("feedbackbrightness")) {
        c.props.feedback.brightness = qBound(-1.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("feedbacksaturation")) {
        c.props.feedback.saturation = qBound(0.0, v, 2.0);
        return true;
    }
    if (p == QLatin1String("feedbackgamma")) {
        c.props.feedback.gamma = qBound(0.1, v, 4.0);
        return true;
    }
    if (p == QLatin1String("feedbackcontrast")) {
        c.props.feedback.contrast = qBound(0.0, v, 2.0);
        return true;
    }
    if (p == QLatin1String("feedbacklayerbrightness")) {
        c.props.feedback.layerBrightness = qBound(-1.0, v, 1.0);
        return true;
    }
    if (p == QLatin1String("feedbacklayersaturation")) {
        c.props.feedback.layerSaturation = qBound(0.0, v, 2.0);
        return true;
    }
    if (p == QLatin1String("feedbacklayergamma")) {
        c.props.feedback.layerGamma = qBound(0.1, v, 4.0);
        return true;
    }
    if (p == QLatin1String("feedbacklayercontrast")) {
        c.props.feedback.layerContrast = qBound(0.0, v, 2.0);
        return true;
    }
    if (p == QLatin1String("clippaused")) {
        c.props.clipPaused = (v >= 0.5);
        return true;
    }
    return false;
}

bool applySetValue(Cell& c, const QString& raw, double v)
{
    const QString p = norm(raw);
    if (kindOf(raw) == Kind::Enum) {
        return applyEnumIndex(c, raw, int(std::lround(v)));
    }
    if (kindOf(raw) == Kind::Boolean) {
        return applyValue(c, raw, v >= 0.5 ? 1.0 : 0.0);
    }
    return applyValue(c, raw, v);
}

bool isFilterParamProperty(const QString& raw)
{
    return parseFilterPropertyRaw(raw, nullptr, nullptr);
}

bool parseFilterParamProperty(const QString& raw, QUuid* node, QString* paramName)
{
    return parseFilterPropertyRaw(raw, node, paramName);
}

bool toggleValue(Cell& c, const QString& raw)
{
    const QString p = norm(raw);
    const Kind k = kindOf(raw);
    if (k == Kind::Boolean) {
        double cur = 0.0;
        readValue(c, raw, &cur);
        return applyValue(c, raw, cur >= 0.5 ? 0.0 : 1.0);
    }
    if (k == Kind::Enum) {
        double cur = 0.0;
        readValue(c, raw, &cur);
        const int n = enumCountOf(raw);
        const int idx = (int(std::lround(cur)) + 1) % qMax(1, n);
        return applyEnumIndex(c, raw, idx);
    }
    double cur = 0.0;
    if (!readValue(c, raw, &cur)) {
        return false;
    }
    double minV = 0.0;
    double maxV = 1.0;
    learnMinMax(raw, &minV, &maxV);
    double inv = 0.0;
    invertContinuous(cur, minV, maxV, &inv);
    return applyValue(c, raw, inv);
}

} // namespace PropertyRegistry
} // namespace pvj::core

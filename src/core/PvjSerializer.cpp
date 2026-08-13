#include "PvjSerializer.h"

#include "EnumStrings.h"
#include "FilterEffectIds.h"
#include "FilterParamSchema.h"
#include "PropertyRegistry.h"

#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtGlobal>

namespace pvj::core {

namespace {

constexpr const char* kRootElement = "PerformanieVJ";
constexpr const char* kFormatVersion = "1";

void writeVisual(QXmlStreamWriter& w, const VisualRef& v)
{
    w.writeStartElement(QStringLiteral("Visual"));
    w.writeAttribute(QStringLiteral("type"), enums::toString(v.type));
    if (v.type == VisualType::Media) {
        w.writeAttribute(QStringLiteral("mediaId"), v.mediaId.toString(QUuid::WithoutBraces));
    } else if (v.type == VisualType::Generator) {
        w.writeAttribute(QStringLiteral("generator"), enums::toString(v.generator));
    }
    w.writeEndElement();
}

void writeProps(QXmlStreamWriter& w, const CellProps& p)
{
    w.writeStartElement(QStringLiteral("Props"));
    w.writeAttribute(QStringLiteral("priority"),      QString::number(p.priority));
    w.writeAttribute(QStringLiteral("transparency"),  QString::number(p.transparency, 'g', 6));
    w.writeAttribute(QStringLiteral("audioGain"),     QString::number(p.audioGain,     'g', 6));
    w.writeAttribute(QStringLiteral("movieSpeed"),    QString::number(p.movieSpeed,   'g', 6));
    w.writeAttribute(QStringLiteral("fade"),          QString::number(p.fade,         'g', 6));
    w.writeAttribute(QStringLiteral("maskType"),       enums::toString(p.maskType));
    w.writeAttribute(QStringLiteral("matteRole"),      enums::toString(p.matteRole));
    w.writeAttribute(QStringLiteral("maskFeather"),    QString::number(p.maskFeather,    'g', 6));
    w.writeAttribute(QStringLiteral("maskRectWidth"),  QString::number(p.maskRectWidth,  'g', 6));
    w.writeAttribute(QStringLiteral("maskRectHeight"), QString::number(p.maskRectHeight, 'g', 6));
    w.writeAttribute(QStringLiteral("maskRadius"),     QString::number(p.maskRadius,     'g', 6));
    w.writeAttribute(QStringLiteral("maskEllipseX"),   QString::number(p.maskEllipseX,   'g', 6));
    w.writeAttribute(QStringLiteral("maskEllipseY"),   QString::number(p.maskEllipseY,   'g', 6));
    w.writeAttribute(QStringLiteral("maskWidth"),      QString::number(p.maskWidth,      'g', 6));
    w.writeAttribute(QStringLiteral("maskSmoothness"), QString::number(p.maskSmoothness, 'g', 6));
    w.writeAttribute(QStringLiteral("rotationZ"),      QString::number(p.rotationZ,      'g', 6));
    w.writeAttribute(QStringLiteral("copyMode"),       enums::toString(p.copyMode));
    w.writeAttribute(QStringLiteral("mixingPresetIndex"), QString::number(p.mixingPresetIndex));
    w.writeAttribute(QStringLiteral("preferredLayer"), QString::number(qBound(0, p.preferredLayer, 12)));
    w.writeAttribute(QStringLiteral("keyChannelR"),   QString::number(p.keyChannelR,   'g', 6));
    w.writeAttribute(QStringLiteral("keyChannelG"),   QString::number(p.keyChannelG,   'g', 6));
    w.writeAttribute(QStringLiteral("keyChannelB"),   QString::number(p.keyChannelB,   'g', 6));
    w.writeAttribute(QStringLiteral("keyingMode"),    enums::toString(p.keyingMode));
    w.writeAttribute(QStringLiteral("keyingEnabled"), p.keyingEnabled ? QStringLiteral("true")
                                                                       : QStringLiteral("false"));
    w.writeAttribute(QStringLiteral("keyLumaCenter"), QString::number(p.keyLumaCenter, 'g', 6));
    w.writeAttribute(QStringLiteral("keyLumaInvert"), p.keyLumaInvert ? QStringLiteral("true")
                                                                       : QStringLiteral("false"));
    w.writeAttribute(QStringLiteral("keyThreshold"), QString::number(p.keyThreshold, 'g', 6));
    w.writeAttribute(QStringLiteral("keySoftness"), QString::number(p.keySoftness, 'g', 6));
    w.writeAttribute(QStringLiteral("keyChromaHue"),  QString::number(p.keyChromaHue, 'g', 6));
    w.writeAttribute(QStringLiteral("keyChromaInvert"), p.keyChromaInvert ? QStringLiteral("true")
                                                                           : QStringLiteral("false"));
    w.writeAttribute(QStringLiteral("playMode"),       QString::number(int(p.playMode)));
    w.writeAttribute(QStringLiteral("clipPaused"),    p.clipPaused ? QStringLiteral("true") : QStringLiteral("false"));
    w.writeAttribute(QStringLiteral("segmentInU"),   QString::number(p.segmentInU,   'g', 6));
    w.writeAttribute(QStringLiteral("segmentOutU"),  QString::number(p.segmentOutU,  'g', 6));
    w.writeAttribute(QStringLiteral("scratchHeadU"), QString::number(p.scratchHeadU, 'g', 6));
    w.writeAttribute(QStringLiteral("overlayText"),  p.overlayText);
    w.writeAttribute(QStringLiteral("tcStart"),       p.tcStart);

    w.writeStartElement(QStringLiteral("Picture"));
    w.writeAttribute(QStringLiteral("zoom"),           QString::number(p.picture.zoom,           'g', 6));
    w.writeAttribute(QStringLiteral("rotationDeg"),    QString::number(p.picture.rotationDeg,    'g', 6));
    w.writeAttribute(QStringLiteral("brightness"),     QString::number(p.picture.brightness,     'g', 6));
    w.writeAttribute(QStringLiteral("contrast"),       QString::number(p.picture.contrast,       'g', 6));
    w.writeAttribute(QStringLiteral("saturation"),     QString::number(p.picture.saturation,     'g', 6));
    w.writeAttribute(QStringLiteral("circularMotion"), QString::number(p.picture.circularMotion, 'g', 6));
    if (p.picture.wrapMode != WrapMode::Clamp) {
        w.writeAttribute(QStringLiteral("wrapMode"), enums::toString(p.picture.wrapMode));
    }
    w.writeEndElement();

    const FeedbackParams fbDefault;
    const bool fbNonDefault = p.feedback.inSaturation != fbDefault.inSaturation
        || p.feedback.inBrightness != fbDefault.inBrightness
        || p.feedback.inContrast != fbDefault.inContrast
        || p.feedback.inHueShift != fbDefault.inHueShift
        || p.feedback.inGamma != fbDefault.inGamma
        || p.feedback.saturation != fbDefault.saturation
        || p.feedback.brightness != fbDefault.brightness
        || p.feedback.contrast != fbDefault.contrast
        || p.feedback.hueShift != fbDefault.hueShift
        || p.feedback.gamma != fbDefault.gamma
        || p.feedback.rotationDeg != fbDefault.rotationDeg
        || p.feedback.zoom != fbDefault.zoom
        || p.feedback.frameDelay != fbDefault.frameDelay
        || p.feedback.inputMode != fbDefault.inputMode
        || p.feedback.wrapMode != fbDefault.wrapMode;
    if (fbNonDefault) {
        w.writeStartElement(QStringLiteral("Feedback"));
        if (p.feedback.inSaturation != fbDefault.inSaturation) {
            w.writeAttribute(QStringLiteral("inSaturation"),
                             QString::number(p.feedback.inSaturation, 'g', 6));
        }
        if (p.feedback.inBrightness != fbDefault.inBrightness) {
            w.writeAttribute(QStringLiteral("inBrightness"),
                             QString::number(p.feedback.inBrightness, 'g', 6));
        }
        if (p.feedback.inContrast != fbDefault.inContrast) {
            w.writeAttribute(QStringLiteral("inContrast"),
                             QString::number(p.feedback.inContrast, 'g', 6));
        }
        if (p.feedback.inHueShift != fbDefault.inHueShift) {
            w.writeAttribute(QStringLiteral("inHueShift"),
                             QString::number(p.feedback.inHueShift, 'g', 6));
        }
        if (p.feedback.inGamma != fbDefault.inGamma) {
            w.writeAttribute(QStringLiteral("inGamma"), QString::number(p.feedback.inGamma, 'g', 6));
        }
        w.writeAttribute(QStringLiteral("saturation"),  QString::number(p.feedback.saturation,  'g', 6));
        w.writeAttribute(QStringLiteral("brightness"),  QString::number(p.feedback.brightness,  'g', 6));
        w.writeAttribute(QStringLiteral("contrast"),    QString::number(p.feedback.contrast,    'g', 6));
        w.writeAttribute(QStringLiteral("hueShift"),   QString::number(p.feedback.hueShift,   'g', 6));
        w.writeAttribute(QStringLiteral("gamma"),       QString::number(p.feedback.gamma,       'g', 6));
        w.writeAttribute(QStringLiteral("rotationDeg"), QString::number(p.feedback.rotationDeg, 'g', 6));
        w.writeAttribute(QStringLiteral("zoom"),        QString::number(p.feedback.zoom,        'g', 6));
        if (p.feedback.frameDelay != fbDefault.frameDelay) {
            w.writeAttribute(QStringLiteral("frameDelay"), QString::number(p.feedback.frameDelay));
        }
        if (p.feedback.inputMode != fbDefault.inputMode) {
            w.writeAttribute(QStringLiteral("inputMode"),
                             enums::toString(p.feedback.inputMode));
        }
        if (p.feedback.wrapMode != fbDefault.wrapMode) {
            w.writeAttribute(QStringLiteral("wrapMode"), enums::toString(p.feedback.wrapMode));
        }
        w.writeEndElement();
    }

    w.writeEndElement();
}

void writeEffect(QXmlStreamWriter& w, const Effect& e)
{
    w.writeStartElement(QStringLiteral("Effect"));
    w.writeAttribute(QStringLiteral("name"), e.name);
    for (const auto& p : e.params) {
        w.writeStartElement(QStringLiteral("Param"));
        w.writeAttribute(QStringLiteral("name"),  p.name);
        w.writeAttribute(QStringLiteral("value"), QString::number(p.value, 'g', 6));
        w.writeEndElement();
    }
    w.writeEndElement();
}

void writeMapping(QXmlStreamWriter& w, const PropertyMapping& m)
{
    w.writeStartElement(QStringLiteral("Mapping"));
    w.writeAttribute(QStringLiteral("property"), m.property);
    w.writeAttribute(QStringLiteral("input"),    enums::toString(m.input));
    w.writeAttribute(QStringLiteral("channel"),  QString::number(m.channel));
    w.writeAttribute(QStringLiteral("number"),   QString::number(m.number));
    w.writeAttribute(QStringLiteral("min"),      QString::number(m.minValue, 'g', 6));
    w.writeAttribute(QStringLiteral("max"),      QString::number(m.maxValue, 'g', 6));
    w.writeAttribute(QStringLiteral("buttonMode"), enums::toString(m.buttonMode));
    w.writeAttribute(QStringLiteral("buttonValue"), QString::number(m.buttonValue, 'g', 6));
    w.writeEndElement();
}

void writeFilterChain(QXmlStreamWriter& w, const QList<CellFilterNode>& chain)
{
    if (chain.isEmpty()) {
        return;
    }
    w.writeStartElement(QStringLiteral("FilterChain"));
    for (const auto& n : chain) {
        w.writeStartElement(QStringLiteral("FilterNode"));
        w.writeAttribute(QStringLiteral("id"), n.id.toString(QUuid::WithoutBraces));
        w.writeAttribute(QStringLiteral("type"), n.typeId);
        for (const auto& p : n.params) {
            w.writeStartElement(QStringLiteral("Param"));
            w.writeAttribute(QStringLiteral("name"), p.name);
            w.writeAttribute(QStringLiteral("value"), QString::number(p.value, 'g', 6));
            w.writeEndElement();
        }
        w.writeEndElement();
    }
    w.writeEndElement();
}

void writeCell(QXmlStreamWriter& w, const Cell& c)
{
    w.writeStartElement(QStringLiteral("Cell"));
    w.writeAttribute(QStringLiteral("index"), QString::number(c.index));
    if (!c.name.isEmpty()) {
        w.writeAttribute(QStringLiteral("name"), c.name);
    }
    writeVisual(w, c.visual);
    writeProps(w, c.props);
    if (c.effect) {
        writeEffect(w, *c.effect);
    }
    if (!c.propertyMappings.isEmpty()) {
        w.writeStartElement(QStringLiteral("PropertyMappings"));
        for (const auto& m : c.propertyMappings) {
            writeMapping(w, m);
        }
        w.writeEndElement();
    }
    writeFilterChain(w, c.filterChain);
    w.writeEndElement();
}

void writeBank(QXmlStreamWriter& w, const Bank& b)
{
    w.writeStartElement(QStringLiteral("Bank"));
    w.writeAttribute(QStringLiteral("index"), QString::number(b.index));
    w.writeAttribute(QStringLiteral("name"),  b.name);
    for (const auto& c : b.cells) {
        // Skip completely default empty cells to keep file size small.
        const bool isDefault = (c.visual.type == VisualType::Empty)
            && c.name.isEmpty()
            && !c.effect.has_value()
            && c.propertyMappings.isEmpty()
            && c.filterChain.isEmpty()
            && c.props.transparency == 1.0
            && c.props.movieSpeed == 1.0
            && c.props.fade == 0.0
            && c.props.priority == 0
            && c.props.maskType == MaskType::None
            && c.props.matteRole == LayerMatteRole::None
            && c.props.maskFeather == 0.1
            && c.props.maskRectWidth == 1.0
            && c.props.maskRectHeight == 1.0
            && c.props.maskRadius == 0.5
            && c.props.maskEllipseX == 0.6
            && c.props.maskEllipseY == 0.45
            && c.props.copyMode == CopyMode::Normal
            && c.props.rotationZ == 0.0
            && c.props.mixingPresetIndex == 0
            && c.props.preferredLayer == 4
            && c.props.keyChannelR == 1.0
            && c.props.keyChannelG == 1.0
            && c.props.keyChannelB == 1.0
            && c.props.keyingMode == KeyingMode::Luma
            && !c.props.keyingEnabled
            && c.props.keyLumaCenter == 0.5
            && !c.props.keyLumaInvert
            && c.props.keyThreshold == 0.25
            && c.props.keySoftness == 0.12
            && c.props.keyChromaHue == 0.33
            && !c.props.keyChromaInvert
            && c.props.audioGain == 1.0
            && c.props.playMode == PlayMode::LoopForward
            && !c.props.clipPaused
            && c.props.segmentInU == 0.0
            && c.props.segmentOutU == 1.0
            && c.props.scratchHeadU == 0.5
            && c.props.overlayText.isEmpty()
            && c.props.tcStart == QStringLiteral("00:00:00:00")
            && c.props.picture.zoom == 0.0
            && c.props.picture.rotationDeg == 0.0
            && c.props.picture.brightness == 0.0
            && c.props.picture.contrast == 1.0
            && c.props.picture.saturation == 1.0
            && c.props.picture.circularMotion == 0.0
            && c.props.picture.wrapMode == WrapMode::Clamp
            && c.props.feedback.inSaturation == 1.0
            && c.props.feedback.inBrightness == 0.0
            && c.props.feedback.inContrast == 1.0
            && c.props.feedback.inHueShift == 0.0
            && c.props.feedback.inGamma == 1.0
            && c.props.feedback.saturation == 1.0
            && c.props.feedback.brightness == 0.0
            && c.props.feedback.contrast == 1.0
            && c.props.feedback.hueShift == 0.0
            && c.props.feedback.gamma == 1.0
            && c.props.feedback.rotationDeg == 0.0
            && c.props.feedback.zoom == 0.0
            && c.props.feedback.frameDelay == 0
            && c.props.feedback.inputMode == FeedbackInputMode::StackComposite
            && c.props.feedback.wrapMode == WrapMode::Black;
        if (isDefault) {
            continue;
        }
        writeCell(w, c);
    }
    w.writeEndElement();
}

void writeBankSet(QXmlStreamWriter& w, const BankSet& s)
{
    w.writeStartElement(QStringLiteral("BankSet"));
    w.writeAttribute(QStringLiteral("type"), enums::toString(s.type));
    for (const auto& b : s.banks) {
        writeBank(w, b);
    }
    w.writeEndElement();
}

void writeMedia(QXmlStreamWriter& w, const QList<MediaItem>& lib)
{
    w.writeStartElement(QStringLiteral("MediaLibrary"));
    for (const auto& m : lib) {
        w.writeStartElement(QStringLiteral("Item"));
        w.writeAttribute(QStringLiteral("id"),   m.id.toString(QUuid::WithoutBraces));
        w.writeAttribute(QStringLiteral("path"), m.path);
        if (!m.displayName.isEmpty()) {
            w.writeAttribute(QStringLiteral("name"), m.displayName);
        }
        w.writeEndElement();
    }
    w.writeEndElement();
}

void writeTriggers(QXmlStreamWriter& w, const QList<TriggerMapping>& triggers)
{
    w.writeStartElement(QStringLiteral("TriggerMappings"));
    for (const auto& t : triggers) {
        w.writeStartElement(QStringLiteral("Trigger"));
        w.writeAttribute(QStringLiteral("input"),    enums::toString(t.input));
        w.writeAttribute(QStringLiteral("channel"),  QString::number(t.channel));
        w.writeAttribute(QStringLiteral("number"),   QString::number(t.number));
        if (!t.keyText.isEmpty()) {
            w.writeAttribute(QStringLiteral("keyText"), t.keyText);
        }
        w.writeAttribute(QStringLiteral("target"),   enums::toString(t.target));
        w.writeAttribute(QStringLiteral("bankSetIndex"), QString::number(t.bankSetIndex));
        w.writeAttribute(QStringLiteral("bankIndex"),    QString::number(t.bankIndex));
        w.writeAttribute(QStringLiteral("cellIndex"),    QString::number(t.cellIndex));
        if (!t.propertyName.isEmpty()) {
            w.writeAttribute(QStringLiteral("property"), t.propertyName);
        }
        w.writeEndElement();
    }
    w.writeEndElement();
}

void writeSettings(QXmlStreamWriter& w, const Settings& s)
{
    w.writeStartElement(QStringLiteral("Settings"));

    w.writeStartElement(QStringLiteral("Audio"));
    w.writeAttribute(QStringLiteral("driver"),        s.audio.driver);
    w.writeAttribute(QStringLiteral("inputDevice"),   s.audio.inputDevice);
    w.writeAttribute(QStringLiteral("outputDevice"),  s.audio.outputDevice);
    w.writeAttribute(QStringLiteral("bufferSize"),    QString::number(s.audio.bufferSize));
    w.writeAttribute(QStringLiteral("sampleRate"),    QString::number(s.audio.sampleRate));
    w.writeEndElement();

    w.writeStartElement(QStringLiteral("UI"));
    w.writeAttribute(QStringLiteral("mediaLibraryVisible"),  s.ui.mediaLibraryVisible  ? QStringLiteral("true") : QStringLiteral("false"));
    w.writeAttribute(QStringLiteral("bankPanelVisible"),     s.ui.bankPanelVisible     ? QStringLiteral("true") : QStringLiteral("false"));
    w.writeAttribute(QStringLiteral("parameterTabsVisible"), s.ui.parameterTabsVisible ? QStringLiteral("true") : QStringLiteral("false"));
    w.writeAttribute(QStringLiteral("mediaLibraryWidth"),    QString::number(s.ui.mediaLibraryWidth));
    w.writeAttribute(QStringLiteral("bankPanelHeight"),      QString::number(s.ui.bankPanelHeight));
    w.writeEndElement();

    w.writeStartElement(QStringLiteral("Matrix"));
    w.writeAttribute(QStringLiteral("width"),  QString::number(s.matrix.width));
    w.writeAttribute(QStringLiteral("height"), QString::number(s.matrix.height));
    w.writeAttribute(QStringLiteral("gridRows"), QString::number(s.matrix.gridRows));
    w.writeAttribute(QStringLiteral("gridCols"), QString::number(s.matrix.gridCols));
    w.writeEndElement();

    w.writeStartElement(QStringLiteral("Output"));
    writeFilterChain(w, s.output.filterChain);
    w.writeEndElement();

    w.writeEndElement();
}

// Loader helpers

VisualRef readVisual(QXmlStreamReader& r)
{
    VisualRef v;
    const auto a = r.attributes();
    v.type = enums::visualTypeFromString(a.value(QStringLiteral("type")).toString());
    if (v.type == VisualType::Media) {
        v.mediaId = QUuid::fromString(a.value(QStringLiteral("mediaId")).toString());
    } else if (v.type == VisualType::Generator) {
        v.generator = enums::generatorFromString(a.value(QStringLiteral("generator")).toString());
    }
    r.skipCurrentElement();
    return v;
}

CellProps readProps(QXmlStreamReader& r)
{
    CellProps p;
    const auto a = r.attributes();
    p.priority       = a.value(QStringLiteral("priority")).toInt();
    p.transparency   = a.value(QStringLiteral("transparency")).toDouble();
    if (a.hasAttribute(QStringLiteral("audioGain"))) {
        p.audioGain = a.value(QStringLiteral("audioGain")).toDouble();
    }
    p.movieSpeed     = a.value(QStringLiteral("movieSpeed")).toDouble();
    p.fade           = a.value(QStringLiteral("fade")).toDouble();
    p.maskType       = enums::maskTypeFromString(a.value(QStringLiteral("maskType")).toString());
    if (a.hasAttribute(QStringLiteral("matteRole"))) {
        p.matteRole = enums::layerMatteRoleFromString(
            a.value(QStringLiteral("matteRole")).toString(), LayerMatteRole::None);
    }
    if (a.hasAttribute(QStringLiteral("maskFeather"))) {
        p.maskFeather = a.value(QStringLiteral("maskFeather")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("maskRectWidth"))) {
        p.maskRectWidth = a.value(QStringLiteral("maskRectWidth")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("maskRectHeight"))) {
        p.maskRectHeight = a.value(QStringLiteral("maskRectHeight")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("maskRadius"))) {
        p.maskRadius = a.value(QStringLiteral("maskRadius")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("maskEllipseX"))) {
        p.maskEllipseX = a.value(QStringLiteral("maskEllipseX")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("maskEllipseY"))) {
        p.maskEllipseY = a.value(QStringLiteral("maskEllipseY")).toDouble();
    }
    p.maskWidth      = a.value(QStringLiteral("maskWidth")).toDouble();
    p.maskSmoothness = a.value(QStringLiteral("maskSmoothness")).toDouble();
    p.rotationZ      = a.value(QStringLiteral("rotationZ")).toDouble();
    p.copyMode       = enums::copyModeFromString(a.value(QStringLiteral("copyMode")).toString());
    if (a.hasAttribute(QStringLiteral("mixingPresetIndex"))) {
        p.mixingPresetIndex = a.value(QStringLiteral("mixingPresetIndex")).toInt();
    }
    if (a.hasAttribute(QStringLiteral("preferredLayer"))) {
        p.preferredLayer = qBound(0, a.value(QStringLiteral("preferredLayer")).toInt(), 12);
    } else if (a.hasAttribute(QStringLiteral("layerBand"))) {
        // Legacy migration: old bands Back/Mid/Front map to first slot of each 4-layer block.
        const int lb = a.value(QStringLiteral("layerBand")).toInt();
        if (lb == int(LayerBand::Back)) {
            p.preferredLayer = 0;
        } else if (lb == int(LayerBand::Front)) {
            p.preferredLayer = 8;
        } else {
            p.preferredLayer = 4;
        }
    }
    if (a.hasAttribute(QStringLiteral("keyChannelR"))) {
        p.keyChannelR = a.value(QStringLiteral("keyChannelR")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("keyChannelG"))) {
        p.keyChannelG = a.value(QStringLiteral("keyChannelG")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("keyChannelB"))) {
        p.keyChannelB = a.value(QStringLiteral("keyChannelB")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("keyingMode"))) {
        p.keyingMode = enums::keyingModeFromString(a.value(QStringLiteral("keyingMode")).toString(), KeyingMode::Luma);
    }
    if (a.hasAttribute(QStringLiteral("keyingEnabled"))) {
        const QString v = a.value(QStringLiteral("keyingEnabled")).toString().toLower();
        p.keyingEnabled = (v == QLatin1String("1") || v == QLatin1String("true") || v == QLatin1String("yes"));
    }
    if (a.hasAttribute(QStringLiteral("keyLumaCenter"))) {
        p.keyLumaCenter = a.value(QStringLiteral("keyLumaCenter")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("keyLumaInvert"))) {
        const QString v = a.value(QStringLiteral("keyLumaInvert")).toString().toLower();
        p.keyLumaInvert = (v == QLatin1String("1") || v == QLatin1String("true") || v == QLatin1String("yes"));
    }
    if (a.hasAttribute(QStringLiteral("keyThreshold"))) {
        p.keyThreshold = a.value(QStringLiteral("keyThreshold")).toDouble();
    } else if (a.hasAttribute(QStringLiteral("maskWidth"))) {
        // Legacy migration: key threshold used to share maskWidth.
        p.keyThreshold = a.value(QStringLiteral("maskWidth")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("keySoftness"))) {
        p.keySoftness = a.value(QStringLiteral("keySoftness")).toDouble();
    } else if (a.hasAttribute(QStringLiteral("maskSmoothness"))) {
        // Legacy migration: key softness used to share maskSmoothness.
        p.keySoftness = a.value(QStringLiteral("maskSmoothness")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("keyChromaHue"))) {
        p.keyChromaHue = a.value(QStringLiteral("keyChromaHue")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("keyChromaInvert"))) {
        const QString v = a.value(QStringLiteral("keyChromaInvert")).toString().toLower();
        p.keyChromaInvert = (v == QLatin1String("1") || v == QLatin1String("true") || v == QLatin1String("yes"));
    }
    if (a.hasAttribute(QStringLiteral("playMode"))) {
        const int pm = a.value(QStringLiteral("playMode")).toInt();
        if (pm >= 0 && pm <= 10) {
            p.playMode = static_cast<PlayMode>(pm);
        }
    }
    if (a.hasAttribute(QStringLiteral("clipPaused"))) {
        const QString v = a.value(QStringLiteral("clipPaused")).toString().toLower();
        p.clipPaused = (v == QLatin1String("1") || v == QLatin1String("true") || v == QLatin1String("yes"));
    }
    if (a.hasAttribute(QStringLiteral("segmentInU"))) {
        p.segmentInU = a.value(QStringLiteral("segmentInU")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("segmentOutU"))) {
        p.segmentOutU = a.value(QStringLiteral("segmentOutU")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("scratchHeadU"))) {
        p.scratchHeadU = a.value(QStringLiteral("scratchHeadU")).toDouble();
    }
    if (a.hasAttribute(QStringLiteral("overlayText"))) {
        p.overlayText = a.value(QStringLiteral("overlayText")).toString();
    }
    if (a.hasAttribute(QStringLiteral("tcStart"))) {
        p.tcStart = a.value(QStringLiteral("tcStart")).toString();
    }
    if (p.tcStart.isEmpty()) {
        p.tcStart = QStringLiteral("00:00:00:00");
    }

    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("Picture")) {
            const auto pictureAttrs = r.attributes();
            if (pictureAttrs.hasAttribute(QStringLiteral("zoom"))) {
                p.picture.zoom = pictureAttrs.value(QStringLiteral("zoom")).toDouble();
            }
            if (pictureAttrs.hasAttribute(QStringLiteral("rotationDeg"))) {
                p.picture.rotationDeg = pictureAttrs.value(QStringLiteral("rotationDeg")).toDouble();
            }
            if (pictureAttrs.hasAttribute(QStringLiteral("brightness"))) {
                p.picture.brightness = pictureAttrs.value(QStringLiteral("brightness")).toDouble();
            }
            if (pictureAttrs.hasAttribute(QStringLiteral("contrast"))) {
                p.picture.contrast = pictureAttrs.value(QStringLiteral("contrast")).toDouble();
            }
            if (pictureAttrs.hasAttribute(QStringLiteral("saturation"))) {
                p.picture.saturation = pictureAttrs.value(QStringLiteral("saturation")).toDouble();
            }
            if (pictureAttrs.hasAttribute(QStringLiteral("circularMotion"))) {
                p.picture.circularMotion = pictureAttrs.value(QStringLiteral("circularMotion")).toDouble();
            }
            if (pictureAttrs.hasAttribute(QStringLiteral("wrapMode"))) {
                p.picture.wrapMode = enums::wrapModeFromString(
                    pictureAttrs.value(QStringLiteral("wrapMode")).toString());
            }
            r.skipCurrentElement();
        } else if (r.name() == QLatin1String("Feedback")) {
            const auto fbAttrs = r.attributes();
            auto rd = [&](const char* k, double def) {
                return fbAttrs.hasAttribute(k) ? fbAttrs.value(k).toDouble() : def;
            };
            // Legacy retention/smear/liveInject ignored.
            p.feedback.inSaturation = rd("inSaturation", 1.0);
            p.feedback.inBrightness = rd("inBrightness", 0.0);
            p.feedback.inContrast   = rd("inContrast", 1.0);
            {
                const double raw = rd("inHueShift", 0.0);
                p.feedback.inHueShift = raw < 0.0 ? qBound(0.0, (raw + 1.0) * 0.5, 1.0)
                                                  : qBound(0.0, raw, 1.0);
            }
            p.feedback.inGamma      = rd("inGamma", 1.0);
            p.feedback.saturation  = rd("saturation", 1.0);
            p.feedback.brightness  = rd("brightness", 0.0);
            p.feedback.contrast    = rd("contrast", 1.0);
            {
                const double raw = rd("hueShift", 0.0);
                p.feedback.hueShift = raw < 0.0 ? qBound(0.0, (raw + 1.0) * 0.5, 1.0)
                                               : qBound(0.0, raw, 1.0);
            }
            p.feedback.gamma       = rd("gamma", 1.0);
            p.feedback.rotationDeg = rd("rotationDeg", 0.0);
            p.feedback.zoom = qBound(kFeedbackZoomMin, rd("zoom", 0.0), kFeedbackZoomMax);
            if (fbAttrs.hasAttribute(QStringLiteral("frameDelay"))) {
                p.feedback.frameDelay =
                    qBound(0, int(qRound(rd("frameDelay", 0.0))), kFeedbackMaxFrameDelay);
            }
            if (fbAttrs.hasAttribute(QStringLiteral("inputMode"))) {
                p.feedback.inputMode = enums::feedbackInputModeFromString(
                    fbAttrs.value(QStringLiteral("inputMode")).toString(),
                    FeedbackInputMode::StackComposite);
            } else {
                p.feedback.inputMode = FeedbackInputMode::StackComposite;
            }
            if (fbAttrs.hasAttribute(QStringLiteral("wrapMode"))) {
                p.feedback.wrapMode = enums::wrapModeFromString(
                    fbAttrs.value(QStringLiteral("wrapMode")).toString(), WrapMode::Black);
            }
            r.skipCurrentElement();
        } else {
            r.skipCurrentElement();
        }
    }

    return p;
}

Effect readEffect(QXmlStreamReader& r)
{
    Effect e;
    e.name = r.attributes().value(QStringLiteral("name")).toString();
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("Param")) {
            EffectParam p;
            p.name  = r.attributes().value(QStringLiteral("name")).toString();
            p.value = r.attributes().value(QStringLiteral("value")).toDouble();
            e.params.append(p);
            r.skipCurrentElement();
        } else {
            r.skipCurrentElement();
        }
    }
    return e;
}

PropertyMapping readMapping(QXmlStreamReader& r)
{
    PropertyMapping m;
    const auto a = r.attributes();
    m.property = a.value(QStringLiteral("property")).toString();
    m.input    = enums::inputTypeFromString(a.value(QStringLiteral("input")).toString());
    m.channel  = a.value(QStringLiteral("channel")).toInt();
    m.number   = a.value(QStringLiteral("number")).toInt();
    m.minValue = a.value(QStringLiteral("min")).toDouble();
    m.maxValue = a.value(QStringLiteral("max")).toDouble();
    {
        const QString resolved = PropertyRegistry::resolvePropertyId(m.property);
        PropertyRegistry::learnMinMax(resolved, &m.minValue, &m.maxValue);
    }
    if (a.hasAttribute(QStringLiteral("buttonMode"))) {
        m.buttonMode = enums::propertyButtonModeFromString(a.value(QStringLiteral("buttonMode")).toString());
    }
    if (a.hasAttribute(QStringLiteral("buttonValue"))) {
        m.buttonValue = a.value(QStringLiteral("buttonValue")).toDouble();
    }
    r.skipCurrentElement();
    return m;
}

CellFilterNode readFilterNode(QXmlStreamReader& r)
{
    CellFilterNode n;
    const auto a = r.attributes();
    n.id     = QUuid::fromString(a.value(QStringLiteral("id")).toString());
    n.typeId = a.value(QStringLiteral("type")).toString();
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("Param")) {
            EffectParam p;
            p.name  = r.attributes().value(QStringLiteral("name")).toString();
            p.value = r.attributes().value(QStringLiteral("value")).toDouble();
            n.params.append(p);
            r.skipCurrentElement();
        } else {
            r.skipCurrentElement();
        }
    }
    const auto defaults = defaultParamsFor(n.typeId);
    for (const auto& def : defaults) {
        bool found = false;
        for (auto& p : n.params) {
            if (p.name == def.name) {
                found = true;
                break;
            }
        }
        if (!found) {
            n.params.append(def);
        }
    }
    return n;
}

Cell readCell(QXmlStreamReader& r)
{
    Cell c;
    c.index = r.attributes().value(QStringLiteral("index")).toInt();
    c.name  = r.attributes().value(QStringLiteral("name")).toString();
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("Visual")) {
            c.visual = readVisual(r);
        } else if (r.name() == QLatin1String("Props")) {
            c.props = readProps(r);
        } else if (r.name() == QLatin1String("Effect")) {
            c.effect = readEffect(r);
        } else if (r.name() == QLatin1String("PropertyMappings")) {
            while (r.readNextStartElement()) {
                if (r.name() == QLatin1String("Mapping")) {
                    c.propertyMappings.append(readMapping(r));
                } else {
                    r.skipCurrentElement();
                }
            }
        } else if (r.name() == QLatin1String("FilterChain")) {
            while (r.readNextStartElement()) {
                if (r.name() == QLatin1String("FilterNode")) {
                    c.filterChain.append(readFilterNode(r));
                } else {
                    r.skipCurrentElement();
                }
            }
        } else {
            r.skipCurrentElement();
        }
    }
    sanitizeCellFilterChain(c.filterChain);
    return c;
}

Bank readBank(QXmlStreamReader& r)
{
    Bank b;
    b.index = r.attributes().value(QStringLiteral("index")).toInt();
    b.name  = r.attributes().value(QStringLiteral("name")).toString();
    // Pre-fill with 64 empty cells so sparse cells can be placed by index.
    b.cells.reserve(64);
    for (int i = 0; i < 64; ++i) {
        Cell c;
        c.index = i;
        b.cells.append(c);
    }
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("Cell")) {
            Cell c = readCell(r);
            if (c.index >= 0 && c.index < b.cells.size()) {
                b.cells[c.index] = c;
            } else {
                b.cells.append(c);
            }
        } else {
            r.skipCurrentElement();
        }
    }
    return b;
}

BankSet readBankSet(QXmlStreamReader& r)
{
    BankSet s;
    s.type = enums::bankSetTypeFromString(r.attributes().value(QStringLiteral("type")).toString());
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("Bank")) {
            s.banks.append(readBank(r));
        } else {
            r.skipCurrentElement();
        }
    }
    return s;
}

QList<MediaItem> readMedia(QXmlStreamReader& r)
{
    QList<MediaItem> out;
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("Item")) {
            MediaItem m;
            m.id   = QUuid::fromString(r.attributes().value(QStringLiteral("id")).toString());
            m.path = r.attributes().value(QStringLiteral("path")).toString();
            m.displayName = r.attributes().value(QStringLiteral("name")).toString();
            out.append(m);
            r.skipCurrentElement();
        } else {
            r.skipCurrentElement();
        }
    }
    return out;
}

QList<TriggerMapping> readTriggers(QXmlStreamReader& r)
{
    QList<TriggerMapping> out;
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("Trigger")) {
            TriggerMapping t;
            const auto a = r.attributes();
            t.input        = enums::inputTypeFromString(a.value(QStringLiteral("input")).toString());
            t.channel      = a.value(QStringLiteral("channel")).toInt();
            t.number       = a.value(QStringLiteral("number")).toInt();
            t.keyText      = a.value(QStringLiteral("keyText")).toString();
            t.target       = enums::triggerTargetFromString(a.value(QStringLiteral("target")).toString());
            t.bankSetIndex = a.value(QStringLiteral("bankSetIndex")).toInt();
            t.bankIndex    = a.value(QStringLiteral("bankIndex")).toInt();
            t.cellIndex    = a.value(QStringLiteral("cellIndex")).toInt();
            t.propertyName = a.value(QStringLiteral("property")).toString();
            out.append(t);
            r.skipCurrentElement();
        } else {
            r.skipCurrentElement();
        }
    }
    return out;
}

Settings readSettings(QXmlStreamReader& r)
{
    Settings s;
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("Audio")) {
            const auto a = r.attributes();
            s.audio.driver       = a.value(QStringLiteral("driver")).toString();
            s.audio.inputDevice  = a.value(QStringLiteral("inputDevice")).toString();
            s.audio.outputDevice = a.value(QStringLiteral("outputDevice")).toString();
            s.audio.bufferSize   = a.value(QStringLiteral("bufferSize")).toInt();
            s.audio.sampleRate   = a.value(QStringLiteral("sampleRate")).toInt();
            r.skipCurrentElement();
        } else if (r.name() == QLatin1String("UI")) {
            const auto a = r.attributes();
            s.ui.mediaLibraryVisible  = a.value(QStringLiteral("mediaLibraryVisible"))  == QLatin1String("true");
            s.ui.bankPanelVisible     = a.value(QStringLiteral("bankPanelVisible"))     == QLatin1String("true");
            s.ui.parameterTabsVisible = a.value(QStringLiteral("parameterTabsVisible")) == QLatin1String("true");
            s.ui.mediaLibraryWidth    = a.value(QStringLiteral("mediaLibraryWidth")).toInt();
            s.ui.bankPanelHeight      = a.value(QStringLiteral("bankPanelHeight")).toInt();
            r.skipCurrentElement();
        } else if (r.name() == QLatin1String("Matrix")) {
            const auto a = r.attributes();
            s.matrix.width  = a.value(QStringLiteral("width")).toInt();
            s.matrix.height = a.value(QStringLiteral("height")).toInt();
            s.matrix.gridRows = a.value(QStringLiteral("gridRows")).toInt();
            s.matrix.gridCols = a.value(QStringLiteral("gridCols")).toInt();
            if (s.matrix.width <= 0) {
                s.matrix.width = 1920;
            }
            if (s.matrix.height <= 0) {
                s.matrix.height = 1080;
            }
            if (s.matrix.gridRows <= 0) {
                s.matrix.gridRows = 4;
            }
            if (s.matrix.gridCols <= 0) {
                s.matrix.gridCols = 12;
            }
            r.skipCurrentElement();
        } else if (r.name() == QLatin1String("Output")) {
            while (r.readNextStartElement()) {
                if (r.name() == QLatin1String("FilterChain")) {
                    while (r.readNextStartElement()) {
                        if (r.name() == QLatin1String("FilterNode")) {
                            s.output.filterChain.append(readFilterNode(r));
                        } else {
                            r.skipCurrentElement();
                        }
                    }
                } else {
                    r.skipCurrentElement();
                }
            }
        } else {
            r.skipCurrentElement();
        }
    }
    sanitizeOutputFilterChain(s.output.filterChain);
    return s;
}

} // namespace

PvjSerializer::Result PvjSerializer::save(const Project& project, const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {false, QStringLiteral("Cannot open file for writing: %1").arg(filePath)};
    }

    Project out = project;
    out.ensureSingleBankSet();

    QXmlStreamWriter w(&file);
    w.setAutoFormatting(true);
    w.setAutoFormattingIndent(2);
    w.writeStartDocument();

    w.writeStartElement(QString::fromLatin1(kRootElement));
    w.writeAttribute(QStringLiteral("version"), QString::fromLatin1(kFormatVersion));

    writeSettings(w, out.settings);
    writeMedia(w, out.mediaLibrary);
    for (const auto& set : out.bankSets) {
        writeBankSet(w, set);
    }
    writeTriggers(w, out.triggerMappings);

    w.writeEndElement();  // PerformanieVJ
    w.writeEndDocument();

    if (w.hasError()) {
        return {false, QStringLiteral("XML writer reported an error")};
    }
    return {true, {}};
}

PvjSerializer::Result PvjSerializer::load(Project& project, const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {false, QStringLiteral("Cannot open file for reading: %1").arg(filePath)};
    }

    project = Project();
    project.filePath = filePath;
    project.sourceFormat = QStringLiteral("pvj");

    QXmlStreamReader r(&file);
    if (!r.readNextStartElement() || r.name() != QLatin1String(kRootElement)) {
        return {false, QStringLiteral("Not a PerformanieVJ project file")};
    }
    project.formatVersion = r.attributes().value(QStringLiteral("version")).toString();

    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("Settings")) {
            project.settings = readSettings(r);
        } else if (r.name() == QLatin1String("MediaLibrary")) {
            project.mediaLibrary = readMedia(r);
        } else if (r.name() == QLatin1String("BankSet")) {
            project.bankSets.append(readBankSet(r));
        } else if (r.name() == QLatin1String("TriggerMappings")) {
            project.triggerMappings = readTriggers(r);
        } else {
            r.skipCurrentElement();
        }
    }

    if (r.hasError()) {
        return {false, r.errorString()};
    }
    project.ensureSingleBankSet();
    if (project.bankSets.isEmpty()) {
        project.initializeDefault();
    } else {
        project.resizeBanksForGrid(project.settings.matrix.gridRows, project.settings.matrix.gridCols);
    }
    project.stripMaxineFiltersFromCells();
    project.sanitizeOutputFilters();
    project.normalizeCellSlotTriggers();
    return {true, {}};
}

} // namespace pvj::core

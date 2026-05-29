#include "Vj2Importer.h"

#include "EnumStrings.h"
#include "FilterEffectIds.h"
#include "FilterParamSchema.h"
#include "PropertyRegistry.h"

#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QXmlStreamReader>

#include <array>

namespace pvj::core {

namespace {

QString normalizePath(const QString& path)
{
    QString out = path;
    out.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return out;
}

QString normalizeTokenKey(QString s)
{
    s = s.toLower();
    s.remove(QLatin1Char(' '));
    s.remove(QLatin1Char('_'));
    s.remove(QLatin1Char('-'));
    return s;
}

CopyMode mapCopyMode(const QString& s)
{
    QString key = s.trimmed();
    const QString norm = normalizeTokenKey(key);
    if (norm == QLatin1String("addition")) {
        key = QStringLiteral("add");
    } else if (norm == QLatin1String("substraction") || norm == QLatin1String("subtraction")) {
        key = QStringLiteral("subtract");
    } else if (norm == QLatin1String("pinlight")) {
        key = QStringLiteral("pinlight");
    }
    return enums::copyModeFromString(key, CopyMode::Normal);
}

MaskType mapMaskTypeValue(const QString& s)
{
    bool ok = false;
    const int asInt = int(s.toDouble(&ok));
    if (ok) {
        switch (asInt) {
        case 0: return MaskType::None;
        case 1: return MaskType::Rectangle;
        case 2: return MaskType::Circle;
        case 3: return MaskType::SoftEdge;
        case 4: return MaskType::Ellipse;
        case 5: return MaskType::Custom;
        default: break;
        }
    }
    return enums::maskTypeFromString(s, MaskType::None);
}

GeneratorKind mapGrandVjGenerator(const QString& raw)
{
    QString name = raw.trimmed();
    if (name.startsWith(QLatin1String("#generator:"), Qt::CaseInsensitive)) {
        name = name.mid(11).trimmed();
    }
    const QString k = normalizeTokenKey(name);
    if (k.contains(QLatin1String("spout"))) {
        return GeneratorKind::InputSpout;
    }
    if (k.contains(QLatin1String("syphon"))) {
        return GeneratorKind::InputSyphon;
    }
    if (k.contains(QLatin1String("ndi"))) {
        return GeneratorKind::InputNdi;
    }
    if (k.contains(QLatin1String("feedback"))) {
        return GeneratorKind::InternalFeedback;
    }
    if (k.contains(QLatin1String("testpattern")) || k == QLatin1String("test")) {
        return GeneratorKind::TestPattern;
    }
    return enums::generatorFromString(name, GeneratorKind::None);
}

QString mapGrandVjEffectTypeId(const QString& name)
{
    static const QHash<QString, QString> kMap = [] {
        QHash<QString, QString> m;
        const auto add = [&](const char* gvj, const char* typeId) {
            m.insert(normalizeTokenKey(QString::fromLatin1(gvj)), QString::fromLatin1(typeId));
        };
        add("Checker", "checkerboard");
        add("Color Correction", "color_correction");
        add("Color Distortion", "distortion");
        add("Contrast", "contrast");
        add("Cropping", "crop");
        add("Cylindrical Correction", "cylinder");
        add("Invert", "invert");
        add("Irisation", "vignette");
        add("Kaleido", "kaleido");
        add("Larsen", "trails");
        add("Mirror", "mirror");
        add("Plane", "transform");
        add("Plane Rotation", "rotate");
        add("Ripple", "ripple");
        add("RotoZoom", "zoom");
        add("Scroller", "shift");
        add("Split Scrolling", "shift");
        add("Tiling", "tile");
        add("Twirl Feedback", "twirl");
        add("Twirl", "twirl");
        return m;
    }();
    return kMap.value(normalizeTokenKey(name));
}

bool grandVjEffectUsesFeedback(const QString& name)
{
    const QString k = normalizeTokenKey(name);
    return k.contains(QLatin1String("feedback")) || k == QLatin1String("larsen");
}

const QHash<QString, QString>& grandVjCellTargetMap()
{
    static const QHash<QString, QString> kMap = [] {
        QHash<QString, QString> m;
        const auto add = [&](const char* gvjToken, const char* property) {
            m.insert(normalizeTokenKey(QString::fromLatin1(gvjToken)),
                     QString::fromLatin1(property));
        };
        add("TRSP", "transparency");
        add("OPAC", "transparency");
        add("OPACITY", "transparency");
        add("TRANSPARENCY", "transparency");
        add("FADE", "fade");
        add("MSPD", "movieSpeed");
        add("SPED", "movieSpeed");
        add("MOVIESPEED", "movieSpeed");
        add("VOLM", "audioGain");
        add("VOL", "audioGain");
        add("AGAIN", "audioGain");
        add("AUDIOGAIN", "audioGain");
        add("ROTZ", "rotationZ");
        add("PRIO", "priority");
        add("PRIORITY", "priority");
        // GrandVJ MASKW = keying mask threshold (not mask rectangle width)
        add("MSKW", "keyThreshold");
        add("MASKW", "keyThreshold");
        // GrandVJ SIZX/SIZY = clip size -> single pictureZoom in our model
        add("SIZX", "pictureZoom");
        add("SIZY", "pictureZoom");
        add("MSKH", "maskRectHeight");
        add("MRAD", "maskRadius");
        add("MELX", "maskEllipseX");
        add("MELY", "maskEllipseY");
        add("MFTH", "maskFeather");
        add("MASKTYPE", "maskType");
        add("MTYP", "maskType");
        add("MSKT", "maskType");
        add("CMOD", "copyMode");
        add("CPMD", "copyMode");
        add("COPYMODE", "copyMode");
        add("BRGT", "pictureBrightness");
        add("BRIGHT", "pictureBrightness");
        add("BRIGHTNESS", "pictureBrightness");
        add("CONT", "pictureContrast");
        add("CTRT", "pictureContrast");
        add("CONTRAST", "pictureContrast");
        add("SATU", "pictureSaturation");
        add("SATURATION", "pictureSaturation");
        add("PZOM", "pictureZoom");
        add("ZOOM", "pictureZoom");
        add("PROT", "pictureRotationDeg");
        add("PICTUREROTATION", "pictureRotationDeg");
        add("CMOT", "pictureCircularMotion");
        add("PWRP", "pictureWrapMode");
        add("KEYR", "keyChannelR");
        add("KEYG", "keyChannelG");
        add("KEYB", "keyChannelB");
        add("KEYC", "keyLumaCenter");
        add("KEYT", "keyThreshold");
        add("KEYS", "keySoftness");
        add("KEYH", "keyChromaHue");
        add("KEYE", "keyingEnabled");
        add("KEYM", "keyingMode");
        add("KLIN", "keyLumaInvert");
        add("KCIN", "keyChromaInvert");
        add("SEGI", "segmentInU");
        add("SEGO", "segmentOutU");
        add("SCRH", "scratchHeadU");
        add("SCRATCH", "scratchHeadU");
        add("PLBM", "playMode");
        add("PLAYMODE", "playMode");
        add("PAUS", "clipPaused");
        add("PAUSED", "clipPaused");
        add("FBLR", "feedbackLoopRetention");
        add("FBLI", "feedbackLiveInject");
        add("FBSA", "feedbackSaturation");
        add("FBBR", "feedbackBrightness");
        add("FBCO", "feedbackContrast");
        add("FBHU", "feedbackHueShift");
        add("FBGA", "feedbackGamma");
        add("FBRT", "feedbackRotationDeg");
        add("FBZM", "feedbackZoom");
        add("FBFD", "feedbackFrameDelay");
        add("FBIM", "feedbackInputMode");
        add("FBWP", "feedbackWrapMode");
        add("MIXP", "mixingPresetIndex");
        add("MATTE", "matteRole");
        return m;
    }();
    return kMap;
}

void applyPropertyMappingMinMax(PropertyMapping& m)
{
    double minV = 0.0;
    double maxV = 1.0;
    PropertyRegistry::learnMinMax(m.property, &minV, &maxV);
    m.minValue = minV;
    m.maxValue = maxV;
}

QString mapGrandVjPropertyTarget(const QString& target)
{
    QString t = target.trimmed();
    if (t.isEmpty()) {
        return {};
    }

    if (t.startsWith(QLatin1Char('/'))) {
        const QString upper = t.toUpper();
        if (upper == QLatin1String("/MATRIXBANK/NEXT")) {
            return QStringLiteral("__trigger_bankNext");
        }
        if (upper == QLatin1String("/MATRIXBANK/PREVIOUS")) {
            return QStringLiteral("__trigger_bankPrev");
        }
        t = t.section(QLatin1Char('/'), -1);
    }

    const QString upper = t.toUpper();
    // FXP* filter-parameter MIDI bindings are intentionally not imported.
    if (upper.startsWith(QLatin1String("FXP"))) {
        return {};
    }

    const QString key = normalizeTokenKey(t);
    const auto it = grandVjCellTargetMap().find(key);
    if (it != grandVjCellTargetMap().end()) {
        return it.value();
    }

    for (const QString& name : PropertyRegistry::allPropertyNames()) {
        if (normalizeTokenKey(name) == key) {
            return name;
        }
    }
    return {};
}

struct LibEntry {
    QUuid id;
    QString path;
};

using LibMap = QHash<QString, LibEntry>;

QUuid mediaIdFor(LibMap& map, QList<MediaItem>& library, const QString& rawPath)
{
    const QString key = normalizePath(rawPath);
    if (key.isEmpty()) {
        return {};
    }
    auto it = map.find(key);
    if (it != map.end()) {
        return it->id;
    }
    LibEntry e;
    e.id   = QUuid::createUuid();
    e.path = key;
    map.insert(key, e);
    MediaItem m;
    m.id = e.id;
    m.path = key;
    m.displayName = QFileInfo(key).fileName();
    library.append(m);
    return e.id;
}

/// GrandVJ stores MIDI channels 1..16 in project files; PerformanieVJ uses 0..15 (MIDI status).
int grandVjMidiChannelToZeroBased(int grandVjChannel)
{
    if (grandVjChannel >= 1 && grandVjChannel <= 16) {
        return grandVjChannel - 1;
    }
    return qBound(0, grandVjChannel, 15);
}

bool parseGrandVjChannel(const QString& channel, InputType& input, int& midiChannel, int& number)
{
    const QStringList parts = channel.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return false;
    }
    const QString kind = parts[0].toLower();
    if (kind == QLatin1String("midi")) {
        // midi:device:channel:cc:number  (GrandVJ 2.7)
        if (parts.size() >= 5) {
            const QString msg = parts[3].toLower();
            if (msg == QLatin1String("cc")) {
                input = InputType::MidiCC;
                midiChannel = grandVjMidiChannelToZeroBased(parts[2].toInt());
                number = parts[4].toInt();
                return true;
            }
            if (msg == QLatin1String("note")) {
                input = InputType::MidiNote;
                midiChannel = grandVjMidiChannelToZeroBased(parts[2].toInt());
                number = parts[4].toInt();
                return true;
            }
        }
        // midi:channel:cc:number
        if (parts.size() >= 4) {
            const QString msg = parts[2].toLower();
            if (msg == QLatin1String("cc")) {
                input = InputType::MidiCC;
                midiChannel = grandVjMidiChannelToZeroBased(parts[1].toInt());
                number = parts[3].toInt();
                return true;
            }
            if (msg == QLatin1String("note")) {
                input = InputType::MidiNote;
                midiChannel = grandVjMidiChannelToZeroBased(parts[1].toInt());
                number = parts[3].toInt();
                return true;
            }
        }
        return false;
    }
    if (kind == QLatin1String("keyboard") || kind == QLatin1String("key")) {
        input = InputType::Key;
        midiChannel = parts.size() > 1 ? parts[1].toInt() : 0;
        number = parts.size() > 2 ? parts[2].toInt() : 0;
        return true;
    }
    if (kind == QLatin1String("osc")) {
        input = InputType::Osc;
        midiChannel = parts.size() > 1 ? parts[1].toInt() : 0;
        number = parts.size() > 2 ? parts[2].toInt() : 0;
        return true;
    }
    return false;
}

QString gvj27ValueAttr(const QXmlStreamAttributes& attrs)
{
    if (attrs.hasAttribute(QLatin1String("VALUE"))) {
        return attrs.value(QLatin1String("VALUE")).toString();
    }
    return attrs.value(QLatin1String("TARGET")).toString();
}

void readPropertyMappingList(QXmlStreamReader& r, Cell& cell, bool legacyAttrs, QStringList& warnings)
{
    while (r.readNextStartElement()) {
        if (r.name() != QLatin1String("PROPERTYMAPPING") && r.name() != QLatin1String("MAPPING")) {
            r.skipCurrentElement();
            continue;
        }

        QString target;
        QString channelStr;
        QString rawProperty;
        QString inputKind;
        QString legacyChannelAttr;
        QString legacyNumberAttr;
        QString legacyMinAttr;
        QString legacyMaxAttr;
        bool hasLegacyMin = false;
        bool hasLegacyMax = false;

        const auto a = r.attributes();
        target           = a.value(QLatin1String("TARGET")).toString();
        channelStr       = a.value(QLatin1String("CHANNEL")).toString();
        if (channelStr.isEmpty()) {
            channelStr = a.value(QLatin1String("MAPPING")).toString();
        }
        rawProperty      = a.value(QLatin1String("PROPERTY")).toString();
        inputKind        = a.value(QLatin1String("INPUT")).toString();
        legacyChannelAttr = a.value(QLatin1String("CHANNEL")).toString();
        legacyNumberAttr  = a.value(QLatin1String("NUMBER")).toString();
        hasLegacyMin     = a.hasAttribute(QLatin1String("MIN"));
        hasLegacyMax     = a.hasAttribute(QLatin1String("MAX"));
        legacyMinAttr    = a.value(QLatin1String("MIN")).toString();
        legacyMaxAttr    = a.value(QLatin1String("MAX")).toString();

        while (r.readNextStartElement()) {
            const auto tag = r.name();
            const QString val = gvj27ValueAttr(r.attributes());
            if (tag == QLatin1String("TARGET")) {
                target = val;
            } else if (tag == QLatin1String("CHANNEL") || tag == QLatin1String("MAPPING")) {
                channelStr = val;
            } else if (tag == QLatin1String("PROPERTY")) {
                rawProperty = val;
            } else if (tag == QLatin1String("INPUT")) {
                inputKind = val;
            } else if (tag == QLatin1String("NUMBER")) {
                legacyNumberAttr = val;
            } else if (tag == QLatin1String("MIN")) {
                hasLegacyMin = true;
                legacyMinAttr = val;
            } else if (tag == QLatin1String("MAX")) {
                hasLegacyMax = true;
                legacyMaxAttr = val;
            }
            r.skipCurrentElement();
        }

        PropertyMapping m;
        const QString channelLabel = legacyAttrs
            ? QStringLiteral("ch%1#%2").arg(legacyChannelAttr, legacyNumberAttr)
            : channelStr;

        if (legacyAttrs) {
            m.property = mapGrandVjPropertyTarget(rawProperty);
            if (m.property.isEmpty()) {
                warnings.append(QStringLiteral("Unknown GrandVJ property target \"%1\" - "
                                                "mapping dropped (channel: %2)")
                                    .arg(rawProperty, channelLabel));
                continue;
            }
            if (m.property == QLatin1String("__trigger_bankNext")
                || m.property == QLatin1String("__trigger_bankPrev")) {
                continue;
            }
            const QString inp = inputKind.toLower();
            if (inp.contains(QLatin1String("note"))) {
                m.input = InputType::MidiNote;
            } else if (inp.contains(QLatin1String("cc"))) {
                m.input = InputType::MidiCC;
            } else if (inp.contains(QLatin1String("key"))) {
                m.input = InputType::Key;
            } else if (inp.contains(QLatin1String("osc"))) {
                m.input = InputType::Osc;
            }
            InputType parsedInput = InputType::None;
            int ch = 0;
            int num = 0;
            if (legacyChannelAttr.contains(QLatin1Char(':'))
                && parseGrandVjChannel(legacyChannelAttr, parsedInput, ch, num)) {
                m.input = parsedInput;
                m.channel = ch;
                m.number = num;
            } else {
                m.channel = grandVjMidiChannelToZeroBased(legacyChannelAttr.toInt());
                m.number  = legacyNumberAttr.toInt();
            }
            if (hasLegacyMin && hasLegacyMax) {
                m.minValue = legacyMinAttr.toDouble();
                m.maxValue = legacyMaxAttr.toDouble();
            } else {
                applyPropertyMappingMinMax(m);
            }
        } else {
            m.property = mapGrandVjPropertyTarget(target);
            if (m.property.isEmpty()) {
                warnings.append(QStringLiteral("Unknown GrandVJ property target \"%1\" - "
                                                "mapping dropped (channel: %2)")
                                    .arg(target, channelLabel));
                continue;
            }
            if (m.property == QLatin1String("__trigger_bankNext")
                || m.property == QLatin1String("__trigger_bankPrev")) {
                continue;
            }
            InputType parsedInput = InputType::None;
            int ch = 0;
            int num = 0;
            if (!parseGrandVjChannel(channelStr, parsedInput, ch, num)) {
                warnings.append(QStringLiteral("Unparsed GrandVJ channel \"%1\"")
                                    .arg(channelStr));
                continue;
            }
            m.input = parsedInput;
            m.channel = ch;
            m.number = num;
            applyPropertyMappingMinMax(m);
        }
        cell.propertyMappings.append(m);
    }
}

void applyEffectToCell(const QString& effectName, const std::array<double, 4>& fxParams, Cell& cell,
                       QStringList& warnings)
{
    if (effectName.isEmpty()) {
        return;
    }
    const QString typeId = mapGrandVjEffectTypeId(effectName);
    if (typeId.isEmpty()) {
        warnings.append(QStringLiteral("Unknown GrandVJ effect \"%1\", ignored").arg(effectName));
        return;
    }
    if (grandVjEffectUsesFeedback(effectName)) {
        CellFilterNode marker;
        marker.typeId = feedbackMarkerTypeId();
        cell.filterChain.append(marker);
    }
    CellFilterNode node;
    node.typeId = typeId;
    node.params = defaultParamsFor(typeId);
    for (int i = 0; i < 4 && i < node.params.size(); ++i) {
        node.params[i].value = fxParams[static_cast<std::size_t>(i)];
    }
    cell.filterChain.append(node);

    Effect legacy;
    legacy.name = effectName;
    for (int i = 0; i < 4; ++i) {
        if (i < node.params.size()) {
            EffectParam p;
            p.name = node.params.at(i).name;
            p.value = node.params.at(i).value;
            legacy.params.append(p);
        }
    }
    cell.effect = legacy;
}

void readCellLegacy(QXmlStreamReader& r, Bank& bank, LibMap& libMap, QList<MediaItem>& library,
                    QStringList& warnings)
{
    Cell cell;
    const auto attr = r.attributes();
    cell.index = attr.value(QStringLiteral("INDEX")).toInt();

    auto getD = [&](QLatin1String n, double def) {
        const auto v = attr.value(n);
        return v.isEmpty() ? def : v.toDouble();
    };
    cell.props.priority       = attr.value(QLatin1String("PRIORITY")).toInt();
    cell.props.transparency   = getD(QLatin1String("TRANSPARENCY"), 1.0);
    cell.props.movieSpeed     = getD(QLatin1String("MOVIESPEED"), 1.0);
    cell.props.fade           = getD(QLatin1String("FADE"), 0.0);
    cell.props.maskWidth      = getD(QLatin1String("MASKWIDTH"), 0.0);
    cell.props.maskSmoothness = getD(QLatin1String("MASKSMOOTHNESS"), 0.0);
    cell.props.rotationZ      = getD(QLatin1String("ROTATIONZ"), 0.0);
    cell.props.maskType       = mapMaskTypeValue(attr.value(QLatin1String("MASKTYPE")).toString());
    cell.props.copyMode       = mapCopyMode(attr.value(QLatin1String("COPYMODE")).toString());

    const QString visualPath = attr.value(QLatin1String("VISUAL")).toString();
    const QString generator  = attr.value(QLatin1String("GENERATOR")).toString();
    if (!visualPath.isEmpty()) {
        if (visualPath.startsWith(QLatin1String("#generator:"), Qt::CaseInsensitive)) {
            cell.visual.type = VisualType::Generator;
            cell.visual.generator = mapGrandVjGenerator(visualPath);
        } else {
            cell.visual.type = VisualType::Media;
            cell.visual.mediaId = mediaIdFor(libMap, library, visualPath);
        }
    } else if (!generator.isEmpty()) {
        cell.visual.type = VisualType::Generator;
        cell.visual.generator = mapGrandVjGenerator(generator);
    }
    if (cell.visual.type == VisualType::Generator && cell.visual.generator == GeneratorKind::None) {
        warnings.append(QStringLiteral("Unknown GrandVJ generator \"%1\"").arg(generator.isEmpty() ? visualPath : generator));
    }

    std::array<double, 4> fxParams { 0.0, 0.0, 0.0, 0.0 };
    QString effectName;
    bool effectApplied = false;

    while (r.readNextStartElement()) {
        const auto tag = r.name();
        if (tag == QLatin1String("EFFECT")) {
            effectName = r.attributes().value(QStringLiteral("NAME")).toString();
            if (effectName.isEmpty()) {
                effectName = r.attributes().value(QStringLiteral("VALUE")).toString();
            }
            for (int i = 0; i < 4; ++i) {
                const QString key = QStringLiteral("PARAM%1").arg(i);
                const auto v = r.attributes().value(key);
                if (!v.isEmpty()) {
                    fxParams[static_cast<std::size_t>(i)] = v.toDouble();
                }
            }
            r.skipCurrentElement();
        } else if (tag == QLatin1String("PROPERTYMAPPINGLIST")) {
            if (!effectApplied) {
                applyEffectToCell(effectName, fxParams, cell, warnings);
                effectApplied = true;
            }
            readPropertyMappingList(r, cell, true, warnings);
        } else {
            r.skipCurrentElement();
        }
    }

    if (!effectApplied) {
        applyEffectToCell(effectName, fxParams, cell, warnings);
    }

    if (cell.index >= 0 && cell.index < bank.cells.size()) {
        bank.cells[cell.index] = cell;
    } else {
        bank.cells.append(cell);
    }
}

void readCellGvj27(QXmlStreamReader& r, int cellIndex, Bank& bank, LibMap& libMap,
                   QList<MediaItem>& library, QStringList& warnings)
{
    Cell cell;
    cell.index = cellIndex;
    std::array<double, 4> fxParams { 0.0, 0.0, 0.0, 0.0 };
    QString effectName;
    bool effectApplied = false;

    while (r.readNextStartElement()) {
        const auto tag = r.name();
        const QString val = r.attributes().value(QLatin1String("VALUE")).toString();
        if (tag == QLatin1String("VISUAL")) {
            if (val.startsWith(QLatin1String("#generator:"), Qt::CaseInsensitive)) {
                cell.visual.type = VisualType::Generator;
                cell.visual.generator = mapGrandVjGenerator(val);
                if (cell.visual.generator == GeneratorKind::None) {
                    warnings.append(QStringLiteral("Unknown GrandVJ generator \"%1\"").arg(val));
                }
            } else if (!val.isEmpty()) {
                cell.visual.type = VisualType::Media;
                cell.visual.mediaId = mediaIdFor(libMap, library, val);
            }
        } else if (tag == QLatin1String("EFFECT")) {
            effectName = val;
        } else if (tag == QLatin1String("FXPARAM1")) {
            fxParams[0] = val.toDouble();
        } else if (tag == QLatin1String("FXPARAM2")) {
            fxParams[1] = val.toDouble();
        } else if (tag == QLatin1String("FXPARAM3")) {
            fxParams[2] = val.toDouble();
        } else if (tag == QLatin1String("FXPARAM4")) {
            fxParams[3] = val.toDouble();
        } else if (tag == QLatin1String("PRIORITY")) {
            cell.props.priority = int(val.toDouble());
        } else if (tag == QLatin1String("TRANSPARENCY")) {
            cell.props.transparency = val.toDouble();
        } else if (tag == QLatin1String("MOVIESPEED")) {
            cell.props.movieSpeed = val.toDouble();
        } else if (tag == QLatin1String("FADE")) {
            cell.props.fade = val.toDouble();
        } else if (tag == QLatin1String("MASKTYPE")) {
            cell.props.maskType = mapMaskTypeValue(val);
        } else if (tag == QLatin1String("MASKWIDTH")) {
            cell.props.maskWidth = val.toDouble();
            cell.props.maskRectWidth = val.toDouble();
        } else if (tag == QLatin1String("MASKSMOOTH") || tag == QLatin1String("MASKSMOOTHNESS")) {
            cell.props.maskSmoothness = val.toDouble();
            cell.props.maskFeather = val.toDouble();
        } else if (tag == QLatin1String("ROTATIONZ")) {
            cell.props.rotationZ = val.toDouble();
        } else if (tag == QLatin1String("COPYMODE")) {
            cell.props.copyMode = mapCopyMode(val);
        } else if (tag == QLatin1String("PROPERTYMAPPINGLIST")) {
            if (!effectApplied) {
                applyEffectToCell(effectName, fxParams, cell, warnings);
                effectApplied = true;
            }
            readPropertyMappingList(r, cell, false, warnings);
            continue;
        } else if (tag == QLatin1String("TRIGGERED") || tag == QLatin1String("MOVIELOOPLENGTH")) {
            // playback flags — no direct field yet
        }
        r.skipCurrentElement();
    }

    if (!effectApplied) {
        applyEffectToCell(effectName, fxParams, cell, warnings);
    }

    if (cellIndex >= 0 && cellIndex < bank.cells.size()) {
        bank.cells[cellIndex] = cell;
    } else if (cellIndex == bank.cells.size()) {
        bank.cells.append(cell);
    }
}

void readCell(QXmlStreamReader& r, int cellIndex, Bank& bank, LibMap& libMap, QList<MediaItem>& library,
              QStringList& warnings)
{
    const auto attr = r.attributes();
    const bool legacy = attr.hasAttribute(QLatin1String("INDEX"))
        || attr.hasAttribute(QLatin1String("PRIORITY"))
        || attr.hasAttribute(QLatin1String("TRANSPARENCY"))
        || attr.hasAttribute(QLatin1String("VISUAL"));
    if (legacy) {
        readCellLegacy(r, bank, libMap, library, warnings);
    } else {
        readCellGvj27(r, cellIndex, bank, libMap, library, warnings);
    }
}

Bank readBank(QXmlStreamReader& r, int bankIndex, LibMap& libMap, QList<MediaItem>& library,
              QStringList& warnings, int cellsPerBank)
{
    Bank bank;
    const auto attr = r.attributes();
    bank.index = attr.hasAttribute(QLatin1String("INDEX")) ? attr.value(QLatin1String("INDEX")).toInt()
                                                          : bankIndex;
    bank.name = attr.value(QLatin1String("NAME")).toString();
    if (bank.name.isEmpty()) {
        bank.name = QStringLiteral("Bank %1").arg(bank.index + 1);
    }
    bank.cells.clear();
    bank.cells.reserve(cellsPerBank);
    for (int i = 0; i < cellsPerBank; ++i) {
        Cell c;
        c.index = i;
        bank.cells.append(c);
    }

    int cellIndex = 0;
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("CELL")) {
            readCell(r, cellIndex, bank, libMap, library, warnings);
            ++cellIndex;
        } else if (r.name() == QLatin1String("CELLS")) {
            int nestedIndex = 0;
            while (r.readNextStartElement()) {
                if (r.name() == QLatin1String("CELL")) {
                    readCell(r, nestedIndex, bank, libMap, library, warnings);
                    ++nestedIndex;
                } else {
                    r.skipCurrentElement();
                }
            }
        } else if (r.name() == QLatin1String("NAME")) {
            const QString n = r.attributes().value(QLatin1String("VALUE")).toString();
            if (!n.isEmpty()) {
                bank.name = n;
            }
            r.skipCurrentElement();
        } else {
            r.skipCurrentElement();
        }
    }
    return bank;
}

BankSet readBankSet(QXmlStreamReader& r, LibMap& libMap, QList<MediaItem>& library,
                    QStringList& warnings, int cellsPerBank)
{
    BankSet set;
    const int typeValue = r.attributes().value(QLatin1String("TYPE")).toInt();
    set.type = (typeValue == 1) ? BankSetType::TypeB : BankSetType::TypeA;
    int bankIndex = 0;
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("BANK")) {
            set.banks.append(readBank(r, bankIndex, libMap, library, warnings, cellsPerBank));
            ++bankIndex;
        } else {
            r.skipCurrentElement();
        }
    }
    return set;
}

void readMediaLibrary(QXmlStreamReader& r, LibMap& libMap, QList<MediaItem>& library)
{
    while (r.readNextStartElement()) {
        const auto tag = r.name();
        if (tag == QLatin1String("ITEM") || tag == QLatin1String("MEDIA")) {
            mediaIdFor(libMap, library, r.attributes().value(QLatin1String("PATH")).toString());
            r.skipCurrentElement();
        } else if (tag == QLatin1String("FILE")) {
            mediaIdFor(libMap, library, r.attributes().value(QLatin1String("PATH")).toString());
            r.skipCurrentElement();
        } else {
            r.skipCurrentElement();
        }
    }
}

void readDataMatrix(QXmlStreamReader& r, Project& project)
{
    const auto a = r.attributes();
    int cols = a.value(QLatin1String("MATRIXWIDTH")).toInt();
    int rows = a.value(QLatin1String("MATRIXHEIGHT")).toInt();
    if (cols <= 0) {
        cols = a.value(QLatin1String("WIDTH")).toInt();
    }
    if (rows <= 0) {
        rows = a.value(QLatin1String("HEIGHT")).toInt();
    }
    if (cols > 0) {
        project.settings.matrix.gridCols = cols;
    }
    if (rows > 0) {
        project.settings.matrix.gridRows = rows;
    }
    r.skipCurrentElement();
}

void readTriggerMappings(QXmlStreamReader& r, QList<TriggerMapping>& out, QStringList& warnings)
{
    while (r.readNextStartElement()) {
        const auto tag = r.name();
        if (tag == QLatin1String("MAPPING") || tag == QLatin1String("TRIGGER")) {
            TriggerMapping t;
            const auto a = r.attributes();
            const QString inp = a.value(QLatin1String("INPUT")).toString().toLower();
            if (inp.contains(QLatin1String("note"))) {
                t.input = InputType::MidiNote;
            } else if (inp.contains(QLatin1String("cc"))) {
                t.input = InputType::MidiCC;
            } else if (inp.contains(QLatin1String("key"))) {
                t.input = InputType::Key;
            } else if (inp.contains(QLatin1String("osc"))) {
                t.input = InputType::Osc;
            }
            t.channel      = a.value(QLatin1String("CHANNEL")).toInt();
            t.number       = a.value(QLatin1String("NUMBER")).toInt();
            t.keyText      = a.value(QLatin1String("KEY")).toString();
            t.bankSetIndex = a.value(QLatin1String("BANKSET")).toInt();
            t.bankIndex    = a.value(QLatin1String("BANKINDEX")).toInt();
            t.cellIndex    = a.value(QLatin1String("CELLINDEX")).toInt();
            t.propertyName = a.value(QLatin1String("PROPERTY")).toString();

            const QString target = a.value(QLatin1String("TARGET")).toString().toLower();
            if (target.contains(QLatin1String("next"))) {
                t.target = TriggerTarget::BankNext;
            } else if (target.contains(QLatin1String("prev"))) {
                t.target = TriggerTarget::BankPrev;
            } else if (target.contains(QLatin1String("select"))) {
                t.target = TriggerTarget::BankSelect;
            } else if (target.contains(QLatin1String("switch"))) {
                t.target = TriggerTarget::BankSetSwitch;
            } else if (target.contains(QLatin1String("property"))) {
                t.target = TriggerTarget::Property;
            } else {
                t.target = TriggerTarget::Cell;
            }
            out.append(t);
            r.skipCurrentElement();
        } else if (tag == QLatin1String("CELLTRIGGERMAPPING")) {
            const auto a = r.attributes();
            TriggerMapping t;
            t.target = TriggerTarget::Cell;
            t.bankSetIndex = 0;
            t.bankIndex = 0;
            t.cellIndex = a.value(QLatin1String("CELLID")).toInt();

            InputType parsed = InputType::None;
            int ch = 0;
            int num = 0;
            if (!parseGrandVjChannel(a.value(QLatin1String("MAPPING")).toString(), parsed, ch, num)) {
                warnings.append(QStringLiteral("Unparsed cell trigger \"%1\"")
                                    .arg(a.value(QLatin1String("MAPPING")).toString()));
                r.skipCurrentElement();
                continue;
            }
            t.input = parsed;
            t.channel = ch;
            t.number = num;
            if (parsed == InputType::Key) {
                t.keyText = QString::number(num);
            }
            out.append(t);
            r.skipCurrentElement();
        } else {
            r.skipCurrentElement();
        }
    }
}

void processVj2Elements(QXmlStreamReader& r, Project& project, LibMap& libMap, Vj2Importer::Result& res)
{
    while (r.readNextStartElement()) {
        const auto tag = r.name();
        if (tag.compare(QLatin1String("MEDIALIBRARY"), Qt::CaseInsensitive) == 0
            || tag.compare(QLatin1String("LIBRARY"), Qt::CaseInsensitive) == 0) {
            readMediaLibrary(r, libMap, project.mediaLibrary);
        } else if (tag.compare(QLatin1String("DATA"), Qt::CaseInsensitive) == 0) {
            readDataMatrix(r, project);
        } else if (tag.compare(QLatin1String("BANKSET"), Qt::CaseInsensitive) == 0) {
            const int cellsPerBank = qMax(
                1, project.settings.matrix.gridRows * project.settings.matrix.gridCols);
            project.bankSets.append(
                readBankSet(r, libMap, project.mediaLibrary, res.warnings, cellsPerBank));
        } else if (tag.compare(QLatin1String("TRIGGERMAPPINGS"), Qt::CaseInsensitive) == 0) {
            readTriggerMappings(r, project.triggerMappings, res.warnings);
        } else if (tag.compare(QLatin1String("MATRIX"), Qt::CaseInsensitive) == 0) {
            const auto a = r.attributes();
            project.settings.matrix.width  = a.value(QLatin1String("WIDTH")).toInt();
            project.settings.matrix.height = a.value(QLatin1String("HEIGHT")).toInt();
            if (project.settings.matrix.width == 0) {
                project.settings.matrix.width = 1920;
            }
            if (project.settings.matrix.height == 0) {
                project.settings.matrix.height = 1080;
            }
            const int rows = a.value(QLatin1String("ROWS")).toInt();
            const int cols = a.value(QLatin1String("COLS")).toInt();
            if (rows > 0) {
                project.settings.matrix.gridRows = rows;
            }
            if (cols > 0) {
                project.settings.matrix.gridCols = cols;
            }
            r.skipCurrentElement();
        } else if (tag.compare(QLatin1String("SETTINGS"), Qt::CaseInsensitive) == 0) {
            const auto a = r.attributes();
            if (a.hasAttribute(QLatin1String("AUDIOBUFFERSIZE"))) {
                project.settings.audio.bufferSize = a.value(QLatin1String("AUDIOBUFFERSIZE")).toInt();
            }
            if (a.hasAttribute(QLatin1String("AUDIODRIVER"))) {
                project.settings.audio.driver = a.value(QLatin1String("AUDIODRIVER")).toString();
            }
            r.skipCurrentElement();
        } else if (tag.compare(QLatin1String("PROJECT"), Qt::CaseInsensitive) == 0) {
            processVj2Elements(r, project, libMap, res);
        } else {
            res.warnings.append(QStringLiteral("Unhandled element \"%1\" in .vj2").arg(tag.toString()));
            r.skipCurrentElement();
        }
    }
}

} // namespace

Vj2Importer::Result Vj2Importer::importFile(Project& project, const QString& filePath)
{
    Result res;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        res.errorMessage = QStringLiteral("Cannot open file: %1").arg(filePath);
        return res;
    }

    project = Project();
    project.sourceFormat = QStringLiteral("vj2");

    LibMap libMap;

    QXmlStreamReader r(&file);
    if (!r.readNextStartElement()) {
        res.errorMessage = QStringLiteral("Empty file");
        return res;
    }
    const QString root = r.name().toString();
    if (root.compare(QLatin1String("GrandVJ"), Qt::CaseInsensitive) != 0
        && root.compare(QLatin1String("GRANDVJ"), Qt::CaseInsensitive) != 0
        && root.compare(QLatin1String("PROJECT"), Qt::CaseInsensitive) != 0) {
        res.warnings.append(QStringLiteral("Root element is \"%1\", expected \"GrandVJ\" or \"PROJECT\" - importing anyway")
                            .arg(root));
    }
    project.formatVersion = r.attributes().value(QStringLiteral("version")).toString();

    processVj2Elements(r, project, libMap, res);

    if (r.hasError()) {
        res.errorMessage = r.errorString();
        return res;
    }

    if (project.bankSets.isEmpty()) {
        project.initializeDefault();
        res.warnings.append(QStringLiteral("No BANKSET elements found; populated with empty defaults"));
    } else {
        project.ensureSingleBankSet();
        project.resizeBanksForGrid(project.settings.matrix.gridRows, project.settings.matrix.gridCols);
    }
    project.normalizeCellSlotTriggers();

    res.ok = true;
    return res;
}

} // namespace pvj::core

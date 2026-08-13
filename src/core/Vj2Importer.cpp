#include "Vj2Importer.h"

#include "BankOps.h"
#include "EnumStrings.h"
#include "FilterEffectIds.h"
#include "FilterParamSchema.h"
#include "PropertyRegistry.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QUuid>
#include <QXmlStreamReader>

#include <array>

namespace pvj::core {

namespace {

void readTriggerMappings(QXmlStreamReader& r, QList<TriggerMapping>& out, QStringList& warnings);
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
        add("FBIS", "feedbackInSaturation");
        add("FBIB", "feedbackInBrightness");
        add("FBIC", "feedbackInContrast");
        add("FBIH", "feedbackInHueShift");
        add("FBIG", "feedbackInGamma");
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

/// Map GrandVJ keyboard tokens (`F1`, `A`, `#1`, `SFT`, …) to Qt portable key-sequence text.
QString grandVjKeyTokenToPortableText(QString token)
{
    token = token.trimmed();
    if (token.isEmpty()) {
        return {};
    }

    // Same physical keys under different GrandVJ export spellings.
    if (token.compare(QLatin1String("SFT"), Qt::CaseInsensitive) == 0
        || token == QLatin1String("#1")) {
        return QStringLiteral("Shift");
    }
    if (token == QLatin1String("#2")) {
        return QStringLiteral("\\");
    }

    if (token.size() >= 2
        && (token[0] == QLatin1Char('F') || token[0] == QLatin1Char('f'))) {
        bool ok = false;
        const int n = QStringView{token}.mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 24) {
            return QStringLiteral("F%1").arg(n);
        }
    }

    static const QHash<QString, QString> named = {
        {QStringLiteral("SPACE"), QStringLiteral("Space")},
        {QStringLiteral("SPC"), QStringLiteral("Space")},
        {QStringLiteral("ESC"), QStringLiteral("Esc")},
        {QStringLiteral("ENTER"), QStringLiteral("Return")},
        {QStringLiteral("RETURN"), QStringLiteral("Return")},
        {QStringLiteral("TAB"), QStringLiteral("Tab")},
        {QStringLiteral("BKSP"), QStringLiteral("Backspace")},
        {QStringLiteral("BACKSPACE"), QStringLiteral("Backspace")},
        {QStringLiteral("DEL"), QStringLiteral("Delete")},
        {QStringLiteral("DELETE"), QStringLiteral("Delete")},
        {QStringLiteral("INS"), QStringLiteral("Insert")},
        {QStringLiteral("INSERT"), QStringLiteral("Insert")},
        {QStringLiteral("HOME"), QStringLiteral("Home")},
        {QStringLiteral("END"), QStringLiteral("End")},
        {QStringLiteral("PGUP"), QStringLiteral("PgUp")},
        {QStringLiteral("PGDN"), QStringLiteral("PgDown")},
        {QStringLiteral("LEFT"), QStringLiteral("Left")},
        {QStringLiteral("RIGHT"), QStringLiteral("Right")},
        {QStringLiteral("UP"), QStringLiteral("Up")},
        {QStringLiteral("DOWN"), QStringLiteral("Down")},
        {QStringLiteral("CTRL"), QStringLiteral("Ctrl")},
        {QStringLiteral("ALT"), QStringLiteral("Alt")},
        {QStringLiteral("SHIFT"), QStringLiteral("Shift")},
    };
    const auto namedIt = named.constFind(token.toUpper());
    if (namedIt != named.cend()) {
        return *namedIt;
    }

    if (token.size() == 1) {
        const QChar c = token.at(0);
        if (c.isLetter()) {
            return QString(c.toUpper());
        }
        return token;
    }

    return token;
}

QString grandVjKeyboardKeyText(const QString& channel)
{
    const QStringList parts = channel.split(QLatin1Char(':'), Qt::KeepEmptyParts);
    if (parts.size() < 3) {
        return {};
    }
    const QString kind = parts[0].toLower();
    if (kind != QLatin1String("keyboard") && kind != QLatin1String("key")) {
        return {};
    }
    // keyboard:<device>:<keyToken> — key token may theoretically contain ':'
    return grandVjKeyTokenToPortableText(parts.mid(2).join(QLatin1Char(':')));
}

bool parseGrandVjChannel(const QString& channel, InputType& input, int& midiChannel, int& number,
                         QString* keyText = nullptr)
{
    const QStringList parts = channel.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return false;
    }
    const QString kind = parts[0].toLower();
    if (kind == QLatin1String("midi")) {
        // midi:device:channel:cc:number  (GrandVJ 2.7) — device may be "all"
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
        // keyboard:0:F1 — third field is a key name, not an integer
        const QString text = grandVjKeyboardKeyText(channel);
        if (text.isEmpty()) {
            return false;
        }
        input = InputType::Key;
        midiChannel = parts.size() > 1 ? parts[1].toInt() : 0;
        number = 0;
        if (keyText) {
            *keyText = text;
        }
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
            // GrandVJ uses <NAME name="Babel2"/>; some exports use VALUE.
            QString n = r.attributes().value(QLatin1String("name")).toString();
            if (n.isEmpty()) {
                n = r.attributes().value(QLatin1String("NAME")).toString();
            }
            if (n.isEmpty()) {
                n = r.attributes().value(QLatin1String("VALUE")).toString();
            }
            if (n.isEmpty()) {
                n = gvj27ValueAttr(r.attributes());
            }
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
                    QStringList& warnings, int cellsPerBank, int bankSetIndex,
                    QList<TriggerMapping>& triggerMappings)
{
    BankSet set;
    const int typeValue = r.attributes().value(QLatin1String("TYPE")).toInt();
    set.type = (typeValue == 1) ? BankSetType::TypeB : BankSetType::TypeA;
    int bankIndex = 0;
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("BANK")) {
            set.banks.append(readBank(r, bankIndex, libMap, library, warnings, cellsPerBank));
            ++bankIndex;
        } else if (r.name() == QLatin1String("TRIGGERMAPPINGS")) {
            // GrandVJ nests cell keyboard/MIDI/OSC triggers inside each BANKSET.
            QList<TriggerMapping> local;
            readTriggerMappings(r, local, warnings);
            for (TriggerMapping& t : local) {
                t.bankSetIndex = bankSetIndex;
                if (t.target == TriggerTarget::Cell) {
                    t.bankIndex = kBankIndexAllBanks;
                }
                triggerMappings.append(t);
            }
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
    int cols = 0;
    int rows = 0;

    // Prefer GrandVJ matrix cell-grid attributes (not stage pixel size).
    for (const QXmlStreamAttribute& attr : a) {
        const QString name = attr.qualifiedName().toString().toUpper();
        const int value = attr.value().toInt();
        if (value <= 0) {
            continue;
        }
        if (name == QLatin1String("MATRIXWIDTH") || name == QLatin1String("COLS")
            || name == QLatin1String("COLUMNS") || name == QLatin1String("GRIDCOLS")) {
            cols = value;
        } else if (name == QLatin1String("MATRIXHEIGHT") || name == QLatin1String("ROWS")
                   || name == QLatin1String("GRIDROWS")) {
            rows = value;
        }
    }

    // Nested GrandVJ 2.7 style: <MATRIXWIDTH VALUE="12"/> etc.
    while (r.readNextStartElement()) {
        const QString tag = r.name().toString().toUpper();
        const int value = gvj27ValueAttr(r.attributes()).toInt();
        if (value > 0) {
            if (tag == QLatin1String("MATRIXWIDTH") || tag == QLatin1String("COLS")
                || tag == QLatin1String("COLUMNS") || tag == QLatin1String("GRIDCOLS")) {
                cols = value;
            } else if (tag == QLatin1String("MATRIXHEIGHT") || tag == QLatin1String("ROWS")
                       || tag == QLatin1String("GRIDROWS")) {
                rows = value;
            }
        }
        r.skipCurrentElement();
    }

    if (cols > 0) {
        project.settings.matrix.gridCols = cols;
    }
    if (rows > 0) {
        project.settings.matrix.gridRows = rows;
    }
}

/// If DATA omitted the grid size, derive rows/cols from the densest bank.
void inferGridDimensionsFromBanks(Project& project)
{
    if (project.bankSets.isEmpty()) {
        return;
    }
    int maxCells = 0;
    for (const Bank& bank : project.bankSets[0].banks) {
        maxCells = qMax(maxCells, bank.cells.size());
    }
    if (maxCells <= 0) {
        return;
    }

    int cols = project.settings.matrix.gridCols;
    int rows = project.settings.matrix.gridRows;
    if (cols <= 0) {
        cols = 12;
    }
    if (rows <= 0) {
        rows = 4;
    }

    // Grow so every imported cell index fits the grid.
    if (maxCells > rows * cols) {
        // Prefer keeping column count (GrandVJ MATRIXWIDTH); add rows as needed.
        rows = (maxCells + cols - 1) / cols;
        // If that exceeds the usual max, try a nearer rectangular fit.
        if (rows > 16) {
            cols = qMin(16, maxCells);
            rows = (maxCells + cols - 1) / cols;
            cols = (maxCells + rows - 1) / rows;
        }
        project.settings.matrix.gridCols = cols;
        project.settings.matrix.gridRows = rows;
    }
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
            t.bankIndex = kBankIndexAllBanks;
            t.cellIndex = a.value(QLatin1String("CELLID")).toInt();

            const QString mapping = a.value(QLatin1String("MAPPING")).toString();
            InputType parsed = InputType::None;
            int ch = 0;
            int num = 0;
            QString keyText;
            if (!parseGrandVjChannel(mapping, parsed, ch, num, &keyText)) {
                warnings.append(QStringLiteral("Unparsed cell trigger \"%1\"").arg(mapping));
                r.skipCurrentElement();
                continue;
            }
            t.input = parsed;
            t.channel = ch;
            t.number = num;
            if (parsed == InputType::Key) {
                t.keyText = keyText;
                if (t.keyText.isEmpty()) {
                    warnings.append(QStringLiteral("Empty keyboard cell trigger \"%1\"").arg(mapping));
                    r.skipCurrentElement();
                    continue;
                }
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
            const int bankSetIndex = project.bankSets.size();
            project.bankSets.append(readBankSet(r, libMap, project.mediaLibrary, res.warnings,
                                                cellsPerBank, bankSetIndex,
                                                project.triggerMappings));
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
        inferGridDimensionsFromBanks(project);
        project.resizeBanksForGrid(project.settings.matrix.gridRows, project.settings.matrix.gridCols);
    }
    project.normalizeCellSlotTriggers();

    res.ok = true;
    return res;
}

namespace {

void remapCellMediaIds(Cell& cell, const QHash<QUuid, QUuid>& mediaMap)
{
    if (cell.visual.type != VisualType::Media || cell.visual.mediaId.isNull()) {
        return;
    }
    const auto it = mediaMap.constFind(cell.visual.mediaId);
    if (it != mediaMap.cend()) {
        cell.visual.mediaId = it.value();
    }
}

QHash<QUuid, QUuid> mergeMediaLibrary(Project& destination, const Project& source)
{
    QHash<QUuid, QUuid> mediaMap;
    QHash<QString, QUuid> pathToId;
    for (const MediaItem& m : destination.mediaLibrary) {
        const QString key = QFileInfo(m.path).absoluteFilePath();
        if (!key.isEmpty()) {
            pathToId.insert(key, m.id);
        }
    }

    for (const MediaItem& m : source.mediaLibrary) {
        const QString key = QFileInfo(m.path).absoluteFilePath();
        if (!key.isEmpty()) {
            const auto pathIt = pathToId.constFind(key);
            if (pathIt != pathToId.cend()) {
                mediaMap.insert(m.id, pathIt.value());
                continue;
            }
        }

        MediaItem copy = m;
        if (!destination.findMedia(copy.id)) {
            // keep source id when free
        } else {
            copy.id = QUuid::createUuid();
        }
        mediaMap.insert(m.id, copy.id);
        if (!key.isEmpty()) {
            pathToId.insert(key, copy.id);
        }
        destination.mediaLibrary.append(copy);
    }
    return mediaMap;
}

Bank makeEmptyBank(int index, int cellsPerBank)
{
    Bank bank;
    bank.index = index;
    bank.name = QStringLiteral("Bank %1").arg(index + 1);
    bank.cells.reserve(cellsPerBank);
    for (int i = 0; i < cellsPerBank; ++i) {
        Cell c;
        c.index = i;
        bank.cells.append(c);
    }
    return bank;
}

} // namespace

Vj2Importer::Result Vj2Importer::mergeFile(Project& destination, const QString& filePath,
                                           int destinationStartBank, int sourceStartBank)
{
    Project imported;
    Result res = importFile(imported, filePath);
    if (!res.ok) {
        return res;
    }
    if (imported.bankSets.isEmpty() || imported.bankSets[0].banks.isEmpty()) {
        res.ok = false;
        res.errorMessage = QStringLiteral("Imported project has no banks");
        return res;
    }

    destination.ensureSingleBankSet();
    if (destination.bankSets.isEmpty()) {
        destination.initializeDefault();
    }

    // Grow grid if the imported matrix is larger.
    const int rows = qMax(destination.settings.matrix.gridRows, imported.settings.matrix.gridRows);
    const int cols = qMax(destination.settings.matrix.gridCols, imported.settings.matrix.gridCols);
    destination.resizeBanksForGrid(rows, cols);

    BankSet& destSet = destination.bankSets[0];
    const BankSet& srcSet = imported.bankSets[0];
    const int cellsPerBank =
        qMax(1, destination.settings.matrix.gridRows * destination.settings.matrix.gridCols);

    const int srcStart = qMax(0, sourceStartBank);
    if (srcStart >= srcSet.banks.size()) {
        res.ok = false;
        res.errorMessage = QStringLiteral("Source start bank %1 is past the end (%2 banks)")
                               .arg(srcStart + 1)
                               .arg(srcSet.banks.size());
        return res;
    }

    const int destStart = qMax(0, destinationStartBank);
    const int srcCount = srcSet.banks.size() - srcStart;
    const int needed = destStart + srcCount;
    while (destSet.banks.size() < needed) {
        destSet.banks.append(makeEmptyBank(destSet.banks.size(), cellsPerBank));
    }
    // Keep bank indices consistent after padding.
    for (int i = 0; i < destSet.banks.size(); ++i) {
        destSet.banks[i].index = i;
    }

    const QHash<QUuid, QUuid> mediaMap = mergeMediaLibrary(destination, imported);

    int merged = 0;
    for (int i = 0; i < srcCount; ++i) {
        Bank& dst = destSet.banks[destStart + i];
        const Bank& src = srcSet.banks[srcStart + i];
        // Ensure destination bank has enough cells before copy.
        if (dst.cells.size() < cellsPerBank) {
            const int old = dst.cells.size();
            dst.cells.resize(cellsPerBank);
            for (int c = old; c < cellsPerBank; ++c) {
                dst.cells[c].index = c;
            }
        }
        copyBankContent(dst, src);
        for (Cell& cell : dst.cells) {
            remapCellMediaIds(cell, mediaMap);
        }
        ++merged;
    }

    // Merge cell-slot triggers (keyboard / MIDI note) from the imported file.
    for (const TriggerMapping& t : imported.triggerMappings) {
        if (t.target != TriggerTarget::Cell) {
            continue;
        }
        if (t.input == InputType::Key && !t.keyText.isEmpty()) {
            destination.setKeyboardTriggerForCell(0, kBankIndexAllBanks, t.cellIndex, t.keyText,
                                                  t.number);
        } else if (t.input == InputType::MidiNote) {
            destination.setMidiCellTriggerForCell(0, t.cellIndex, t.channel, t.number);
        }
    }
    destination.normalizeCellSlotTriggers();

    res.banksMerged = merged;
    res.ok = true;
    return res;
}

} // namespace pvj::core

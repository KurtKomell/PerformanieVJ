#include "Vj2Importer.h"

#include "EnumStrings.h"

#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QXmlStreamReader>

namespace pvj::core {

namespace {

// GrandVJ uses Windows-style paths inside its XML; convert to forward slashes
// for cross-platform consistency inside our model.
QString normalizePath(const QString& path)
{
    QString out = path;
    out.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return out;
}

CopyMode mapCopyMode(const QString& s)
{
    // GrandVJ copy modes happen to map closely to our enum; unknowns fall back.
    return enums::copyModeFromString(s.toLower(), CopyMode::Normal);
}

MaskType mapMaskType(const QString& s)
{
    return enums::maskTypeFromString(s.toLower(), MaskType::None);
}

struct LibEntry {
    QUuid id;
    QString path;
};

using LibMap = QHash<QString, LibEntry>;  // path -> entry

QUuid mediaIdFor(LibMap& map, QList<MediaItem>& library, const QString& rawPath)
{
    QString key = normalizePath(rawPath);
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

void readCell(QXmlStreamReader& r, Bank& bank, LibMap& libMap, QList<MediaItem>& library,
              QStringList& warnings)
{
    Cell cell;
    const auto attr = r.attributes();
    cell.index = attr.value(QStringLiteral("INDEX")).toInt();

    // Read core properties from attributes when present.
    auto getD = [&](QLatin1String n, double def) {
        const auto v = attr.value(n);
        return v.isEmpty() ? def : v.toDouble();
    };
    cell.props.priority       = attr.value(QLatin1String("PRIORITY")).toInt();
    cell.props.transparency   = getD(QLatin1String("TRANSPARENCY"),   1.0);
    cell.props.movieSpeed     = getD(QLatin1String("MOVIESPEED"),     1.0);
    cell.props.fade           = getD(QLatin1String("FADE"),           0.0);
    cell.props.maskWidth      = getD(QLatin1String("MASKWIDTH"),      0.0);
    cell.props.maskSmoothness = getD(QLatin1String("MASKSMOOTHNESS"), 0.0);
    cell.props.rotationZ      = getD(QLatin1String("ROTATIONZ"),      0.0);
    cell.props.maskType       = mapMaskType(attr.value(QLatin1String("MASKTYPE")).toString());
    cell.props.copyMode       = mapCopyMode(attr.value(QLatin1String("COPYMODE")).toString());

    // Visual source can be either a media path or a generator name.
    const QString visualPath = attr.value(QLatin1String("VISUAL")).toString();
    const QString generator  = attr.value(QLatin1String("GENERATOR")).toString();

    if (!visualPath.isEmpty()) {
        cell.visual.type = VisualType::Media;
        cell.visual.mediaId = mediaIdFor(libMap, library, visualPath);
    } else if (!generator.isEmpty()) {
        cell.visual.type = VisualType::Generator;
        cell.visual.generator = enums::generatorFromString(generator, GeneratorKind::None);
        if (cell.visual.generator == GeneratorKind::None) {
            warnings.append(QStringLiteral("Unknown GrandVJ generator \"%1\", stored as empty").arg(generator));
        }
    }

    while (r.readNextStartElement()) {
        const auto tag = r.name();
        if (tag == QLatin1String("EFFECT")) {
            Effect e;
            e.name = r.attributes().value(QStringLiteral("NAME")).toString();
            // Effect params in GrandVJ: PARAM0..PARAM3
            for (int i = 0; i < 4; ++i) {
                const QString key = QStringLiteral("PARAM%1").arg(i);
                const auto v = r.attributes().value(key);
                if (!v.isEmpty()) {
                    EffectParam p;
                    p.name = QStringLiteral("param%1").arg(i);
                    p.value = v.toDouble();
                    e.params.append(p);
                }
            }
            cell.effect = e;
            r.skipCurrentElement();
        } else if (tag == QLatin1String("PROPERTYMAPPINGLIST")) {
            while (r.readNextStartElement()) {
                if (r.name() == QLatin1String("MAPPING")) {
                    PropertyMapping m;
                    const auto a = r.attributes();
                    m.property = a.value(QLatin1String("PROPERTY")).toString();
                    const QString inp = a.value(QLatin1String("INPUT")).toString().toLower();
                    if      (inp.contains(QLatin1String("note"))) m.input = InputType::MidiNote;
                    else if (inp.contains(QLatin1String("cc")))   m.input = InputType::MidiCC;
                    else if (inp.contains(QLatin1String("key")))  m.input = InputType::Key;
                    else if (inp.contains(QLatin1String("osc")))  m.input = InputType::Osc;
                    m.channel  = a.value(QLatin1String("CHANNEL")).toInt();
                    m.number   = a.value(QLatin1String("NUMBER")).toInt();
                    m.minValue = a.value(QLatin1String("MIN")).toDouble();
                    m.maxValue = a.value(QLatin1String("MAX")).toDouble();
                    if (a.hasAttribute(QLatin1String("MAX")) == false) {
                        m.maxValue = 1.0;
                    }
                    cell.propertyMappings.append(m);
                    r.skipCurrentElement();
                } else {
                    r.skipCurrentElement();
                }
            }
        } else {
            r.skipCurrentElement();
        }
    }

    if (cell.index >= 0 && cell.index < bank.cells.size()) {
        bank.cells[cell.index] = cell;
    } else {
        bank.cells.append(cell);
    }
}

Bank readBank(QXmlStreamReader& r, LibMap& libMap, QList<MediaItem>& library, QStringList& warnings)
{
    Bank bank;
    const auto attr = r.attributes();
    bank.index = attr.value(QStringLiteral("INDEX")).toInt();
    bank.name  = attr.value(QStringLiteral("NAME")).toString();
    if (bank.name.isEmpty()) {
        bank.name = QStringLiteral("Bank %1").arg(bank.index + 1);
    }
    bank.cells.reserve(64);
    for (int i = 0; i < 64; ++i) {
        Cell c;
        c.index = i;
        bank.cells.append(c);
    }
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("CELL")) {
            readCell(r, bank, libMap, library, warnings);
        } else {
            r.skipCurrentElement();
        }
    }
    return bank;
}

BankSet readBankSet(QXmlStreamReader& r, LibMap& libMap, QList<MediaItem>& library,
                    QStringList& warnings)
{
    BankSet set;
    const int typeValue = r.attributes().value(QLatin1String("TYPE")).toInt();
    set.type = (typeValue == 1) ? BankSetType::TypeB : BankSetType::TypeA;
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("BANK")) {
            set.banks.append(readBank(r, libMap, library, warnings));
        } else {
            r.skipCurrentElement();
        }
    }
    return set;
}

void readMediaLibrary(QXmlStreamReader& r, LibMap& libMap, QList<MediaItem>& library)
{
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("ITEM") || r.name() == QLatin1String("MEDIA")) {
            const QString path = r.attributes().value(QLatin1String("PATH")).toString();
            mediaIdFor(libMap, library, path);
            r.skipCurrentElement();
        } else {
            r.skipCurrentElement();
        }
    }
}

void readTriggerMappings(QXmlStreamReader& r, QList<TriggerMapping>& out)
{
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("MAPPING") || r.name() == QLatin1String("TRIGGER")) {
            TriggerMapping t;
            const auto a = r.attributes();
            const QString inp = a.value(QLatin1String("INPUT")).toString().toLower();
            if      (inp.contains(QLatin1String("note"))) t.input = InputType::MidiNote;
            else if (inp.contains(QLatin1String("cc")))   t.input = InputType::MidiCC;
            else if (inp.contains(QLatin1String("key")))  t.input = InputType::Key;
            else if (inp.contains(QLatin1String("osc")))  t.input = InputType::Osc;
            t.channel      = a.value(QLatin1String("CHANNEL")).toInt();
            t.number       = a.value(QLatin1String("NUMBER")).toInt();
            t.keyText      = a.value(QLatin1String("KEY")).toString();
            t.bankSetIndex = a.value(QLatin1String("BANKSET")).toInt();
            t.bankIndex    = a.value(QLatin1String("BANKINDEX")).toInt();
            t.cellIndex    = a.value(QLatin1String("CELLINDEX")).toInt();
            t.propertyName = a.value(QLatin1String("PROPERTY")).toString();

            const QString target = a.value(QLatin1String("TARGET")).toString().toLower();
            if      (target.contains(QLatin1String("next")))     t.target = TriggerTarget::BankNext;
            else if (target.contains(QLatin1String("prev")))     t.target = TriggerTarget::BankPrev;
            else if (target.contains(QLatin1String("select")))   t.target = TriggerTarget::BankSelect;
            else if (target.contains(QLatin1String("switch")))   t.target = TriggerTarget::BankSetSwitch;
            else if (target.contains(QLatin1String("property"))) t.target = TriggerTarget::Property;
            else                                                 t.target = TriggerTarget::Cell;

            out.append(t);
            r.skipCurrentElement();
        } else {
            r.skipCurrentElement();
        }
    }
}

// GrandVJ sometimes wraps MEDIALIBRARY / BANKSET / … in a PROJECT node, or uses
// PROJECT as the document root. This scans one nesting level (and recurses into PROJECT).
void processVj2Elements(QXmlStreamReader& r, Project& project, LibMap& libMap, Vj2Importer::Result& res)
{
    while (r.readNextStartElement()) {
        const auto tag = r.name();
        if (tag.compare(QLatin1String("MEDIALIBRARY"), Qt::CaseInsensitive) == 0) {
            readMediaLibrary(r, libMap, project.mediaLibrary);
        } else if (tag.compare(QLatin1String("BANKSET"), Qt::CaseInsensitive) == 0) {
            project.bankSets.append(readBankSet(r, libMap, project.mediaLibrary, res.warnings));
        } else if (tag.compare(QLatin1String("TRIGGERMAPPINGS"), Qt::CaseInsensitive) == 0) {
            readTriggerMappings(r, project.triggerMappings);
        } else if (tag.compare(QLatin1String("MATRIX"), Qt::CaseInsensitive) == 0) {
            const auto a = r.attributes();
            project.settings.matrix.width  = a.value(QLatin1String("WIDTH")).toInt();
            project.settings.matrix.height = a.value(QLatin1String("HEIGHT")).toInt();
            if (project.settings.matrix.width == 0)  project.settings.matrix.width  = 1920;
            if (project.settings.matrix.height == 0) project.settings.matrix.height = 1080;
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

    res.ok = true;
    return res;
}

} // namespace pvj::core

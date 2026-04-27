#include "AvcImporter.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QUuid>
#include <QXmlStreamReader>

namespace pvj::core {

namespace {

struct ParsedClip {
    int   layerIndex  = 0;    // row
    int   columnIndex = 0;    // col
    QString filePath;
};

QString normalize(const QString& p)
{
    QString out = p;
    out.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return out;
}

// Recursively scans the document looking for Clip elements that reference a
// video file. The .avc XML structure is quite deep and has varied across
// Resolume versions; we rely on the presence of:
//   <Clip ...>
//     <VideoTrack>
//       <Clips>
//         <Clip file=... or <ContentFile fileName=...>
// but to stay version-tolerant we just watch for any element that carries a
// file reference while tracking the current layer/column indices.
void scan(QXmlStreamReader& r, QList<ParsedClip>& out, QStringList& warnings,
          int currentLayer, int currentColumn)
{
    while (!r.atEnd() && !r.hasError()) {
        const auto tok = r.readNext();
        if (tok == QXmlStreamReader::StartElement) {
            const QString name = r.name().toString();
            const auto attrs = r.attributes();

            int nextLayer  = currentLayer;
            int nextColumn = currentColumn;

            if (name.compare(QLatin1String("Layer"), Qt::CaseInsensitive) == 0) {
                // Resolume layers ordered top-to-bottom; use "index" or rely on
                // document order.
                if (attrs.hasAttribute(QLatin1String("index"))) {
                    nextLayer = attrs.value(QLatin1String("index")).toInt();
                } else {
                    nextLayer = currentLayer + 1;
                }
            } else if (name.compare(QLatin1String("Column"), Qt::CaseInsensitive) == 0) {
                if (attrs.hasAttribute(QLatin1String("index"))) {
                    nextColumn = attrs.value(QLatin1String("index")).toInt();
                } else {
                    nextColumn = currentColumn + 1;
                }
            } else if (name.compare(QLatin1String("Clip"), Qt::CaseInsensitive) == 0) {
                // Attributes that can directly carry the file reference.
                for (const auto& a : attrs) {
                    const QString an = a.name().toString();
                    if (an.compare(QLatin1String("file"), Qt::CaseInsensitive) == 0
                        || an.compare(QLatin1String("path"), Qt::CaseInsensitive) == 0
                        || an.compare(QLatin1String("fileName"), Qt::CaseInsensitive) == 0) {
                        ParsedClip c;
                        c.layerIndex  = currentLayer;
                        c.columnIndex = currentColumn;
                        c.filePath    = normalize(a.value().toString());
                        if (!c.filePath.isEmpty()) {
                            out.append(c);
                        }
                    }
                }
            } else if (name.compare(QLatin1String("ContentFile"), Qt::CaseInsensitive) == 0
                       || name.compare(QLatin1String("FileSource"), Qt::CaseInsensitive) == 0
                       || name.compare(QLatin1String("VideoFile"), Qt::CaseInsensitive) == 0) {
                QString path;
                for (const auto& a : attrs) {
                    const QString an = a.name().toString();
                    if (an.compare(QLatin1String("fileName"), Qt::CaseInsensitive) == 0
                        || an.compare(QLatin1String("file"), Qt::CaseInsensitive) == 0
                        || an.compare(QLatin1String("path"), Qt::CaseInsensitive) == 0) {
                        path = normalize(a.value().toString());
                        break;
                    }
                }
                if (!path.isEmpty()) {
                    ParsedClip c;
                    c.layerIndex  = currentLayer;
                    c.columnIndex = currentColumn;
                    c.filePath    = path;
                    out.append(c);
                }
            } else if (name.compare(QLatin1String("Effect"), Qt::CaseInsensitive) == 0) {
                // We intentionally ignore effect definitions.
                const QString fxName = attrs.value(QLatin1String("name")).toString();
                if (!fxName.isEmpty()) {
                    warnings.append(QStringLiteral("Resolume effect \"%1\" ignored").arg(fxName));
                }
            }

            scan(r, out, warnings, nextLayer, nextColumn);
        } else if (tok == QXmlStreamReader::EndElement) {
            return;
        }
    }
}

} // namespace

AvcImporter::Result AvcImporter::importFile(Project& project, const QString& filePath)
{
    Result res;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        res.errorMessage = QStringLiteral("Cannot open file: %1").arg(filePath);
        return res;
    }

    project = Project();
    project.sourceFormat = QStringLiteral("avc");
    project.initializeDefault();

    QXmlStreamReader r(&file);
    if (!r.readNextStartElement()) {
        res.errorMessage = QStringLiteral("Empty or malformed .avc file");
        return res;
    }

    QList<ParsedClip> clips;
    scan(r, clips, res.warnings, -1, -1);

    if (r.hasError()) {
        res.errorMessage = r.errorString();
        return res;
    }

    if (clips.isEmpty()) {
        res.warnings.append(QStringLiteral("No clips recognised in composition"));
    }

    // Populate media library (dedup by path).
    QHash<QString, QUuid> byPath;
    auto& typeA = project.bankSets[0];  // TypeA initialized by initializeDefault
    for (const auto& c : clips) {
        QUuid id;
        auto it = byPath.find(c.filePath);
        if (it != byPath.end()) {
            id = *it;
        } else {
            id = QUuid::createUuid();
            byPath.insert(c.filePath, id);
            MediaItem m;
            m.id = id;
            m.path = c.filePath;
            m.displayName = QFileInfo(c.filePath).fileName();
            project.mediaLibrary.append(m);
        }

        // Map column -> bank index, layer -> cell index (clamped).
        const int bankIdx = qBound(0, c.columnIndex, typeA.banks.size() - 1);
        const int cellIdx = qBound(0, c.layerIndex,  63);

        auto& bank = typeA.banks[bankIdx];
        auto& cell = bank.cells[cellIdx];
        cell.visual.type = VisualType::Media;
        cell.visual.mediaId = id;
    }

    res.ok = true;
    return res;
}

} // namespace pvj::core

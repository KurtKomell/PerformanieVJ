#include "core/CellOps.h"

#include <QHash>

namespace pvj::core {
namespace {

bool parseFilterProperty(const QString& raw, QUuid* nodeId, QString* paramName)
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
    if (id.isNull() || parts[2].isEmpty()) {
        return false;
    }
    if (nodeId) {
        *nodeId = id;
    }
    if (paramName) {
        *paramName = parts[2];
    }
    return true;
}

QString remapFilterProperty(const QString& raw, const QHash<QUuid, QUuid>& idMap)
{
    QUuid nodeId;
    QString paramName;
    if (!parseFilterProperty(raw, &nodeId, &paramName)) {
        return raw;
    }
    const auto it = idMap.constFind(nodeId);
    if (it == idMap.constEnd()) {
        return raw;
    }
    return QStringLiteral("filter.%1.%2")
        .arg(QString::fromUtf8(it.value().toString(QUuid::WithoutBraces).toUtf8()), paramName);
}

void copyFilterChainAndMappings(Cell& dst, const Cell& src, bool copyMidiMappings)
{
    QHash<QUuid, QUuid> idMap;
    dst.filterChain.clear();
    dst.filterChain.reserve(src.filterChain.size());
    for (const CellFilterNode& node : src.filterChain) {
        CellFilterNode copy = node;
        const QUuid newId   = QUuid::createUuid();
        idMap.insert(node.id, newId);
        copy.id = newId;
        dst.filterChain.append(copy);
    }

    dst.propertyMappings.clear();
    if (!copyMidiMappings) {
        return;
    }
    dst.propertyMappings.reserve(src.propertyMappings.size());
    for (PropertyMapping mapping : src.propertyMappings) {
        mapping.property = remapFilterProperty(mapping.property, idMap);
        dst.propertyMappings.append(mapping);
    }
}

} // namespace

void copyCellLayerSettings(Cell& dst, const Cell& src)
{
    dst.props  = src.props;
    dst.effect = src.effect;
    copyFilterChainAndMappings(dst, src, /*copyMidiMappings=*/true);
}

void copyCellSettings(Cell& dst, const Cell& src)
{
    copyCellLayerSettings(dst, src);
}

void copyCellContent(Cell& dst, const Cell& src)
{
    dst.name   = src.name;
    dst.visual = src.visual;
    copyCellLayerSettings(dst, src);
}

void pasteCellContent(Cell& dst, const Cell& src, CellPasteMode mode)
{
    switch (mode) {
    case CellPasteMode::FullWithMidi:
        copyCellContent(dst, src);
        break;
    case CellPasteMode::FullWithoutMidi:
        dst.name   = src.name;
        dst.visual = src.visual;
        dst.props  = src.props;
        dst.effect = src.effect;
        copyFilterChainAndMappings(dst, src, /*copyMidiMappings=*/false);
        break;
    case CellPasteMode::ParamsWithMidi:
        copyCellLayerSettings(dst, src);
        break;
    }
}

void copyCellPreferredLayer(Cell& dst, const Cell& src)
{
    dst.props.preferredLayer = src.props.preferredLayer;
}

void copyCellMidiPropertyMappings(Cell& dst, const Cell& src)
{
    QHash<QUuid, QUuid> idMap;
    const int n = qMin(src.filterChain.size(), dst.filterChain.size());
    for (int i = 0; i < n; ++i) {
        if (src.filterChain[i].typeId == dst.filterChain[i].typeId) {
            idMap.insert(src.filterChain[i].id, dst.filterChain[i].id);
        }
    }

    dst.propertyMappings.clear();
    dst.propertyMappings.reserve(src.propertyMappings.size());
    for (PropertyMapping mapping : src.propertyMappings) {
        QUuid srcNodeId;
        if (parseFilterProperty(mapping.property, &srcNodeId, nullptr)) {
            if (!idMap.contains(srcNodeId)) {
                continue;
            }
            mapping.property = remapFilterProperty(mapping.property, idMap);
        }
        dst.propertyMappings.append(mapping);
    }
}

} // namespace pvj::core

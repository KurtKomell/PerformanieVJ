#include "MixerFilterOps.h"

#include "FilterEffectIds.h"

namespace pvj::core {

bool cellIsMixerFilter(const Cell& cell)
{
    return cell.visual.type == VisualType::MixerFilter && !cell.filterChain.isEmpty();
}

QList<CellFilterNode> buildEffectiveOutputFilterChain(
    const QList<CellFilterNode>& outputTabChain,
    const QList<QList<CellFilterNode>>& activeCellChainsInOrder)
{
    QList<CellFilterNode> glsl;
    QList<CellFilterNode> maxine;

    auto partitionAppend = [&](const QList<CellFilterNode>& chain) {
        for (const CellFilterNode& node : chain) {
            if (isFeedbackMarkerNode(node.typeId)) {
                continue;
            }
            if (filterUsesMaxineBackend(node.typeId)) {
                maxine.append(node);
            } else {
                glsl.append(node);
            }
        }
    };

    for (const QList<CellFilterNode>& chain : activeCellChainsInOrder) {
        partitionAppend(chain);
    }
    partitionAppend(outputTabChain);

    glsl.append(maxine);
    return glsl;
}

} // namespace pvj::core

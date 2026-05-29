#pragma once

#include "Model.h"

#include <QList>

namespace pvj::core {

/// True when the cell is a mixer-wide filter preset (non-empty chain).
bool cellIsMixerFilter(const Cell& cell);

/// Merges active cell chains (trigger order) then output-tab chain. Maxine nodes always last.
QList<CellFilterNode> buildEffectiveOutputFilterChain(
    const QList<CellFilterNode>& outputTabChain,
    const QList<QList<CellFilterNode>>& activeCellChainsInOrder);

} // namespace pvj::core

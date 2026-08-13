#pragma once

#include "core/Model.h"

namespace pvj::core {

/// How paste applies a clipboard cell onto a destination cell.
enum class CellPasteMode {
    /// Visual + name + props + filter + effect + propertyMappings (full cell content).
    FullWithMidi,
    /// Like FullWithMidi but destination propertyMappings are cleared (no parameter MIDI).
    FullWithoutMidi,
    /// Props + filter + effect + propertyMappings only; keeps destination visual and name.
    ParamsWithMidi,
};

/// Copies clip content from `src` into `dst`, keeping `dst.index` unchanged.
/// Regenerates filter node IDs and remaps `filter.<uuid>.<param>` property mappings.
void copyCellContent(Cell& dst, const Cell& src);

/// Copies layer/playback settings (props incl. preferred mix layer, keying, mixing,
/// picture, feedback), effect, filter chain, and MIDI/property mappings from `src`
/// into `dst`, but keeps `dst.visual` and `dst.name` unchanged.
void copyCellLayerSettings(Cell& dst, const Cell& src);

/// Same as `copyCellLayerSettings` (alias for mapping-menu apply).
void copyCellSettings(Cell& dst, const Cell& src);

/// Paste `src` onto `dst` according to `mode` (see CellPasteMode).
void pasteCellContent(Cell& dst, const Cell& src, CellPasteMode mode);

/// Copies only the preferred mix-layer slot (`props.preferredLayer`).
void copyCellPreferredLayer(Cell& dst, const Cell& src);

/// Copies only MIDI/property mappings. Filter-parameter mappings are remapped onto
/// matching destination filter nodes (same typeId at the same chain index); unmapped
/// filter mappings are skipped.
void copyCellMidiPropertyMappings(Cell& dst, const Cell& src);

} // namespace pvj::core

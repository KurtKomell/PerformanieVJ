#pragma once

#include "core/Model.h"

namespace pvj::core {

/// Copies clip content from `src` into `dst`, keeping `dst.index` unchanged.
/// Regenerates filter node IDs and remaps `filter.<uuid>.<param>` property mappings.
void copyCellContent(Cell& dst, const Cell& src);

/// Copies layer/playback settings (props incl. preferred mix layer, keying, mixing,
/// picture, feedback), effect, filter chain, and MIDI/property mappings from `src`
/// into `dst`, but keeps `dst.visual` and `dst.name` unchanged.
void copyCellLayerSettings(Cell& dst, const Cell& src);

/// Same as `copyCellLayerSettings` (alias for mapping-menu apply).
void copyCellSettings(Cell& dst, const Cell& src);

} // namespace pvj::core

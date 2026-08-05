#pragma once

#include "core/FilterEffectIds.h"
#include "core/Model.h"
#include "render/ShaderLibrary.h"

#include <QSize>

namespace pvj::render {

/// Fill the 64-byte effect UBO for a filter node.
void packFilterUniformBuffer(EffectQuadUbo2& ubo, const pvj::core::CellFilterNode& node,
                             const QSize& pixelSize, float elapsedSec, int internalPass,
                             const float keyChannelRgb[3], quint32 presentFrame = 0);

} // namespace pvj::render

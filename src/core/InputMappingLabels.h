#pragma once

#include "Model.h"

#include <QString>

namespace pvj::core {

/// Human-readable label for a property MIDI mapping, e.g. "CH1 CC7" or "CH2 N40".
QString formatPropertyMappingLabel(const PropertyMapping& mapping);

/// Human-readable label for a trigger input, e.g. "CH1 N40". Empty if input is None.
QString formatTriggerInputLabel(InputType input, int channel, int number);

} // namespace pvj::core

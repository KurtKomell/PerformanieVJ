#pragma once

#include "core/Model.h"

namespace pvj::core {

/// Copies clip content of every cell from `src` into `dst`, keeping `dst.index` and `dst.name`.
void copyBankContent(Bank& dst, const Bank& src);

} // namespace pvj::core

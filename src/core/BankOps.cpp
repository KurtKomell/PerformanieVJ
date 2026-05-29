#include "core/BankOps.h"

#include "core/CellOps.h"

namespace pvj::core {

void copyBankContent(Bank& dst, const Bank& src)
{
    const int n = qMin(dst.cells.size(), src.cells.size());
    for (int i = 0; i < n; ++i) {
        copyCellContent(dst.cells[i], src.cells[i]);
    }
}

} // namespace pvj::core

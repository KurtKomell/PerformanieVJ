#pragma once

#include "Project.h"

#include <QString>
#include <QStringList>

namespace pvj::core {

// Imports GrandVJ .vj2 project files (read-only), including the GrandVJ 2.7 XML
// layout (<GrandVJ>/<PROJECT>/<LIBRARY>/<BANKSET>/<CELLS>/<CELL> with VALUE
// children) and the older attribute-based export. Data is mapped onto our
// internal Project model; unknown effects/attributes are logged in `warnings`.
// After a successful import, the Project carries no filePath and sourceFormat
// == "vj2"; saving will produce a .pvj file.
class Vj2Importer
{
public:
    struct Result {
        bool        ok = false;
        QString     errorMessage;
        QStringList warnings;
        int         banksMerged = 0; ///< set by mergeFile()
    };

    static Result importFile(Project& project, const QString& filePath);

    /// Import banks from a .vj2 into an existing project without replacing it.
    /// `destinationStartBank` / `sourceStartBank` are 0-based indices.
    /// Media is merged by path (UUIDs remapped); cell keyboard/MIDI triggers from
    /// the file replace matching cell-slot triggers on the destination.
    static Result mergeFile(Project& destination, const QString& filePath,
                            int destinationStartBank, int sourceStartBank = 0);
};

} // namespace pvj::core

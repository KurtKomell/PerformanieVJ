#pragma once

#include "Project.h"

#include <QString>
#include <QStringList>

namespace pvj::core {

// Imports GrandVJ .vj2 project files (read-only). Data is mapped onto our
// internal Project model; unknown attributes are ignored but logged into
// `warnings`. After a successful import, the Project carries no filePath and
// sourceFormat == "vj2"; saving will produce a .pvj file.
class Vj2Importer
{
public:
    struct Result {
        bool        ok = false;
        QString     errorMessage;
        QStringList warnings;
    };

    static Result importFile(Project& project, const QString& filePath);
};

} // namespace pvj::core

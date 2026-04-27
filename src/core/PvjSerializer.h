#pragma once

#include "Project.h"

#include <QString>

namespace pvj::core {

// Native .pvj format: single XML file using a schema owned by PerformanieVJ.
// Thumbnails and other per-project artifacts are stored as sidecars in the
// folder "<project>.pvj.thumbs/" next to the .pvj file.
class PvjSerializer
{
public:
    struct Result {
        bool    ok = false;
        QString errorMessage;
    };

    static Result save(const Project& project, const QString& filePath);
    static Result load(Project& project, const QString& filePath);
};

} // namespace pvj::core
